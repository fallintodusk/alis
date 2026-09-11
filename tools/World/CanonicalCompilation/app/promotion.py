from __future__ import annotations

import json
import hashlib
import os
import shutil
import zipfile
from pathlib import Path, PurePosixPath
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, exclusive_file_lock
from World.SourceIngestion.api import validate_document as validate_source_document

from .artifacts import accepted_base
from .contracts import CompilerError, canonical_hash, file_hash, read_json, resolve_owned_path, validate_document, write_json
from .pipeline import compiler_implementation_hash, compiler_run_inputs_hash, load_profile
from .territory_admission import (
    admit_territory,
    current_external_inputs,
    load_budget,
    owner_data_root,
    validate_profile_ownership,
)


OBSERVATIONAL_OUTPUT_PATHS = {"reports/metrics.json"}


def _authority_root(repo_root: Path, profile: dict[str, Any]) -> Path:
    return owner_data_root(repo_root, profile["world_data_plugin"]) / "Canonical" / profile["profile_id"]


def _validate_budget(repo_root: Path, profile: dict[str, Any], result_path: Path, result: dict[str, Any]) -> None:
    budget_record = load_budget(repo_root, profile)
    if budget_record is None:
        return
    budget, _ = budget_record
    source_path = resolve_owned_path(repo_root, profile["source_profile"])
    source = read_json(source_path)
    validate_document(source, source_path)
    metrics_entries = [entry for entry in result["outputs"] if entry["path"] == "reports/metrics.json"]
    if len(metrics_entries) != 1:
        raise CompilerError("promotion_budget_evidence_missing", "Compile result has no unique metrics report")
    metrics_path = resolve_owned_path(result_path.parent.resolve(), metrics_entries[0]["path"])
    metrics = read_json(metrics_path)
    validate_document(metrics, metrics_path)
    if metrics["profile_id"] != profile["profile_id"] or metrics["cell_count"] != len(profile["target_cells"]):
        raise CompilerError("promotion_scope_mismatch", "Compiler metrics differ from the promoted profile scope")
    actuals = {
        "source_bytes": (sum(item["expected_bytes"] for item in source["sources"] if item["adapter"] != "synthetic_json"), "bytes"),
        "canonical_bytes": (metrics["canonical_bytes"], "bytes"),
        "full_compile_seconds": (metrics["duration_ms"] / 1000.0, "seconds"),
        "features": (metrics["feature_count"], "canonical_features"),
        "cells": (metrics["cell_count"], "canonical_cells"),
    }
    for name, (actual, unit) in actuals.items():
        ceiling = budget["ceilings"][name]
        if ceiling["kind"] != "hard" or ceiling["unit"] != unit:
            raise CompilerError("promotion_budget_invalid", "Canonical budget ceiling has invalid semantics", ceiling=name)
        if actual > ceiling["max"]:
            raise CompilerError(
                "promotion_budget_exceeded", "Canonical compile exceeds its frozen hard budget", ceiling=name, actual=actual
            )


def _candidate_files(
    profile_path: Path,
    profile: dict[str, Any],
    result_path: Path,
    external_inputs: dict[str, Any],
) -> tuple[dict[str, Any], list[tuple[str, Path]]]:
    result = read_json(result_path)
    validate_document(result, result_path)
    if result.get("status") != "accepted" or result.get("operation") != "compile":
        raise CompilerError("promotion_result_rejected", "Canonical promotion requires an accepted compile result")
    if result.get("profile_id") != profile["profile_id"]:
        raise CompilerError("promotion_profile_mismatch", "Compile result belongs to another profile")
    root = result_path.parent.resolve()
    files: list[tuple[str, Path]] = []
    seen: set[str] = set()
    for entry in result["outputs"]:
        relative = entry["path"]
        if relative in seen or PurePosixPath(relative).suffix.lower() != ".json":
            raise CompilerError("promotion_output_invalid", "Canonical output must be a schema-valid JSON document", path=relative)
        seen.add(relative)
        path = resolve_owned_path(root, relative)
        if path.stat().st_size != entry["byte_size"] or file_hash(path) != entry["sha256"]:
            raise CompilerError("promotion_output_changed", "Compile output differs from its receipt", path=relative)
        if path.suffix.lower() == ".json":
            validate_document(read_json(path), path)
        if relative not in OBSERVATIONAL_OUTPUT_PATHS:
            files.append((relative, path))
    if "compile_result.json" in seen:
        raise CompilerError("promotion_output_invalid", "Compile result cannot list itself as an output")
    contract_path = resolve_owned_path(root, "run_contract.json")
    contract = read_json(contract_path)
    validate_document(contract, contract_path)
    if (
        contract.get("profile_id") != result["profile_id"]
        or contract.get("run_inputs_hash") != result["inputs_hash"]
        or contract.get("profile_sha256") != file_hash(profile_path)
        or contract.get("source_inputs_hash") != external_inputs["source_run_inputs_hash"]
        or contract.get("overlay_sha256") != external_inputs["authored_overlay_sha256"]
        or contract.get("fixture_features_sha256") != external_inputs["fixture_features_sha256"]
        or contract.get("implementation_sha256") != compiler_implementation_hash()
        or contract.get("run_inputs_hash") != compiler_run_inputs_hash(contract)
    ):
        raise CompilerError("promotion_profile_changed", "Compiler profile differs from the accepted run")
    canonical_result = {
        **result,
        "outputs": [entry for entry in result["outputs"] if entry["path"] not in OBSERVATIONAL_OUTPUT_PATHS],
    }
    validate_document(canonical_result, result_path)
    return canonical_result, sorted(files)


