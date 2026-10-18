from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from .control_network import qualify_control_network
from .contracts import CompilerError, file_hash, read_json, resolve_owned_path, validate_document
from .pipeline import compile_world, plan_world, repo_root
from .promotion import canonical_active_path, materialize_canonical, promote_canonical, validate_canonical_authority
from .territory_admission import admit_territory


def _path(value: str | None) -> Path | None:
    if value is None:
        return None
    supplied = Path(value)
    return supplied.resolve() if supplied.is_absolute() else (repo_root() / supplied).resolve()


def _bounds(value: str) -> tuple[float, float, float, float]:
    try:
        west, south, east, north = (float(item) for item in value.split(","))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("bounds must use west,south,east,north") from exc
    if west >= east or south >= north:
        raise argparse.ArgumentTypeError("bounds must have positive width and height")
    return west, south, east, north


def _validate_result(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise CompilerError("result_missing", "Compiler result does not exist", path=str(path))
    result = read_json(path)
    validate_document(result, path)
    if result.get("status") != "accepted":
        raise CompilerError("result_rejected", "Compiler result is not accepted")
    root = path.parent
    for entry in result["outputs"]:
        output = resolve_owned_path(root, entry["path"])
        if output.stat().st_size != entry["byte_size"] or file_hash(output) != entry["sha256"]:
            raise CompilerError("accepted_output_changed", "Compiler output differs from its receipt", path=entry["path"])
    return {
        "$schema": "https://alis.world/schemas/world-compiler/compile-result-v1.json",
        "schema_version": 1,
        "operation_id": f"validate:{result['profile_id']}:{result['inputs_hash'][:12]}",
        "operation": "validate",
        "status": "accepted",
        "profile_id": result["profile_id"],
        "inputs_hash": result["inputs_hash"],
        "path_base": "output_root",
        "outputs": [{
            "kind": "compile_result",
            "path": path.name,
            "sha256": file_hash(path),
            "byte_size": path.stat().st_size,
        }],
        "errors": [],
    }


def _failure(error: CompilerError, profile: str | None) -> dict[str, Any]:
    return {
        "$schema": "https://alis.world/schemas/world-compiler/compile-result-v1.json",
        "schema_version": 1,
        "operation_id": f"compile:{profile or 'none'}:failed",
        "operation": "compile",
        "status": "failed",
        "profile_id": profile,
        "inputs_hash": None,
        "path_base": "output_root",
        "outputs": [],
        "errors": [{"code": error.code, "message": str(error), "details": error.details}],
    }


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Compile accepted world source records into canonical ALIS cells")
    subparsers = parser.add_subparsers(dest="operation", required=True)
    run = subparsers.add_parser("run")
    run.add_argument("--profile", required=True)
    run.add_argument("--source-result")
    run.add_argument("--output-root")
    run.add_argument("--base-result")
    run.add_argument("--terrain-change-bounds", action="append", type=_bounds, default=[])
    run.add_argument("--feature-change-id", action="append", default=[])
    run.add_argument("--dry-run", action="store_true")
    validate = subparsers.add_parser("validate")
    validate.add_argument("--result", required=True)
    controls = subparsers.add_parser("controls")
    controls.add_argument("--profile", required=True)
    admit = subparsers.add_parser("admit")
    admit.add_argument("--profile", required=True)
    promote = subparsers.add_parser("promote")
    promote.add_argument("--profile", required=True)
    promote.add_argument("--result", required=True)
    authority = subparsers.add_parser("authority")
    authority.add_argument("--profile", required=True)
    materialize = subparsers.add_parser("materialize")
    materialize.add_argument("--profile", required=True)
    prepare_selection = subparsers.add_parser("prepare-selection-transition")
    prepare_selection.add_argument("--profile", required=True)
    prepare_selection.add_argument("--candidate", required=True)
    prepare_selection.add_argument("--expected-active-sha256", required=True)
    selection = subparsers.add_parser("selection-transition")
    selection.add_argument("--transition", required=True)
    selection.add_argument("--action", required=True, choices=("validate", "publish", "restore"))
    schema_reference = subparsers.add_parser("prepare-schema-reference-transition")
    schema_reference.add_argument("--profile", required=True)
    schema_reference.add_argument("--expected-active-sha256", required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    profile: str | None = None
    args = None
    try:
        args = _parser().parse_args(argv)
        if args.operation == "prepare-schema-reference-transition":
            from ..refinement.api import prepare_schema_reference_transition
            profile = args.profile
            result = prepare_schema_reference_transition(repo_root(), _path(args.profile) or Path(),
                args.expected_active_sha256)
        elif args.operation == "prepare-selection-transition":
            from ..refinement.api import prepare_selection_transition
            profile = args.profile
            result = prepare_selection_transition(repo_root(), _path(args.profile) or Path(),
                _path(args.candidate) or Path(), args.expected_active_sha256)
        elif args.operation == "selection-transition":
            from ..refinement.api import validate_selection_transition, publish_selection_transition
            transition_path = _path(args.transition) or Path()
            if not transition_path.is_relative_to(repo_root() / 'tmp'):
                raise CompilerError('canonical_selection_unproven', 'Selection recovery input must belong to repository tmp')
            transition = read_json(transition_path)
            if args.action == 'validate':
                result = validate_selection_transition(repo_root(), transition)
            else:
                result = publish_selection_transition(repo_root(), transition, restore=args.action == 'restore')
        elif args.operation == "validate":
            result = _validate_result(_path(args.result) or Path())
        elif args.operation == "controls":
            profile = args.profile
            result, _ = qualify_control_network(repo_root(), _path(args.profile) or Path())
        elif args.operation == "admit":
            profile = args.profile
            result, _ = admit_territory(repo_root(), _path(args.profile) or Path())
        elif args.operation == "promote":
            profile = args.profile
            result, _ = promote_canonical(
                repo_root(), _path(args.profile) or Path(), _path(args.result) or Path()
            )
        elif args.operation in {"authority", "materialize"}:
            profile = args.profile
            active_path = canonical_active_path(repo_root(), _path(args.profile) or Path())
            if args.operation == "authority":
                result = validate_canonical_authority(repo_root(), active_path, _path(args.profile) or Path())
            else:
                result_path = materialize_canonical(repo_root(), active_path, _path(args.profile) or Path())
                result = {
                    "operation": "materialize_canonical_authority",
                    "status": "accepted",
                    "profile_id": read_json(result_path)["profile_id"],
                    "result_path": str(result_path.relative_to(repo_root())).replace("\\", "/"),
                }
        else:
            profile = args.profile
            if args.dry_run:
                if args.output_root or args.base_result or args.terrain_change_bounds or args.feature_change_id:
                    raise CompilerError("dry_run_arguments_invalid", "Dry run accepts only profile and source result")
                result = plan_world(args.profile, _path(args.source_result))
            else:
                result, _ = compile_world(
                    args.profile,
                    _path(args.source_result),
                    _path(args.output_root),
                    _path(args.base_result),
                    args.terrain_change_bounds,
                    args.feature_change_id,
                )
        print(json.dumps(result, sort_keys=True, separators=(",", ":"), ensure_ascii=True))
        return 0
    except CompilerError as error:
        failure = _selection_failure(args, error) if _is_selection(args) else _failure(error, profile)
        print(json.dumps(failure, sort_keys=True, separators=(",", ":")), file=sys.stderr)
        return 6
    except Exception as error:
        failure = CompilerError("compiler_internal_error", "Unexpected compiler failure", type=type(error).__name__)
        result = _selection_failure(args, failure) if _is_selection(args) else _failure(failure, profile)
        print(json.dumps(result, sort_keys=True, separators=(",", ":")), file=sys.stderr)
        return 6


def _is_selection(args: argparse.Namespace | None) -> bool:
    return args is not None and args.operation in {'prepare-selection-transition', 'selection-transition', 'prepare-schema-reference-transition'}


def _selection_failure(args: argparse.Namespace, error: CompilerError) -> dict:
    return {'status': 'rejected', 'operation': args.operation, 'code': error.code,
            'message': str(error), 'details': error.details}
