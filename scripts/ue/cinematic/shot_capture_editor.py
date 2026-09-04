"""Build one camera move from a shot plan and render it through MRQ.

Runs inside a cold-started Unreal Editor opened on the plan's map. These are
disposable raw takes for a human to cut later: the route makes no release claim,
binds to no Candidate, and writes to its own root.
"""

from __future__ import annotations

import datetime as dt
import json
import math
import os
import traceback
from pathlib import Path

import unreal


PLAN_ENV = "PROJECT_CINEMATIC_SHOT_PLAN"
STATUS_ENV = "PROJECT_CINEMATIC_SHOT_STATUS"
OUTPUT_ENV = "PROJECT_CINEMATIC_SHOT_OUTPUT"
PREVIEW_ENV = "PROJECT_CINEMATIC_SHOT_PREVIEW"

GENERATED_PACKAGE_ROOT = "/Game/Cinematics/Generated"
DEFAULT_PRESET = "/Game/Cinematics/MP_Config_Prod"
CAPTURE_SCALABILITY = 2
WARM_UP_TICKS = 900

_runtime: dict[str, object] = {}


def _write_status(state: str, **values: object) -> None:
    path = Path(os.environ[STATUS_ENV])
    existing: dict[str, object] = {}
    if path.is_file():
        try:
            existing = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            existing = {}
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(
            {
                **existing,
                "state": state,
                "updated_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
                **values,
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    unreal.log(f"[ShotCapture] state={state}")


def _object_path(package_path: str) -> str:
    return f"{package_path}.{package_path.rsplit('/', 1)[-1]}"


def _read_plan() -> dict[str, object]:
    plan = json.loads(Path(os.environ[PLAN_ENV]).read_text(encoding="utf-8"))
    for field in ("id", "map", "duration", "fps", "fov", "camera"):
        if field not in plan:
            raise ValueError(f"Shot plan is missing '{field}'")
    keys = plan["camera"]
    times = [float(key["t"]) for key in keys]
    if len(keys) < 2 or times != sorted(times) or len(set(times)) != len(times):
        raise ValueError("Camera keys must be at least two and strictly increasing")
    if abs(times[0]) > 1e-6 or abs(times[-1] - float(plan["duration"])) > 1e-6:
        raise ValueError("Camera keys must span exactly 0 to duration")
    return plan


def _authenticate_editor_world(expected_map: str) -> str:
    """The editor opens a fallback world when the requested map fails to load and
    then renders a perfectly valid capture of the wrong place. Refuse instead."""
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        raise ValueError("No editor world is loaded")
    actual = world.get_path_name().split(".")[0]
    if actual != expected_map:
        raise ValueError(f"Editor loaded '{actual}' but the plan requested '{expected_map}'")
    return actual


def _build_sequence(plan: dict[str, object]) -> tuple[str, int]:
    shot_id = str(plan["id"])
    fps = int(plan["fps"])
    total_frames = int(round(float(plan["duration"]) * fps))
    asset_path = f"{GENERATED_PACKAGE_ROOT}/{shot_id}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    sequence = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name=shot_id,
        package_path=GENERATED_PACKAGE_ROOT,
        asset_class=unreal.LevelSequence,
        factory=unreal.LevelSequenceFactoryNew(),
    )
    if sequence is None:
        raise RuntimeError(f"Could not create the generated sequence at {asset_path}")

    sequence.set_display_rate(unreal.FrameRate(fps, 1))
    # Set before adding the spawnable so its spawn section already covers the take.
    sequence.set_playback_start(0)
    sequence.set_playback_end(total_frames)

    binding = sequence.add_spawnable_from_class(unreal.CineCameraActor)
    component = binding.get_object_template().get_editor_property("camera_component")
    sensor_width = float(
        component.get_editor_property("filmback").get_editor_property("sensor_width")
    )
    component.set_editor_property(
        "current_focal_length",
        (sensor_width / 2.0) / math.tan(math.radians(float(plan["fov"])) / 2.0),
    )
    # Depth of field off: an autofocus plane tracking a 14 km territory pumps
    # focus across the take and destroys the readability the shot exists for.
    focus = component.get_editor_property("focus_settings")
    focus.set_editor_property("focus_method", unreal.CameraFocusMethod.DISABLE)
    component.set_editor_property("focus_settings", focus)

    for track in binding.get_tracks():
        if track.get_class().get_name() == "MovieSceneSpawnTrack":
            for section in track.get_sections():
                section.set_range(0, total_frames)

    section = binding.add_track(unreal.MovieScene3DTransformTrack).add_section()
    section.set_range(0, total_frames)
    interpolation = (
        unreal.MovieSceneKeyInterpolation.LINEAR
        if str(plan.get("interpolation", "cubic")) == "linear"
        else unreal.MovieSceneKeyInterpolation.AUTO
    )
    channels = section.get_all_channels()
    # MovieScene3DTransformSection channel order: location XYZ, rotation
    # roll/pitch/yaw, scale XYZ.
    for key in plan["camera"]:
        frame = unreal.FrameNumber(int(round(float(key["t"]) * fps)))
        for index, field in enumerate(("x", "y", "z", "roll", "pitch", "yaw")):
            channels[index].add_key(
                frame,
                float(key.get(field, 0.0)),
                0.0,
                unreal.MovieSceneTimeUnit.DISPLAY_RATE,
                interpolation,
            )

    cut = sequence.add_track(unreal.MovieSceneCameraCutTrack).add_section()
    cut.set_range(0, total_frames)
    binding_id = unreal.MovieSceneObjectBindingID()
    binding_id.set_editor_property("guid", binding.get_id())
    cut.set_camera_binding_id(binding_id)

    unreal.EditorAssetLibrary.save_asset(asset_path)
    return asset_path, total_frames


def main() -> None:
    plan = _read_plan()
    output_root = Path(os.environ[OUTPUT_ENV])
    output_root.mkdir(parents=True, exist_ok=True)
    _write_status("starting", id=plan["id"])

    loaded_map = _authenticate_editor_world(str(plan["map"]))
    preset = unreal.load_asset(str(plan.get("preset", DEFAULT_PRESET)))
    if preset is None:
        raise ValueError(f"Preset could not be loaded: {plan.get('preset', DEFAULT_PRESET)}")

    sequence_path, total_frames = _build_sequence(plan)

    world = unreal.EditorLevelLibrary.get_editor_world()
    for name in (
        "sg.ViewDistanceQuality",
        "sg.AntiAliasingQuality",
        "sg.ShadowQuality",
        "sg.GlobalIlluminationQuality",
        "sg.ReflectionQuality",
        "sg.PostProcessQuality",
        "sg.TextureQuality",
        "sg.EffectsQuality",
        "sg.FoliageQuality",
        "sg.ShadingQuality",
        "sg.LandscapeQuality",
    ):
        unreal.SystemLibrary.execute_console_command(world, f"{name} {CAPTURE_SCALABILITY}")

    queue = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    queue.get_queue().delete_all_jobs()
    job = queue.get_queue().allocate_new_job(unreal.MoviePipelineExecutorJob)
    job.sequence = unreal.SoftObjectPath(_object_path(sequence_path))
    job.map = unreal.SoftObjectPath(_object_path(str(plan["map"])))
    # Copy the preset into the job so MRQ cannot refresh it after these overrides.
    job.set_configuration(preset)
    job.job_name = str(plan["id"])

    config = job.get_configuration()
    output = config.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    directory = unreal.DirectoryPath()
    directory.path = output_root.as_posix()
    output.set_editor_property("output_directory", directory)
    output.set_editor_property("file_name_format", "{sequence_name}.{frame_number}")
    output.set_editor_property("override_existing_output", True)
    output.set_editor_property("auto_version", False)
    output.set_editor_property("use_custom_frame_rate", True)
    output.set_editor_property("output_frame_rate", unreal.FrameRate(int(plan["fps"]), 1))
    if "resolution" in plan:
        width, height = (int(value) for value in plan["resolution"])
        output.set_editor_property("output_resolution", unreal.IntPoint(width, height))

    if os.environ.get(PREVIEW_ENV) == "1":
        # A preview answers "is the framing and the motion right", which survives a
        # quarter of the linear resolution and a single temporal sample. Streaming
        # and camera path are untouched, so what a preview shows about void walls
        # and composition still holds at production.
        output.set_editor_property("output_resolution", unreal.IntPoint(480, 270))
        config.find_or_add_setting_by_class(
            unreal.MoviePipelineAntiAliasingSetting
        ).set_editor_property("temporal_sample_count", 1)

    # World Partition streams asynchronously, so the first rendered frame needs the
    # cells already resident. Warm-up is unconditional; there is no knob for the
    # loaded extent because none was found to work - see docs/cinematics/raw_capture.md.
    config.find_or_add_setting_by_class(
        unreal.MoviePipelineAntiAliasingSetting
    ).set_editor_property("engine_warm_up_count", WARM_UP_TICKS)

    _write_status(
        "queued",
        map=loaded_map,
        sequence=sequence_path,
        frames=total_frames,
        scalability=CAPTURE_SCALABILITY,
        engine_version=unreal.SystemLibrary.get_engine_version(),
    )

    executor = queue.render_queue_with_executor(unreal.MoviePipelinePIEExecutor)

    def on_finished(in_executor, success) -> None:
        errored = (
            in_executor.get_errored_jobs_count()
            if hasattr(in_executor, "get_errored_jobs_count")
            else 0
        )
        files = [
            {"path": item.name, "size_bytes": item.stat().st_size}
            for item in sorted(output_root.rglob("*"))
            if item.is_file()
        ]
        accepted = bool(success) and errored <= 0 and bool(files)
        _write_status(
            "finished" if accepted else "error",
            success=accepted,
            errored_jobs=errored,
            outputs=files,
        )
        unreal.SystemLibrary.quit_editor()

    executor.on_executor_finished_delegate.add_callable(on_finished)
    _runtime["executor"] = executor
    _runtime["on_finished"] = on_finished
    _write_status("rendering")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        _write_status("error", reason=str(exc), traceback=traceback.format_exc())
        unreal.SystemLibrary.quit_editor()
