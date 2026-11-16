from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

from .contract_inputs import common_contract_files
from .contracts import ValidationFailure, canonical_hash, file_hash, read_json, tree_size, validate_against
from .execution import REPO_ROOT, SOURCE_RUN, _powershell, _run
from .producer_descriptors import ProducerDescriptorError, load_producer_descriptors, recorded_verify


REQUIRED_TEST_SUITE_NAMES = {
    "execution_environment",
    "source_ingestion",
    "canonical_compilation",
    "end_to_end_validation",
    "world_architecture",
    "world_lifecycle_scripts",
    "twin_realization",
    "unreal_realization",
}
def common_contract_hash(repo_root: Path = REPO_ROOT) -> str:
    return canonical_hash({
        path.relative_to(repo_root).as_posix(): file_hash(path)
        for path in common_contract_files(repo_root)
    })


def _preflight_verify_revisions(repo_root: Path = REPO_ROOT) -> None:
    """Reject recorded revision drift cheaply; native projection comparison still runs."""
    try:
        descriptors = load_producer_descriptors(repo_root)
    except ProducerDescriptorError as error:
        raise ValidationFailure('verify_baseline_descriptor_invalid', str(error)) from error
    revisions = {item.label: read_json(repo_root / item.path).get('output_revision')
                 for item in descriptors if not item.is_pipeline}
    if any(type(value) is not int or value < 1 for value in revisions.values()):
        raise ValidationFailure('verify_baseline_revision_unknown', 'Producer output revisions must be positive integers')
    problems = []
    for item in descriptors:
        try:
            verify = recorded_verify(repo_root, item)
        except ProducerDescriptorError as error:
            raise ValidationFailure('verify_baseline_descriptor_invalid', str(error)) from error
        if verify is None:
            continue
        identity = read_json(repo_root / item.baseline_path).get('identity', {})
        if not isinstance(identity, dict):
            raise ValidationFailure('verify_baseline_revision_unknown', 'Verify identity must be an object', baseline=item.baseline_path)
        if item.is_pipeline:
            declared = read_json(repo_root / item.path).get('pipeline_revision')
            if (type(declared) is not int or declared < 1 or
                    type(identity.get('pipeline_revision')) is not int or identity['pipeline_revision'] != declared):
                problems.append(item.baseline_path + ': pipeline revision')
            embedded = identity.get('embedded')
            if not isinstance(embedded, list) or not embedded:
                problems.append(item.baseline_path + ': missing embedded identities')
                continue
            for value in embedded:
                fields = value.split('|') if isinstance(value, str) else []
                if (len(fields) != 3 or fields[0] not in revisions or
                        fields[1] != str(revisions.get(fields[0])) or not fields[2].isdigit() or int(fields[2]) < 1):
                    problems.append(item.baseline_path + ': embedded ' + str(value))
        elif (identity.get('producer_id') != item.label or
              type(identity.get('output_revision')) is not int or
              identity['output_revision'] != revisions[item.label]):
            problems.append(item.baseline_path + ': producer revision')
    if problems:
        raise ValidationFailure('verify_baseline_revision_stale',
            'Declared verify revisions differ; probe and review the owning native output before recording',
            baselines=problems)


def _run_test_suites(logs: Path, powershell: str) -> list[dict[str, Any]]:
    suites = [
        ("execution_environment", [sys.executable, "-m", "unittest", "discover", "tools/World/ExecutionEnvironment/tests"]),
        ("world_architecture", [sys.executable, "-m", "unittest", "discover", "tools/World/tests"]),
        ("source_ingestion", [sys.executable, "-m", "unittest", "discover", "tools/World/SourceIngestion/tests", "-t", "tools"]),
        ("canonical_compilation", [sys.executable, "-m", "unittest", "discover", "tools/World/CanonicalCompilation/tests", "-t", "tools"]),
        ("end_to_end_validation", [sys.executable, "-m", "unittest", "discover", "tools/World/EndToEndValidation/tests"]),
        (
            "world_lifecycle_scripts",
            [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(REPO_ROOT / "scripts" / "ue" / "world" / "test" / "run_all.ps1"),
            ],
        ),
        (
            "twin_realization",
            [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(REPO_ROOT / "scripts" / "ue" / "world" / "test" / "integration" / "verify_twin_realization.ps1"),
            ],
        ),
        (
            "unreal_realization",
            [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(REPO_ROOT / "scripts" / "ue" / "world" / "test" / "integration" / "verify_native_realization.ps1"),
                "-EvidenceDirectory",
                str(REPO_ROOT / "tmp" / "world" / "end_to_end_validation" / "native" / logs.parent.name),
            ],
        ),
    ]
    results = []
    for name, command in suites:
        duration = _run(f"test_{name}", command, logs)
        results.append({
            "name": name,
            "status": "accepted",
            "duration_seconds": duration,
            "log": str(logs / f"test_{name}.log"),
        })
    return results


