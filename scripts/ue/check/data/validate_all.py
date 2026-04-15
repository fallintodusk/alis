#!/usr/bin/env python3
"""Run all cross-reference data validators in a single pass.

Usage:
    python scripts/ue/check/data/validate_all.py
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _lib.discovery import (
    discover_audio_presets,
    discover_dialogue_trees,
    discover_loot_profiles,
    discover_object_definitions,
)
from _lib.reporting import ErrorCollector
from validate_audio_refs import validate_audio_refs
from validate_dialogue_refs import validate_dialogue_refs
from validate_object_refs import validate_object_refs


def main() -> int:
    print("Discovering data files...")
    obj_defs = discover_object_definitions()
    dlg_trees = discover_dialogue_trees()
    audio = discover_audio_presets()
    loot = discover_loot_profiles()
    print(
        f"  {len(obj_defs)} object defs, "
        f"{len(dlg_trees)} dialogue trees, "
        f"{len(audio)} audio presets, "
        f"{len(loot)} loot profiles"
    )

    errors = ErrorCollector()
    validate_object_refs(errors)
    validate_dialogue_refs(errors)
    validate_audio_refs(errors)

    return errors.print_summary("Cross-reference data validation")


if __name__ == "__main__":
    raise SystemExit(main())