def canonical_active_path(repo_root: Path, profile_path: Path) -> Path:
    return _authority_root(repo_root, load_profile(profile_path)) / "active.json"


def _result_bytes(result: dict[str, Any]) -> bytes:
    return (json.dumps(result, indent=2, ensure_ascii=True) + "\n").encode("utf-8")


def _write_deterministic_bundle(path: Path, files: list[tuple[str, Path]], result: dict[str, Any]) -> None:
    entries: list[tuple[str, Path | bytes]] = [*files, ("compile_result.json", _result_bytes(result))]
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for relative, source in sorted(entries):
            info = zipfile.ZipInfo(relative, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            with bundle.open(info, "w", force_zip64=True) as target:
                if isinstance(source, bytes):
                    target.write(source)
                else:
                    with source.open("rb") as stream:
                        for block in iter(lambda: stream.read(1024 * 1024), b""):
                            target.write(block)


def _promote_canonical_locked(
    repo_root: Path,
    profile_path: Path,
    result_path: Path,
    authority_root: Path | None = None,
) -> tuple[dict[str, Any], Path]:
    profile = load_profile(profile_path)
    validate_profile_ownership(repo_root, profile_path, profile)
    external_inputs = current_external_inputs(repo_root, profile_path, profile)
    admission_receipt_sha256 = None
    admission_files: list[tuple[str, Path]] = []
    admission_artifacts: list[dict[str, Any]] = []
    if profile.get("budget_profile") is not None or profile.get("control_profile") is not None:
        admission, admission_path = admit_territory(repo_root, profile_path)
        admission_receipt_sha256 = file_hash(admission_path)
        admission_sources = [("territory_admission_receipt.json", admission_path)] + [
            (entry["path"], resolve_owned_path(admission_path.parent, entry["path"]))
            for entry in admission["evidence"]
        ]
        for name, source in admission_sources:
            relative = f"admission/{name}"
            admission_files.append((relative, source))
            admission_artifacts.append({
                "path": relative,
                "sha256": file_hash(source),
                "byte_size": source.stat().st_size,
            })
    result, files = _candidate_files(profile_path, profile, result_path, external_inputs)
    _validate_budget(repo_root, profile, result_path, read_json(result_path))
    root = authority_root.resolve() if authority_root else _authority_root(repo_root, profile).resolve()
    bundle_identity = canonical_hash({
        "compile_inputs_hash": result["inputs_hash"],
        "admission_receipt_sha256": admission_receipt_sha256,
    })
    staging_root = repo_root / "tmp" / "world" / "canonical_compilation" / "promotion"
    staging_root.mkdir(parents=True, exist_ok=True)
    staging = staging_root / f"{profile['profile_id']}.{bundle_identity}.zip.tmp"
    staging.unlink(missing_ok=True)
    _write_deterministic_bundle(staging, [*files, *admission_files], result)
    bundle_relative = f"bundles/{bundle_identity}.zip"
    bundle_path = root / bundle_relative
    bundle_path.parent.mkdir(parents=True, exist_ok=True)
    if bundle_path.exists():
        if file_hash(bundle_path) != file_hash(staging):
            staging.unlink()
            raise CompilerError("canonical_authority_conflict", "Immutable canonical bundle already differs")
        staging.unlink()
    else:
        os.replace(staging, bundle_path)
    schema_path = Path(__file__).resolve().parents[1] / "contracts" / "canonical-authority.schema.json"
    active_path = root / "active.json"
    authority = {
        "$schema": Path(os.path.relpath(schema_path, active_path.parent)).as_posix(),
        "schema_version": 1,
        "authority_id": f"{profile['profile_id']}:{bundle_identity}",
        "world_data_plugin": profile["world_data_plugin"],
        "profile_id": profile["profile_id"],
        "inputs_hash": result["inputs_hash"],
        "profile_sha256": file_hash(profile_path),
        "external_inputs": external_inputs,
        "admission_receipt_sha256": admission_receipt_sha256,
        "admission_artifacts": admission_artifacts,
        "compile_result_sha256": file_hash_bytes(_result_bytes(result)),
        "output_count": len(files),
        "bundle": {
            "format": "deterministic_zip_v1",
            "path": bundle_relative,
            "byte_size": bundle_path.stat().st_size,
            "sha256": file_hash(bundle_path),
        },
    }
    candidate_active_path = active_path.with_suffix(active_path.suffix + ".candidate")
    candidate_active_path.unlink(missing_ok=True)
    write_json(candidate_active_path, authority)
    try:
        validate_canonical_authority(repo_root, candidate_active_path, profile_path)
    except Exception:
        candidate_active_path.unlink(missing_ok=True)
        raise
    os.replace(candidate_active_path, active_path)
    return authority, active_path


def promote_canonical(
    repo_root: Path,
    profile_path: Path,
    result_path: Path,
    authority_root: Path | None = None,
) -> tuple[dict[str, Any], Path]:
    lock_identity = canonical_hash({"profile_path": str(profile_path.resolve())})
    lock_path = (
        repo_root / "tmp" / "world" / "canonical_compilation" / "promotion"
        / "locks" / f"{lock_identity}.lock"
    )
    try:
        with exclusive_file_lock(lock_path):
            return _promote_canonical_locked(repo_root, profile_path, result_path, authority_root)
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code, str(error)) from error


