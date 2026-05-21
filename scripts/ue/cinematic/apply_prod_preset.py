"""Apply ALIS's Prod (trailer-grade) settings to MP_Config_Prod.

This is the data half of the recipe; the engine-side logic lives in
[mrq_config_updater.py](mrq_config_updater.py).

Encodes the trailer preset baseline. Values target a ~30% wall-clock
reduction vs the pre-2026-05-19 baseline (TSC=8, warmup=32+32, VDS=50,
SDS=10, Grass=50, VT=1) with no visible difference in the final ProRes
master. To roll back, revert the values in this file and re-run.

  Output:
    1920x1080 @ 60fps (custom framerate enabled)

  AntiAliasing:
    1 spatial * 7 temporal samples, TSR
    16 + 16 warmup frames
    (Odd TSC lands a sample on frame-center; warmup 16 covers TSR
    convergence + groom physics settle at 60 fps. Streaming pop-in is
    not the limiter because Texture Streaming = Disable + LODZero ON.)

  Console Variables (verified against UE 5.7 DumpCVars):
    r.DBuffer=1                         - deferred decal buffer baseline
    r.CustomDepth=3                     - stencil headroom for highlight PP
    r.MotionBlurQuality=4               - highest MB tier
    r.PathTracing.SamplesPerPixel=256   - inert when PT off, sane fallback
    r.PathTracing.MaxBounces=8          - inert when PT off, sane fallback

  WidgetRenderer: composite UMG onto final image.
  GameOverride:
    GameMode override = AAlisCinematicGameMode so HUD/pawn/interaction
      tick at render time (the engine default GameMode hides everything).
    view_distance_scale = 25           - was 50; 25x gameplay still
                                          exceeds every shot's far plane.
    shadow_distance_scale = 5          - was 10; canonical cinematic
                                          recommendation.
    grass_cull_distance_scale = 25     - was 50; vastly exceeds the
                                          camera frustum on hero shots.
    virtual_texture_feedback_factor = 2 - was 1; with TSR accumulating 7
                                          sub-frames, one-frame VT lag
                                          is invisible.

Idempotent: re-running produces the same asset state. Run when:
  - The Prod preset was reverted or reset.
  - You want to re-pin the recipe after manual MRQ UI changes.

Run via ue-mcp or the editor `py` console:

    py <project-root>/scripts/ue/cinematic/apply_prod_preset.py

Output: Saved/cinematic_apply_prod_result.json
"""
from __future__ import annotations

import sys

# Make sibling module importable when launched via `py <path>`.
sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))

from mrq_config_updater import run_preset  # noqa: E402

ASSET = "/Game/Cinematics/MP_Config_Prod"

OUTPUT = {
    "output_resolution": (1920, 1080),
    "use_custom_frame_rate": True,
    "output_frame_rate": (60, 1),
}

ANTI_ALIASING = {
    "spatial_sample_count": 1,
    # Odd TSC lands a sample at frame-center (cleaner sub-frame averaging
    # on animated impacts than even counts). At 60 fps the shutter window
    # is 16.7 ms; 7 samples (~2.4 ms apart) is indistinguishable from 8
    # in TSR-accumulated motion blur.
    "temporal_sample_count": 7,
    "override_anti_aliasing": False,
    "anti_aliasing_method": "TSR",
    # 16+16 covers TSR convergence (8-12 frames at 60 fps) + groom physics
    # (8-10 frames) + a margin. Streaming pop-in is not the limiter because
    # Texture Streaming = Disable + LODZero ON. If a future shot shows
    # snap-in on frames 1-4, lift to 24+24.
    "render_warm_up_count": 16,
    "engine_warm_up_count": 16,
    # NOTE: Prod currently ships with this False on the asset. If a future
    # trailer needs cut-aware warmup, flip to True here (and re-run).
    "use_camera_cut_for_warm_up": False,
}

CONSOLE_VARIABLES = {
    "set": {
        "r.DBuffer": 1.0,
        "r.CustomDepth": 3.0,
        "r.MotionBlurQuality": 4.0,
        "r.PathTracing.SamplesPerPixel": 256.0,
        "r.PathTracing.MaxBounces": 8.0,
    },
    # Stale CVar names from earlier ad-hoc patch runs. Safe no-op if absent.
    "remove_stale": (
        "r.Decals",
        "r.DBuffer.Decals",
        "r.PostProcessAAQuality",
        "r.PathTracing.Decals",
        "r.PathTracing.Decals.MaxCount",
        "r.Decal.AllowAtlasing",
        "r.Decal.DBufferAllow",
    ),
}

WIDGET_RENDERER = {
    "composite_onto_final_image": True,
}

GAME_OVERRIDE = {
    "soft_game_mode_override": "/Script/ProjectSinglePlay.AlisCinematicGameMode",
    # Scalability trims: every multiplier here was previously set to a
    # "be safe" value that exceeded what any recorded shot's camera
    # frustum actually contained. The values below stay generous (still
    # 5-25x gameplay defaults) but stop paying for headroom we never use.
    "view_distance_scale": 25.0,
    "shadow_distance_scale": 5.0,
    "grass_cull_distance_scale": 25.0,
    "virtual_texture_feedback_factor": 2,
}


def main() -> None:
    run_preset(
        asset_path=ASSET,
        output=OUTPUT,
        anti_aliasing=ANTI_ALIASING,
        console_variables=CONSOLE_VARIABLES,
        widget_renderer=WIDGET_RENDERER,
        game_override=GAME_OVERRIDE,
        out_filename="cinematic_apply_prod_result.json",
    )


if __name__ == "__main__":
    main()
