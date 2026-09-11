from __future__ import annotations

import argparse
import json
import shutil
import sys
from datetime import UTC, datetime
from pathlib import Path

from .checks import execute_checks, load_common_checks
from .contracts import (
    ValidationFailure,
    file_hash,
    load_profile,
    profile_path,
    validation_profile_contract,
    write_result,
)
from .execution import REPO_ROOT, execute
from .validation import _validate_bootstrap, validate


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="ALIS world pipeline end-to-end validation")
    subcommands = parser.add_subparsers(dest="operation", required=True)
    check = subcommands.add_parser("check")
    check.add_argument("--bootstrap-preflight", required=True)
    run = subcommands.add_parser("run")
    run.add_argument("--profile", required=True)
    run.add_argument("--bootstrap-preflight", required=True)
    run.add_argument("--check-result", required=True)
    accept = subcommands.add_parser("accept")
    accept.add_argument("--p0-run", required=True)
    accept.add_argument("--representative-run", required=True)
    accept.add_argument("--profile", required=True)
    accept.add_argument("--bootstrap-preflight", required=True)
    enroll = subcommands.add_parser("enroll")
    enroll.add_argument("--run", required=True)
    enroll.add_argument("--profile", required=True)
    enroll.add_argument("--bootstrap-preflight", required=True)
    return parser


def _base_result(operation_id: str, profile_id: str) -> dict[str, object]:
    return {
        "$schema": "https://alis.world/schemas/world-validation/validation-result-v1.json",
        "schema_version": 1,
        "operation_id": operation_id,
        "operation": "world_pipeline_validation",
        "status": "failed",
        "profile_id": profile_id,
        "checks": [],
        "metrics": {},
        "evidence": {},
        "errors": [],
    }


# Evidence retention. A territory run writes gigabytes; 91 accumulated runs once
# filled the volume and the next gate failed inside osmium with an unexplained
# "Write failed", far from the real cause. Each invocation therefore prunes older
# evidence OF ITS OWN KIND before writing new evidence: "check-" prunes checks,
# "run-" prunes runs, and neither touches the other or anything outside
# Saved/Validation/WorldPipeline. Durable authority never lives here - manifests
# reference nothing under this root - so pruning cannot break an accepted gate.
EVIDENCE_RETAINED = 5
WORK_RUNS_RETAINED = 2


def _prune_evidence(evidence_root: Path, prefix: str, retain: int = EVIDENCE_RETAINED) -> None:
    parent = evidence_root.parent
    if not parent.is_dir():
        return
    existing = sorted(
        (item for item in parent.iterdir() if item.is_dir() and item.name.startswith(prefix)),
        key=lambda item: item.name,
        reverse=True,
    )
    for stale in existing[retain:]:
        shutil.rmtree(stale, ignore_errors=True)


