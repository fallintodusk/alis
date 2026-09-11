from __future__ import annotations

import json
import os
from datetime import UTC, datetime
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
OBSERVED_ROOTS = {
    "python_and_native_environment": REPO_ROOT / "tmp" / "world" / "execution_environment",
    "source_cache_and_outputs": REPO_ROOT / "tmp" / "world" / "source_ingestion",
    "compiler_outputs": REPO_ROOT / "tmp" / "world" / "canonical_compilation",
    "validation_work": REPO_ROOT / "tmp" / "world" / "end_to_end_validation",
    "validation_evidence": REPO_ROOT / "Saved" / "Validation" / "WorldPipeline",
    "test_generated_packages": REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Content" / "Generated",
    "test_generated_external_actors": REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Content" / "__ExternalActors__" / "Generated",
    "test_generated_external_objects": REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Content" / "__ExternalObjects__" / "Generated",
    "production_generated_packages": REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Content" / "Generated",
    "production_generated_external_actors": REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Content" / "__ExternalActors__" / "Generated",
    "production_generated_external_objects": REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Content" / "__ExternalObjects__" / "Generated",
}


def capture() -> Path:
    stamp = datetime.now(UTC).strftime("%Y%m%dT%H%M%S%fZ")
    observations = [
        {
            "id": identifier,
            "path": path.relative_to(REPO_ROOT).as_posix(),
            "present": path.exists(),
        }
        for identifier, path in OBSERVED_ROOTS.items()
    ]
    document = {
        "$schema": "https://alis.world/schemas/world-validation/bootstrap-preflight-v1.json",
        "schema_version": 1,
        "operation_id": f"bootstrap-preflight:{stamp}",
        "status": "accepted",
        "all_absent": all(not item["present"] for item in observations),
        "observations": observations,
    }
    root = REPO_ROOT / "Saved" / "Validation" / "WorldBootstrapPreflight" / f"run-{stamp}"
    path = root / "preflight.json"
    path.parent.mkdir(parents=True, exist_ok=False)
    staging = path.with_suffix(".json.tmp")
    staging.write_text(json.dumps(document, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    os.replace(staging, path)
    return path
