#!/usr/bin/env python3
"""Refresh the GitHub projection after the reviewed game projection is signed."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

import prepare_release as release


GENERATED = {
    "README.txt",
    "INSTALL_ALIS_PLAYER.ps1",
    "release_manifest.json",
    *release.GENERATED_SIGNING_FILES,
}


def refresh(source: Path, output: Path, game: Path, archive_report_path: Path) -> Path:
    source = source.resolve()
    output = output.resolve()
    game = game.resolve()
    if output.exists():
        raise release.ReleaseError(f"Refreshed GitHub output must not exist: {output}")
    manifest = release.read_json(source / "release_manifest.json")
    release.verify_release_manifest(source, require_ready=True)
    report = release.read_json(archive_report_path.resolve())
    release.require_equal(report.get("schema"), "alis-game-archive-v1", "signed game archive schema")
    release.require_equal(report.get("status"), "accepted", "signed game archive status")
    release.require_equal(report.get("game_tree_sha256"), release.package_tree_digest(game), "signed game tree")
    parts = report.get("parts")
    if not isinstance(parts, list) or not parts:
        raise release.ReleaseError("Signed game archive contains no parts")

    version = str(manifest.get("release_version", ""))
    old_prefix = f"ALIS_Win64_v{release.normalize_version(version)}.zip"
    output.mkdir(parents=True)
    copied: dict[str, Path] = {}
    try:
        for source_path in source.iterdir():
            if not source_path.is_file():
                raise release.ReleaseError(f"GitHub projection must remain flat: {source_path}")
            if source_path.name in GENERATED or source_path.name.startswith(old_prefix):
                continue
            release.copy_asset(source_path, output, source_path.name, copied)
        for part in parts:
            name = release.require_safe_file_name(part.get("name"), "signed game archive part")
            part_path = archive_report_path.parent / name
            release.require_equal(part.get("byte_size"), part_path.stat().st_size, f"archive size for {name}")
            release.require_equal(part.get("sha256"), release.sha256_file(part_path), f"archive hash for {name}")
            release.copy_asset(part_path, output, name, copied)

        release.write_player_installer(
            Path(__file__).resolve().parent / "install_player_release.ps1",
            output,
            report,
            copied,
        )
        developer_paths = sorted(output.glob("*.developer-payload.json"))
        if len(developer_paths) != 1:
            raise release.ReleaseError("GitHub projection must contain one Developer payload manifest")
        developer_manifest = release.read_json(developer_paths[0])
        release.write_release_guide(
            output,
            version,
            str(manifest.get("release_tag", "")),
            report,
            developer_manifest,
            copied,
        )
        artifacts = [
            {"name": path.name, "byte_size": path.stat().st_size, "sha256": release.sha256_file(path)}
            for path in sorted(output.iterdir(), key=lambda item: item.name)
            if path.is_file()
        ]
        manifest["artifacts"] = artifacts
        manifest["player_distribution"] = {
            "game_tree_sha256": report["game_tree_sha256"],
            "signature_manifest_sha256": release.sha256_file(game / "Verification/SHA256SUMS.txt"),
            "signature_sha256": release.sha256_file(game / "Verification/SHA256SUMS.txt.asc"),
        }
        target = output / "release_manifest.json"
        target.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        release.verify_release_manifest(output, require_ready=True)
        return target
    except Exception:
        shutil.rmtree(output, ignore_errors=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-github", type=Path, required=True)
    parser.add_argument("--output-github", type=Path, required=True)
    parser.add_argument("--game-root", type=Path, required=True)
    parser.add_argument("--archive-report", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = refresh(args.source_github, args.output_github, args.game_root, args.archive_report)
    except (release.ReleaseError, OSError, ValueError, KeyError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    print(f"[OK] refresh: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