def _prune_work_runs(
    work_parent: Path,
    prefix: str = "run-",
    retain: int = WORK_RUNS_RETAINED,
) -> None:
    if not work_parent.is_dir():
        return
    existing = sorted(
        (item for item in work_parent.iterdir() if item.is_dir() and item.name.startswith(prefix)),
        key=lambda item: item.name,
        reverse=True,
    )
    for stale in existing[retain:]:
        shutil.rmtree(stale, ignore_errors=True)


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    if args.operation == "enroll":
        from .enrollment import enroll

        try:
            outcome = enroll(args.run, args.profile)
        except ValidationFailure as error:
            print(json.dumps({"status": "rejected", "code": error.code, "message": str(error), "details": error.details}), file=sys.stderr)
            return 3
        print(json.dumps({"status": "accepted", **outcome}))
        return 0
    if args.operation == "accept":
        from .acceptance import accept

        try:
            outcome = accept(args.p0_run, args.representative_run, args.profile, args.bootstrap_preflight)
        except ValidationFailure as error:
            print(json.dumps({"status": "rejected", "code": error.code, "message": str(error)}), file=sys.stderr)
            return 3
        print(json.dumps({"status": "accepted", **outcome}))
        return 0
    stamp = datetime.now(UTC).strftime("%Y%m%dT%H%M%SZ")
    if args.operation == "check":
        evidence_root = REPO_ROOT / "Saved" / "Validation" / "WorldPipeline" / f"check-{stamp}"
        _prune_evidence(evidence_root, "check-")
        result_path = evidence_root / "result.json"
        result = _base_result(f"check:common:{stamp}", "common_checks")
        try:
            execution = execute_checks(evidence_root, Path(args.bootstrap_preflight))
            bootstrap = _validate_bootstrap(execution["environment"])
            result.update({
                "status": "accepted",
                "checks": [
                    {"check": "bootstrap_environment", "status": "passed", **bootstrap},
                    {"check": "fault_injection_and_architecture", "status": "passed", "suites": len(execution["test_suites"])},
                ],
                "metrics": {"environment": execution["environment"]},
                "evidence": execution,
            })
            exit_code = 0
        except ValidationFailure as error:
            result["status"] = "rejected"
            result["errors"] = [{"code": error.code, "message": str(error), "details": error.details}]
            exit_code = 3
        except Exception as error:
            result["errors"] = [{"code": "internal_error", "message": "Unexpected common-check failure", "details": {"type": type(error).__name__}}]
            exit_code = 6
        write_result(result_path, result)
        stream = sys.stdout if exit_code == 0 else sys.stderr
        print(json.dumps({"status": result["status"], "result": str(result_path)}, ensure_ascii=True), file=stream)
        return exit_code
    try:
        resolved_profile_path = profile_path(args.profile)
        validation_profile_hash = file_hash(resolved_profile_path)
        initial_profile_contract = validation_profile_contract(resolved_profile_path)
        profile = load_profile(resolved_profile_path)
        if validation_profile_contract(resolved_profile_path) != initial_profile_contract:
            raise ValidationFailure(
                "validation_profile_changed_during_run",
                "The validation profile or a transitive input changed while it was being loaded",
            )
    except ValidationFailure as error:
        print(json.dumps({"status": "rejected", "code": error.code, "message": str(error)}), file=sys.stderr)
        return 3
    profile_id = profile["profile_id"]
    operation_id = f"validate:{profile_id}:{stamp}"
    evidence_root = REPO_ROOT / "Saved" / "Validation" / "WorldPipeline" / f"run-{stamp}"
    _prune_evidence(evidence_root, "run-")
    _prune_work_runs(REPO_ROOT / "tmp" / "world" / "end_to_end_validation", retain=1)
    result_path = evidence_root / "result.json"
    result = _base_result(operation_id, profile_id)
    try:
        common_checks = load_common_checks(Path(args.check_result))
        execution = execute(
            profile,
            f"run-{stamp}",
            evidence_root,
            Path(args.bootstrap_preflight),
            common_checks,
        )
        if validation_profile_contract(resolved_profile_path) != initial_profile_contract:
            raise ValidationFailure(
                "validation_profile_changed_during_run",
                "The validation profile or a transitive input changed during the Matrix run",
            )
        execution["validation_profile"] = {
            "path": resolved_profile_path.relative_to(REPO_ROOT).as_posix(),
            "sha256": validation_profile_hash,
        }
        execution["profile_input_contract"] = initial_profile_contract
        checks, metrics = validate(profile, execution)
        result.update({
            "status": "accepted",
            "checks": checks,
            "metrics": metrics,
            "evidence": execution,
        })
        exit_code = 0
    except ValidationFailure as error:
        result["status"] = "rejected"
        result["errors"] = [{"code": error.code, "message": str(error), "details": error.details}]
        exit_code = 3
    except Exception as error:
        result["errors"] = [{
            "code": "internal_error",
            "message": "Unexpected end-to-end validation failure",
            "details": {"type": type(error).__name__},
        }]
        exit_code = 6
    write_result(result_path, result)
    stream = sys.stdout if exit_code == 0 else sys.stderr
    print(json.dumps({"status": result["status"], "result": str(result_path)}, ensure_ascii=True), file=stream)
    return exit_code