def validate_canonical_authority(repo_root: Path, active_path: Path, profile_path: Path) -> dict[str, Any]:
    authority = read_json(active_path)
    validate_document(authority, active_path)
    profile = load_profile(profile_path)
    if (
        authority["profile_id"] != profile["profile_id"]
        or authority["world_data_plugin"] != profile["world_data_plugin"]
        or authority["profile_sha256"] != file_hash(profile_path)
    ):
        raise CompilerError("canonical_profile_changed", "Canonical authority belongs to another compiler profile")
    current_files = current_external_inputs(repo_root, profile_path, profile, include_source_runtime=False)
    accepted_files = {key: value for key, value in authority["external_inputs"].items() if key != "source_run_inputs_hash"}
    if current_files != accepted_files:
        raise CompilerError("canonical_external_input_changed", "Canonical authority external inputs have changed")
    bundle_identity = canonical_hash({
        "compile_inputs_hash": authority["inputs_hash"],
        "admission_receipt_sha256": authority["admission_receipt_sha256"],
    })
    if (
        authority["authority_id"] != f"{authority['profile_id']}:{bundle_identity}"
        or authority["bundle"]["path"] != f"bundles/{bundle_identity}.zip"
    ):
        raise CompilerError("canonical_authority_identity_invalid", "Canonical authority identity is not derived from its inputs")
    bundle_path = resolve_owned_path(active_path.parent, authority["bundle"]["path"])
    if bundle_path.stat().st_size != authority["bundle"]["byte_size"] or file_hash(bundle_path) != authority["bundle"]["sha256"]:
        raise CompilerError("canonical_bundle_changed", "Canonical authority bundle differs from its active record")
    with zipfile.ZipFile(bundle_path) as bundle:
        names = bundle.namelist()
        if len(names) != len(set(names)) or any(
            PurePosixPath(name).is_absolute() or ".." in PurePosixPath(name).parts for name in names
        ):
            raise CompilerError("canonical_bundle_invalid", "Canonical bundle paths are unsafe or duplicated")
        if "compile_result.json" not in names:
            raise CompilerError("canonical_bundle_invalid", "Canonical bundle has no compile result")
        result = json.loads(bundle.read("compile_result.json"))
        validate_document(result, Path("compile_result.json"))
        if (
            result.get("status") != "accepted"
            or result.get("operation") != "compile"
            or result.get("profile_id") != authority["profile_id"]
            or result.get("inputs_hash") != authority["inputs_hash"]
        ):
            raise CompilerError("canonical_bundle_invalid", "Canonical compile result identity differs from authority")
        if file_hash_bytes(bundle.read("compile_result.json")) != authority["compile_result_sha256"]:
            raise CompilerError("canonical_bundle_changed", "Bundled compile result differs from authority")
        expected = {
            "compile_result.json",
            *(entry["path"] for entry in result["outputs"]),
            *(entry["path"] for entry in authority["admission_artifacts"]),
        }
        if set(names) != expected or len(result["outputs"]) != authority["output_count"]:
            raise CompilerError("canonical_bundle_invalid", "Canonical bundle inventory differs from compile receipt")
        for entry in result["outputs"]:
            payload = bundle.read(entry["path"])
            if len(payload) != entry["byte_size"] or file_hash_bytes(payload) != entry["sha256"]:
                raise CompilerError("canonical_bundle_changed", "Bundled output differs from compile receipt", path=entry["path"])
            if entry["path"].endswith(".json"):
                validate_document(json.loads(payload), Path(entry["path"]))
        admission_receipt = None
        for entry in authority["admission_artifacts"]:
            payload = bundle.read(entry["path"])
            if len(payload) != entry["byte_size"] or file_hash_bytes(payload) != entry["sha256"]:
                raise CompilerError("canonical_bundle_changed", "Bundled admission evidence differs", path=entry["path"])
            document = json.loads(payload)
            if str(document.get("$schema", "")).startswith("https://alis.world/schemas/world-source/"):
                validate_source_document(document, Path(entry["path"]))
            else:
                validate_document(document, Path(entry["path"]))
            if entry["path"] == "admission/territory_admission_receipt.json":
                admission_receipt = document
        receipt_descriptor = next(
            (entry for entry in authority["admission_artifacts"] if entry["path"] == "admission/territory_admission_receipt.json"),
            None,
        )
        expects_admission = authority["external_inputs"]["budget_profile_path"] is not None
        if (
            (receipt_descriptor or {}).get("sha256") != authority["admission_receipt_sha256"]
            or expects_admission != (admission_receipt is not None)
        ):
            raise CompilerError("canonical_bundle_invalid", "Canonical admission receipt identity differs")
        if admission_receipt is not None:
            external = authority["external_inputs"]
            if (
                admission_receipt["compiler_profile_sha256"] != authority["profile_sha256"]
                or admission_receipt["source_profile_sha256"] != external["source_profile_sha256"]
                or admission_receipt["source_run_inputs_hash"] != external["source_run_inputs_hash"]
                or admission_receipt["control_profile_sha256"] != external["control_profile_sha256"]
                or admission_receipt["budget_profile_sha256"] != external["budget_profile_sha256"]
            ):
                raise CompilerError("canonical_bundle_invalid", "Canonical admission evidence differs from authority")
            bundled_evidence = {
                entry["path"].removeprefix("admission/"): {
                    "path": entry["path"].removeprefix("admission/"),
                    "sha256": entry["sha256"],
                    "byte_size": entry["byte_size"],
                }
                for entry in authority["admission_artifacts"]
                if entry["path"] != "admission/territory_admission_receipt.json"
            }
            if bundled_evidence != {entry["path"]: entry for entry in admission_receipt["evidence"]}:
                raise CompilerError("canonical_bundle_invalid", "Canonical admission artifact inventory differs")
    return authority


