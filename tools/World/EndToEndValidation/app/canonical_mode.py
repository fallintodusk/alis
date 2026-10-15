from __future__ import annotations

import json
import subprocess
from pathlib import Path

from .profile_inputs import ProfileInputError, profile_input_files


REPO_ROOT = Path(__file__).resolve().parents[4]

class CanonicalModeError(RuntimeError):
    code = "canonical_mode_upstream_changed"


UPSTREAM_ROOTS = (
    "tools/World/SourceIngestion/",
    "tools/World/CanonicalCompilation/",
    "tools/World/ExecutionEnvironment/",
)
UPSTREAM_TEST_ROOTS = tuple(root + "tests/" for root in UPSTREAM_ROOTS)
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
    paths: list[str], base: str | None = None, repo_root: Path = REPO_ROOT,
    *, validation_profile: Path | None = None,
) -> bool:
    authority_inputs = None
    canonical_roots = ()
    if validation_profile is not None and base is not None:
        try:
            profile_path = validation_profile.resolve().relative_to(repo_root.resolve()).as_posix()
            if _validation_profile_upstream_changed(profile_path, base, repo_root):
                return True
            authority_inputs = frozenset(
                path.relative_to(repo_root.resolve()).as_posix()
                for path in profile_input_files(validation_profile, repo_root, canonical_only=True)
            )
            document = json.loads(validation_profile.read_text(encoding="utf-8"))
            owners = {
                settings["world_data_plugin"] for settings in document["profiles"].values()
                if settings.get("canonical_authority") is True
            }
            if any(not isinstance(owner, str) or Path(owner).name != owner for owner in owners):
                return True
            canonical_roots = tuple(f"Plugins/World/{owner}/Data/Canonical/" for owner in owners)
        except (ProfileInputError, OSError, ValueError, KeyError, TypeError, AttributeError):
            return True
    for path in paths:
        normalized = path.replace("\\", "/")
        if (normalized.startswith(UPSTREAM_ROOTS) and not normalized.endswith(".md")
                and not normalized.startswith(UPSTREAM_TEST_ROOTS)):
            return True
        if authority_inputs is not None and (
            normalized in authority_inputs and normalized != profile_path
            or normalized.startswith(canonical_roots)
        ):
            return True
        parts = tuple(normalized.split("/"))
        if len(parts) >= 5 and parts[:2] == ("Plugins", "World"):
            relative = parts[3:]
            if any(relative[:len(prefix)] == prefix for prefix in UPSTREAM_DATA_PARTS):
                if authority_inputs is None:
                    return True
            if relative[:3] == ("Data", "Profiles", "EndToEndValidation") and normalized.endswith(".validation.json"):
                if _validation_profile_upstream_changed(normalized, base, repo_root):
                    return True
    return False


def require_world_only_changes(
    paths: list[str], base: str | None = None, repo_root: Path = REPO_ROOT,
    *, validation_profile: Path | None = None,
) -> None:
    if not upstream_replay_required(paths, base, repo_root, validation_profile=validation_profile):
        return
    upstream = sorted(path for path in paths if upstream_replay_required(
        [path], base, repo_root, validation_profile=validation_profile,
    ))
    if upstream:
        raise CanonicalModeError(
            "Changed source or canonical inputs require the full Matrix: " + ", ".join(upstream)
        )
