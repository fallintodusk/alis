#!/usr/bin/env python3
"""Validate ProjectMind data files with schema checks and cross-reference validation."""

from __future__ import annotations

import json
import re
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

# Signal tag patterns emitted by DialogueServiceImpl:
#   Node entered:    Dialogue.<TreeId>.<NodeId>
#   Option selected: Dialogue.Option.<TreeId>.<FromNodeId>.Next.<NextNodeId>
_SIGNAL_NODE_RE = re.compile(r"^Dialogue\.([^.]+)\.([^.]+)$")
_SIGNAL_OPTION_RE = re.compile(
    r"^Dialogue\.Option\.([^.]+)\.([^.]+)\.Next\.([^.]+)$"
)


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[5]


def _mind_data_dir() -> Path:
    return _repo_root() / "Plugins/Gameplay/ProjectMind/Data"


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


# ---------------------------------------------------------------------------
# Dialogue tree discovery
# ---------------------------------------------------------------------------

def _discover_dialogue_trees(root: Path) -> dict[str, dict[str, Any]]:
    """Return {tree_id: {node_id: node_data, ...}} for all DLG_*.json files."""
    trees: dict[str, dict[str, Any]] = {}
    content_dir = root / "Plugins/Resources/ProjectObject/Content"
    if not content_dir.exists():
        return trees

    for dlg_file in content_dir.rglob("DLG_*.json"):
        try:
            with dlg_file.open("r", encoding="utf-8") as fh:
                data = json.load(fh)
        except Exception:
            continue

        tree_id = data.get("id")
        nodes = data.get("nodes")
        if isinstance(tree_id, str) and isinstance(nodes, dict):
            trees[tree_id] = nodes

    return trees


def _collect_valid_option_transitions(
    nodes: dict[str, Any],
) -> set[tuple[str, str]]:
    """Return set of (from_node_id, next_node_id) from all options in tree."""
    transitions: set[tuple[str, str]] = set()
    for node_id, node_data in nodes.items():
        if not isinstance(node_data, dict):
            continue
        options = node_data.get("options")
        if not isinstance(options, list):
            continue
        for option in options:
            if not isinstance(option, dict):
                continue
            next_id = option.get("next")
            if isinstance(next_id, str) and next_id:
                # $end is valid but not a real node
                sanitized = "end" if next_id == "$end" else next_id
                transitions.add((node_id, sanitized))
    return transitions


# ---------------------------------------------------------------------------
# Cross-reference: signal tags vs dialogue trees
# ---------------------------------------------------------------------------

def _validate_signal_tags_against_trees(
    signal_tags: list[str],
    trees: dict[str, dict[str, Any]],
    label: str,
    errors: list[str],
) -> None:
    """Check that every signal tag references a real tree/node."""
    for tag in signal_tags:
        m_node = _SIGNAL_NODE_RE.match(tag)
        if m_node:
            tree_id, node_id = m_node.group(1), m_node.group(2)
            if tree_id not in trees:
                _error(
                    errors,
                    f"{label}: signal_tag '{tag}' references unknown "
                    f"dialogue tree '{tree_id}'",
                )
            elif node_id not in trees[tree_id]:
                _error(
                    errors,
                    f"{label}: signal_tag '{tag}' references unknown "
                    f"node '{node_id}' in tree '{tree_id}'",
                )
            continue

        m_opt = _SIGNAL_OPTION_RE.match(tag)
        if m_opt:
            tree_id = m_opt.group(1)
            from_node = m_opt.group(2)
            next_node = m_opt.group(3)
            if tree_id not in trees:
                _error(
                    errors,
                    f"{label}: signal_tag '{tag}' references unknown "
                    f"dialogue tree '{tree_id}'",
                )
            else:
                nodes = trees[tree_id]
                if from_node not in nodes:
                    _error(
                        errors,
                        f"{label}: signal_tag '{tag}' references unknown "
                        f"from-node '{from_node}' in tree '{tree_id}'",
                    )
                else:
                    valid_transitions = _collect_valid_option_transitions(nodes)
                    if (from_node, next_node) not in valid_transitions:
                        _error(
                            errors,
                            f"{label}: signal_tag '{tag}' references "
                            f"non-existent option transition "
                            f"'{from_node}' -> '{next_node}' "
                            f"in tree '{tree_id}'",
                        )
            continue

        # Unknown signal format -- not necessarily an error (could be
        # a custom signal), but warn so authors notice typos.
        _error(
            errors,
            f"{label}: signal_tag '{tag}' does not match known "
            f"dialogue signal format (Dialogue.<Tree>.<Node> or "
            f"Dialogue.Option.<Tree>.<From>.Next.<To>)",
        )


