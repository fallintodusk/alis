from __future__ import annotations

import os
import shutil
import subprocess
import tarfile
import time
import urllib.request
import zipfile
from pathlib import Path
from typing import Any

from .contracts import (
    ExecutionEnvironmentError,
    exclusive_file_lock,
    file_hash,
    read_json,
    validate_document,
    write_json,
)
from .paths import environment_data_root


RECEIPT_SCHEMA = "https://alis.world/schemas/world-source/toolchain-receipt-v1.json"
_VERIFIED_TOOL_LOCKS: set[tuple[str, str]] = set()


def _load_lock(repo_root: Path) -> tuple[dict[str, Any], str]:
    lock_path = repo_root / "tools" / "World" / "ExecutionEnvironment" / "toolchain.lock.json"
    lock = read_json(lock_path)
    validate_document(lock, lock_path)
    return lock, file_hash(lock_path)


def tool_install_root(repo_root: Path, lock_sha256: str) -> Path:
    return environment_data_root(repo_root) / "tools" / "installs" / lock_sha256


def _promote_install(staging: Path, install_root: Path) -> None:
    install_root.parent.mkdir(parents=True, exist_ok=True)
    promotion_attempts = 12
    for attempt in range(promotion_attempts):
        try:
            if install_root.exists():
                shutil.rmtree(install_root)
            os.replace(staging, install_root)
            return
        except PermissionError as exc:
            if attempt == promotion_attempts - 1:
                raise ExecutionEnvironmentError("tool_promotion_failed", "Tool installation could not be promoted", error=str(exc)) from exc
            time.sleep(0.25 * min(attempt + 1, 4))


def _safe_target(root: Path, member_name: str) -> Path:
    target = (root / member_name).resolve()
    if not target.is_relative_to(root.resolve()):
        raise ExecutionEnvironmentError("unsafe_archive", "Archive member escapes its extraction root", member=member_name)
    return target


def _extract(archive: Path, archive_type: str, target: Path) -> None:
    target.mkdir(parents=True, exist_ok=True)
    if archive_type == "zip":
        with zipfile.ZipFile(archive) as bundle:
            for member in bundle.infolist():
                _safe_target(target, member.filename)
            bundle.extractall(target)
        return
    if archive_type == "tar.bz2":
        with tarfile.open(archive, "r:bz2") as bundle:
            for member in bundle.getmembers():
                _safe_target(target, member.name)
            bundle.extractall(target, filter="data")
        return
    raise ExecutionEnvironmentError("unsupported_archive", "Tool archive type is unsupported", archive_type=archive_type)


def _download_artifact(artifact: dict[str, Any], archives_root: Path) -> Path:
    content_root = archives_root / artifact["sha256"]
    content_root.mkdir(parents=True, exist_ok=True)
    archive = content_root / artifact["file_name"]
    if archive.exists() and (
        archive.stat().st_size != artifact["byte_size"] or file_hash(archive) != artifact["sha256"]
    ):
        archive.unlink()
    if not archive.exists():
        staging = archive.with_suffix(archive.suffix + ".partial")
        request = urllib.request.Request(artifact["url"], headers={"User-Agent": "ALIS-WorldSource/1"})
        try:
            with urllib.request.urlopen(request, timeout=120) as response, staging.open("wb") as stream:
                shutil.copyfileobj(response, stream, length=1024 * 1024)
        except OSError as exc:
            raise ExecutionEnvironmentError("tool_download_failed", "Tool download failed", url=artifact["url"], error=str(exc)) from exc
        os.replace(staging, archive)
    if archive.stat().st_size != artifact["byte_size"] or file_hash(archive) != artifact["sha256"]:
        raise ExecutionEnvironmentError("tool_hash_mismatch", "Tool archive verification failed", file_name=artifact["file_name"])
    return archive


def tool_environment(repo_root: Path, install_root: Path | None = None) -> tuple[dict[str, str], dict[str, Path]]:
    if install_root is None:
        _, lock_sha256 = _load_lock(repo_root)
        install_root = tool_install_root(repo_root, lock_sha256)
    osmium = install_root / "osmium" / "root" / "bin" / "osmium.exe"
    gdal_root = install_root / "gdal" / "root"
    paths = {
        "osmium": osmium,
        "gdalbuildvrt": gdal_root / "bin" / "gdal" / "apps" / "gdalbuildvrt.exe",
        "gdalinfo": gdal_root / "bin" / "gdal" / "apps" / "gdalinfo.exe",
        "gdal_rasterize": gdal_root / "bin" / "gdal" / "apps" / "gdal_rasterize.exe",
        "gdal_translate": gdal_root / "bin" / "gdal" / "apps" / "gdal_translate.exe",
        "gdalwarp": gdal_root / "bin" / "gdal" / "apps" / "gdalwarp.exe",
        "ogr2ogr": gdal_root / "bin" / "gdal" / "apps" / "ogr2ogr.exe",
        "projinfo": gdal_root / "bin" / "proj9" / "apps" / "projinfo.exe",
    }
    env = os.environ.copy()
    path_entries = [
        str(osmium.parent),
        str(gdal_root / "bin"),
        str(gdal_root / "bin" / "gdal" / "apps"),
        str(gdal_root / "bin" / "proj9" / "apps"),
    ]
    env["PATH"] = os.pathsep.join(path_entries + [env.get("PATH", "")])
    env["GDAL_DATA"] = str(gdal_root / "bin" / "gdal-data")
    env["GDAL_DRIVER_PATH"] = "disable"
    env["GDAL_PYTHON_DRIVER_PATH"] = "disable"
    env["PROJ_DATA"] = str(gdal_root / "bin" / "proj9" / "SHARE")
    return env, paths


