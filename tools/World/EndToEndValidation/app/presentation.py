from __future__ import annotations

import os
import subprocess
from pathlib import Path
from typing import Any

from .contracts import ValidationFailure, file_hash, read_json, validate_against


REPO_ROOT = Path(__file__).resolve().parents[4]


def _shipping_executable(package_root: Path) -> Path:
    candidates = sorted(
        path
        for path in (package_root / "Windows").rglob("Alis-Win64-Shipping.exe")
        if path.is_file()
    )
    if len(candidates) != 1:
        raise ValidationFailure(
            "presentation_gate_executable_ambiguous",
            "Exactly one staged Shipping executable is required for rendered validation",
            paths=[str(path) for path in candidates],
        )
    return candidates[0]


def _launch_executable(package_root: Path) -> Path:
    executable = package_root / "Windows" / "Alis.exe"
    if not executable.is_file():
        raise ValidationFailure(
            "presentation_gate_launcher_missing",
            "The staged project launcher is required for rendered validation",
            path=str(executable),
        )
    return executable


def _runtime_frame_budget(runtime_contract: dict[str, str]) -> float:
    document = read_json(REPO_ROOT / runtime_contract["path"])
    try:
        budget = float(document["budgets"]["p95_frame_time_ms"])
    except (KeyError, TypeError, ValueError) as error:
        raise ValidationFailure(
            "presentation_gate_budget_invalid",
            "The pinned runtime profile has no valid p95 frame-time budget",
            path=runtime_contract["path"],
        ) from error
    if budget <= 0:
        raise ValidationFailure(
            "presentation_gate_budget_invalid",
            "The pinned runtime profile p95 frame-time budget must be positive",
            path=runtime_contract["path"],
        )
    return budget


def _command(
    executable: Path,
    operation_id: str,
    result_path: Path,
    map_package: str,
    world_data_plugin: str,
    presentation: dict[str, str],
    runtime: dict[str, str],
    settings: dict[str, Any],
    frame_budget_ms: float,
) -> list[str]:
    resolution_x, resolution_y = settings["resolution"]
    camera_roles = ",".join(settings["camera_roles"])
    return [
        str(executable),
        map_package,
        "-game",
        "-windowed",
        "-ForceRes",
        "-NoSplash",
        "-NoVSync",
        "-unattended",
        "-ProjectSkipFrontEnd",
        "-stdout",
        "-FullStdOutLogOutput",
        f"-ResX={resolution_x}",
        f"-ResY={resolution_y}",
        "-ProjectWorldPresentationGate",
        f"-ProjectWorldGateOperation={operation_id}",
        f"-ProjectWorldGateResult={result_path}",
        f"-ProjectWorldGateMap={map_package}",
        f"-ProjectWorldGateWorldDataPlugin={world_data_plugin}",
        f"-ProjectWorldGateMachine={settings['machine_profile_id']}",
        f"-ProjectWorldGatePresentation={presentation['profile_id']}",
        f"-ProjectWorldGatePresentationHash={presentation['sha256']}",
        f"-ProjectWorldGateRuntime={runtime['profile_id']}",
        f"-ProjectWorldGateRuntimeHash={runtime['sha256']}",
        f"-ProjectWorldGateCameras={camera_roles}",
        f"-ProjectWorldGateResX={resolution_x}",
        f"-ProjectWorldGateResY={resolution_y}",
        f"-ProjectWorldGateScalability={settings['scalability_level']}",
        f"-ProjectWorldGateWarmup={settings['warmup_frames']}",
        f"-ProjectWorldGateSamples={settings['sample_frames']}",
        f"-ProjectWorldGateBudgetMs={frame_budget_ms}",
    ]


def run_packaged_gate(
    profile: dict[str, Any],
    profile_records: dict[str, dict[str, Any]],
    package_root: Path,
    evidence_root: Path,
    logs: Path,
    operation_id: str,
) -> dict[str, Any] | None:
    settings = profile.get("presentation_gate")
    if settings is None:
        return None
    world_name = settings["world_profile"]
    world_settings = profile["profiles"].get(world_name)
    world_record = profile_records.get(world_name)
    if world_settings is None or world_record is None:
        raise ValidationFailure(
            "presentation_gate_world_unknown",
            "Presentation Gate references an unknown world profile",
            world_profile=world_name,
        )
    presentation = world_record["presentation_profile"]
    runtime = world_record.get("runtime_profile")
    if runtime is None:
        raise ValidationFailure(
            "presentation_gate_runtime_missing",
            "Presentation Gate requires a pinned playable runtime profile",
            world_profile=world_name,
        )

    shipping_executable = _shipping_executable(package_root)
    executable = _launch_executable(package_root)
    result_path = evidence_root / "presentation_gate.json"
    result_path.unlink(missing_ok=True)
    frame_budget_ms = _runtime_frame_budget(runtime)
    command = _command(
        executable,
        operation_id,
        result_path,
        world_settings["map_package"],
        world_settings["world_data_plugin"],
        presentation,
        runtime,
        settings,
        frame_budget_ms,
    )
    log_path = logs / "presentation_gate.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as log:
        completed = subprocess.run(
            command,
            cwd=package_root / "Windows",
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=600,
        )
    if not result_path.is_file():
        raise ValidationFailure(
            "presentation_gate_result_missing",
            "The packaged rendered run emitted no structured result",
            returncode=completed.returncode,
            log=str(log_path),
        )
    result = read_json(result_path)
    validate_against(result, "presentation-result.schema.json")
    if completed.returncode != 0 or result["status"] != "accepted":
        raise ValidationFailure(
            "presentation_gate_rejected",
            "The packaged rendered Presentation Gate rejected the world",
            returncode=completed.returncode,
            errors=result["errors"],
            log=str(log_path),
        )
    _verify_measured_process(result, shipping_executable, operation_id)
    return {
        "result": str(result_path),
        "log": str(log_path),
        "operation_id": operation_id,
        "command_executable": str(executable),
        "shipping_executable": str(shipping_executable),
        "shipping_executable_sha256": file_hash(shipping_executable),
        "frame_budget_ms": frame_budget_ms,
    }


def _verify_measured_process(
    result: dict[str, Any],
    shipping_executable: Path,
    operation_id: str,
) -> None:
    """Prove the receipt was written by the staged Shipping executable.

    A Shipping binary sitting beside the launcher proves nothing about which
    process performed the measurement; the receipt's self-reported process
    identity must match the discovered staged Shipping executable exactly.
    """
    if result["operation_id"] != operation_id:
        raise ValidationFailure(
            "presentation_gate_operation_mismatch",
            "The rendered receipt belongs to another end-to-end operation",
            expected=operation_id,
            actual=result["operation_id"],
        )
    if result["build_configuration"] != "Shipping":
        raise ValidationFailure(
            "presentation_gate_configuration_mismatch",
            "The measured packaged process is not a Shipping build",
            actual=result["build_configuration"],
        )
    reported = os.path.normcase(str(Path(result["executable"]).resolve()))
    expected = os.path.normcase(str(shipping_executable.resolve()))
    if reported != expected:
        raise ValidationFailure(
            "presentation_gate_executable_mismatch",
            "The measured process is not the staged Shipping executable",
            expected=expected,
            actual=reported,
        )
