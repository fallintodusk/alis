from __future__ import annotations

import json
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]

class CanonicalModeError(RuntimeError):
    code = "canonical_mode_upstream_changed"


UPSTREAM_ROOTS = (
    "tools/World/SourceIngestion/",
    "tools/World/CanonicalCompilation/",
    "tools/World/ExecutionEnvironment/",
)
UPSTREAM_DATA_PARTS = (
    ("Data", "Profiles", "SourceIngestion"),
    ("Data", "Profiles", "CanonicalCompilation"),
    ("Data", "Profiles", "Budgets"),
    ("Data", "Controls"),
    ("Data", "Canonical"),
)
UPSTREAM_PROFILE_FIELDS = (
    "world_data_plugin", "source_profile", "source_profile_path",
    "compiler_profile", "compiler_profile_path", "canonical_authority",
    "incremental_bounds",
)
UPSTREAM_BUDGET_FIELDS = (
    "provider_payload_bytes", "full_compile_seconds", "incremental_compile_seconds",
)


def _profile_upstream_projection(document: dict) -> dict:
    profiles = document["profiles"]
    thresholds = document["thresholds"]
    if not isinstance(profiles, dict) or not isinstance(thresholds, dict):
        raise ValueError("Validation profile has no usable upstream contract")
    return {
        "profile_id": document["profile_id"],
        "profiles": {
            name: {field: settings.get(field) for field in UPSTREAM_PROFILE_FIELDS}
            for name, settings in profiles.items()
        },
        "thresholds": {field: thresholds.get(field) for field in UPSTREAM_BUDGET_FIELDS},
    }


def _validation_profile_upstream_changed(path: str, base: str | None, repo_root: Path) -> bool:
    if base is None:
        return True
    prior = subprocess.run(
        ["git", "show", f"{base}:{path}"], cwd=repo_root, capture_output=True, check=False
    )
    if prior.returncode != 0:
        return True
    try:
        previous = json.loads(prior.stdout)
        current = json.loads((repo_root / path).read_text(encoding="utf-8"))
        return _profile_upstream_projection(previous) != _profile_upstream_projection(current)
    except (OSError, ValueError, KeyError, TypeError, AttributeError):
        return True


def upstream_replay_required(
    paths: list[str], base: str | None = None, repo_root: Path = REPO_ROOT
) -> bool:
    for path in paths:
        normalized = path.replace("\\", "/")
        if normalized.startswith(UPSTREAM_ROOTS) and not normalized.endswith(".md"):
            return True
        parts = tuple(normalized.split("/"))
        if len(parts) >= 5 and parts[:2] == ("Plugins", "World"):
            relative = parts[3:]
            if any(relative[:len(prefix)] == prefix for prefix in UPSTREAM_DATA_PARTS):
                return True
            if relative[:3] == ("Data", "Profiles", "EndToEndValidation") and normalized.endswith(".validation.json"):
                if _validation_profile_upstream_changed(normalized, base, repo_root):
                    return True
    return False


def require_world_only_changes(
    paths: list[str], base: str | None = None, repo_root: Path = REPO_ROOT
) -> None:
    upstream = sorted(path for path in paths if upstream_replay_required([path], base, repo_root))
    if upstream:
        raise CanonicalModeError(
            "Changed source or canonical inputs require the full Matrix: " + ", ".join(upstream)
        )
