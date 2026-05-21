"""Verification probe: open take, force the AlisCinematicProxy SpawnTrack
section to a wide range using direct property reflection (the Python
extension lib setters silently no-op on MovieSceneSpawnSection in 5.7).

The section's range lives in SectionRange (FMovieSceneFrameRange) which
wraps a TRange<FFrameNumber>. We reach in via get_editor_property and
mutate it.
"""
from __future__ import annotations

import sys
import json
from pathlib import Path

import unreal

ASSET = sys.argv[1] if len(sys.argv) > 1 else "/Game/Cinematics/Takes/2026-05-19/Scene_1_02"
OUT = Path(unreal.Paths.project_saved_dir()) / "cinematic_fix_spawn_range_result.json"


def main() -> None:
    out: dict = {"asset": ASSET}
    seq = unreal.load_asset(ASSET)
    if seq is None:
        out["error"] = "load_asset failed"
        OUT.write_text(json.dumps(out, indent=2))
        return

    SeqExt = unreal.MovieSceneSequenceExtensions
    BindExt = unreal.MovieSceneBindingExtensions
    SecExt = unreal.MovieSceneSectionExtensions

    pb_start_disp = int(SeqExt.get_playback_start(seq))
    pb_end_disp = int(SeqExt.get_playback_end(seq))
    tick_res = SeqExt.get_tick_resolution(seq)
    display_rate = SeqExt.get_display_rate(seq)
    tick_per_disp = (tick_res.numerator / tick_res.denominator) / (display_rate.numerator / display_rate.denominator)
    pb_start_tick = int(pb_start_disp * tick_per_disp)
    pb_end_tick = int(pb_end_disp * tick_per_disp)
    out["playback_display_frames"] = {"start": pb_start_disp, "end": pb_end_disp}
    out["playback_tick_frames"] = {"start": pb_start_tick, "end": pb_end_tick}
    out["tick_resolution"] = f"{tick_res.numerator}/{tick_res.denominator}"
    out["display_rate"] = f"{display_rate.numerator}/{display_rate.denominator}"

    # Wide window: ±3600 display frames (~60s) past playback bounds.
    pad_disp = 3600
    new_start_disp = pb_start_disp - pad_disp
    new_end_disp = pb_end_disp + pad_disp
    new_start_tick = int(new_start_disp * tick_per_disp)
    new_end_tick = int(new_end_disp * tick_per_disp)

    fixed = []
    bindings = SeqExt.get_bindings(seq)
    for b in bindings:
        name = BindExt.get_name(b)
        if name != "AlisCinematicProxy":
            continue
        for t in BindExt.get_tracks(b):
            if t.get_class().get_name() != "MovieSceneSpawnTrack":
                continue
            for s in t.get_sections():
                before = {
                    "via_secext_start": SecExt.get_start_frame(s) if SecExt.has_start_frame(s) else None,
                    "via_secext_end": SecExt.get_end_frame(s) if SecExt.has_end_frame(s) else None,
                }
                # Read raw range property
                try:
                    sr = s.get_editor_property("SectionRange")
                    before["section_range_repr"] = str(sr)
                except Exception as exc:
                    before["section_range_err"] = repr(exc)

                # Approach 1: SecExt.set_range with FrameNumberRange (tick units).
                try:
                    new_range = unreal.SequencerScriptingRange()
                    new_range.has_start_value = True
                    new_range.has_end_value = True
                    new_range.inclusive_start = new_start_disp
                    new_range.exclusive_end = new_end_disp
                    SecExt.set_range(s, new_range)
                    before["set_range_attempt"] = "ok"
                except Exception as exc:
                    before["set_range_err"] = repr(exc)

                # Approach 2: also try SetStartFrame / SetEndFrame explicitly.
                try:
                    SecExt.set_start_frame_bounded(s, True)
                    SecExt.set_end_frame_bounded(s, True)
                    SecExt.set_start_frame(s, new_start_disp)
                    SecExt.set_end_frame(s, new_end_disp)
                except Exception as exc:
                    before["bound_or_set_err"] = repr(exc)

                # Approach 3: direct mutation via reflection on SectionRange struct.
                try:
                    sr = s.get_editor_property("SectionRange")
                    # FMovieSceneFrameRange has a "Value" of TRange<FFrameNumber>
                    inner = sr.get_editor_property("Value") if sr else None
                    before["section_range_inner_type"] = type(inner).__name__ if inner else None
                except Exception as exc:
                    before["section_range_struct_err"] = repr(exc)

                after = {
                    "via_secext_start": SecExt.get_start_frame(s) if SecExt.has_start_frame(s) else None,
                    "via_secext_end": SecExt.get_end_frame(s) if SecExt.has_end_frame(s) else None,
                }
                try:
                    sr2 = s.get_editor_property("SectionRange")
                    after["section_range_repr"] = str(sr2)
                except Exception as exc:
                    after["section_range_err"] = repr(exc)

                # Mark dirty
                try:
                    s.modify()
                except Exception as exc:
                    after["modify_err"] = repr(exc)

                fixed.append({"before": before, "after": after,
                              "wanted_start_disp": new_start_disp,
                              "wanted_end_disp": new_end_disp})

    out["fixed_sections"] = fixed

    if fixed:
        ok = unreal.EditorAssetLibrary.save_loaded_asset(seq, only_if_is_dirty=False)
        out["save_result"] = bool(ok)
    else:
        out["save_result"] = "no_sections_found"

    OUT.write_text(json.dumps(out, indent=2))
    unreal.log(f"[FixSpawnRange] {out}")


if __name__ == "__main__":
    main()
