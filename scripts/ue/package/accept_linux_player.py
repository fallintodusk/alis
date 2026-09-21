#!/usr/bin/env python3
"""Accept the exact Linux player archive inside a WSL Linux filesystem."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import uuid
from pathlib import Path, PurePosixPath
from typing import Any


ARCHIVE_SCHEMA = "alis-player-archive-v2"
ACCEPTANCE_SCHEMA = "alis-linux-player-acceptance-v1"
PLATFORM = "linux-x86_64"
LINUX_LAUNCHER = Path("Alis.sh")
LINUX_EXECUTABLE = Path("Alis/Binaries/Linux/Alis-Linux-Shipping")
REQUIRED_MODE = 0o755
ROUTES = (
    {
        "id": "kazan",
        "experience": "KazanTerritory",
        "map": "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory",
        "runtime": "kazan_territory_512_1536_v1",
        "skip_interaction": False,
    },
    {
        "id": "manhattan",
        "experience": "ManhattanShowcase",
        "map": "/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase",
        "runtime": "manhattan_showcase_512_1536_v1",
        "skip_interaction": True,
    },
)


class AcceptanceError(RuntimeError):
    pass


def read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise AcceptanceError(f"Expected a JSON object: {path.name}")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_equal(actual: Any, expected: Any, label: str) -> None:
    if actual != expected:
        raise AcceptanceError(f"{label} mismatch: expected {expected!r}, got {actual!r}")


def require_safe_name(value: Any, label: str) -> str:
    name = str(value or "")
    if not name or Path(name).name != name or name in {".", ".."}:
        raise AcceptanceError(f"Unsafe {label}: {name!r}")
    return name


def validate_report(report: dict[str, Any]) -> list[dict[str, Any]]:
    require_equal(report.get("schema"), ARCHIVE_SCHEMA, "Linux archive schema")
    require_equal(report.get("status"), "accepted", "Linux archive status")
    require_equal(report.get("platform"), PLATFORM, "Linux archive platform")
    require_equal(report.get("format"), "tar", "Linux archive format")
    require_equal(report.get("shipping_executable"), LINUX_EXECUTABLE.as_posix(), "Linux executable")
    require_equal(report.get("launcher"), LINUX_LAUNCHER.as_posix(), "Linux launcher")
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", str(report.get("release_version", ""))):
        raise AcceptanceError("Linux archive report has an invalid release version")
    require_equal(
        report.get("logical_name"),
        f"ALIS_Linux_x86_64_v{report['release_version']}.tar",
        "Linux archive logical name",
    )
    for field, pattern in (
        ("source_revision", r"[0-9a-f]{40}"),
        ("source_state_sha256", r"[0-9a-f]{64}"),
        ("archive_sha256", r"[0-9a-f]{64}"),
        ("game_tree_sha256", r"[0-9a-f]{64}"),
        ("shipping_executable_sha256", r"[0-9a-f]{64}"),
    ):
        if not re.fullmatch(pattern, str(report.get(field, ""))):
            raise AcceptanceError(f"Linux archive report has an invalid {field}")
    if not isinstance(report.get("archive_byte_size"), int) or report["archive_byte_size"] < 1:
        raise AcceptanceError("Linux archive report has an invalid archive_byte_size")
    parts = report.get("parts")
    if not isinstance(parts, list) or not parts:
        raise AcceptanceError("Linux archive report contains no parts")
    seen: set[str] = set()
    normalized: list[dict[str, Any]] = []
    for index, part in enumerate(parts, start=1):
        if not isinstance(part, dict):
            raise AcceptanceError(f"Linux archive part {index} is not an object")
        name = require_safe_name(part.get("name"), f"Linux archive part {index}")
        if name in seen:
            raise AcceptanceError(f"Duplicate Linux archive part: {name}")
        seen.add(name)
        byte_size = part.get("byte_size")
        digest = str(part.get("sha256", ""))
        if not isinstance(byte_size, int) or byte_size < 1 or len(digest) != 64:
            raise AcceptanceError(f"Invalid Linux archive part identity: {name}")
        normalized.append({"name": name, "byte_size": byte_size, "sha256": digest})
    return normalized


def linux_filesystem_type(path: Path) -> str:
    result = subprocess.run(
        ["findmnt", "-n", "-o", "FSTYPE", "--target", str(path)],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip().lower()


def assert_linux_filesystem(path: Path) -> str:
    resolved = path.resolve()
    if resolved == Path("/mnt") or Path("/mnt") in resolved.parents:
        raise AcceptanceError("Linux acceptance work root cannot be under /mnt")
    filesystem = linux_filesystem_type(resolved)
    if filesystem != "ext4":
        raise AcceptanceError(f"Linux acceptance requires the WSL ext4 filesystem, got {filesystem!r}")
    return filesystem


def validate_acceptance_environment(
    os_release: dict[str, str],
    system: str,
    machine: str,
    kernel: str,
    wsl_interop: bool,
    display: bool,
    dxg: bool,
) -> dict[str, Any]:
    require_equal(system, "Linux", "Linux acceptance operating system")
    if machine.lower() not in {"x86_64", "amd64"}:
        raise AcceptanceError(f"Linux acceptance requires x86-64, got {machine!r}")
    require_equal(os_release.get("ID"), "ubuntu", "Linux acceptance distribution")
    require_equal(os_release.get("VERSION_ID"), "22.04", "Linux acceptance distribution version")
    if "microsoft" not in kernel.lower() or not wsl_interop:
        raise AcceptanceError("Linux acceptance requires WSL2 interoperability")
    if not display or not dxg:
        raise AcceptanceError("Linux acceptance requires the WSLg display and /dev/dxg device")
    return {
        "baseline": "Ubuntu 22.04 x86-64 under WSL2/WSLg",
        "distribution": os_release["ID"],
        "distribution_version": os_release["VERSION_ID"],
        "kernel": kernel,
        "wsl_interop": True,
        "wslg": True,
        "dxg": True,
    }


def acceptance_environment() -> dict[str, Any]:
    values: dict[str, str] = {}
    for line in Path("/etc/os-release").read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value.strip().strip('"')
    return validate_acceptance_environment(
        values,
        platform.system(),
        platform.machine(),
        platform.release(),
        bool(os.environ.get("WSL_INTEROP")),
        bool(os.environ.get("WAYLAND_DISPLAY") or os.environ.get("DISPLAY")),
        Path("/dev/dxg").exists(),
    )


def copy_and_join_archive(report_path: Path, work_root: Path) -> tuple[dict[str, Any], Path]:
    report = read_json(report_path)
    parts = validate_report(report)
    input_root = work_root / "input"
    input_root.mkdir()
    archive = work_root / "ALIS_Linux_x86_64.tar"
    with archive.open("wb") as combined:
        for part in parts:
            source = report_path.parent / part["name"]
            if not source.is_file():
                raise AcceptanceError(f"Linux archive part is missing: {part['name']}")
            require_equal(source.stat().st_size, part["byte_size"], f"size for {part['name']}")
            require_equal(sha256_file(source), part["sha256"], f"hash for {part['name']}")
            copied = input_root / part["name"]
            shutil.copyfile(source, copied)
            require_equal(sha256_file(copied), part["sha256"], f"copied hash for {part['name']}")
            with copied.open("rb") as stream:
                shutil.copyfileobj(stream, combined, 1024 * 1024)
    require_equal(archive.stat().st_size, report.get("archive_byte_size"), "Linux archive size")
    require_equal(sha256_file(archive), report.get("archive_sha256"), "Linux archive hash")
    return report, archive


def _safe_member_path(name: str) -> PurePosixPath:
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise AcceptanceError(f"Unsafe Linux archive member: {name!r}")
    return path


def extract_archive(archive_path: Path, destination: Path, report: dict[str, Any]) -> None:
    destination.mkdir()
    required_modes = report.get("required_executable_modes")
    if not isinstance(required_modes, dict):
        raise AcceptanceError("Linux archive report has no executable-mode contract")
    expected_modes = {
        LINUX_LAUNCHER.as_posix(): "0755",
        LINUX_EXECUTABLE.as_posix(): "0755",
    }
    require_equal(required_modes, expected_modes, "Linux executable-mode contract")
    with tarfile.open(archive_path, mode="r:") as archive:
        members = archive.getmembers()
        names: set[str] = set()
        for member in members:
            relative = _safe_member_path(member.name)
            name = relative.as_posix()
            if name in names:
                raise AcceptanceError(f"Duplicate Linux archive member: {name}")
            names.add(name)
            if not (member.isfile() or member.isdir()):
                raise AcceptanceError(f"Unsupported Linux archive member type: {name}")
            if name in expected_modes:
                require_equal(member.mode & 0o777, REQUIRED_MODE, f"archive mode for {name}")
            target = destination.joinpath(*relative.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            if member.isdir():
                target.mkdir(exist_ok=True)
                os.chmod(target, member.mode & 0o777)
                continue
            source = archive.extractfile(member)
            if source is None:
                raise AcceptanceError(f"Cannot read Linux archive member: {name}")
            with source, target.open("wb") as output:
                shutil.copyfileobj(source, output, 1024 * 1024)
            os.chmod(target, member.mode & 0o777)
    for relative in (LINUX_LAUNCHER, LINUX_EXECUTABLE):
        target = destination / relative
        if not target.is_file():
            raise AcceptanceError(f"Required Linux player file is missing: {relative.as_posix()}")
        require_equal(target.stat().st_mode & 0o777, REQUIRED_MODE, f"extracted mode for {relative.as_posix()}")
    with (destination / LINUX_EXECUTABLE).open("rb") as stream:
        require_equal(stream.read(4), b"\x7fELF", "Linux Shipping executable format")


def _route_arguments(route: dict[str, Any], operation: str, result: Path, log: Path, runtime_hash: str) -> list[str]:
    arguments = [
        f"-ProjectMenuPlayAutoExperience={route['experience']}",
        "-ProjectMenuPlayAutoMode=SinglePlayer",
        "-ProjectWorldProductRouteGate",
        "-ProjectWorldProductRouteRestorePreviewFlight",
        f"-ProjectWorldProductOperation={operation}",
        f"-ProjectWorldProductResult={result}",
        f"-ProjectWorldProductMap={route['map']}",
        f"-ProjectWorldProductRuntime={route['runtime']}",
        f"-ProjectWorldProductRuntimeHash={runtime_hash}",
        "-ProjectWorldProductMachine=wsl2_wslg_linux_x86_64",
        "-ProjectWorldProductEdge=651041,511455,60000",
        "-ResX=1280",
        "-ResY=720",
        "-Windowed",
        "-ForceRes",
        "-novsync",
        "-unattended",
        "-nosplash",
        "-NoMessaging",
        f"-abslog={log}",
    ]
    if route["skip_interaction"]:
        arguments.append("-ProjectWorldProductRouteSkipInteraction")
    return arguments


def validate_route_receipt(receipt: dict[str, Any], route: dict[str, Any], operation: str, runtime_hash: str) -> dict[str, Any]:
    expected = {
        "status": "accepted",
        "operation_id": operation,
        "map_package": route["map"],
        "runtime_profile": route["runtime"],
        "runtime_profile_sha256": runtime_hash,
        "build_configuration": "Shipping",
        "game_mode": "/Script/ProjectSinglePlay.SinglePlayerGameMode",
        "pawn_class": "/Script/ProjectCharacter.DefinitionCharacter",
    }
    for key, value in expected.items():
        require_equal(receipt.get(key), value, f"{route['id']} receipt {key}")
    for key in (
        "project_loading_provenance",
        "possessed_player",
        "normal_movement",
        "terrain_collision",
        "road_collision",
        "building_collision",
        "center_unloaded_at_edge",
        "edge_loaded",
        "center_reloaded",
        "preview_flight_restored",
    ):
        require_equal(receipt.get(key), True, f"{route['id']} receipt {key}")
    if route["skip_interaction"]:
        require_equal(receipt.get("gameplay_interaction_required"), False, "Manhattan interaction policy")
    else:
        require_equal(receipt.get("gameplay_interaction"), True, "Kazan gameplay interaction")
    rhi = str(receipt.get("rhi", ""))
    gpu = str(receipt.get("gpu_adapter", ""))
    if "vulkan" not in rhi.lower():
        raise AcceptanceError(f"{route['id']} did not use Vulkan: {rhi!r}")
    if not gpu or any(marker in gpu.lower() for marker in ("llvmpipe", "software", "cpu")):
        raise AcceptanceError(f"{route['id']} did not use a GPU-backed Vulkan adapter: {gpu!r}")
    return {"route": route["id"], "map": route["map"], "rhi": rhi, "gpu_adapter": gpu}


def run_routes(game_root: Path, runtime_hashes: dict[str, str], timeout_seconds: int) -> list[dict[str, Any]]:
    launcher = game_root / LINUX_LAUNCHER
    evidence = game_root.parent / "evidence"
    evidence.mkdir()
    accepted: list[dict[str, Any]] = []
    for route in ROUTES:
        runtime_hash = runtime_hashes.get(route["id"], "")
        if len(runtime_hash) != 64:
            raise AcceptanceError(f"Missing runtime profile hash for {route['id']}")
        operation = f"linux_{route['id']}_{uuid.uuid4().hex}"
        result_path = evidence / f"{route['id']}.json"
        log_path = evidence / f"{route['id']}.log"
        console_path = evidence / f"{route['id']}.console.log"
        command = [str(launcher), *_route_arguments(route, operation, result_path, log_path, runtime_hash)]
        try:
            with console_path.open("wb") as console:
                result = subprocess.run(
                    command,
                    cwd=game_root,
                    timeout=timeout_seconds,
                    check=False,
                    stdout=console,
                    stderr=subprocess.STDOUT,
                )
        except subprocess.TimeoutExpired as error:
            raise AcceptanceError(f"{route['id']} Linux player route exceeded {timeout_seconds} seconds") from error
        if result.returncode != 0:
            tail = ""
            if log_path.is_file():
                tail = "\n".join(log_path.read_text(encoding="utf-8", errors="replace").splitlines()[-20:])
            raise AcceptanceError(f"{route['id']} Linux player route exited {result.returncode}\n{tail}")
        if not result_path.is_file():
            raise AcceptanceError(f"{route['id']} Linux player route produced no receipt")
        accepted.append(validate_route_receipt(read_json(result_path), route, operation, runtime_hash))
    return accepted


def accept(report_path: Path, output_path: Path, runtime_hashes: dict[str, str], timeout_seconds: int) -> Path:
    report_path = report_path.resolve()
    output_path = output_path.resolve()
    if output_path.exists():
        raise AcceptanceError(f"Linux acceptance output already exists: {output_path}")
    work_root = Path(tempfile.mkdtemp(prefix="alis-linux-acceptance-", dir=Path.home()))
    try:
        filesystem = assert_linux_filesystem(work_root)
        environment = acceptance_environment()
        environment["filesystem"] = filesystem
        report, archive = copy_and_join_archive(report_path, work_root)
        game_root = work_root / "game"
        extract_archive(archive, game_root, report)
        routes = run_routes(game_root, runtime_hashes, timeout_seconds)
        receipt = {
            "schema": ACCEPTANCE_SCHEMA,
            "status": "accepted",
            "platform": PLATFORM,
            "environment": environment,
            "archive_sha256": report["archive_sha256"],
            "archive_byte_size": report["archive_byte_size"],
            "runtime_payload_tree_sha256": report["game_tree_sha256"],
            "shipping_executable_sha256": report["shipping_executable_sha256"],
            "release_version": report.get("release_version"),
            "source_revision": report.get("source_revision"),
            "source_state_sha256": report.get("source_state_sha256"),
            "routes": routes,
        }
        output_path.parent.mkdir(parents=True, exist_ok=True)
        temporary = output_path.with_name(output_path.name + ".tmp")
        temporary.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        os.replace(temporary, output_path)
        return output_path
    finally:
        shutil.rmtree(work_root, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive-report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--kazan-runtime-sha256", required=True)
    parser.add_argument("--manhattan-runtime-sha256", required=True)
    parser.add_argument("--timeout-seconds", type=int, default=720)
    args = parser.parse_args()
    try:
        result = accept(
            args.archive_report,
            args.output,
            {"kazan": args.kazan_runtime_sha256, "manhattan": args.manhattan_runtime_sha256},
            args.timeout_seconds,
        )
    except (AcceptanceError, OSError, ValueError, json.JSONDecodeError, tarfile.TarError, subprocess.SubprocessError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    print(f"[OK] Linux player accepted: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
