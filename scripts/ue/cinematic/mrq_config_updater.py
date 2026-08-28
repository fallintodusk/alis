"""Reusable updater for MoviePipelinePrimaryConfig assets.

This module is the **engine-side recipe applier**. It does not own any
preset's data -- the data lives in `apply_dev_preset.py` and
`apply_prod_preset.py` (or any other caller). Each caller declares which
sections of the preset it wants to enforce; this module knows how to
poke each section through UE's Python API without clobbering unrelated
state.

Design rules:

  - **Patch, not replace.** Sections not declared by the caller stay
    untouched. Sections declared by the caller are enforced via
    set_editor_property / add_or_update_console_variable so re-runs are
    idempotent.
  - **Never invent a setting we don't already have.** AntiAliasing is
    the load-bearing example: creating a fresh AntiAliasingSetting
    resets sample counts and AA method to engine defaults, undoing
    manual tuning. We refuse to create it.
  - **Convert friendly Python values at the boundary.** Tuples become
    IntPoint / FrameRate, string enums become AntiAliasingMethod, soft
    class paths become UClass via load_class. Callers stay readable.
  - **Diagnostics over silent success.** Every operation appends to
    `result["ops"]`; any failure appends to `result["errors"]`. The
    final JSON is written to `Saved/<out_filename>` so a CI run or a
    user can inspect what actually changed.

Run via ue-mcp / the editor `py` console; see the apply_*_preset.py
companions.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import unreal

# --------------------------------------------------------------------- conversions

def _to_int_point(value: Any) -> unreal.IntPoint:
    if isinstance(value, unreal.IntPoint):
        return value
    if isinstance(value, (tuple, list)) and len(value) == 2:
        return unreal.IntPoint(int(value[0]), int(value[1]))
    raise TypeError(f"Cannot convert {value!r} to IntPoint (want (W, H))")


def _to_frame_rate(value: Any) -> unreal.FrameRate:
    if isinstance(value, unreal.FrameRate):
        return value
    if isinstance(value, (tuple, list)) and len(value) == 2:
        fr = unreal.FrameRate()
        fr.numerator = int(value[0])
        fr.denominator = int(value[1])
        return fr
    if isinstance(value, (int, float)):
        fr = unreal.FrameRate()
        fr.numerator = int(value)
        fr.denominator = 1
        return fr
    raise TypeError(f"Cannot convert {value!r} to FrameRate (want (num, den))")


_AA_METHOD_MAP = {
    "NONE": unreal.AntiAliasingMethod.AAM_NONE,
    "FXAA": unreal.AntiAliasingMethod.AAM_FXAA,
    "TAA":  unreal.AntiAliasingMethod.AAM_TEMPORAL_AA,
    "MSAA": unreal.AntiAliasingMethod.AAM_MSAA,
    "TSR":  unreal.AntiAliasingMethod.AAM_TSR,
}

_TEXTURE_STREAMING_MAP = {
    "NONE": unreal.MoviePipelineTextureStreamingMethod.NONE,
    "DISABLED": unreal.MoviePipelineTextureStreamingMethod.DISABLED,
    "FULLY_LOAD": unreal.MoviePipelineTextureStreamingMethod.FULLY_LOAD,
}


def _to_aa_method(value: Any):
    if isinstance(value, unreal.AntiAliasingMethod):
        return value
    if isinstance(value, str):
        key = value.upper().replace("AAM_", "")
        if key in _AA_METHOD_MAP:
            return _AA_METHOD_MAP[key]
    raise TypeError(f"Cannot convert {value!r} to AntiAliasingMethod")


def _to_texture_streaming(value: Any):
    if isinstance(value, unreal.MoviePipelineTextureStreamingMethod):
        return value
    if isinstance(value, str) and value.upper() in _TEXTURE_STREAMING_MAP:
        return _TEXTURE_STREAMING_MAP[value.upper()]
    raise TypeError(f"Cannot convert {value!r} to MoviePipelineTextureStreamingMethod")


# Properties that need value conversion before set_editor_property.
_OUTPUT_CONVERTERS = {
    "output_resolution": _to_int_point,
    "output_frame_rate": _to_frame_rate,
}

_AA_CONVERTERS = {
    "anti_aliasing_method": _to_aa_method,
}

_GAME_OVERRIDE_CONVERTERS = {
    "texture_streaming": _to_texture_streaming,
}


# --------------------------------------------------------------------- helpers

def _find_setting(cfg, class_name: str):
    """Return the first setting whose UClass name matches, or None.

    We compare class names instead of using find_or_add so optional
    sections (e.g. AntiAliasing) can be skipped when the preset doesn't
    already carry them.
    """
    for s in cfg.get_all_settings():
        if s.get_class().get_name() == class_name:
            return s
    return None


def _apply_props(setting, props: dict, converters: dict | None = None) -> dict:
    """set_editor_property for every key in props, with optional conversion.

    Returns {"before": {...}, "after": {...}} for the diff log.
    """
    converters = converters or {}
    before, after = {}, {}
    for name, value in props.items():
        try:
            before[name] = repr(setting.get_editor_property(name))
        except Exception as exc:
            before[name] = f"<read err: {exc!r}>"

        coerced = converters[name](value) if name in converters else value
        setting.set_editor_property(name, coerced)

        try:
            after[name] = repr(setting.get_editor_property(name))
        except Exception as exc:
            after[name] = f"<read err: {exc!r}>"
    return {"before": before, "after": after}


# --------------------------------------------------------------------- sections

def apply_output(cfg, props: dict, result: dict) -> None:
    if not props:
        return
    setting = cfg.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    diff = _apply_props(setting, props, _OUTPUT_CONVERTERS)
    result["ops"].append({"section": "output", **diff})


def apply_anti_aliasing(cfg, props: dict, result: dict) -> None:
    """Enforce AA props ONLY on an existing AntiAliasingSetting.

    Skips with a clear note if no AA setting is on the preset, because
    creating a fresh one would reset sample counts and AA method to
    engine defaults.
    """
    if not props:
        return
    setting = _find_setting(cfg, "MoviePipelineAntiAliasingSetting")
    if setting is None:
        result["errors"].append(
            "anti_aliasing: no existing MoviePipelineAntiAliasingSetting on preset; "
            "refusing to create one (would clobber sample counts / AA method). "
            "Add it manually in the MRQ UI first."
        )
        return
    diff = _apply_props(setting, props, _AA_CONVERTERS)
    result["ops"].append({"section": "anti_aliasing", **diff})


def apply_console_variables(cfg, spec: dict, result: dict) -> None:
    """Apply a ConsoleVariableSetting spec.

    spec = {
        "set": {name: value, ...},
        "remove_stale": [name, name, ...]  # optional
    }
    """
    if not spec:
        return
    cv = cfg.find_or_add_setting_by_class(unreal.MoviePipelineConsoleVariableSetting)

    # Snapshot before for the diff log.
    before = []
    for entry in cv.get_console_variables():
        before.append({
            "name": entry.get_editor_property("name"),
            "value": entry.get_editor_property("value"),
        })

    for stale in spec.get("remove_stale", ()) or ():
        try:
            cv.remove_console_variable(stale)
        except Exception:
            pass  # Not present == nothing to remove.

    for name, value in (spec.get("set", {}) or {}).items():
        cv.add_or_update_console_variable(name, float(value))

    after = []
    for entry in cv.get_console_variables():
        after.append({
            "name": entry.get_editor_property("name"),
            "value": entry.get_editor_property("value"),
        })

    result["ops"].append({"section": "console_variables", "before": before, "after": after})


def apply_widget_renderer(cfg, props: dict, result: dict) -> None:
    if not props:
        return
    wr = cfg.find_or_add_setting_by_class(unreal.MoviePipelineWidgetRenderer)
    diff = _apply_props(wr, props)
    wr.set_is_enabled(True)
    result["ops"].append({"section": "widget_renderer", **diff})


def apply_game_override(cfg, props: dict, result: dict) -> None:
    """Apply GameOverride properties. soft_game_mode_override accepts a
    string class path; we resolve it via load_class because UE's Python
    binding rejects raw FSoftClassPath for this UProperty.
    """
    if not props:
        return
    go = cfg.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)

    resolved: dict[str, Any] = {}
    for name, value in props.items():
        if name == "soft_game_mode_override" and isinstance(value, str):
            cls = unreal.load_class(None, value)
            if cls is None:
                result["errors"].append(
                    f"game_override.soft_game_mode_override: class '{value}' not loaded. "
                    "Rebuild the editor and re-run."
                )
                continue
            resolved[name] = cls
        else:
            resolved[name] = value

    diff = _apply_props(go, resolved, _GAME_OVERRIDE_CONVERTERS)
    go.set_is_enabled(True)
    result["ops"].append({"section": "game_override", **diff})


# --------------------------------------------------------------------- entrypoint

def run_preset(
    *,
    asset_path: str,
    output: dict | None = None,
    anti_aliasing: dict | None = None,
    console_variables: dict | None = None,
    widget_renderer: dict | None = None,
    game_override: dict | None = None,
    out_filename: str,
) -> dict:
    """Apply the declared sections to the MoviePipelinePrimaryConfig at
    `asset_path`. Writes a JSON report to Saved/<out_filename>.
    Returns the result dict for in-process inspection.
    """
    result: dict[str, Any] = {"asset": asset_path, "ops": [], "errors": []}

    cfg = unreal.load_asset(asset_path)
    if cfg is None:
        result["errors"].append(f"load_asset returned None for {asset_path}")
    else:
        result["before_settings"] = [s.get_class().get_name() for s in cfg.get_all_settings()]

        apply_output(cfg, output or {}, result)
        apply_anti_aliasing(cfg, anti_aliasing or {}, result)
        apply_console_variables(cfg, console_variables or {}, result)
        apply_widget_renderer(cfg, widget_renderer or {}, result)
        apply_game_override(cfg, game_override or {}, result)

        try:
            unreal.EditorAssetLibrary.save_loaded_asset(cfg, only_if_is_dirty=False)
            result["ops"].append({"save": True})
        except Exception as exc:
            result["errors"].append(f"save: {exc!r}")

        result["after_settings"] = [s.get_class().get_name() for s in cfg.get_all_settings()]

    out_path = Path(unreal.Paths.project_saved_dir()) / out_filename
    out_path.write_text(json.dumps(result, indent=2))
    print(f"Wrote {out_path}")
    return result
