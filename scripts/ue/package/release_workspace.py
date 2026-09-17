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


WORKSPACE_SCHEMA = "alis-release-workspace-v1"
WORKSPACE_FILE = "release-workspace.json"
GAME_DIRECTORY = "game"
GITHUB_DIRECTORY = "github"
PACKAGE_SUMMARY = "package_summary.txt"
GAME_VERIFICATION_FILES = {
    "VERIFY_ALIS.bat",
    "Verification/VERIFY_ALIS.ps1",
    "Verification/ALIS_PUBLIC_KEY.asc",
    "Verification/SHA256SUMS.txt",
    "Verification/SHA256SUMS.txt.asc",
}
GITHUB_BACKUP_PATTERN = re.compile(r"^github-previous-[0-9a-f]{32}$")


def read_json(path: Path) -> dict[str, Any]:
    return release.read_json(path)


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def release_manifest(workspace: Path) -> dict[str, Any]:
    manifest = read_json(workspace / GITHUB_DIRECTORY / "release_manifest.json")
    release.require_equal(manifest.get("schema"), "alis-release-manifest-v3", "release manifest schema")
    return manifest


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
        if relative_game in GAME_VERIFICATION_FILES:
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


def initialize_workspace(workspace: Path, candidate: Path, version: str) -> Path:
    workspace = workspace.resolve()
    candidate = candidate.resolve()
    if (workspace / WORKSPACE_FILE).exists():
        raise release.ReleaseError(f"Release workspace state already exists: {workspace}")
    if not (workspace / GITHUB_DIRECTORY).is_dir():
        raise release.ReleaseError("Prepared workspace has no github directory")
    manifest = release_manifest(workspace)
    release.require_equal(manifest.get("release_version"), version, "release workspace version")
    release.require_equal(manifest.get("status"), "pending_owner_approval", "release workspace state")
    if not (candidate / "Windows").is_dir() or not (candidate / PACKAGE_SUMMARY).is_file():
        raise release.ReleaseError("Accepted World Candidate is incomplete")
    extras = sorted(item.name for item in candidate.iterdir() if item.name not in {"Windows", PACKAGE_SUMMARY})
    if extras:
        raise release.ReleaseError(f"Accepted World Candidate contains unexpected entries: {extras}")
    state = {
        "schema": WORKSPACE_SCHEMA,
        "status": "awaiting_game_adoption",
        "release_version": version,
        "release_tag": manifest["release_tag"],
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


def verify_workspace(workspace: Path, allowed_workspace_entries: set[str] | None = None) -> Path:
    workspace = workspace.resolve()
    state_path = workspace / WORKSPACE_FILE
    state = read_json(state_path)
    release.require_equal(state.get("schema"), WORKSPACE_SCHEMA, "release workspace schema")
    release.require_equal(state.get("status"), "complete", "release workspace status")
    manifest = release_manifest(workspace)
    assert_workspace_inventory(workspace, allowed_workspace_entries)
    verify_release_identity(workspace, state, manifest)
    return state_path


def recover_github_projection(workspace: Path) -> Path:
    workspace = workspace.resolve()
    state_path = workspace / WORKSPACE_FILE
    state = read_json(state_path)
    release.require_equal(state.get("schema"), WORKSPACE_SCHEMA, "release workspace schema")
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
    release.require_equal(state.get("schema"), WORKSPACE_SCHEMA, "release workspace schema")
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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    initialize = commands.add_parser("initialize")
    initialize.add_argument("--workspace-root", type=Path, required=True)
    initialize.add_argument("--candidate-root", type=Path, required=True)
    initialize.add_argument("--release-version", required=True)
    adopt = commands.add_parser("adopt")
    adopt.add_argument("--workspace-root", type=Path, required=True)
    adopt.add_argument("--candidate-root", type=Path, required=True)
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
        elif args.command == "adopt":
            result = adopt_game(args.workspace_root, args.candidate_root)
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
