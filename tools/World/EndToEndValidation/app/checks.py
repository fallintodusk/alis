from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

from .contract_inputs import common_contract_files
from .contracts import ValidationFailure, canonical_hash, file_hash, read_json, tree_size, validate_against
from .execution import REPO_ROOT, SOURCE_RUN, _powershell, _run


REQUIRED_TEST_SUITE_NAMES = {
    "execution_environment",
    "source_ingestion",
    "canonical_compilation",
    "end_to_end_validation",
    "world_architecture",
    "world_lifecycle_scripts",
    "unreal_realization",
}
def common_contract_hash(repo_root: Path = REPO_ROOT) -> str:
    return canonical_hash({
        path.relative_to(repo_root).as_posix(): file_hash(path)
        for path in common_contract_files(repo_root)
    })


def _run_test_suites(logs: Path, powershell: str) -> list[dict[str, Any]]:
    suites = [
        ("execution_environment", [sys.executable, "-m", "unittest", "discover", "tools/World/ExecutionEnvironment/tests"]),
        ("source_ingestion", [sys.executable, "-m", "unittest", "discover", "tools/World/SourceIngestion/tests"]),
        ("canonical_compilation", [sys.executable, "-m", "unittest", "discover", "tools/World/CanonicalCompilation/tests"]),
        ("end_to_end_validation", [sys.executable, "-m", "unittest", "discover", "tools/World/EndToEndValidation/tests"]),
        ("world_architecture", [sys.executable, "-m", "unittest", "discover", "tools/World/tests"]),
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
            "unreal_realization",
            [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(REPO_ROOT / "scripts" / "ue" / "test" / "unit" / "iterate.ps1"),
                "-TestFilter",
                "Project.World.Realization",
                "-Mode",
                "Gate",
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
    initial_contract_hash = common_contract_hash()
    preflight = read_json(preflight_path)
    validate_against(preflight, "bootstrap-preflight.schema.json")
    logs = evidence_root / "logs"
    cache_root = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "cache"
    cache_before = tree_size(cache_root)
    powershell = _powershell()
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
