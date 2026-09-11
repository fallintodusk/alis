from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .adapters import decode_sources
from .acquisition import build_ledger, locate_sources, materialize_sources
from .contracts import (
    RESULT_SCHEMA,
    IngestionError,
    canonical_hash,
    exclusive_file_lock,
    file_hash,
    read_json,
    validate_document,
    write_json,
)
from .profiles import build_plan, validate_profile
from World.ExecutionEnvironment.api import (
    ExecutionEnvironmentError,
    bootstrap_tools,
    dependency_lock_hash,
    verify_python_runtime,
)

from .run_identity import default_output_root, profile_path, run_contract, source_data_root


OPERATIONS = ("plan", "fetch", "verify", "decode", "run")
@dataclass(frozen=True)
class ProfileContext:
    profile_path: Path
    profile: dict[str, Any]
    run_contract: dict[str, Any]
    cache_root: Path
    output_root: Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[4]


def _path(repo_root: Path, value: str | None, fallback: Path) -> Path:
    if not value:
        return fallback
    supplied = Path(value)
    return supplied.resolve() if supplied.is_absolute() else (repo_root / supplied).resolve()


def _artifact_path(path: Path, output_root: Path) -> str:
    try:
        return path.resolve().relative_to(output_root.resolve()).as_posix()
    except ValueError as exc:
        raise IngestionError("invalid_output_path", "Artifact escaped the declared output root", path=str(path)) from exc


def _file_output(kind: str, path: Path, output_root: Path, **fields: Any) -> dict[str, Any]:
    if not path.is_file():
        raise IngestionError("missing_output", "Declared output file is missing", kind=kind)
    return {
        "kind": kind,
        "path": _artifact_path(path, output_root),
        "sha256": file_hash(path),
        "byte_size": path.stat().st_size,
        **fields,
    }


def _result(
    operation: str,
    status: str,
    profile_id: str | None,
    input_hash: str | None,
    outputs: list[dict[str, Any]],
    errors: list[dict[str, Any]] | None = None,
) -> dict[str, Any]:
    suffix = (input_hash or "none")[:12]
    return {
        "$schema": RESULT_SCHEMA,
        "schema_version": 1,
        "operation_id": f"{operation}:{profile_id or 'none'}:{suffix}",
        "operation": operation,
        "status": status,
        "profile_id": profile_id,
        "inputs_hash": input_hash,
        "path_base": "output_root",
        "outputs": outputs,
        "errors": errors or [],
    }


def _run_profile_operation(
    args: argparse.Namespace,
    repo_root: Path,
    profile_path: Path,
    profile: dict[str, Any],
    run_contract: dict[str, Any],
    cache_root: Path,
    output_root: Path,
) -> dict[str, Any]:
    run_contract_path = output_root / "run_contract.json"
    write_json(run_contract_path, run_contract)
    plan = build_plan(profile, repo_root)
    plan_path = output_root / "plan.json"
    write_json(plan_path, plan)
    outputs = [
        _file_output("run_contract", run_contract_path, output_root),
        _file_output("plan", plan_path, output_root),
    ]
    if args.operation == "plan":
        return _result("plan", "accepted", profile["profile_id"], run_contract["run_inputs_hash"], outputs)

    if args.operation in {"fetch", "run"}:
        paths = materialize_sources(profile, profile_path, repo_root, cache_root)
    else:
        paths = locate_sources(profile, profile_path, cache_root)
    ledger = build_ledger(profile, paths)
    ledger_path = output_root / "source_ledger.json"
    write_json(ledger_path, ledger)
    outputs.append(_file_output("source_ledger", ledger_path, output_root))
    if args.operation in {"decode", "run"}:
        if args.operation == "run" and any(source["adapter"] != "synthetic_json" for source in profile["sources"]):
            receipt = bootstrap_tools(repo_root)
            outputs.append({"kind": "toolchain_receipt", "lock_sha256": receipt["lock_sha256"]})
        decoded = decode_sources(profile, paths, ledger, output_root / "normalized", repo_root)
        for item in decoded:
            path = item.pop("path")
            kind = item.pop("kind")
            item.pop("sha256", None)
            item.pop("byte_size", None)
            outputs.append(_file_output(kind, path, output_root, **item))
    return _result(args.operation, "accepted", profile["profile_id"], run_contract["run_inputs_hash"], outputs)


def _prepare_profile(args: argparse.Namespace, repo_root: Path) -> ProfileContext:
    try:
        verify_python_runtime(repo_root)
    except (OSError, RuntimeError, ValueError) as exc:
        raise IngestionError("python_environment_invalid", str(exc)) from exc
    selected_profile_path = profile_path(repo_root, args.profile)
    profile = read_json(selected_profile_path)
    validate_profile(profile, selected_profile_path, repo_root)
    selected_run_contract = run_contract(repo_root, profile)
    cache_root = _path(repo_root, args.cache_root, source_data_root(repo_root) / "cache")
    default_root = default_output_root(repo_root, profile, selected_run_contract["run_inputs_hash"], args.operation)
    output_root = _path(repo_root, args.output_root, default_root)
    return ProfileContext(selected_profile_path, profile, selected_run_contract, cache_root, output_root)


def _output_lock(output_root: Path) -> Path:
    return output_root.parent / ".locks" / f"{canonical_hash({'output_root': str(output_root.resolve())})}.lock"


