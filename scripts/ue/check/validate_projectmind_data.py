#!/usr/bin/env python3
"""Validate ProjectMind data files with lightweight schema checks."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ALLOWED_CHANNELS = {"Toast", "Journal", "ToastAndJournal"}
ALLOWED_SOURCE_TYPES = {
    "Unknown",
    "Dialogue",
    "Vitals",
    "Inventory",
    "Scan",
    "Quest",
    "Beacon",
    "System",
}


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def _error(errors: list[str], message: str) -> None:
    errors.append(message)


def _load_json(path: Path, errors: list[str]) -> dict[str, Any] | None:
    try:
        with path.open("r", encoding="utf-8") as fh:
            loaded = json.load(fh)
    except Exception as exc:  # noqa: BLE001 - report parser errors directly
        _error(errors, f"{path}: invalid JSON ({exc})")
        return None

    if not isinstance(loaded, dict):
        _error(errors, f"{path}: root must be an object")
        return None

    return loaded


def _validate_thought_common(
    entry: dict[str, Any],
    idx: int,
    errors: list[str],
    path: Path,
    required_fields: tuple[str, ...],
) -> None:
    label = f"{path}: entries[{idx}]"

    for key in required_fields:
        value = entry.get(key)
        if value is None:
            _error(errors, f"{label}: missing required field '{key}'")
            continue
        if key in {"tree_id", "node_id", "state_tag", "thought_id", "text", "channel", "source_type"}:
            if not isinstance(value, str) or not value.strip():
                _error(errors, f"{label}: field '{key}' must be a non-empty string")

    if "channel" in entry and entry["channel"] not in ALLOWED_CHANNELS:
        _error(errors, f"{label}: field 'channel' has invalid value '{entry['channel']}'")

    if "source_type" in entry and entry["source_type"] not in ALLOWED_SOURCE_TYPES:
        _error(errors, f"{label}: field 'source_type' has invalid value '{entry['source_type']}'")

    if "priority" in entry and not isinstance(entry["priority"], int):
        _error(errors, f"{label}: field 'priority' must be an integer")

    ttl = entry.get("time_to_live_sec")
    if ttl is not None:
        if not isinstance(ttl, (int, float)) or float(ttl) < 0.1:
            _error(errors, f"{label}: field 'time_to_live_sec' must be >= 0.1")

    cooldown = entry.get("cooldown_sec")
    if cooldown is not None:
        if not isinstance(cooldown, (int, float)) or float(cooldown) < 0.0:
            _error(errors, f"{label}: field 'cooldown_sec' must be >= 0.0")

    dedupe_window = entry.get("dedupe_window_sec")
    if dedupe_window is not None:
        if not isinstance(dedupe_window, (int, float)) or float(dedupe_window) < 0.0:
            _error(errors, f"{label}: field 'dedupe_window_sec' must be >= 0.0")

    dedupe_key = entry.get("dedupe_key")
    if dedupe_key is not None:
        if not isinstance(dedupe_key, str) or not dedupe_key.strip():
            _error(errors, f"{label}: field 'dedupe_key' must be a non-empty string")


def _validate_dialogue_entry(entry: dict[str, Any], idx: int, errors: list[str], path: Path) -> None:
    _validate_thought_common(
        entry,
        idx,
        errors,
        path,
        ("tree_id", "node_id", "thought_id", "text", "channel", "priority", "source_type"),
    )


def _validate_vitals_entry(entry: dict[str, Any], idx: int, errors: list[str], path: Path) -> None:
    _validate_thought_common(
        entry,
        idx,
        errors,
        path,
        ("state_tag", "thought_id", "text", "channel", "priority", "source_type"),
    )


def _validate_scan_rule_entry(entry: dict[str, Any], idx: int, errors: list[str], path: Path) -> None:
    label = f"{path}: entries[{idx}]"

    required_strings = ("rule_id", "thought_id_prefix", "text_template", "channel", "source_type")
    for key in required_strings:
        value = entry.get(key)
        if not isinstance(value, str) or not value.strip():
            _error(errors, f"{label}: field '{key}' must be a non-empty string")

    match_any_tags = entry.get("match_any_tags")
    if not isinstance(match_any_tags, list) or len(match_any_tags) == 0:
        _error(errors, f"{label}: field 'match_any_tags' must be a non-empty array")
    else:
        for tag_idx, tag in enumerate(match_any_tags):
            if not isinstance(tag, str) or not tag.strip():
                _error(errors, f"{label}: match_any_tags[{tag_idx}] must be a non-empty string")

    if "channel" in entry and entry["channel"] not in ALLOWED_CHANNELS:
        _error(errors, f"{label}: field 'channel' has invalid value '{entry['channel']}'")

    if "source_type" in entry and entry["source_type"] not in ALLOWED_SOURCE_TYPES:
        _error(errors, f"{label}: field 'source_type' has invalid value '{entry['source_type']}'")

    priority_base = entry.get("priority_base")
    if not isinstance(priority_base, int):
        _error(errors, f"{label}: field 'priority_base' must be an integer")

    distance_cm = entry.get("distance_cm")
    if distance_cm is not None:
        if not isinstance(distance_cm, dict):
            _error(errors, f"{label}: field 'distance_cm' must be an object when present")
        else:
            min_distance = distance_cm.get("min")
            max_distance = distance_cm.get("max")
            if min_distance is not None:
                if not isinstance(min_distance, (int, float)) or float(min_distance) < 0.0:
                    _error(errors, f"{label}: field 'distance_cm.min' must be >= 0.0")
            if max_distance is not None:
                if not isinstance(max_distance, (int, float)) or float(max_distance) < 0.0:
                    _error(errors, f"{label}: field 'distance_cm.max' must be >= 0.0")
            if isinstance(min_distance, (int, float)) and isinstance(max_distance, (int, float)):
                if float(max_distance) < float(min_distance):
                    _error(errors, f"{label}: field 'distance_cm.max' must be >= distance_cm.min")

    min_view_dot = entry.get("min_view_dot")
    if min_view_dot is not None:
        if not isinstance(min_view_dot, (int, float)) or not (-1.0 <= float(min_view_dot) <= 1.0):
            _error(errors, f"{label}: field 'min_view_dot' must be in range [-1.0, 1.0]")

    los_required = entry.get("los_required")
    if los_required is not None and not isinstance(los_required, bool):
        _error(errors, f"{label}: field 'los_required' must be a boolean")

    ttl = entry.get("time_to_live_sec")
    if ttl is not None:
        if not isinstance(ttl, (int, float)) or float(ttl) < 0.1:
            _error(errors, f"{label}: field 'time_to_live_sec' must be >= 0.1")

    cooldown = entry.get("cooldown_sec")
    if cooldown is not None:
        if not isinstance(cooldown, (int, float)) or float(cooldown) < 0.0:
            _error(errors, f"{label}: field 'cooldown_sec' must be >= 0.0")

    dedupe_window = entry.get("dedupe_window_sec")
    if dedupe_window is not None:
        if not isinstance(dedupe_window, (int, float)) or float(dedupe_window) < 0.0:
            _error(errors, f"{label}: field 'dedupe_window_sec' must be >= 0.0")

    dedupe_key_prefix = entry.get("dedupe_key_prefix")
    if dedupe_key_prefix is not None:
        if not isinstance(dedupe_key_prefix, str) or not dedupe_key_prefix.strip():
            _error(errors, f"{label}: field 'dedupe_key_prefix' must be a non-empty string")


def _validate_mapping_file(
    data_path: Path,
    errors: list[str],
    entry_validator,
) -> None:
    if not data_path.exists():
        _error(errors, f"{data_path}: file is missing")
        return

    data = _load_json(data_path, errors)
    if data is None:
        return

    schema_ref = data.get("$schema")
    if not isinstance(schema_ref, str) or not schema_ref.strip():
        _error(errors, f"{data_path}: '$schema' must be a non-empty string")
    else:
        schema_path = (data_path.parent / schema_ref).resolve()
        if not schema_path.exists():
            _error(errors, f"{data_path}: '$schema' target does not exist ({schema_ref})")

    entries = data.get("entries")
    if not isinstance(entries, list):
        _error(errors, f"{data_path}: 'entries' must be an array")
        return

    for idx, raw_entry in enumerate(entries):
        if not isinstance(raw_entry, dict):
            _error(errors, f"{data_path}: entries[{idx}] must be an object")
            continue
        entry_validator(raw_entry, idx, errors, data_path)


def validate_dialogue_thought_mappings(errors: list[str]) -> None:
    root = _repo_root()
    _validate_mapping_file(
        root / "Content/Data/Schema/Gameplay/ProjectMind/dialogue_thought_mappings.json",
        errors,
        _validate_dialogue_entry,
    )


def validate_vitals_thought_mappings(errors: list[str]) -> None:
    root = _repo_root()
    _validate_mapping_file(
        root / "Content/Data/Schema/Gameplay/ProjectMind/vitals_thought_mappings.json",
        errors,
        _validate_vitals_entry,
    )


def validate_scan_thought_rules(errors: list[str]) -> None:
    root = _repo_root()
    _validate_mapping_file(
        root / "Content/Data/Schema/Gameplay/ProjectMind/scan_thought_rules.json",
        errors,
        _validate_scan_rule_entry,
    )


def main() -> int:
    errors: list[str] = []
    validate_dialogue_thought_mappings(errors)
    validate_vitals_thought_mappings(errors)
    validate_scan_thought_rules(errors)

    if errors:
        print("ProjectMind data validation failed:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print("ProjectMind data validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
