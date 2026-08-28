"""Apply ALIS's Dev (fast preview) settings to MP_Config_Dev.

This is the data half of the recipe; the engine-side logic lives in
[mrq_config_updater.py](mrq_config_updater.py).

Goal: drop render time by ~10-30x for movement-artefact previews while
keeping frame count and motion-blur smoothness intact, AND render fast
camera moves cleanly (highest motion-blur tier, modest temporal
accumulation).

What we change vs the Prod baseline:

  Output:
    output_resolution           1920x1080 -> 960x540  (4x fewer pixels)

  AntiAliasing:
    temporal_sample_count        7 -> 6              (less accumulation,
                                                      enough sub-frames for
                                                      smooth motion blur on
                                                      fast camera)
    render_warm_up_count        32 -> 8
    engine_warm_up_count        32 -> 8
    use_camera_cut_for_warm_up  False (kept)         (see WARNING below)

  Console Variables:
    r.MotionBlurQuality         4 -> 4 (KEPT TOP)    (compensates fast
                                                      camera motion in
                                                      render; near-free
                                                      at render time)

WARNING -- `use_camera_cut_for_warm_up` MUST stay False until takes
reliably carry handle frames before the Camera Cut section's playback
start. A5's auto-stamper writes a Camera Cut track spanning ONLY the
playback range (no pre-roll), so True causes MRQ to log
`"Shot was asked to use excess Camera Cut section data for warm-up but
no warmup range was detected"` and silently skip warmup. Skipped
warmup = unsettled groom physics, unconverged TSR, texture pop, first-
frame hitches. Keep False; revert only when takes get pre-roll handles.

CVar discipline -- only UE 5.7-verified names. Invented names (
`r.PostProcessAAQuality`, `r.Decals`, `r.DBuffer.Decals`,
`r.PathTracing.Decals`, `r.PathTracing.Decals.MaxCount`) cause MRQ to
warn `"no cvar by that name. Ignoring"`. They are listed in
`remove_stale` so old asset state is scrubbed on re-apply.

What we deliberately do NOT touch:

  - output_frame_rate, output_frame_step, playback range -> frame count
  - spatial_sample_count                                 -> stays at 1
  - r.CustomDepth, r.DBuffer, r.Decals                   -> highlight /
                                                            decal pipeline
                                                            (speed cost
                                                            negligible)
  - WidgetRenderer compositing                           -> authored UI only;
                                                            Kazan aerial Render
                                                            removes gameplay UMG
  - GameMode override                                    -> normal init

Idempotent: re-running produces the same asset state. Run when:
  - The Dev preset was reverted or reset.
  - You want to re-pin the recipe after manual MRQ UI changes.

Run via ue-mcp or the editor `py` console:

    py <project-root>/scripts/ue/cinematic/apply_dev_preset.py

Output: Saved/cinematic_apply_dev_result.json
"""
from __future__ import annotations

import sys

# Make sibling module importable when launched via `py <path>`.
sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))

from mrq_config_updater import run_preset  # noqa: E402

ASSET = "/Game/Cinematics/MP_Config_Dev"

OUTPUT = {
    "output_resolution": (960, 540),
}

ANTI_ALIASING = {
    # 6 temporal samples = ~2.5x cheaper than Prod's 15 but still gives
    # enough sub-frame averaging for smooth motion blur on fast camera.
    "temporal_sample_count": 6,
    "render_warm_up_count": 8,
    "engine_warm_up_count": 8,
    # See header WARNING: must stay False until takes carry pre-roll
    # handles before the Camera Cut section.
    "use_camera_cut_for_warm_up": False,
}

CONSOLE_VARIABLES = {
    "set": {
        # Defensive: keep deferred decal buffer on under any scalability swing.
        "r.DBuffer": 1.0,
        # Stencil headroom for the interaction highlight post-process material.
        "r.CustomDepth": 3.0,
        # Highest motion blur tier -- compensates fast camera motion at
        # render time. Cost vs lower tiers is small.
        "r.MotionBlurQuality": 4.0,
    },
    # Stale / invented CVar names from earlier patch runs. UE 5.7 has no
    # such CVars and MRQ logs "no cvar by that name. Ignoring" for each.
    # Safe no-op if absent.
    "remove_stale": (
        "r.PostProcessAAQuality",   # invented; was added in error
        "r.Decals",
        "r.DBuffer.Decals",
        "r.PathTracing.Decals",
        "r.PathTracing.Decals.MaxCount",
        "r.Decal.AllowAtlasing",
        "r.Decal.DBufferAllow",
        "r.PathTracing.SamplesPerPixel",
        "r.PathTracing.MaxBounces",
    ),
}

WIDGET_RENDERER = {
    "composite_onto_final_image": True,
}

GAME_OVERRIDE = {
    "soft_game_mode_override": "/Script/ProjectCinematic.CinematicGameMode",
    "cinematic_quality_settings": False,
    "texture_streaming": "NONE",
    "use_lod_zero": False,
    "disable_hlods": False,
    "use_high_quality_shadows": False,
    "shadow_distance_scale": 1,
    "shadow_radius_threshold": 0.03,
    "override_view_distance_scale": False,
    "view_distance_scale": 1,
    "flush_grass_streaming": False,
    "override_grass_cull_distance_scale": False,
    "grass_cull_distance_scale": 1.0,
    "override_grass_density_scale": False,
    "grass_density_scale": 1.0,
    "flush_streaming_managers": True,
    "virtual_texture_feedback_factor": 1,
}


def main() -> None:
    run_preset(
        asset_path=ASSET,
        output=OUTPUT,
        anti_aliasing=ANTI_ALIASING,
        console_variables=CONSOLE_VARIABLES,
        widget_renderer=WIDGET_RENDERER,
        game_override=GAME_OVERRIDE,
        out_filename="cinematic_apply_dev_result.json",
    )


if __name__ == "__main__":
    main()