def _accepted_existing(
    output_root: Path, operation: str, profile_id: str, inputs_hash: str
) -> dict[str, Any] | None:
    result_path = output_root / f"{operation}_result.json"
    if not result_path.is_file():
        return None
    try:
        result = read_json(result_path)
        validate_document(result, result_path)
    except IngestionError:
        result_path.unlink(missing_ok=True)
        return None
    if result.get("status") != "accepted":
        return None
    if result.get("profile_id") != profile_id or result.get("inputs_hash") != inputs_hash:
        raise IngestionError("output_contract_conflict", "Output root contains another accepted source run")
    for entry in result["outputs"]:
        if "path" not in entry:
            continue
        path = (output_root / entry["path"]).resolve()
        if (
            not path.is_relative_to(output_root.resolve())
            or not path.is_file()
            or path.stat().st_size != entry["byte_size"]
            or file_hash(path) != entry["sha256"]
        ):
            raise IngestionError(
                "output_identity_conflict", "Accepted source output differs at the same identity", path=entry["path"]
            )
    return result


def _execute_profile(
    args: argparse.Namespace, repo_root: Path, context: ProfileContext | None = None
) -> tuple[dict[str, Any], Path]:
    context = context or _prepare_profile(args, repo_root)
    context.output_root.parent.mkdir(parents=True, exist_ok=True)
    with exclusive_file_lock(_output_lock(context.output_root)):
        existing = _accepted_existing(
            context.output_root,
            args.operation,
            context.profile["profile_id"],
            context.run_contract["run_inputs_hash"],
        )
        if existing is not None:
            return existing, context.output_root
        staging = context.output_root.with_name(f"{context.output_root.name}.staging-{os.getpid()}")
        if staging.exists():
            shutil.rmtree(staging)
        staging.mkdir(parents=True)
        try:
            result = _run_profile_operation(
                args, repo_root, context.profile_path, context.profile, context.run_contract,
                context.cache_root, staging,
            )
            write_json(staging / f"{args.operation}_result.json", result)
            if context.output_root.exists():
                shutil.rmtree(context.output_root)
            os.replace(staging, context.output_root)
        except Exception:
            if staging.exists():
                shutil.rmtree(staging)
            raise
    return result, context.output_root


def _exit_code(error: IngestionError) -> int:
    if error.code in {"profile_missing", "invalid_json", "unsupported_profile", "invalid_area", "invalid_budget"}:
        return 2
    if error.code.startswith("fetch"):
        return 4
    if error.code.startswith("tool") or error.code in {"unsupported_platform", "unsupported_archive", "unsafe_archive"}:
        return 5
    return 3


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="ALIS provider-preserving world source ingestion")
    subcommands = parser.add_subparsers(dest="operation", required=True)
    subcommands.add_parser("bootstrap-tools", help="Fetch and verify the pinned portable source tools")
    for operation in OPERATIONS:
        command = subcommands.add_parser(operation)
        command.add_argument("--profile", default="synthetic_two_cell")
        command.add_argument("--cache-root")
        command.add_argument("--output-root")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    repo_root = _repo_root()
    fallback_root = source_data_root(repo_root) / "runs" / "failures" / args.operation
    output_root = _path(repo_root, getattr(args, "output_root", None), fallback_root)
    context: ProfileContext | None = None
    try:
        if args.operation == "bootstrap-tools":
            try:
                verify_python_runtime(repo_root)
            except (OSError, RuntimeError, ValueError) as exc:
                raise IngestionError("python_environment_invalid", str(exc)) from exc
            bootstrap_hash = canonical_hash({
                "toolchain_lock_sha256": file_hash(repo_root / "tools" / "World" / "ExecutionEnvironment" / "toolchain.lock.json"),
                "python_dependency_lock_sha256": dependency_lock_hash(repo_root),
            })
            output_root = source_data_root(repo_root) / "runs" / "bootstrap-tools" / bootstrap_hash
            with exclusive_file_lock(output_root / ".ingestion.lock"):
                result_path = output_root / "bootstrap-tools_result.json"
                result_path.unlink(missing_ok=True)
                receipt = bootstrap_tools(repo_root)
                outputs = [{"kind": "toolchain_receipt", "receipt": receipt}]
                result = _result("bootstrap-tools", "accepted", None, bootstrap_hash, outputs)
                write_json(result_path, result)
        else:
            context = _prepare_profile(args, repo_root)
            output_root = context.output_root
            result, output_root = _execute_profile(args, repo_root, context)
        print(json.dumps(result, ensure_ascii=True, separators=(",", ":")))
        return 0
    except (IngestionError, ExecutionEnvironmentError) as error:
        input_hash = context.run_contract["run_inputs_hash"] if context else None
        profile_id = context.profile["profile_id"] if context else getattr(args, "profile", None)
        if error.code == "output_contract_conflict":
            output_root = fallback_root / (input_hash or "unknown")
        result = _result(
            args.operation,
            "rejected" if _exit_code(error) == 3 else "failed",
            profile_id,
            input_hash,
            [],
            [{"code": error.code, "message": str(error), "details": error.details}],
        )
        with exclusive_file_lock(_output_lock(output_root)):
            write_json(output_root / f"{args.operation}_result.json", result)
        print(json.dumps(result, ensure_ascii=True, separators=(",", ":")), file=sys.stderr)
        return _exit_code(error)
    except Exception as error:
        input_hash = context.run_contract["run_inputs_hash"] if context else None
        profile_id = context.profile["profile_id"] if context else getattr(args, "profile", None)
        result = _result(
            args.operation, "failed", profile_id, input_hash, [],
            [{"code": "internal_error", "message": "Unexpected ingestion failure", "details": {"type": type(error).__name__}}],
        )
        with exclusive_file_lock(_output_lock(output_root)):
            write_json(output_root / f"{args.operation}_result.json", result)
        print(json.dumps(result, ensure_ascii=True, separators=(",", ":")), file=sys.stderr)
        return 6
