from __future__ import annotations

import platform
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import dependency_lock_hash

from .contracts import IngestionError, canonical_hash, file_hash


RUN_INPUTS_SCHEMA = "https://alis.world/schemas/world-source/run-inputs-v1.json"


def source_root(repo_root: Path) -> Path:
    return repo_root / "tools" / "World" / "SourceIngestion"


def source_data_root(repo_root: Path) -> Path:
    return repo_root / "tmp" / "world" / "source_ingestion"


def profile_path(repo_root: Path, value: str) -> Path:
    supplied = Path(value)
    if supplied.suffix == ".json" or supplied.is_absolute() or "/" in value or "\\" in value:
        path = supplied if supplied.is_absolute() else repo_root / supplied
    else:
        path = source_root(repo_root) / "profiles" / f"{value}.source.json"
    resolved = path.resolve()
    if not resolved.is_file():
        raise IngestionError("profile_missing", "Source profile does not exist", profile=value)
    return resolved


def run_contract(repo_root: Path, profile: dict[str, Any]) -> dict[str, Any]:
    world_root = repo_root / "tools" / "World"
    ingestion_root = source_root(repo_root)
    environment_root = world_root / "ExecutionEnvironment"
    implementation_files = [
        ingestion_root / "run.py",
        ingestion_root / "bootstrap.py",
        ingestion_root / "api.py",
        *sorted((ingestion_root / "app").glob("*.py")),
        *sorted((ingestion_root / "contracts").glob("*.json")),
        environment_root / "api.py",
        *sorted((environment_root / "app").glob("*.py")),
        *sorted((environment_root / "contracts").glob("*.json")),
    ]
    implementation_hash = canonical_hash({
        "files": {
            path.relative_to(world_root).as_posix(): file_hash(path)
            for path in implementation_files
        }
    })
    components = {
        "source_profile_sha256": canonical_hash(profile),
        "toolchain_lock_sha256": file_hash(environment_root / "toolchain.lock.json"),
        "python_dependency_lock_sha256": dependency_lock_hash(repo_root),
        "python_runtime": platform.python_version(),
        "implementation_sha256": implementation_hash,
    }
    return {
        "$schema": RUN_INPUTS_SCHEMA,
        "schema_version": 1,
        "profile_id": profile["profile_id"],
        **components,
        "run_inputs_hash": canonical_hash(components),
    }


def default_output_root(
    repo_root: Path, profile: dict[str, Any], run_inputs_hash: str, operation: str = "run"
) -> Path:
    return source_data_root(repo_root) / "runs" / profile["profile_id"] / run_inputs_hash / operation