def file_hash_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def materialize_canonical(repo_root: Path, active_path: Path, profile_path: Path) -> Path:
    authority = validate_canonical_authority(repo_root, active_path, profile_path)
    lock_path = (
        repo_root / "tmp" / "world" / "canonical_compilation" / "materialized"
        / "locks" / f"{authority['bundle']['sha256']}.lock"
    )
    try:
        with exclusive_file_lock(lock_path):
            return _materialize_canonical_locked(repo_root, active_path, authority)
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code, str(error)) from error


def _materialize_canonical_locked(repo_root: Path, active_path: Path, authority: dict[str, Any]) -> Path:
    bundle_path = resolve_owned_path(active_path.parent, authority["bundle"]["path"])
    output_root = (
        repo_root / "tmp" / "world" / "canonical_compilation" / "materialized"
        / authority["profile_id"] / authority["bundle"]["sha256"]
    )
    result_path = output_root / "compile_result.json"
    if result_path.is_file():
        accepted_base(result_path)
        return result_path
    staging = output_root.with_name(output_root.name + ".staging")
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)
    with zipfile.ZipFile(bundle_path) as bundle:
        for name in bundle.namelist():
            target = (staging / name).resolve()
            if not target.is_relative_to(staging.resolve()):
                raise CompilerError("canonical_bundle_invalid", "Canonical bundle path escapes materialization")
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(bundle.read(name))
    output_root.parent.mkdir(parents=True, exist_ok=True)
    os.replace(staging, output_root)
    accepted_base(result_path)
    return result_path
