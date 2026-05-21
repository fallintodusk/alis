"""Drive a Movie Render Queue render of a recorded take, headlessly.

Built for the E2E autonomous flow:
  - Editor is already running (started by the script orchestrator).
  - This script is sent into the editor via `py <path>` from MCP.
  - It loads the LevelSequence + preset, queues a render job, starts the
    executor, and writes a progress JSON so the orchestrator can poll.

Run via ue-mcp console:

    py <project-root>/scripts/ue/cinematic/_drive_mrq_render.py

Output:
  Saved/cinematic_render_status.json  -- updated as render progresses
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal

SEQUENCE_PATH = "/Game/Cinematics/Takes/2026-05-19/Scene_1_01"
PRESET_PATH = "/Game/Cinematics/MP_Config_Dev"
# job.map must be a UWorld asset path, NOT the sequence path. Take Recorder
# tracks have absolute object paths under /Memory/UEDPIE_*/City17_Persistent_WP,
# so render must spin up that exact persistent world.
MAP_PATH = "/City17/Maps/City17_Persistent_WP.City17_Persistent_WP"

OUT_DIR = Path(unreal.Paths.project_saved_dir())
STATUS = OUT_DIR / "cinematic_render_status.json"


def write_status(state: str, **extras) -> None:
    payload = {"state": state, **extras}
    STATUS.write_text(json.dumps(payload, indent=2))
    unreal.log(f"[DriveMRQRender] {state}: {extras}")


def main() -> None:
    write_status("starting")

    seq = unreal.load_asset(SEQUENCE_PATH)
    if seq is None:
        write_status("error", reason=f"load_asset failed: {SEQUENCE_PATH}")
        return

    preset = unreal.load_asset(PRESET_PATH)
    if preset is None:
        write_status("error", reason=f"load_asset failed: {PRESET_PATH}")
        return

    qss_class = unreal.MoviePipelineQueueSubsystem
    qss = unreal.get_editor_subsystem(qss_class)
    if qss is None:
        write_status("error", reason="UMoviePipelineQueueSubsystem not available (not in editor?)")
        return

    queue = qss.get_queue()
    queue.delete_all_jobs()

    # Prefer Epic's helper: it creates the job AND sets job.map to the current
    # editor world. Falls back to manual allocation if helper missing or job.map
    # comes out unset.
    job = None
    helper = getattr(unreal, "MoviePipelineEditorLibrary", None)
    if helper is not None and hasattr(helper, "create_job_from_sequence"):
        try:
            job = helper.create_job_from_sequence(queue, seq)
        except Exception as exc:  # noqa: BLE001 - log and fall through
            unreal.log_warning(f"[DriveMRQRender] create_job_from_sequence failed: {exc}")
            job = None

    if job is None:
        job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
        job.sequence = unreal.SoftObjectPath(SEQUENCE_PATH)

    # Hard-pin map if helper left it unset OR if it picked the wrong world.
    map_str = str(job.map) if job.map else ""
    if not map_str or "City17_Persistent_WP" not in map_str:
        job.map = unreal.SoftObjectPath(MAP_PATH)

    job.set_preset_origin(preset)
    job.job_name = "AlisAutonomousMRQ"

    write_status("queued",
        sequence=str(job.sequence),
        map=str(job.map),
        preset=PRESET_PATH,
        job_name=job.job_name)

    # Pick a PIE executor (renders in-editor offscreen).
    executor_class = unreal.MoviePipelinePIEExecutor
    executor = qss.render_queue_with_executor(executor_class)
    if executor is None:
        write_status("error", reason="render_queue_with_executor returned None")
        return

    state = {"errored": False, "error_reason": None}

    def on_errored(in_executor, in_pipeline, is_fatal, error_text):
        state["errored"] = True
        state["error_reason"] = str(error_text)
        write_status("error",
            fatal=bool(is_fatal),
            reason=str(error_text))

    def on_finished(in_executor, success):
        errored_count = (
            in_executor.get_errored_jobs_count()
            if hasattr(in_executor, "get_errored_jobs_count")
            else -1
        )
        # Treat finished-with-errors or executor-errored signal as failure even
        # if MRQ's `success` flag says True (it sometimes does).
        actually_succeeded = bool(success) and not state["errored"] and errored_count <= 0
        write_status(
            "finished" if actually_succeeded else "error",
            success=actually_succeeded,
            raw_success_flag=bool(success),
            errored_count=errored_count,
            error_reason=state["error_reason"])

    if hasattr(executor, "on_executor_errored_delegate"):
        executor.on_executor_errored_delegate.add_callable(on_errored)
    executor.on_executor_finished_delegate.add_callable(on_finished)
    write_status("rendering",
        message="executor started; on_finished+on_errored delegates bound",
        map=str(job.map))


if __name__ == "__main__":
    main()
