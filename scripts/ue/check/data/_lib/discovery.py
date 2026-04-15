"""Shared data-file discovery and ID indexing for cross-reference validation.

Each discover_* function scans once and caches results module-level so
multiple validators in the same process share a single scan pass.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def repo_root() -> Path:
    return Path(__file__).resolve().parents[5]


def _project_object_root() -> Path:
    return repo_root() / "Plugins" / "Resources" / "ProjectObject"


def _content_root() -> Path:
    return _project_object_root() / "Content"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _load_json(path: Path) -> dict[str, Any] | None:
    try:
        with path.open("r", encoding="utf-8") as fh:
            data = json.load(fh)
    except Exception:
        return None
    return data if isinstance(data, dict) else None


def _stem_prefix(path: Path) -> str:
    """Return filename prefix before the first underscore (e.g. 'DLG')."""
    return path.stem.split("_", 1)[0] if "_" in path.stem else ""


# ---------------------------------------------------------------------------
# Object definitions  (excludes DLG_*, AUDIO_*, Loot_*)
# ---------------------------------------------------------------------------

_object_defs: dict[str, Path] | None = None
_EXCLUDED_PREFIXES = {"DLG", "AUDIO", "Loot"}


def discover_object_definitions() -> dict[str, Path]:
    global _object_defs
    if _object_defs is not None:
        return _object_defs

    _object_defs = {}
    root = _content_root()
    if not root.exists():
        return _object_defs

    for path in root.rglob("*.json"):
        if _stem_prefix(path) in _EXCLUDED_PREFIXES:
            continue
        data = _load_json(path)
        if data and isinstance(data.get("id"), str):
            _object_defs[data["id"]] = path

    return _object_defs


# ---------------------------------------------------------------------------
# Dialogue trees  (DLG_*.json)
# ---------------------------------------------------------------------------

_dialogue_trees: dict[str, tuple[Path, dict[str, Any]]] | None = None


def discover_dialogue_trees() -> dict[str, tuple[Path, dict[str, Any]]]:
    global _dialogue_trees
    if _dialogue_trees is not None:
        return _dialogue_trees

    _dialogue_trees = {}
    root = _content_root()
    if not root.exists():
        return _dialogue_trees

    for path in root.rglob("DLG_*.json"):
        data = _load_json(path)
        if not data:
            continue
        tree_id = data.get("id")
        nodes = data.get("nodes")
        if isinstance(tree_id, str) and isinstance(nodes, dict):
            _dialogue_trees[tree_id] = (path, nodes)

    return _dialogue_trees


# ---------------------------------------------------------------------------
# Audio presets  (AUDIO_*.json)
# ---------------------------------------------------------------------------

_audio_presets: dict[str, Path] | None = None


def discover_audio_presets() -> dict[str, Path]:
    global _audio_presets
    if _audio_presets is not None:
        return _audio_presets

    _audio_presets = {}
    root = _content_root()
    if not root.exists():
        return _audio_presets

    for path in root.rglob("AUDIO_*.json"):
        data = _load_json(path)
        if data and isinstance(data.get("id"), str):
            _audio_presets[data["id"]] = path

    return _audio_presets


# ---------------------------------------------------------------------------
# Loot profiles  (Data/LootProfiles/**/*.json)
# ---------------------------------------------------------------------------

_loot_profiles: dict[str, tuple[Path, dict[str, Any]]] | None = None


def discover_loot_profiles() -> dict[str, tuple[Path, dict[str, Any]]]:
    global _loot_profiles
    if _loot_profiles is not None:
        return _loot_profiles

    _loot_profiles = {}
    loot_dir = _project_object_root() / "Data" / "LootProfiles"
    if not loot_dir.exists():
        return _loot_profiles

    for path in loot_dir.rglob("*.json"):
        data = _load_json(path)
        if not data:
            continue
        profile_id = data.get("profileId")
        if isinstance(profile_id, str):
            _loot_profiles[profile_id] = (path, data)

    return _loot_profiles
