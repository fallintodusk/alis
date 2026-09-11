from __future__ import annotations

import hashlib
import os
import platform
import re
import sys
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path
from typing import Any


REQUIREMENT_PATTERN = re.compile(r"^([A-Za-z0-9_.-]+)==([^\s]+)\s+--hash=sha256:([0-9a-f]{64})$")


def dependency_lock_path(repo_root: Path) -> Path:
    return repo_root / "tools" / "World" / "ExecutionEnvironment" / "requirements.lock.txt"


def dependency_lock_hash(repo_root: Path) -> str:
    return hashlib.sha256(dependency_lock_path(repo_root).read_bytes()).hexdigest()


def dependency_lock(repo_root: Path) -> dict[str, Any]:
    path = dependency_lock_path(repo_root)
    metadata: dict[str, str] = {}
    packages: list[dict[str, str]] = []
    pending_license: str | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.startswith("#"):
            key, separator, value = line[1:].partition(":")
            if separator:
                if key.strip() == "license":
                    pending_license = value.strip()
                else:
                    metadata[key.strip()] = value.strip()
            continue
        match = REQUIREMENT_PATTERN.fullmatch(line)
        if match is None or pending_license is None:
            raise ValueError(f"Invalid hash-locked Python requirement: {line}")
        packages.append({
            "name": match.group(1),
            "version": match.group(2),
            "sha256": match.group(3),
            "license": pending_license,
        })
        pending_license = None
    if metadata.get("python-version") is None or metadata.get("platform") is None or not packages:
        raise ValueError("Python dependency lock metadata is incomplete")
    return {"metadata": metadata, "packages": packages, "sha256": dependency_lock_hash(repo_root)}


def verify_python_host(repo_root: Path) -> dict[str, Any]:
    lock = dependency_lock(repo_root)
    expected_python = tuple(int(part) for part in lock["metadata"]["python-version"].split("."))
    if sys.implementation.name != "cpython" or sys.version_info[:2] != expected_python:
        raise RuntimeError(f"World ingestion requires CPython {lock['metadata']['python-version']}")
    if os.name != "nt" or platform.machine().lower() not in {"amd64", "x86_64"}:
        raise RuntimeError("World ingestion requires Windows x86-64")
    return lock


def verify_python_runtime(repo_root: Path) -> dict[str, Any]:
    lock = verify_python_host(repo_root)
    installed: dict[str, str] = {}
    for package in lock["packages"]:
        try:
            installed_version = version(package["name"])
        except PackageNotFoundError as exc:
            raise RuntimeError(f"Missing Python dependency: {package['name']}") from exc
        if installed_version != package["version"]:
            raise RuntimeError(
                f"Python dependency version mismatch: {package['name']} {installed_version} != {package['version']}"
            )
        installed[package["name"]] = installed_version
    return {"lock_sha256": lock["sha256"], "python": lock["metadata"]["python-version"], "packages": installed}
