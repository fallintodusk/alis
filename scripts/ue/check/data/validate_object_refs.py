#!/usr/bin/env python3
"""Validate object-definition cross-references: seedEntries, loot profiles, lootProfileId.

Checks:
  1. seedEntries objectId in Loot_*.json and object defs -> must match an object def id
  2. Loot profile entries objectId -> must match an object def id
  3. Object def lootProfileId -> must match a loot profile profileId
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _lib.discovery import (
    discover_loot_profiles,
    discover_object_definitions,
    repo_root,
)
from _lib.reporting import ErrorCollector


_OBJECT_DEF_PREFIX = "ObjectDefinition:"
_LOOT_PROFILE_PREFIX = "LootProfileDefinition:"


def _parse_prefixed_id(value: str, prefix: str) -> str | None:
    """Strip known PrimaryAssetId prefix, return bare id or None."""
    if not isinstance(value, str) or not value.strip():
        return None
    v = value.strip()
    if v.startswith(prefix):
        return v[len(prefix):]
    return v


# ---------------------------------------------------------------------------
# 1. seedEntries objectId
# ---------------------------------------------------------------------------

def _validate_seed_entries(
    file_label: str,
    seed_entries: list,
    object_defs: dict[str, Path],
    errors: ErrorCollector,
) -> None:
    for idx, entry in enumerate(seed_entries):
        if not isinstance(entry, dict):
            continue
        raw = entry.get("objectId")
        obj_id = _parse_prefixed_id(raw, _OBJECT_DEF_PREFIX)
        if obj_id is None:
            errors.add(file_label, f"seedEntries[{idx}].objectId", "is missing or empty")
            continue
        if obj_id not in object_defs:
            errors.add(
                file_label,
                f"seedEntries[{idx}].objectId",
                f"references unknown object definition '{obj_id}'",
            )


# ---------------------------------------------------------------------------
# 2. Loot profile entries objectId
# ---------------------------------------------------------------------------

def _validate_loot_profile_entries(
    object_defs: dict[str, Path],
    errors: ErrorCollector,
) -> None:
    for profile_id, (path, data) in discover_loot_profiles().items():
        entries = data.get("entries")
        if not isinstance(entries, list):
            continue
        label = path.name
        for idx, entry in enumerate(entries):
            if not isinstance(entry, dict):
                continue
            raw = entry.get("objectId")
            obj_id = _parse_prefixed_id(raw, _OBJECT_DEF_PREFIX)
            if obj_id is None:
                errors.add(label, f"entries[{idx}].objectId", "is missing or empty")
                continue
            if obj_id not in object_defs:
                errors.add(
                    label,
                    f"entries[{idx}].objectId",
                    f"references unknown object definition '{obj_id}'",
                )


# ---------------------------------------------------------------------------
# 3. Object def lootProfileId
# ---------------------------------------------------------------------------

def _validate_loot_profile_ids(
    object_defs: dict[str, Path],
    loot_profiles: dict,
    errors: ErrorCollector,
) -> None:
    content_root = repo_root() / "Plugins" / "Resources" / "ProjectObject" / "Content"
    if not content_root.exists():
        return

    for path in content_root.rglob("*.json"):
        data = _load_json_safe(path)
        if not data:
            continue

        storage = _get_storage_section(data)
        if not storage:
            continue

        # Check lootProfileId
        raw_profile = storage.get("lootProfileId")
        if isinstance(raw_profile, str) and raw_profile.strip():
            profile_id = _parse_prefixed_id(raw_profile, _LOOT_PROFILE_PREFIX)
            if profile_id and profile_id not in loot_profiles:
                errors.add(
                    path.name,
                    "sections.storage.lootProfileId",
                    f"references unknown loot profile '{profile_id}'",
                )

        # Also check seedEntries in object defs (not just Loot_* files)
        seed_entries = storage.get("seedEntries")
        if isinstance(seed_entries, list):
            _validate_seed_entries(path.name, seed_entries, object_defs, errors)


def _get_storage_section(data: dict) -> dict | None:
    sections = data.get("sections")
    if not isinstance(sections, dict):
        return None
    storage = sections.get("storage")
    return storage if isinstance(storage, dict) else None


def _load_json_safe(path: Path) -> dict | None:
    import json
    try:
        with path.open("r", encoding="utf-8") as fh:
            d = json.load(fh)
    except Exception:
        return None
    return d if isinstance(d, dict) else None


# ---------------------------------------------------------------------------
# Public entry point
# ---------------------------------------------------------------------------

def validate_object_refs(errors: ErrorCollector) -> None:
    object_defs = discover_object_definitions()
    loot_profiles = discover_loot_profiles()

    _validate_loot_profile_entries(object_defs, errors)
    _validate_loot_profile_ids(object_defs, loot_profiles, errors)


def main() -> int:
    errors = ErrorCollector()
    print("Validating object references...")
    validate_object_refs(errors)
    return errors.print_summary("Object reference validation")


if __name__ == "__main__":
    raise SystemExit(main())