def run_tool(repo_root: Path, tool: str, arguments: list[str]) -> str:
    env, paths = tool_environment(repo_root)
    executable = paths[tool]
    if not executable.is_file():
        raise ExecutionEnvironmentError("tool_missing", "Pinned tool is not bootstrapped", tool=tool)
    completed = subprocess.run(
        [str(executable), *arguments],
        cwd=repo_root,
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if completed.returncode != 0:
        raise ExecutionEnvironmentError(
            "tool_failed",
            "External source tool failed",
            tool=tool,
            exit_code=completed.returncode,
            stderr=completed.stderr[-4000:],
        )
    return completed.stdout


def _probe_components(repo_root: Path, lock: dict[str, Any], install_root: Path) -> list[dict[str, Any]]:
    env, _ = tool_environment(repo_root, install_root)
    probes = []
    for component in lock["components"]:
        binary_hashes = {}
        for relative_path, expected_hash in component["binary_hashes"].items():
            binary = install_root / relative_path
            if not binary.is_file() or file_hash(binary) != expected_hash:
                raise ExecutionEnvironmentError("tool_hash_mismatch", "Extracted tool binary verification failed", path=relative_path)
            binary_hashes[relative_path] = expected_hash
        for dependency in component["runtime_dependencies"]:
            binary = install_root / dependency["path"]
            if not binary.is_file() or file_hash(binary) != dependency["sha256"]:
                raise ExecutionEnvironmentError("tool_hash_mismatch", "Runtime dependency verification failed", path=dependency["path"])
        executable = install_root / component["executable"]
        if not executable.is_file():
            raise ExecutionEnvironmentError("tool_probe_failed", "Pinned tool executable is missing", component=component["component_id"])
        completed = subprocess.run(
            [str(executable), *component["version_probe"]],
            cwd=repo_root,
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        output = completed.stdout + completed.stderr
        if completed.returncode != 0 or component["expected_version_text"] not in output:
            raise ExecutionEnvironmentError("tool_probe_failed", "Pinned tool version probe failed", component=component["component_id"])
        capabilities: list[str] = []
        if component["component_id"] == "gdal":
            formats = subprocess.run(
                [str(executable), "--formats"], cwd=repo_root, env=env, capture_output=True,
                text=True, encoding="utf-8", errors="replace", check=False,
            )
            if formats.returncode != 0 or "GTiff" not in formats.stdout or "COG" not in formats.stdout:
                raise ExecutionEnvironmentError("tool_probe_failed", "Built-in GDAL GTiff or COG driver is unavailable")
            capabilities = ["GTiff", "COG"]
        probes.append({
            "component_id": component["component_id"],
            "version": component["version"],
            "binary_hashes": binary_hashes,
            "runtime_dependencies": component["runtime_dependencies"],
            "capabilities": capabilities,
        })
    return probes


def bootstrap_tools(repo_root: Path) -> dict[str, Any]:
    lock, lock_sha256 = _load_lock(repo_root)
    if os.name != "nt" or lock.get("platform") != "windows-x86_64":
        raise ExecutionEnvironmentError("unsupported_platform", "Pinned world toolchain supports Windows x86_64")
    tools_root = environment_data_root(repo_root) / "tools"
    archives_root = tools_root / "archives"
    install_root = tool_install_root(repo_root, lock_sha256)
    receipt_path = install_root / "installed.json"
    verification_key = (str(repo_root.resolve()), lock_sha256)
    with exclusive_file_lock(tools_root / "locks" / f"{lock_sha256}.lock"):
        if receipt_path.is_file():
            try:
                receipt = read_json(receipt_path)
                validate_document(receipt, receipt_path)
            except ExecutionEnvironmentError:
                receipt = {}
            if receipt.get("lock_sha256") == lock_sha256:
                probes = _probe_components(repo_root, lock, install_root)
                if receipt.get("components") == probes:
                    _VERIFIED_TOOL_LOCKS.add(verification_key)
                    return receipt

        staging = tools_root / "staging" / lock_sha256
        if staging.exists():
            shutil.rmtree(staging)
        staging.mkdir(parents=True)
        for component in lock["components"]:
            for artifact in component["artifacts"]:
                archive = _download_artifact(artifact, archives_root)
                _extract(archive, artifact["archive_type"], staging / artifact["extract_root"])

        probes = _probe_components(repo_root, lock, staging)
        receipt = {
            "$schema": RECEIPT_SCHEMA,
            "schema_version": 1,
            "lock_sha256": lock_sha256,
            "install_id": f"sha256:{lock_sha256}",
            "components": probes,
        }
        write_json(staging / "installed.json", receipt)
        _promote_install(staging, install_root)
        _VERIFIED_TOOL_LOCKS.add(verification_key)
        return receipt


def require_tools(repo_root: Path) -> None:
    lock, lock_sha256 = _load_lock(repo_root)
    verification_key = (str(repo_root.resolve()), lock_sha256)
    if verification_key in _VERIFIED_TOOL_LOCKS:
        return
    install_root = tool_install_root(repo_root, lock_sha256)
    receipt_path = install_root / "installed.json"
    try:
        receipt = read_json(receipt_path)
        validate_document(receipt, receipt_path)
    except ExecutionEnvironmentError as exc:
        raise ExecutionEnvironmentError("tool_receipt_invalid", "Run bootstrap-tools to repair the installed tool receipt") from exc
    if receipt.get("lock_sha256") != lock_sha256:
        raise ExecutionEnvironmentError("tool_receipt_invalid", "Installed tool receipt does not match the current lock")
    probes = _probe_components(repo_root, lock, install_root)
    if receipt.get("components") != probes:
        raise ExecutionEnvironmentError("tool_receipt_invalid", "Installed tool receipt does not match verified tool state")
    _VERIFIED_TOOL_LOCKS.add(verification_key)
