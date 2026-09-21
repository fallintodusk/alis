#!/usr/bin/env python3
"""Platform-specific player archive ownership for ALIS releases."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
import tarfile
from pathlib import Path
from typing import Any


GITHUB_RELEASE_ASSET_LIMIT_BYTES = 2 * 1024 * 1024 * 1024
MAX_RELEASE_PART_SIZE_MIB = 1900
LINUX_PLATFORM = "linux-x86_64"
LINUX_EXECUTABLE = Path("Alis/Binaries/Linux/Alis-Linux-Shipping")
LINUX_LAUNCHER = Path("Alis.sh")


class ReleasePlatformError(RuntimeError):
    pass


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _is_elf(path: Path) -> bool:
    if not path.is_file():
        return False
    with path.open("rb") as stream:
        return stream.read(4) == b"\x7fELF"


def linux_file_mode(path: Path) -> int:
    return 0o755 if path.suffix == ".sh" or _is_elf(path) else 0o644


def platform_tree_digest(root: Path, include_modes: bool) -> str:
    if not root.is_dir():
        raise ReleasePlatformError(f"Player game root is missing: {root}")
    lines: list[str] = []
    for path in sorted(
        (item for item in root.rglob("*") if item.is_file()),
        key=lambda item: item.relative_to(root).as_posix().encode("utf-8"),
    ):
        relative = path.relative_to(root).as_posix()
        mode = f"|{linux_file_mode(path):04o}" if include_modes else ""
        lines.append(f"{relative}|{path.stat().st_size}|{sha256_file(path)}{mode}")
    return hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()


def _validate_normal_tree(root: Path) -> None:
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ReleasePlatformError(f"Player archive input cannot contain a symbolic link: {path}")


def _write_linux_tar(game_root: Path, archive_path: Path) -> None:
    with tarfile.open(archive_path, mode="w", format=tarfile.PAX_FORMAT) as archive:
        entries = sorted(
            game_root.rglob("*"),
            key=lambda item: item.relative_to(game_root).as_posix().encode("utf-8"),
        )
        for path in entries:
            relative = path.relative_to(game_root).as_posix()
            info = tarfile.TarInfo(relative)
            info.uid = 0
            info.gid = 0
            info.uname = "root"
            info.gname = "root"
            info.mtime = 0
            if path.is_dir():
                info.type = tarfile.DIRTYPE
                info.mode = 0o755
                archive.addfile(info)
                continue
            if not path.is_file():
                raise ReleasePlatformError(f"Unsupported player archive entry: {path}")
            info.size = path.stat().st_size
            info.mode = linux_file_mode(path)
            with path.open("rb") as stream:
                archive.addfile(info, stream)


def _split_archive(archive_path: Path, split_size_mib: int) -> list[Path]:
    if not 1 <= split_size_mib <= MAX_RELEASE_PART_SIZE_MIB:
        raise ReleasePlatformError(
            f"Player archive split size must be between 1 and {MAX_RELEASE_PART_SIZE_MIB} MiB"
        )
    split_bytes = split_size_mib * 1024 * 1024
    if archive_path.stat().st_size <= split_bytes:
        return [archive_path]
    parts: list[Path] = []
    with archive_path.open("rb") as source:
        index = 1
        while True:
            chunk = source.read(split_bytes)
            if not chunk:
                break
            part = archive_path.with_name(f"{archive_path.name}.{index:03d}")
            part.write_bytes(chunk)
            parts.append(part)
            index += 1
    archive_path.unlink()
    return parts


def _archive_record(path: Path) -> dict[str, Any]:
    return {
        "name": path.name,
        "byte_size": path.stat().st_size,
        "sha256": sha256_file(path),
    }


def archive_linux_game(
    game_root: Path,
    output: Path,
    version: str,
    split_size_mib: int,
    source_revision: str,
    source_state_sha256: str,
) -> Path:
    game_root = game_root.resolve()
    output = output.resolve()
    if output.exists():
        raise ReleasePlatformError(f"Player archive output must not exist: {output}")
    executable = game_root / LINUX_EXECUTABLE
    launcher = game_root / LINUX_LAUNCHER
    if not _is_elf(executable):
        raise ReleasePlatformError(f"Linux Shipping executable is missing or is not ELF: {executable}")
    if not launcher.is_file():
        raise ReleasePlatformError(f"Linux launcher is missing: {launcher}")
    normalized_version = version.removeprefix("v")
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", normalized_version):
        raise ReleasePlatformError(f"Invalid stable release version: {version!r}")
    if not re.fullmatch(r"[0-9a-f]{40}", source_revision) or not re.fullmatch(
        r"[0-9a-f]{64}", source_state_sha256
    ):
        raise ReleasePlatformError("Linux player archive requires exact source revision and state identity")
    _validate_normal_tree(game_root)

    output.mkdir(parents=True)
    archive_path = output / f"ALIS_Linux_x86_64_v{normalized_version}.tar"
    try:
        _write_linux_tar(game_root, archive_path)
        archive_size = archive_path.stat().st_size
        archive_sha256 = sha256_file(archive_path)
        parts = _split_archive(archive_path, split_size_mib)
        if any(part.stat().st_size >= GITHUB_RELEASE_ASSET_LIMIT_BYTES for part in parts):
            raise ReleasePlatformError("Linux player archive exceeded the GitHub release asset limit")
        report = {
            "schema": "alis-player-archive-v2",
            "status": "accepted",
            "platform": LINUX_PLATFORM,
            "format": "tar",
            "logical_name": f"ALIS_Linux_x86_64_v{normalized_version}.tar",
            "release_version": normalized_version,
            "source_revision": source_revision,
            "source_state_sha256": source_state_sha256,
            "archive_byte_size": archive_size,
            "archive_sha256": archive_sha256,
            "game_tree_sha256": platform_tree_digest(game_root, include_modes=True),
            "shipping_executable": LINUX_EXECUTABLE.as_posix(),
            "shipping_executable_sha256": sha256_file(executable),
            "launcher": LINUX_LAUNCHER.as_posix(),
            "required_executable_modes": {
                LINUX_LAUNCHER.as_posix(): "0755",
                LINUX_EXECUTABLE.as_posix(): "0755",
            },
            "parts": [_archive_record(part) for part in parts],
        }
        report_path = output / "linux-player-archive.json"
        report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        return report_path
    except Exception:
        shutil.rmtree(output, ignore_errors=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    archive_linux = commands.add_parser("archive-linux")
    archive_linux.add_argument("--game-root", type=Path, required=True)
    archive_linux.add_argument("--output-dir", type=Path, required=True)
    archive_linux.add_argument("--release-version", required=True)
    archive_linux.add_argument("--source-revision", required=True)
    archive_linux.add_argument("--source-state-sha256", required=True)
    archive_linux.add_argument("--split-size-mib", type=int, default=MAX_RELEASE_PART_SIZE_MIB)
    args = parser.parse_args()
    try:
        result = archive_linux_game(
            args.game_root,
            args.output_dir,
            args.release_version,
            args.split_size_mib,
            args.source_revision,
            args.source_state_sha256,
        )
    except (ReleasePlatformError, OSError, ValueError, tarfile.TarError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    print(f"[OK] {args.command}: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
