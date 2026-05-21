"""Ensure a LevelSequence has an AAlisCinematicProxy binding + Event Track.

PHASE 1 of the cinematic authoring automation.

After this script runs, the target LevelSequence has:
  - One Spawnable binding for AAlisCinematicProxy (created if absent).
  - One Event Track on that binding (created if absent).

The user then adds Event Track Trigger keys in Sequencer UI, picking proxy
methods (OpenInventory, CloseInventory, OpenVitals, CloseVitals,
OpenMindJournal, CloseMindJournal, InteractFocused) per shot.

Phase 2 (stamping event keys from a JSON plan) is deferred until the first
cinematic take confirms the runtime path:
    Sequencer event -> AAlisCinematicProxy -> ASinglePlayController API
    -> real UI / interaction systems.

Why Phase 1 alone is worth the script: for "lots of takes", placing the
proxy + binding + Event Track via UI is the per-take grunt work. Stamping
event keys is per-take creative choice. Automating the former is high
value / low risk; automating the latter (Phase 2) is medium value /
higher risk (UE Python event-endpoint API is finicky).

Idempotent: re-running on a sequence that already has the binding is a
no-op; only the JSON result reflects "existing" vs "created".

Run from inside a running editor (via ue-mcp or the editor py console):

    py <project-root>/scripts/ue/cinematic/ensure_cinematic_proxy_binding.py /Game/Cinematics/Takes/2026-05-18/Scene_1_02

Result JSON is written to Saved/cinematic_proxy_binding_result.json.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal

# AAlisCinematicProxy lives in ProjectSinglePlayClient (see
# Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlayClient/Public/AlisCinematicProxy.h).
PROXY_CLASS_PATH = "/Script/ProjectSinglePlayClient.AlisCinematicProxy"

OUT = Path(unreal.Paths.project_saved_dir()) / "cinematic_proxy_binding_result.json"


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: ensure_cinematic_proxy_binding.py <LevelSequence asset path>")
        raise SystemExit(1)

    seq_path: str = sys.argv[1]
    result: dict = {"sequence": seq_path, "ops": [], "errors": []}

    # ------------------------------------------------------------------ load
    seq = unreal.load_asset(seq_path)
    if seq is None or not isinstance(seq, unreal.LevelSequence):
        result["errors"].append(f"LevelSequence not loaded at {seq_path}")
        OUT.write_text(json.dumps(result, indent=2))
        raise SystemExit(1)

    proxy_class = unreal.load_class(None, PROXY_CLASS_PATH)
    if proxy_class is None:
        result["errors"].append(
            f"Class not loaded: {PROXY_CLASS_PATH}. Rebuild editor first."
        )
        OUT.write_text(json.dumps(result, indent=2))
        raise SystemExit(1)

    # -------------------------------------------------------- find or create binding
    existing_binding = None
    try:
        bindings = seq.get_bindings()
    except Exception as exc:
        result["errors"].append(f"get_bindings: {exc!r}")
        bindings = []

    for b in bindings:
        try:
            template = b.get_object_template()
        except Exception:
            template = None
        if template is not None and template.get_class() == proxy_class:
            existing_binding = b
            break

    if existing_binding is not None:
        binding = existing_binding
        result["ops"].append({"binding": "existing", "name": binding.get_name()})
    else:
        try:
            binding = seq.add_spawnable_from_class(proxy_class)
            result["ops"].append({"binding": "created_spawnable", "name": binding.get_name()})
        except Exception as exc:
            result["errors"].append(f"add_spawnable_from_class: {exc!r}")
            OUT.write_text(json.dumps(result, indent=2))
            raise SystemExit(1)

    # -------------------------------------------------- find or create event track
    event_track = None
    try:
        tracks = binding.get_tracks()
    except Exception as exc:
        result["errors"].append(f"get_tracks: {exc!r}")
        tracks = []

    for t in tracks:
        if t.get_class().get_name() == "MovieSceneEventTrack":
            event_track = t
            break

    if event_track is not None:
        result["ops"].append({"event_track": "existing"})
    else:
        try:
            event_track = binding.add_track(unreal.MovieSceneEventTrack)
            result["ops"].append({"event_track": "created"})
        except Exception as exc:
            result["errors"].append(f"add_track(MovieSceneEventTrack): {exc!r}")
            OUT.write_text(json.dumps(result, indent=2))
            raise SystemExit(1)

    # ------------------------------------------------------------------ save
    try:
        unreal.EditorAssetLibrary.save_loaded_asset(seq, only_if_is_dirty=False)
        result["ops"].append({"save": True})
    except Exception as exc:
        result["errors"].append(f"save: {exc!r}")

    OUT.write_text(json.dumps(result, indent=2))
    print(f"Wrote {OUT}")


if __name__ == "__main__":
    main()