# ---------------------------------------------------------------------------
# Field-level validators
# ---------------------------------------------------------------------------

def _validate_thought_common(
    entry: dict[str, Any],
    idx: int,
    errors: list[str],
    path: Path,
    required_fields: tuple[str, ...],
) -> None:
    label = f"{path.name}: entries[{idx}]"

    for key in required_fields:
        value = entry.get(key)
        if value is None:
            _error(errors, f"{label}: missing required field '{key}'")
            continue
        if key in {
            "tree_id", "node_id", "state_tag", "thought_id",
            "text", "channel", "source_type",
        }:
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


def _validate_dialogue_entry(
    entry: dict[str, Any],
    idx: int,
    errors: list[str],
    path: Path,
    trees: dict[str, dict[str, Any]],
) -> None:
    _validate_thought_common(
        entry, idx, errors, path,
        ("thought_id", "text", "channel", "priority", "source_type"),
    )

    # Collect signal tags from entry
    signal_tags: list[str] = []
    tag = entry.get("signal_tag")
    if isinstance(tag, str) and tag.strip():
        signal_tags.append(tag.strip())

    tags = entry.get("signal_tags")
    if isinstance(tags, list):
        for t in tags:
            if isinstance(t, str) and t.strip():
                signal_tags.append(t.strip())

    # Legacy tree_id/node_id form
    tree_id = entry.get("tree_id")
    node_id = entry.get("node_id")
    if not signal_tags and isinstance(tree_id, str) and isinstance(node_id, str):
        signal_tags.append(f"Dialogue.{tree_id}.{node_id}")

    if not signal_tags:
        _error(errors, f"{path.name}: entries[{idx}]: no signal key (signal_tag, signal_tags, or tree_id+node_id)")
        return

    label = f"{path.name}: entries[{idx}]"
    _validate_signal_tags_against_trees(signal_tags, trees, label, errors)


def _validate_vitals_entry(
    entry: dict[str, Any],
    idx: int,
    errors: list[str],
    path: Path,
    trees: dict[str, dict[str, Any]],
) -> None:
    _validate_thought_common(
        entry, idx, errors, path,
        ("state_tag", "thought_id", "text", "channel", "priority", "source_type"),
    )


def _validate_scan_rule_entry(
    entry: dict[str, Any],
    idx: int,
    errors: list[str],
    path: Path,
    trees: dict[str, dict[str, Any]],
) -> None:
    label = f"{path.name}: entries[{idx}]"

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


# ---------------------------------------------------------------------------
# File-level validation
# ---------------------------------------------------------------------------

def _validate_mapping_file(
    data_path: Path,
    errors: list[str],
    entry_validator,
    trees: dict[str, dict[str, Any]],
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
        entry_validator(raw_entry, idx, errors, data_path, trees)


def validate_dialogue_thought_mappings(
    errors: list[str],
    trees: dict[str, dict[str, Any]],
) -> None:
    _validate_mapping_file(
        _mind_data_dir() / "dialogue_thought_mappings.json",
        errors,
        _validate_dialogue_entry,
        trees,
    )


def validate_vitals_thought_mappings(
    errors: list[str],
    trees: dict[str, dict[str, Any]],
) -> None:
    _validate_mapping_file(
        _mind_data_dir() / "vitals_thought_mappings.json",
        errors,
        _validate_vitals_entry,
        trees,
    )


def validate_scan_thought_rules(
    errors: list[str],
    trees: dict[str, dict[str, Any]],
) -> None:
    _validate_mapping_file(
        _mind_data_dir() / "scan_thought_rules.json",
        errors,
        _validate_scan_rule_entry,
        trees,
    )


def main() -> int:
    root = _repo_root()
    errors: list[str] = []

    print("Discovering dialogue trees...")
    trees = _discover_dialogue_trees(root)
    print(f"  Found {len(trees)} dialogue trees")

    validate_dialogue_thought_mappings(errors, trees)
    validate_vitals_thought_mappings(errors, trees)
    validate_scan_thought_rules(errors, trees)

    if errors:
        print(f"\nProjectMind data validation FAILED ({len(errors)} errors):")
        for error in errors:
            print(f"  [X] {error}")
        return 1

    print("\nProjectMind data validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
