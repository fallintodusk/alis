#!/usr/bin/env python3
"""Own the recoverable transition from a World Candidate to a release workspace."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
from pathlib import Path
from typing import Any

import prepare_release as release
import release_platforms as platforms


WORKSPACE_SCHEMA_V1 = "alis-release-workspace-v1"
WORKSPACE_SCHEMA_V2 = "alis-release-workspace-v2"
WORKSPACE_FILE = "release-workspace.json"
GAME_DIRECTORY = "game"
GITHUB_DIRECTORY = "github"
PACKAGE_SUMMARY = "package_summary.txt"
PLATFORM_KEYS = ("windows-x86_64", "linux-x86_64")
PLATFORM_EXECUTABLES = {
    "windows-x86_64": Path("Alis/Binaries/Win64/Alis-Win64-Shipping.exe"),
    "linux-x86_64": Path("Alis/Binaries/Linux/Alis-Linux-Shipping"),
}
HISTORICAL_GAME_VERIFICATION_FILES = {
    "VERIFY_ALIS.bat",
    "Verification/VERIFY_ALIS.ps1",
    "Verification/ALIS_PUBLIC_KEY.asc",
    "Verification/SHA256SUMS.txt",
    "Verification/SHA256SUMS.txt.asc",
}
PLATFORM_GAME_VERIFICATION_FILES = {
    "windows-x86_64": HISTORICAL_GAME_VERIFICATION_FILES,
    "linux-x86_64": {
        "VERIFY_ALIS.sh",
        "Verification/ALIS_PUBLIC_KEY.asc",
        "Verification/SHA256SUMS.txt",
        "Verification/SHA256SUMS.txt.asc",
    },
}
GITHUB_BACKUP_PATTERN = re.compile(r"^github-previous-[0-9a-f]{32}$")


def read_json(path: Path) -> dict[str, Any]:
    return release.read_json(path)


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def release_manifest(workspace: Path) -> dict[str, Any]:
    return release.verify_release_manifest(workspace / GITHUB_DIRECTORY)


def platform_game_tree_digest(game: Path, platform: str) -> str:
    if platform not in PLATFORM_KEYS or not game.is_dir():
        raise release.ReleaseError(f"Unsupported or missing release platform: {platform}")
    lines: list[str] = []
    for path in sorted(
        (item for item in game.rglob("*") if item.is_file()),
        key=lambda item: item.relative_to(game).as_posix().encode("utf-8"),
    ):
        relative = path.relative_to(game).as_posix()
        if relative in PLATFORM_GAME_VERIFICATION_FILES[platform]:
            continue
        mode = f"|{platforms.linux_file_mode(path):04o}" if platform == "linux-x86_64" else ""
        lines.append(f"{relative}|{path.stat().st_size}|{release.sha256_file(path)}{mode}")
    return hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()


def workspace_package_tree_digest(workspace: Path) -> str:
    game = workspace / GAME_DIRECTORY
    summary = workspace / PACKAGE_SUMMARY
    if not game.is_dir() or not summary.is_file():
        raise release.ReleaseError("Release workspace game or package summary is missing")
    lines: list[str] = []
    paths = sorted(
        (item for item in game.rglob("*") if item.is_file()),
        key=lambda item: item.relative_to(game).as_posix().encode("utf-8"),
    )
    for path in paths:
        relative_game = path.relative_to(game).as_posix()
        if relative_game in HISTORICAL_GAME_VERIFICATION_FILES:
            continue
        relative = f"Windows/{relative_game}"
        lowered = relative.lower()
        if any(
            lowered == prefix or lowered.startswith(prefix + "/")
            for prefix in release.RUNTIME_STATE_PATHS
        ):
            continue
        lines.append(f"{relative}|{path.stat().st_size}|{release.sha256_file(path)}")
    lines.append(f"{PACKAGE_SUMMARY}|{summary.stat().st_size}|{release.sha256_file(summary)}")
    lines.sort(key=lambda line: line.split("|", 1)[0].encode("utf-8"))
    return hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()


def assert_workspace_inventory(workspace: Path, allowed_extras: set[str] | None = None) -> None:
    allowed = {GAME_DIRECTORY, GITHUB_DIRECTORY, PACKAGE_SUMMARY, WORKSPACE_FILE}
    allowed.update(allowed_extras or set())
    actual = {item.name for item in workspace.iterdir()}
    if actual != allowed:
        raise release.ReleaseError(
            f"Release workspace inventory mismatch: unexpected={sorted(actual - allowed)}, "
            f"missing={sorted(allowed - actual)}"
        )
    if not (workspace / GAME_DIRECTORY).is_dir() or not (workspace / GITHUB_DIRECTORY).is_dir():
        raise release.ReleaseError("Release workspace game and github entries must be directories")


def assert_workspace_v2_inventory(workspace: Path, allowed_extras: set[str] | None = None) -> None:
    allowed = {GAME_DIRECTORY, GITHUB_DIRECTORY, WORKSPACE_FILE}
    allowed.update(allowed_extras or set())
    actual = {item.name for item in workspace.iterdir()}
    if actual != allowed:
        raise release.ReleaseError(
            f"Release workspace inventory mismatch: unexpected={sorted(actual - allowed)}, "
            f"missing={sorted(allowed - actual)}"
        )
    game = workspace / GAME_DIRECTORY
    if not game.is_dir() or not (workspace / GITHUB_DIRECTORY).is_dir():
        raise release.ReleaseError("Release workspace game and github entries must be directories")
    actual_platforms = {item.name for item in game.iterdir()}
    if actual_platforms != set(PLATFORM_KEYS) or any(not item.is_dir() for item in game.iterdir()):
        raise release.ReleaseError(
            f"Release workspace platform inventory mismatch: {sorted(actual_platforms)}"
        )


def initialize_workspace(workspace: Path, candidate: Path, version: str) -> Path:
    workspace = workspace.resolve()
    candidate = candidate.resolve()
    if (workspace / WORKSPACE_FILE).exists():
        raise release.ReleaseError(f"Release workspace state already exists: {workspace}")
    if not (workspace / GITHUB_DIRECTORY).is_dir():
        raise release.ReleaseError("Prepared workspace has no github directory")
    manifest = release_manifest(workspace)
    if tuple(map(int, release.normalize_version(version).split("."))) >= (2, 1, 0):
        raise release.ReleaseError(f"Release {version} requires workspace schema {WORKSPACE_SCHEMA_V2}")
    release.require_equal(manifest.get("release_version"), version, "release workspace version")
    release.require_equal(manifest.get("status"), "pending_owner_approval", "release workspace state")
    release.require_equal(manifest.get("schema"), "alis-release-manifest-v3", "release manifest schema")
    if not (candidate / "Windows").is_dir() or not (candidate / PACKAGE_SUMMARY).is_file():
        raise release.ReleaseError("Accepted World Candidate is incomplete")
    extras = sorted(item.name for item in candidate.iterdir() if item.name not in {"Windows", PACKAGE_SUMMARY})
    if extras:
        raise release.ReleaseError(f"Accepted World Candidate contains unexpected entries: {extras}")
    state = {
        "schema": WORKSPACE_SCHEMA_V1,
        "status": "awaiting_game_adoption",
        "release_version": version,
        "release_tag": manifest["release_tag"],
    }
    state_path = workspace / WORKSPACE_FILE
    write_json_atomic(state_path, state)
    return state_path


def initialize_workspace_v2(
    workspace: Path,
    candidates: dict[str, Path],
    version: str,
) -> Path:
    workspace = workspace.resolve()
    if (workspace / WORKSPACE_FILE).exists():
        raise release.ReleaseError(f"Release workspace state already exists: {workspace}")
    if not (workspace / GITHUB_DIRECTORY).is_dir():
        raise release.ReleaseError("Prepared workspace has no github directory")
    manifest = release_manifest(workspace)
    if tuple(map(int, release.normalize_version(version).split("."))) < (2, 1, 0):
        raise release.ReleaseError(f"Release {version} requires workspace schema {WORKSPACE_SCHEMA_V1}")
    release.require_equal(manifest.get("schema"), "alis-release-manifest-v4", "release manifest schema")
    release.require_equal(manifest.get("release_version"), version, "release workspace version")
    release.require_equal(manifest.get("status"), "pending_owner_approval", "release workspace state")
    sources = manifest.get("player_sources")
    if not isinstance(sources, dict) or set(sources) != set(PLATFORM_KEYS):
        raise release.ReleaseError("Release manifest platform map is incomplete or open-ended")
    if set(candidates) != set(PLATFORM_KEYS):
        raise release.ReleaseError("Release Candidate platform map is incomplete or open-ended")
    for platform, candidate in candidates.items():
        if not candidate.resolve().is_dir():
            raise release.ReleaseError(f"Prepared release Candidate is missing for {platform}: {candidate}")
    state = {
        "schema": WORKSPACE_SCHEMA_V2,
        "status": "awaiting_game_adoption",
        "release_version": version,
        "release_tag": manifest["release_tag"],
        "platforms": list(PLATFORM_KEYS),
    }
    state_path = workspace / WORKSPACE_FILE
    write_json_atomic(state_path, state)
    return state_path


def verify_release_identity(workspace: Path, state: dict[str, Any], manifest: dict[str, Any]) -> None:
    release.require_equal(manifest.get("release_version"), state.get("release_version"), "release workspace version")
    release.require_equal(manifest.get("release_tag"), state.get("release_tag"), "release workspace tag")
    player = manifest.get("player_source")
    if not isinstance(player, dict):
        raise release.ReleaseError("Release manifest has no player source authority")
    release.require_equal(
        workspace_package_tree_digest(workspace),
        player.get("package_tree_sha256"),
        "release workspace package tree",
    )
    executable = workspace / GAME_DIRECTORY / "Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
    if not executable.is_file():
        raise release.ReleaseError("Release workspace Shipping executable is missing")
    release.require_equal(
        release.sha256_file(executable),
        player.get("shipping_executable_sha256"),
        "release workspace Shipping executable",
    )


def verify_release_identity_v2(workspace: Path, state: dict[str, Any], manifest: dict[str, Any]) -> None:
    release.require_equal(manifest.get("schema"), "alis-release-manifest-v4", "release manifest schema")
    release.require_equal(manifest.get("release_version"), state.get("release_version"), "release workspace version")
    release.require_equal(manifest.get("release_tag"), state.get("release_tag"), "release workspace tag")
    release.require_equal(state.get("platforms"), list(PLATFORM_KEYS), "release workspace platform order")
    sources = manifest.get("player_sources")
    if not isinstance(sources, dict) or set(sources) != set(PLATFORM_KEYS):
        raise release.ReleaseError("Release manifest platform map is incomplete or open-ended")
    for platform in PLATFORM_KEYS:
        source = sources.get(platform)
        if not isinstance(source, dict):
            raise release.ReleaseError(f"Release manifest has no player source for {platform}")
        game = workspace / GAME_DIRECTORY / platform
        release.require_equal(
            platform_game_tree_digest(game, platform),
            source.get("runtime_payload_tree_sha256"),
            f"release workspace {platform} runtime payload tree",
        )
        executable_relative = PLATFORM_EXECUTABLES[platform]
        release.require_equal(
            source.get("shipping_executable"),
            executable_relative.as_posix(),
            f"release workspace {platform} executable path",
        )
        executable = game / executable_relative
        if not executable.is_file():
            raise release.ReleaseError(f"Release workspace {platform} Shipping executable is missing")
        if platform == "linux-x86_64":
            with executable.open("rb") as stream:
                release.require_equal(stream.read(4), b"\x7fELF", "Linux Shipping executable format")
        release.require_equal(
            release.sha256_file(executable),
            source.get("shipping_executable_sha256"),
            f"release workspace {platform} Shipping executable",
        )


def verify_workspace(workspace: Path, allowed_workspace_entries: set[str] | None = None) -> Path:
    workspace = workspace.resolve()
    state_path = workspace / WORKSPACE_FILE
    state = read_json(state_path)
    release.require_equal(state.get("status"), "complete", "release workspace status")
    manifest = release_manifest(workspace)
    schema = state.get("schema")
    version = release.normalize_version(str(state.get("release_version", "")))
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise release.ReleaseError(f"Invalid release workspace version: {version!r}")
    required_schema = WORKSPACE_SCHEMA_V2 if tuple(map(int, version.split("."))) >= (2, 1, 0) else WORKSPACE_SCHEMA_V1
    release.require_equal(schema, required_schema, f"release {version} requires workspace schema")
    if schema == WORKSPACE_SCHEMA_V1:
        release.require_equal(manifest.get("schema"), "alis-release-manifest-v3", "release manifest schema")
        assert_workspace_inventory(workspace, allowed_workspace_entries)
        verify_release_identity(workspace, state, manifest)
    elif schema == WORKSPACE_SCHEMA_V2:
        assert_workspace_v2_inventory(workspace, allowed_workspace_entries)
        verify_release_identity_v2(workspace, state, manifest)
    else:
        raise release.ReleaseError(f"Unsupported release workspace schema: {schema!r}")
    return state_path


def recover_github_projection(workspace: Path) -> Path:
    workspace = workspace.resolve()
    state_path = workspace / WORKSPACE_FILE
    state = read_json(state_path)
    if state.get("schema") not in {WORKSPACE_SCHEMA_V1, WORKSPACE_SCHEMA_V2}:
        raise release.ReleaseError(f"Unsupported release workspace schema: {state.get('schema')!r}")
    backups = sorted(
        item for item in workspace.iterdir() if GITHUB_BACKUP_PATTERN.fullmatch(item.name)
    )
    if len(backups) > 1:
        raise release.ReleaseError(
            f"Release workspace has ambiguous GitHub projection backups: {[item.name for item in backups]}"
        )
    if not backups:
        return state_path

    release.require_equal(state.get("status"), "complete", "release workspace recovery status")
    backup = backups[0]
    if (
        not backup.is_dir()
        or backup.is_symlink()
        or backup.resolve().parent != workspace
    ):
        raise release.ReleaseError(f"GitHub projection backup is not a normal directory: {backup.name}")

    github = workspace / GITHUB_DIRECTORY
    if not github.exists():
        backup_manifest = release.verify_release_manifest(backup)
        if state.get("schema") == WORKSPACE_SCHEMA_V2:
            verify_release_identity_v2(workspace, state, backup_manifest)
        else:
            verify_release_identity(workspace, state, backup_manifest)
        os.replace(backup, github)
        return verify_workspace(workspace)

    if (
        not github.is_dir()
        or github.is_symlink()
        or github.resolve().parent != workspace
    ):
        raise release.ReleaseError("Current GitHub projection is not a normal directory")
    release.verify_release_manifest(github)
    verify_workspace(workspace, {backup.name})
    shutil.rmtree(backup)
    return verify_workspace(workspace)


def adopt_game(workspace: Path, candidate: Path) -> Path:
    workspace = workspace.resolve()
    candidate = candidate.resolve()
    state_path = workspace / WORKSPACE_FILE
    state = read_json(state_path)
    release.require_equal(state.get("schema"), WORKSPACE_SCHEMA_V1, "release workspace schema")
    if state.get("status") == "complete":
        return verify_workspace(workspace)
    release.require_equal(state.get("status"), "awaiting_game_adoption", "release workspace status")

    game = workspace / GAME_DIRECTORY
    candidate_game = candidate / "Windows"
    summary = workspace / PACKAGE_SUMMARY
    candidate_summary = candidate / PACKAGE_SUMMARY
    if not game.exists():
        if not candidate_game.is_dir():
            raise release.ReleaseError("Neither workspace game nor Candidate Windows directory exists")
        os.replace(candidate_game, game)
    if not summary.exists():
        if not candidate_summary.is_file():
            raise release.ReleaseError("Neither workspace nor Candidate package summary exists")
        os.replace(candidate_summary, summary)

    state["status"] = "complete"
    write_json_atomic(state_path, state)
    verify_workspace(workspace)
    if candidate.exists() and not any(candidate.iterdir()):
        candidate.rmdir()
    return state_path


def adopt_platforms(workspace: Path, candidates: dict[str, Path]) -> Path:
    workspace = workspace.resolve()
    state_path = workspace / WORKSPACE_FILE
    state = read_json(state_path)
    release.require_equal(state.get("schema"), WORKSPACE_SCHEMA_V2, "release workspace schema")
    if state.get("status") == "complete":
        return verify_workspace(workspace)
    release.require_equal(state.get("status"), "awaiting_game_adoption", "release workspace status")
    if set(candidates) != set(PLATFORM_KEYS):
        raise release.ReleaseError("Release Candidate platform map is incomplete or open-ended")
    game = workspace / GAME_DIRECTORY
    game.mkdir(exist_ok=True)
    for platform in PLATFORM_KEYS:
        target = game / platform
        source = candidates[platform].resolve()
        if not target.exists():
            if not source.is_dir():
                raise release.ReleaseError(
                    f"Neither workspace nor Candidate platform directory exists: {platform}"
                )
            os.replace(source, target)
    state["status"] = "complete"
    write_json_atomic(state_path, state)
    verify_workspace(workspace)
    parents = {candidate.resolve().parent for candidate in candidates.values()}
    if len(parents) == 1:
        parent = parents.pop()
        if parent.exists() and not any(parent.iterdir()):
            parent.rmdir()
    return state_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    initialize = commands.add_parser("initialize")
    initialize.add_argument("--workspace-root", type=Path, required=True)
    initialize.add_argument("--candidate-root", type=Path, required=True)
    initialize.add_argument("--release-version", required=True)
    initialize_v2 = commands.add_parser("initialize-v2")
    initialize_v2.add_argument("--workspace-root", type=Path, required=True)
    initialize_v2.add_argument("--windows-candidate-root", type=Path, required=True)
    initialize_v2.add_argument("--linux-candidate-root", type=Path, required=True)
    initialize_v2.add_argument("--release-version", required=True)
    adopt = commands.add_parser("adopt")
    adopt.add_argument("--workspace-root", type=Path, required=True)
    adopt.add_argument("--candidate-root", type=Path, required=True)
    adopt_v2 = commands.add_parser("adopt-v2")
    adopt_v2.add_argument("--workspace-root", type=Path, required=True)
    adopt_v2.add_argument("--windows-candidate-root", type=Path, required=True)
    adopt_v2.add_argument("--linux-candidate-root", type=Path, required=True)
    verify = commands.add_parser("verify")
    verify.add_argument("--workspace-root", type=Path, required=True)
    recover = commands.add_parser("recover-github")
    recover.add_argument("--workspace-root", type=Path, required=True)
    package_tree = commands.add_parser("package-tree")
    package_tree.add_argument("--workspace-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "initialize":
            result = initialize_workspace(args.workspace_root, args.candidate_root, args.release_version)
        elif args.command == "initialize-v2":
            result = initialize_workspace_v2(
                args.workspace_root,
                {
                    "windows-x86_64": args.windows_candidate_root,
                    "linux-x86_64": args.linux_candidate_root,
                },
                args.release_version,
            )
        elif args.command == "adopt":
            result = adopt_game(args.workspace_root, args.candidate_root)
        elif args.command == "adopt-v2":
            result = adopt_platforms(
                args.workspace_root,
                {
                    "windows-x86_64": args.windows_candidate_root,
                    "linux-x86_64": args.linux_candidate_root,
                },
            )
        elif args.command == "verify":
            result = verify_workspace(args.workspace_root)
        elif args.command == "recover-github":
            result = recover_github_projection(args.workspace_root)
        else:
            print(workspace_package_tree_digest(args.workspace_root))
            return 0
    except (release.ReleaseError, OSError, ValueError, KeyError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    print(f"[OK] {args.command}: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
