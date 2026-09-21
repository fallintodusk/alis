#!/usr/bin/env python3
"""Refresh the manifest-v4 GitHub projection from both signed game platforms."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

import prepare_release as legacy
import prepare_release_v4 as release_v4
import release_platforms as platforms
import release_workspace as workspace


GENERATED = {
    "README.txt",
    "INSTALL_ALIS_PLAYER.ps1",
    "release_manifest.json",
    *legacy.GENERATED_SIGNING_FILES,
}


def verify_archive(
    report_path: Path,
    expected_schema: str,
    expected_platform: str,
    expected_tree: str,
) -> tuple[dict, list[Path]]:
    report = legacy.read_json(report_path)
    legacy.require_equal(report.get("schema"), expected_schema, f"{expected_platform} archive schema")
    legacy.require_equal(report.get("status"), "accepted", f"{expected_platform} archive status")
    if expected_schema == "alis-player-archive-v2":
        legacy.require_equal(report.get("platform"), expected_platform, "Linux archive platform")
    legacy.require_equal(report.get("game_tree_sha256"), expected_tree, f"{expected_platform} signed game tree")
    return report, release_v4.validate_parts(report, report_path, expected_platform)


def refresh(
    source: Path,
    output: Path,
    windows_game: Path,
    windows_report_path: Path,
    linux_game: Path,
    linux_report_path: Path,
) -> Path:
    source = source.resolve()
    output = output.resolve()
    windows_game = windows_game.resolve()
    linux_game = linux_game.resolve()
    if output.exists():
        raise legacy.ReleaseError(f"Refreshed GitHub output must not exist: {output}")
    manifest = legacy.read_json(source / "release_manifest.json")
    legacy.verify_release_manifest(source, require_ready=True)
    legacy.require_equal(manifest.get("schema"), release_v4.MANIFEST_SCHEMA, "release manifest schema")
    player_sources = manifest["player_sources"]
    for platform, game in (
        ("windows-x86_64", windows_game),
        ("linux-x86_64", linux_game),
    ):
        legacy.require_equal(
            workspace.platform_game_tree_digest(game, platform),
            player_sources[platform]["runtime_payload_tree_sha256"],
            f"{platform} accepted runtime payload tree",
        )
    linux_source = player_sources["linux-x86_64"]
    acceptance_name = legacy.require_safe_file_name(
        linux_source.get("acceptance_artifact"), "Linux acceptance artifact"
    )
    acceptance_path = source / acceptance_name
    legacy.require_equal(
        legacy.sha256_file(acceptance_path),
        linux_source.get("acceptance_sha256"),
        "Linux acceptance artifact",
    )
    linux_acceptance = legacy.read_json(acceptance_path)
    legacy.require_equal(
        linux_acceptance.get("runtime_payload_tree_sha256"),
        player_sources["linux-x86_64"]["runtime_payload_tree_sha256"],
        "Linux accepted runtime payload tree",
    )
    windows_report, windows_parts = verify_archive(
        windows_report_path.resolve(),
        "alis-game-archive-v1",
        "windows-x86_64",
        legacy.package_tree_digest(windows_game),
    )
    linux_report, linux_parts = verify_archive(
        linux_report_path.resolve(),
        "alis-player-archive-v2",
        "linux-x86_64",
        platforms.platform_tree_digest(linux_game, include_modes=True),
    )

    version = str(manifest.get("release_version", ""))
    archive_prefixes = (
        f"ALIS_Win64_v{legacy.normalize_version(version)}.zip",
        f"ALIS_Linux_x86_64_v{legacy.normalize_version(version)}.tar",
    )
    output.mkdir(parents=True)
    copied: dict[str, Path] = {}
    try:
        for source_path in source.iterdir():
            if not source_path.is_file():
                raise legacy.ReleaseError(f"GitHub projection must remain flat: {source_path}")
            if source_path.name in GENERATED or source_path.name.startswith(archive_prefixes):
                continue
            legacy.copy_asset(source_path, output, source_path.name, copied)
        for part in windows_parts + linux_parts:
            legacy.copy_asset(part, output, part.name, copied)

        legacy.write_player_installer(
            Path(__file__).resolve().parent / "install_player_release.ps1",
            output,
            windows_report,
            copied,
        )
        developer_paths = sorted(output.glob("*.developer-payload.json"))
        if len(developer_paths) != 1:
            raise legacy.ReleaseError("GitHub projection must contain one Developer payload manifest")
        developer_manifest = legacy.read_json(developer_paths[0])
        notices = sorted(output.glob("*.notices.json"))
        if len(notices) != 1:
            raise legacy.ReleaseError("GitHub projection must contain one notices manifest")
        release_v4.write_guide(
            output,
            version,
            str(manifest.get("release_tag", "")),
            windows_report,
            linux_report,
            developer_manifest,
            notices[0].name,
            copied,
        )
        artifacts = [
            {"name": path.name, "byte_size": path.stat().st_size, "sha256": legacy.sha256_file(path)}
            for path in sorted(output.iterdir(), key=lambda item: item.name)
            if path.is_file()
        ]
        manifest["artifacts"] = artifacts
        manifest["player_distribution"] = {
            "windows-x86_64": {
                "game_tree_sha256": windows_report["game_tree_sha256"],
                "runtime_payload_tree_sha256": player_sources["windows-x86_64"]["runtime_payload_tree_sha256"],
                "signature_manifest_sha256": legacy.sha256_file(
                    windows_game / "Verification/SHA256SUMS.txt"
                ),
                "signature_sha256": legacy.sha256_file(
                    windows_game / "Verification/SHA256SUMS.txt.asc"
                ),
            },
            "linux-x86_64": {
                "game_tree_sha256": linux_report["game_tree_sha256"],
                "runtime_payload_tree_sha256": player_sources["linux-x86_64"]["runtime_payload_tree_sha256"],
                "archive_sha256": linux_report["archive_sha256"],
                "archive_byte_size": linux_report["archive_byte_size"],
                "signature_manifest_sha256": legacy.sha256_file(
                    linux_game / "Verification/SHA256SUMS.txt"
                ),
                "signature_sha256": legacy.sha256_file(
                    linux_game / "Verification/SHA256SUMS.txt.asc"
                ),
            },
        }
        target = output / "release_manifest.json"
        target.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        legacy.verify_release_manifest(output, require_ready=True)
        return target
    except Exception:
        shutil.rmtree(output, ignore_errors=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-github", type=Path, required=True)
    parser.add_argument("--output-github", type=Path, required=True)
    parser.add_argument("--windows-game-root", type=Path, required=True)
    parser.add_argument("--windows-archive-report", type=Path, required=True)
    parser.add_argument("--linux-game-root", type=Path, required=True)
    parser.add_argument("--linux-archive-report", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = refresh(
            args.source_github,
            args.output_github,
            args.windows_game_root,
            args.windows_archive_report,
            args.linux_game_root,
            args.linux_archive_report,
        )
    except (legacy.ReleaseError, OSError, ValueError, KeyError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    print(f"[OK] refresh-v4: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
