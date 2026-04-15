#!/usr/bin/env python3
"""Validate dialogue cross-references: actions, conditions, and capability asset paths.

Checks:
  4. dialogue.set_tree action -> DLG_*.json file must exist
  5. inventory.consume/give action -> object definition id must exist
  6. Dialogue condition (type=inventory) id -> object definition id must exist
  7. DialogueTreeAsset capability property -> DLG_*.json file must exist
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _lib.discovery import (
    discover_dialogue_trees,
    discover_object_definitions,
    repo_root,
)
from _lib.reporting import ErrorCollector


# ---------------------------------------------------------------------------
# Action parsing
# ---------------------------------------------------------------------------

# dialogue.set_tree:/ProjectObject/Human/GrandPa/DLG_X.DLG_X
_SET_TREE_RE = re.compile(r"^dialogue\.set_tree:(.+)$")

# inventory.consume:ItemId  or  inventory.consume:ItemId*  or :ItemId*:3
_INVENTORY_ACTION_RE = re.compile(
    r"^inventory\.(consume|give):([A-Za-z][A-Za-z0-9_]*)\*?(?::\d+)?$"
)


def _extract_tree_name_from_asset_path(asset_path: str) -> str | None:
    """Extract DLG tree filename from UE asset path.

    '/ProjectObject/Human/GrandPa/DLG_X.DLG_X' -> 'DLG_X'
    """
    # Strip mount point prefix '/ProjectObject/'
    path = asset_path.strip()
    # Take last segment before dot-duplicated suffix
    segments = path.rsplit("/", 1)
    last = segments[-1] if len(segments) > 1 else segments[0]
    # Remove .DLG_X suffix if present
    name = last.split(".", 1)[0] if "." in last else last
    return name if name else None


# ---------------------------------------------------------------------------
# 4. dialogue.set_tree action
# ---------------------------------------------------------------------------

def _validate_set_tree_actions(
    file_label: str,
    node_id: str,
    actions: list,
    dialogue_trees: dict,
    errors: ErrorCollector,
) -> None:
    for idx, action in enumerate(actions):
        if not isinstance(action, str):
            continue
        m = _SET_TREE_RE.match(action)
        if not m:
            continue
        tree_name = _extract_tree_name_from_asset_path(m.group(1))
        if not tree_name:
            continue
        if tree_name not in dialogue_trees:
            errors.add(
                file_label,
                f"nodes.{node_id}.actions[{idx}]",
                f"dialogue.set_tree references non-existent tree '{tree_name}'",
            )


# ---------------------------------------------------------------------------
# 5. inventory.consume/give action
# ---------------------------------------------------------------------------

def _validate_inventory_actions(
    file_label: str,
    node_id: str,
    actions: list,
    object_defs: dict,
    errors: ErrorCollector,
) -> None:
    for idx, action in enumerate(actions):
        if not isinstance(action, str):
            continue
        m = _INVENTORY_ACTION_RE.match(action)
        if not m:
            continue
        item_id = m.group(2)
        if item_id not in object_defs:
            errors.add(
                file_label,
                f"nodes.{node_id}.actions[{idx}]",
                f"inventory action references unknown object definition '{item_id}'",
            )


# ---------------------------------------------------------------------------
# 6. Dialogue condition (type=inventory) id
# ---------------------------------------------------------------------------

def _validate_option_conditions(
    file_label: str,
    node_id: str,
    options: list,
    object_defs: dict,
    errors: ErrorCollector,
) -> None:
    for opt_idx, option in enumerate(options):
        if not isinstance(option, dict):
            continue
        condition = option.get("condition")
        if not isinstance(condition, dict):
            continue
        if condition.get("type") != "inventory":
            continue
        item_id = condition.get("id")
        if not isinstance(item_id, str) or not item_id.strip():
            continue
        if item_id not in object_defs:
            errors.add(
                file_label,
                f"nodes.{node_id}.options[{opt_idx}].condition.id",
                f"references unknown object definition '{item_id}'",
            )


# ---------------------------------------------------------------------------
# Walk all dialogue trees for checks 4-6
# ---------------------------------------------------------------------------

def _validate_dialogue_tree_internals(
    object_defs: dict,
    dialogue_trees: dict,
    errors: ErrorCollector,
) -> None:
    for tree_id, (path, nodes) in dialogue_trees.items():
        label = path.name
        for node_id, node_data in nodes.items():
            if not isinstance(node_data, dict):
                continue

            actions = node_data.get("actions")
            if isinstance(actions, list):
                _validate_set_tree_actions(label, node_id, actions, dialogue_trees, errors)
                _validate_inventory_actions(label, node_id, actions, object_defs, errors)

            options = node_data.get("options")
            if isinstance(options, list):
                _validate_option_conditions(label, node_id, options, object_defs, errors)


# ---------------------------------------------------------------------------
# 7. DialogueTreeAsset capability property
# ---------------------------------------------------------------------------

def _validate_dialogue_tree_asset_refs(
    dialogue_trees: dict,
    errors: ErrorCollector,
) -> None:
    import json
    content_root = repo_root() / "Plugins" / "Resources" / "ProjectObject" / "Content"
    if not content_root.exists():
        return

    for path in content_root.rglob("*.json"):
        if path.stem.startswith(("DLG_", "AUDIO_", "Loot_")):
            continue
        try:
            with path.open("r", encoding="utf-8") as fh:
                data = json.load(fh)
        except Exception:
            continue
        if not isinstance(data, dict):
            continue

        capabilities = data.get("capabilities")
        if not isinstance(capabilities, list):
            continue

        for cap_idx, cap in enumerate(capabilities):
            if not isinstance(cap, dict):
                continue
            props = cap.get("properties")
            if not isinstance(props, dict):
                continue

            asset_path = props.get("DialogueTreeAsset")
            if not isinstance(asset_path, str) or not asset_path.strip():
                continue

            tree_name = _extract_tree_name_from_asset_path(asset_path)
            if tree_name and tree_name not in dialogue_trees:
                errors.add(
                    path.name,
                    f"capabilities[{cap_idx}].properties.DialogueTreeAsset",
                    f"references non-existent dialogue tree '{tree_name}'",
                )


# ---------------------------------------------------------------------------
# Public entry point
# ---------------------------------------------------------------------------

def validate_dialogue_refs(errors: ErrorCollector) -> None:
    object_defs = discover_object_definitions()
    dialogue_trees = discover_dialogue_trees()

    _validate_dialogue_tree_internals(object_defs, dialogue_trees, errors)
    _validate_dialogue_tree_asset_refs(dialogue_trees, errors)


def main() -> int:
    errors = ErrorCollector()
    print("Validating dialogue references...")
    validate_dialogue_refs(errors)
    return errors.print_summary("Dialogue reference validation")


if __name__ == "__main__":
    raise SystemExit(main())
