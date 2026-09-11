from __future__ import annotations

import re
import time
from pathlib import Path
from typing import Any

from .contracts import ValidationFailure, read_json
from .execution import REPO_ROOT, _run


PACKAGE_SCRIPT = REPO_ROOT / "scripts" / "ue" / "package" / "package_release.ps1"
IOSTORE_INSPECTION_SCRIPT = REPO_ROOT / "scripts" / "ue" / "package" / "inspect_iostore.ps1"


def package_and_inspect(
    powershell: str,
    package_root: Path,
    required_map: str,
    logs: Path,
    iostore_result: Path,
    stage_prefix: str = "",
) -> tuple[float, dict[str, Any]]:
    """Package the enrolled tree and prove the required map in IoStore."""
    started = time.perf_counter()
    _run(
        f"{stage_prefix}package",
        _package_command(powershell, package_root, required_map),
        logs,
        timeout=14400,
    )
    _validate_fresh_world_partition_cook(logs / f"{stage_prefix}package.log")
    seconds = time.perf_counter() - started
    _run(
        f"{stage_prefix}iostore_inspection",
        [
            powershell,
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(IOSTORE_INSPECTION_SCRIPT),
            "-PackageRoot",
            str(package_root / "Windows"),
            "-RequiredPackage",
            required_map,
            "-ResultPath",
            str(iostore_result),
        ],
        logs,
        timeout=300,
    )
    return seconds, read_json(iostore_result)


def _validate_fresh_world_partition_cook(package_log: Path) -> None:
    text = package_log.read_text(encoding="utf-8", errors="replace")
    cook_line = next(
        (line for line in text.splitlines() if re.search(r"(?i)-run=Cook(?:\s|$)", line)),
        "",
    )
    if "-NoAssetRegistryCache" not in cook_line:
        raise ValidationFailure(
            "package_asset_registry_cache_enabled",
            "The cook did not bypass stale Asset Registry discovery state",
        )
    stale_markers = (
        "Duplicate actor descriptor guid",
        "Can't find actor /ProjectWorld/Generated/",
        "Can't find actor /ProjectWorldTestData/Generated/",
        "Can't find actor /ProjectWorldData/Generated/",
    )
    if any(marker in text for marker in stale_markers) or re.search(
        r"(?i)__ExternalActors__[\\/]Generated.*Error opening file",
        text,
    ):
        raise ValidationFailure(
            "package_world_partition_descriptor_stale",
            "The cook observed stale generated World Partition actor descriptors",
        )


def _package_command(powershell: str, package_root: Path, required_map: str) -> list[str]:
    return [
        powershell,
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(PACKAGE_SCRIPT),
        "-OutputDir",
        str(package_root),
        "-RequiredCookMap",
        required_map,
    ]


def _required_map_cooked(package_log: Path, required_map: str) -> bool:
    log_bytes = package_log.read_bytes()
    cook_markers = (
        b"Splitting Package " + required_map.encode("ascii"),
        b"owner object World " + required_map.encode("ascii") + b".",
    )
    return any(marker in log_bytes for marker in cook_markers)