def execute_checks(evidence_root: Path, preflight_path: Path) -> dict[str, Any]:
    logs = evidence_root / "logs"
    powershell = _powershell()
    _run(
        "export_editor_target",
        [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
         str(REPO_ROOT / "scripts" / "ue" / "build" / "export_target_json.ps1")],
        logs,
    )
    _preflight_verify_revisions()
    initial_contract_hash = common_contract_hash()
    preflight = read_json(preflight_path)
    validate_against(preflight, "bootstrap-preflight.schema.json")
    cache_root = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "cache"
    cache_before = tree_size(cache_root)
    _run("bootstrap_tools", [sys.executable, str(SOURCE_RUN), "bootstrap-tools"], logs)
    test_suites = _run_test_suites(logs, powershell)
    if common_contract_hash() != initial_contract_hash:
        raise ValidationFailure(
            "common_contract_changed_during_check",
            "World code, scripts, contracts, or tests changed during the shared Check",
        )
    return {
        "test_suites": test_suites,
        "common_contract_sha256": initial_contract_hash,
        "environment": {
            "python": sys.executable,
            "python_environment_bytes": tree_size(REPO_ROOT / "tmp" / "world" / "execution_environment" / "python"),
            "native_tools_bytes": tree_size(REPO_ROOT / "tmp" / "world" / "execution_environment" / "tools"),
            "immutable_cache_bytes": tree_size(cache_root),
            "network_transfer_bytes": max(0, tree_size(cache_root) - cache_before),
            "bootstrap_preflight_path": str(preflight_path),
            "bootstrap_preflight": preflight,
        },
    }


def load_common_checks(result_path: Path) -> dict[str, Any]:
    resolved = result_path.resolve()
    evidence_root = (REPO_ROOT / "Saved" / "Validation" / "WorldPipeline").resolve()
    if resolved.name != "result.json" or not resolved.is_relative_to(evidence_root):
        raise ValidationFailure(
            "common_checks_scope_invalid",
            "The common-check receipt must belong to Saved/Validation/WorldPipeline",
            path=str(resolved),
        )
    document = read_json(resolved)
    validate_against(document, "validation-result.schema.json")
    if document.get("status") != "accepted" or document.get("profile_id") != "common_checks":
        raise ValidationFailure("common_checks_not_accepted", "The common-check receipt is not accepted")
    execution = document.get("evidence", {})
    suites = execution.get("test_suites", [])
    names = {suite.get("name") for suite in suites if suite.get("status") == "accepted"}
    if names != REQUIRED_TEST_SUITE_NAMES:
        raise ValidationFailure(
            "common_checks_incomplete",
            "The common-check receipt does not cover the exact required suite set",
            expected=sorted(REQUIRED_TEST_SUITE_NAMES),
            actual=sorted(str(name) for name in names),
        )
    current_contract = common_contract_hash()
    if execution.get("common_contract_sha256") != current_contract:
        raise ValidationFailure(
            "common_checks_stale",
            "Tested world code or contracts changed after the common checks",
        )
    return {
        "receipt": str(resolved),
        "receipt_sha256": file_hash(resolved),
        "operation_id": document["operation_id"],
        "common_contract_sha256": current_contract,
        "test_suites": suites,
        "environment": execution["environment"],
    }
