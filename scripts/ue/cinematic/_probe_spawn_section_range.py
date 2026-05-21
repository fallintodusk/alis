"""Probe SpawnTrack section range using MovieSceneSequenceExtensions /
MovieSceneSectionExtensions — UE 5.7's documented snake_case-friendly path
for MovieScene introspection. The plain ULevelSequence accessors fail in
Python (no get_playback_range).

Output: Saved/cinematic_probe_spawn_range.json
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal

ASSET = sys.argv[1] if len(sys.argv) > 1 else "/Game/Cinematics/Takes/2026-05-19/Scene_1_01"
OUT = Path(unreal.Paths.project_saved_dir()) / "cinematic_probe_spawn_range.json"


def main() -> None:
    out: dict = {"asset": ASSET}
    seq = unreal.load_asset(ASSET)
    if seq is None:
        out["error"] = "load_asset failed"
        OUT.write_text(json.dumps(out, indent=2))
        return

    # Playback range via extension lib.
    SeqExt = unreal.MovieSceneSequenceExtensions
    pb_range = SeqExt.get_playback_range(seq)
    pb_start = SeqExt.get_playback_start(seq)
    pb_end = SeqExt.get_playback_end(seq)
    try:
        tick_res = SeqExt.get_tick_resolution(seq)
        display_rate = SeqExt.get_display_rate(seq)
        out["tick_resolution"] = f"{tick_res.numerator}/{tick_res.denominator}"
        out["display_rate"] = f"{display_rate.numerator}/{display_rate.denominator}"
    except Exception as exc:
        out["rate_err"] = repr(exc)

    out["playback_start_frame"] = int(pb_start)
    out["playback_end_frame"] = int(pb_end)
    try:
        out["playback_range_repr"] = str(pb_range)
    except Exception:
        pass

    # Bindings + sections via extension libs.
    bindings = SeqExt.get_bindings(seq)
    out["binding_count"] = len(bindings)
    bind_out = []
    BindExt = unreal.MovieSceneBindingExtensions
    SecExt = unreal.MovieSceneSectionExtensions
    for b in bindings:
        binfo: dict = {}
        try:
            binfo["name"] = BindExt.get_name(b)
        except Exception as exc:
            binfo["name_err"] = repr(exc)
        try:
            binfo["binding_id"] = str(BindExt.get_id(b))
        except Exception:
            pass

        tracks = BindExt.get_tracks(b)
        tlist = []
        for t in tracks:
            tinfo: dict = {"class": t.get_class().get_name()}
            try:
                secs = t.get_sections()
                slist = []
                for s in secs:
                    sinfo: dict = {"class": s.get_class().get_name()}
                    try:
                        sinfo["start_frame"] = int(SecExt.get_start_frame(s))
                    except Exception as exc:
                        sinfo["start_frame_err"] = repr(exc)
                    try:
                        sinfo["end_frame"] = int(SecExt.get_end_frame(s))
                    except Exception as exc:
                        sinfo["end_frame_err"] = repr(exc)
                    try:
                        sinfo["has_start_frame_bound"] = SecExt.has_start_frame(s)
                        sinfo["has_end_frame_bound"] = SecExt.has_end_frame(s)
                    except Exception as exc:
                        sinfo["bound_check_err"] = repr(exc)
                    slist.append(sinfo)
                tinfo["sections"] = slist
            except Exception as exc:
                tinfo["sections_err"] = repr(exc)
            tlist.append(tinfo)
        binfo["tracks"] = tlist
        bind_out.append(binfo)

    out["bindings"] = bind_out
    OUT.write_text(json.dumps(out, indent=2))
    unreal.log(f"[ProbeSpawnRange] wrote {OUT}")


if __name__ == "__main__":
    main()
