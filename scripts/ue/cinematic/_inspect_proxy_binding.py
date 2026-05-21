"""One-shot inspector: dump the AlisCinematicProxy binding inside a
recorded take so we can see whether it has a Spawn Track and what
ScheduledEvents the spawnable template carries.

Run via ue-mcp:
    py <project-root>/scripts/ue/cinematic/_inspect_proxy_binding.py /Game/Cinematics/Takes/2026-05-19/Scene_1_03

Writes JSON to Saved/cinematic_inspect_proxy_<asset>.json.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal

DEFAULT_ASSET = "/Game/Cinematics/Takes/2026-05-19/Scene_1_03"
ASSET = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ASSET

OUT_DIR = Path(unreal.Paths.project_saved_dir())
TAG = ASSET.rsplit("/", 1)[-1]
OUT = OUT_DIR / f"cinematic_inspect_proxy_{TAG}.json"


def describe_track(track) -> dict:
    try:
        cls = track.get_class().get_name()
    except Exception as exc:
        cls = f"<err: {exc!r}>"
    sections_count = 0
    try:
        sections_count = len(track.get_sections())
    except Exception:
        pass
    return {"class": cls, "sections": sections_count}


def main() -> None:
    result: dict = {"asset": ASSET, "bindings": [], "errors": []}

    seq = unreal.load_asset(ASSET)
    if seq is None:
        result["errors"].append(f"load_asset returned None for {ASSET}")
        OUT.write_text(json.dumps(result, indent=2))
        return

    movie_scene = seq.get_movie_scene()
    if movie_scene is None:
        result["errors"].append("get_movie_scene returned None")
        OUT.write_text(json.dumps(result, indent=2))
        return

    # Dump timing/range info so we can see if FQualifiedFrameTime.AsSeconds()
    # could plausibly return huge values during render.
    try:
        pb_range = movie_scene.get_playback_range()
        result["playback_range"] = {
            "start_frame": str(pb_range.inclusive_start) if hasattr(pb_range, "inclusive_start") else str(pb_range),
            "end_frame": str(pb_range.exclusive_end) if hasattr(pb_range, "exclusive_end") else "?",
        }
    except Exception as exc:
        result["playback_range_err"] = repr(exc)
    try:
        dr = movie_scene.get_display_rate()
        result["display_rate"] = f"{dr.numerator}/{dr.denominator}"
    except Exception as exc:
        result["display_rate_err"] = repr(exc)
    try:
        tr = movie_scene.get_tick_resolution()
        result["tick_resolution"] = f"{tr.numerator}/{tr.denominator}"
    except Exception as exc:
        result["tick_resolution_err"] = repr(exc)
    try:
        result["playback_start_seconds"] = movie_scene.get_playback_start_seconds()
        result["playback_end_seconds"] = movie_scene.get_playback_end_seconds()
    except Exception as exc:
        result["playback_seconds_err"] = repr(exc)

    # UE 5.7 sequence-level API: get_bindings() returns MovieSceneBindingProxy
    # for every binding (possessable + spawnable). Tracks per binding via
    # binding.get_tracks(). Spawn detection: binding has a MovieSceneSpawnTrack.
    # Also dump movie-scene PlaybackRange via tick_resolution math.
    try:
        pb_range = movie_scene.get_playback_range()
        # MovieSceneFrameRange has inclusive_start / exclusive_end as FFrameNumber
        sf = pb_range.inclusive_start
        ef = pb_range.exclusive_end
        result["playback_range_frames"] = {"start": int(sf.value), "end": int(ef.value)}
    except Exception as exc:
        result["playback_range_frames_err"] = repr(exc)

    try:
        bindings = seq.get_bindings()
    except Exception as exc:
        bindings = []
        result["errors"].append(f"get_bindings: {exc!r}")

    for b in bindings:
        entry = {"name": "?", "guid": "?", "tracks": []}
        try:
            entry["name"] = b.get_name()
        except Exception:
            pass
        try:
            entry["guid"] = str(b.get_id())
        except Exception:
            pass

        try:
            tracks = b.get_tracks()
        except Exception as exc:
            tracks = []
            entry["tracks_err"] = repr(exc)

        has_spawn_track = False
        for t in tracks:
            d = describe_track(t)
            # For spawn tracks, also dump every section's range.
            if t.get_class().get_name() == "MovieSceneSpawnTrack":
                try:
                    sections_info = []
                    for sec in t.get_sections():
                        sec_range = sec.get_range()
                        # sec_range is FFrameNumberRange in newer API or
                        # similar; try several access patterns.
                        try:
                            start = int(sec_range.inclusive_start.value)
                            end = int(sec_range.exclusive_end.value)
                        except Exception:
                            start = repr(sec_range)
                            end = "?"
                        sections_info.append({"start_frame": start, "end_frame": end})
                    d["spawn_sections"] = sections_info
                except Exception as exc:
                    d["spawn_sections_err"] = repr(exc)
            entry["tracks"].append(d)
            if d["class"] == "MovieSceneSpawnTrack":
                has_spawn_track = True

        entry["is_spawnable"] = has_spawn_track

        # If proxy, dump the template's ScheduledEvents (template = the
        # spawnable's preview/template UObject inside the MovieScene).
        if entry["name"] == "AlisCinematicProxy":
            try:
                # Object template lookup: ULevelSequence.find_object_binding_by_name
                # doesn't give us the template directly; use FindBindingByName.
                # Workaround: walk MovieScene's TArray of object bindings via
                # the extension lib.
                for child in movie_scene.get_outer().get_outer_class_iter() if False else []:
                    pass
                # Direct: ULevelSequence::GetSpawnableObjectTemplate is not
                # in Python. Use UMovieSceneBindingExtensions.get_spawn_template?
                # Fall back: walk subobjects of the MovieScene looking for an
                # AlisCinematicProxy CDO-like instance.
                proxy_class = unreal.load_class(None, "/Script/ProjectSinglePlayClient.AlisCinematicProxy")
                if proxy_class is not None:
                    found_template = None
                    for sub in movie_scene.get_outer().get_full_name() and []:
                        pass
                    # Use UObjectIterator-like traversal via load_object_to_string approach:
                    # The spawnable template is parented under the MovieScene asset's package.
                    pkg = seq.get_outer()
                    for obj in unreal.SystemLibrary.get_object_iterator() if False else []:
                        pass
                    # Cheap path: iterate package's objects via get_all_objects_inside_package
                    try:
                        all_pkg_objs = unreal.EditorAssetLibrary.list_assets(
                            pkg.get_path_name().rsplit("/", 1)[0], False, False)
                    except Exception:
                        all_pkg_objs = []
                    # Best-effort: try the documented API location
                    try:
                        tpl = seq.get_spawnable_object_template(b.get_id())
                    except Exception as exc:
                        tpl = None
                        entry["template_lookup_err"] = repr(exc)
                    if tpl is not None:
                        entry["template_class"] = tpl.get_class().get_name()
                        events = []
                        try:
                            sched = tpl.get_editor_property("ScheduledEvents") or []
                        except Exception as exc:
                            sched = []
                            entry["scheduled_events_err"] = repr(exc)
                        for e in sched:
                            try:
                                events.append({
                                    "id": str(e.get_editor_property("Id")),
                                    "offset": float(e.get_editor_property("OffsetSeconds")),
                                    "method": str(e.get_editor_property("Method")),
                                    "target": str(e.get_editor_property("TargetActor")),
                                })
                            except Exception as exc:
                                events.append({"err": repr(exc)})
                        entry["proxy_scheduled_events"] = events
                        entry["proxy_event_count"] = len(events)
            except Exception as exc:
                entry["proxy_inspect_err"] = repr(exc)

        result["bindings"].append(entry)

    OUT.write_text(json.dumps(result, indent=2))
    print(f"Wrote {OUT}")


if __name__ == "__main__":
    main()
