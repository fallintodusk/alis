"""One-shot inspector for a MoviePipelinePrimaryConfig.

Dumps every setting on the preset and a useful subset of editor
properties so we can compare Dev vs Prod presets before editing.

Run via ue-mcp console:

    py <project-root>/scripts/ue/cinematic/_inspect_mrq_config.py [/Game/Path/Asset]

Output: Saved/cinematic_inspect_<asset>.json
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal

DEFAULT_ASSET = "/Game/Cinematics/Pending_MoviePipelinePrimaryConfig"
ASSET = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ASSET

OUT_DIR = Path(unreal.Paths.project_saved_dir())
ASSET_TAG = ASSET.rsplit("/", 1)[-1]
OUT = OUT_DIR / f"cinematic_inspect_{ASSET_TAG}.json"


def safe_get(obj, prop):
    try:
        v = obj.get_editor_property(prop)
        return repr(v)
    except Exception as exc:
        return f"<err: {exc!r}>"


def main() -> None:
    result: dict = {"asset": ASSET, "settings": [], "errors": []}
    cfg = unreal.load_asset(ASSET)
    if cfg is None:
        result["errors"].append(f"load_asset returned None for {ASSET}")
        OUT.write_text(json.dumps(result, indent=2))
        return

    for s in cfg.get_all_settings():
        cls_name = s.get_class().get_name()
        entry = {
            "class": cls_name,
            "is_enabled": getattr(s, "is_enabled", lambda: None)(),
            "props": {},
        }

        # Class-specific extraction of the props we care about.
        if cls_name == "MoviePipelineOutputSetting":
            for p in (
                "output_resolution",
                "use_custom_frame_rate",
                "output_frame_rate",
                "output_frame_step",
                "use_custom_playback_range",
                "custom_start_frame",
                "custom_end_frame",
                "handle_frame_count",
                "version_number",
                "auto_version",
                "output_directory",
                "file_name_format",
                "zero_pad_frame_numbers",
                "flush_disk_writes_per_shot",
            ):
                entry["props"][p] = safe_get(s, p)

        elif cls_name == "MoviePipelineAntiAliasingSetting":
            for p in (
                "spatial_sample_count",
                "temporal_sample_count",
                "override_anti_aliasing",
                "anti_aliasing_method",
                "render_warm_up_count",
                "use_camera_cut_for_warm_up",
                "render_warm_up_frames",
                "engine_warm_up_count",
            ):
                entry["props"][p] = safe_get(s, p)

        elif cls_name == "MoviePipelineConsoleVariableSetting":
            try:
                entries = []
                for e in s.get_console_variables():
                    entries.append({
                        "name": e.get_editor_property("name"),
                        "value": e.get_editor_property("value"),
                        "enabled": e.get_editor_property("is_enabled"),
                    })
                entry["props"]["console_variables"] = entries
            except Exception as exc:
                entry["props"]["console_variables_err"] = repr(exc)

        elif cls_name == "MoviePipelineGameOverrideSetting":
            for p in (
                "soft_game_mode_override",
                "cinematic_quality_settings",
                "game_mode_override",
                "disable_hlods",
                "use_high_quality_shadows",
                "shadow_distance_scale",
                "shadow_radius_threshold",
                "override_view_distance_scale",
                "view_distance_scale",
                "flush_grass_streaming",
                "flush_streaming_managers",
                "virtual_texture_feedback_factor",
            ):
                entry["props"][p] = safe_get(s, p)

        elif cls_name == "MoviePipelineDeferredPassBase" or cls_name.startswith("MoviePipelineDeferredPass"):
            for p in (
                "disable_multisample_effects",
                "accumulator_includes_alpha",
                "use_32bit_post_process_materials",
            ):
                entry["props"][p] = safe_get(s, p)

        elif cls_name == "MoviePipelineHighResSetting":
            for p in (
                "tile_count",
                "overlap_ratio",
                "texture_sharpness_bias",
                "overscan_percentage",
            ):
                entry["props"][p] = safe_get(s, p)

        elif cls_name == "MoviePipelineWidgetRenderer":
            for p in ("composite_onto_final_image",):
                entry["props"][p] = safe_get(s, p)

        result["settings"].append(entry)

    OUT.write_text(json.dumps(result, indent=2))
    print(f"Wrote {OUT}")


if __name__ == "__main__":
    main()
