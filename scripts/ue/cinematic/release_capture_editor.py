"""Execute one authenticated MRQ release-capture request inside Unreal Editor."""

from __future__ import annotations

import datetime as dt
import hashlib
import json
import os
import re
import traceback
from pathlib import Path

import unreal


REQUEST_ENV = "PROJECT_CINEMATIC_CAPTURE_REQUEST"
STATUS_ENV = "PROJECT_CINEMATIC_CAPTURE_STATUS"
OUTPUT_ENV = "PROJECT_CINEMATIC_CAPTURE_OUTPUT"
OPERATION_ENV = "PROJECT_CINEMATIC_CAPTURE_OPERATION"

_runtime: dict[str, object] = {}


def _utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _write_status(state: str, **values: object) -> None:
    status_path = Path(os.environ[STATUS_ENV])
    existing: dict[str, object] = {}
    if status_path.is_file():
        try:
            existing = json.loads(status_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            existing = {}
    payload = {
        **existing,
        "schema_version": 1,
        "state": state,
        "operation_id": os.environ[OPERATION_ENV],
        "updated_utc": _utc_now(),
        **values,
    }
    status_path.parent.mkdir(parents=True, exist_ok=True)
    status_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    unreal.log(f"[ProjectCinematic::ReleaseCapture] state={state}")


def _fail(reason: str, **values: object) -> None:
    _write_status("error", reason=reason, **values)


def _object_path(package_path: str) -> str:
    return f"{package_path}.{package_path.rsplit('/', 1)[-1]}"


def _read_request() -> dict[str, object]:
    request_path = Path(os.environ[REQUEST_ENV])
    request = json.loads(request_path.read_text(encoding="utf-8"))
    required = {
        "schema_version",
        "capture_id",
        "map_package",
        "sequence",
        "preset",
        "camera_class",
        "playback_start",
        "playback_end",
        "scalability_quality",
        "timeout_seconds",
        "release_acceptance",
        "package_root",
    }
    missing = sorted(required.difference(request))
    if missing:
        raise ValueError(f"Request is missing fields: {missing}")
    if request["schema_version"] != 2:
        raise ValueError("Only release-capture request schema_version 2 is supported")
    if not re.fullmatch(r"[a-z0-9][a-z0-9_-]+", str(request["capture_id"])):
        raise ValueError("capture_id is not a stable lowercase identity")
    if request["scalability_quality"] != 2:
        raise ValueError("Release capture must use the accepted High scalability tier")
    if int(request["playback_end"]) <= int(request["playback_start"]):
        raise ValueError("playback_end must be greater than playback_start")
    return request


def _setting_snapshot(config: object) -> dict[str, object]:
    output: dict[str, object] = {"classes": []}
    for setting in config.get_all_settings():
        class_name = setting.get_class().get_name()
        output["classes"].append(class_name)
        if class_name == "MoviePipelineGameOverrideSetting":
            props = (
                "soft_game_mode_override",
                "cinematic_quality_settings",
                "texture_streaming",
                "use_lod_zero",
                "disable_hlods",
                "use_high_quality_shadows",
                "override_view_distance_scale",
                "flush_grass_streaming",
                "override_grass_cull_distance_scale",
                "flush_streaming_managers",
                "virtual_texture_feedback_factor",
            )
            output["game_override"] = {
                name: repr(setting.get_editor_property(name)) for name in props
            }
        elif class_name == "MoviePipelineOutputSetting":
            output["output"] = {
                "resolution": repr(setting.get_editor_property("output_resolution")),
                "frame_rate": repr(setting.get_editor_property("output_frame_rate")),
                "use_custom_frame_rate": bool(
                    setting.get_editor_property("use_custom_frame_rate")
                ),
            }
    return output


def _audit_sequence(sequence: object, request: dict[str, object]) -> dict[str, object]:
    bindings = sequence.get_bindings()
    spawnables = sequence.get_spawnables()
    camera_bindings = [
        binding
        for binding in bindings
        if binding.get_object_template()
        and binding.get_object_template().get_class().get_name()
        == request["camera_class"]
    ]
    camera_cut_tracks = [
        track
        for track in sequence.get_tracks()
        if track.get_class().get_name() == "MovieSceneCameraCutTrack"
    ]
    if len(spawnables) != 1 or len(camera_bindings) != 1:
        raise ValueError("Sequence must own exactly one spawnable CineCameraActor")
    if len(camera_cut_tracks) != 1 or len(camera_cut_tracks[0].get_sections()) != 1:
        raise ValueError("Sequence must own exactly one camera-cut section")
    if sequence.get_playback_start() != request["playback_start"]:
        raise ValueError("Sequence playback start does not match request")
    if sequence.get_playback_end() != request["playback_end"]:
        raise ValueError("Sequence playback end does not match request")
    camera = camera_bindings[0]
    cut = camera_cut_tracks[0].get_sections()[0]
    cut_guid = cut.get_camera_binding_id().get_editor_property("guid").to_string()
    camera_guid = camera.get_id().to_string()
    if cut_guid != camera_guid:
        raise ValueError("Camera-cut section does not target the CineCamera binding")
    return {
        "spawnable_count": len(spawnables),
        "camera_binding_guid": camera_guid,
        "camera_cut_start": int(cut.get_start_frame()),
        "camera_cut_end": int(cut.get_end_frame()),
        "playback_start": sequence.get_playback_start(),
        "playback_end": sequence.get_playback_end(),
    }


def _apply_high_scalability(world: object) -> dict[str, str]:
    values: dict[str, str] = {}
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
        unreal.SystemLibrary.execute_console_command(world, f"{name} 2")
        variable = unreal.SystemLibrary.get_console_variable_int_value(name)
        values[name] = str(variable)
        if variable != 2:
            raise ValueError(f"Unable to set {name}=2 for High capture truth")
    return values


def _output_inventory(output_root: Path) -> tuple[list[dict[str, object]], list[str]]:
    files: list[dict[str, object]] = []
    errors: list[str] = []
    for path in sorted(item for item in output_root.rglob("*") if item.is_file()):
        relative = path.relative_to(output_root).as_posix()
        files.append(
            {
                "path": relative,
                "size_bytes": path.stat().st_size,
                "sha256": _sha256(path),
            }
        )
        if path.stat().st_size == 0:
            errors.append(f"Zero-byte render output: {relative}")
    if not files:
        errors.append("MRQ produced no output files")
    return files, errors


def main() -> None:
    request = _read_request()
    output_root = Path(os.environ[OUTPUT_ENV])
    output_root.mkdir(parents=True, exist_ok=True)
    _write_status("starting", capture_id=request["capture_id"])

    sequence = unreal.load_asset(str(request["sequence"]))
    preset = unreal.load_asset(str(request["preset"]))
    world_asset = unreal.load_asset(str(request["map_package"]))
    if sequence is None or preset is None or world_asset is None:
        raise ValueError("Map, sequence, or preset asset could not be loaded")

    sequence_audit = _audit_sequence(sequence, request)
    preset_snapshot = _setting_snapshot(preset)
    game_override = preset_snapshot.get("game_override", {})
    forbidden_truth = {
        "cinematic_quality_settings": "True",
        "use_lod_zero": "True",
        "disable_hlods": "True",
        "use_high_quality_shadows": "True",
        "override_view_distance_scale": "True",
        "flush_grass_streaming": "True",
        "override_grass_cull_distance_scale": "True",
    }
    for name, forbidden in forbidden_truth.items():
        if game_override.get(name) == forbidden:
            raise ValueError(f"Preset changes packaged-product truth: {name}={forbidden}")

    editor_world = unreal.EditorLevelLibrary.get_editor_world()
    scalability = _apply_high_scalability(editor_world)
    queue_subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    if queue_subsystem is None:
        raise RuntimeError("MoviePipelineQueueSubsystem is unavailable")
    queue = queue_subsystem.get_queue()
    queue.delete_all_jobs()
    job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
    job.sequence = unreal.SoftObjectPath(_object_path(str(request["sequence"])))
    job.map = unreal.SoftObjectPath(_object_path(str(request["map_package"])))
    # Copy the asset into the job so MRQ cannot refresh the preset after the
    # request-specific output path is applied.
    job.set_configuration(preset)
    job.job_name = str(request["capture_id"])

    config = job.get_configuration()
    output = config.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    directory = unreal.DirectoryPath()
    directory.path = output_root.as_posix()
    output.set_editor_property("output_directory", directory)
    output.set_editor_property("file_name_format", "{sequence_name}.{frame_number}")
    output.set_editor_property("override_existing_output", True)
    output.set_editor_property("auto_version", False)

    _write_status(
        "queued",
        request=request,
        sequence_audit=sequence_audit,
        preset_snapshot=_setting_snapshot(config),
        scalability=scalability,
        engine_version=unreal.SystemLibrary.get_engine_version(),
        map=str(job.map),
        sequence=str(job.sequence),
    )

    executor = queue_subsystem.render_queue_with_executor(unreal.MoviePipelinePIEExecutor)
    if executor is None:
        raise RuntimeError("render_queue_with_executor returned None")
    _runtime["executor"] = executor
    _runtime["errored"] = False
    _runtime["error_reason"] = None

    def on_errored(_executor, _pipeline, is_fatal, error_text) -> None:
        _runtime["errored"] = True
        _runtime["error_reason"] = str(error_text)
        _write_status("error", fatal=bool(is_fatal), reason=str(error_text))

    def on_finished(in_executor, success) -> None:
        errored_count = (
            in_executor.get_errored_jobs_count()
            if hasattr(in_executor, "get_errored_jobs_count")
            else -1
        )
        files, output_errors = _output_inventory(output_root)
        accepted = (
            bool(success)
            and not bool(_runtime["errored"])
            and errored_count <= 0
            and not output_errors
        )
        _write_status(
            "finished" if accepted else "error",
            success=accepted,
            raw_success_flag=bool(success),
            errored_count=errored_count,
            error_reason=_runtime["error_reason"],
            outputs=files,
            output_errors=output_errors,
        )
        unreal.SystemLibrary.quit_editor()

    if hasattr(executor, "on_executor_errored_delegate"):
        executor.on_executor_errored_delegate.add_callable(on_errored)
    executor.on_executor_finished_delegate.add_callable(on_finished)
    _runtime["on_errored"] = on_errored
    _runtime["on_finished"] = on_finished
    _write_status("rendering", capture_id=request["capture_id"])


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        _fail(str(exc), traceback=traceback.format_exc())
        unreal.SystemLibrary.quit_editor()
