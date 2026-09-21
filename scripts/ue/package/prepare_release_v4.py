#!/usr/bin/env python3
"""Prepare the two-platform ALIS release manifest v4 projection."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path
from typing import Any

import prepare_release as legacy
import release_platforms as platforms


MANIFEST_SCHEMA = "alis-release-manifest-v4"
PLATFORM_KEYS = ("windows-x86_64", "linux-x86_64")


def validate_parts(report: dict[str, Any], report_path: Path, label: str) -> list[Path]:
    parts = report.get("parts")
    if not isinstance(parts, list) or not parts:
        raise legacy.ReleaseError(f"{label} archive contains no parts")
    paths: list[Path] = []
    seen: set[str] = set()
    logical_size = 0
    for index, part in enumerate(parts, start=1):
        if not isinstance(part, dict):
            raise legacy.ReleaseError(f"{label} archive part {index} is invalid")
        name = legacy.require_safe_file_name(part.get("name"), f"{label} archive part")
        if name in seen:
            raise legacy.ReleaseError(f"Duplicate {label} archive part: {name}")
        seen.add(name)
        path = report_path.parent / name
        if not path.is_file():
            raise legacy.ReleaseError(f"{label} archive part is missing: {name}")
        legacy.require_equal(part.get("byte_size"), path.stat().st_size, f"{label} archive part size")
        legacy.require_equal(part.get("sha256"), legacy.sha256_file(path), f"{label} archive part hash")
        logical_size += path.stat().st_size
        paths.append(path)
    if report.get("archive_byte_size") is not None:
        legacy.require_equal(report.get("archive_byte_size"), logical_size, f"{label} logical archive size")
    return paths


def validate_linux(
    inputs: legacy.ReleaseInputs,
    validated: dict[str, Any],
    game_root: Path,
    report_path: Path,
    acceptance_path: Path,
) -> tuple[dict[str, Any], dict[str, Any], list[Path]]:
    game_root = game_root.resolve()
    report_path = report_path.resolve()
    acceptance_path = acceptance_path.resolve()
    report = legacy.read_json(report_path)
    legacy.require_equal(report.get("schema"), "alis-player-archive-v2", "Linux archive schema")
    legacy.require_equal(report.get("status"), "accepted", "Linux archive status")
    legacy.require_equal(report.get("platform"), "linux-x86_64", "Linux archive platform")
    legacy.require_equal(report.get("format"), "tar", "Linux archive format")
    legacy.require_equal(
        report.get("logical_name"),
        f"ALIS_Linux_x86_64_v{inputs.release_version}.tar",
        "Linux archive logical name",
    )
    legacy.require_equal(
        legacy.normalize_version(str(report.get("release_version", ""))),
        inputs.release_version,
        "Linux archive release version",
    )
    legacy.require_equal(report.get("source_revision"), validated["private_revision"], "Linux source revision")
    legacy.require_equal(report.get("source_state_sha256"), validated["private_state"], "Linux source state")
    legacy.require_equal(
        report.get("game_tree_sha256"),
        platforms.platform_tree_digest(game_root, include_modes=True),
        "Linux game tree",
    )
    executable_relative = platforms.LINUX_EXECUTABLE
    executable = game_root / executable_relative
    if not executable.is_file():
        raise legacy.ReleaseError("Linux Shipping executable is missing")
    with executable.open("rb") as stream:
        legacy.require_equal(stream.read(4), b"\x7fELF", "Linux Shipping executable format")
    legacy.require_equal(
        report.get("shipping_executable_sha256"),
        legacy.sha256_file(executable),
        "Linux Shipping executable",
    )
    paths = validate_parts(report, report_path, "Linux player")

    acceptance = legacy.read_json(acceptance_path)
    legacy.require_equal(
        acceptance.get("schema"),
        "alis-linux-player-acceptance-v1",
        "Linux acceptance schema",
    )
    legacy.require_equal(acceptance.get("status"), "accepted", "Linux acceptance status")
    legacy.require_equal(acceptance.get("platform"), "linux-x86_64", "Linux acceptance platform")
    for field in (
        "release_version",
        "source_revision",
        "source_state_sha256",
        "archive_sha256",
        "archive_byte_size",
        "shipping_executable_sha256",
    ):
        legacy.require_equal(acceptance.get(field), report.get(field), f"Linux acceptance {field}")
    legacy.require_equal(
        acceptance.get("runtime_payload_tree_sha256"),
        report.get("game_tree_sha256"),
        "Linux acceptance runtime payload tree",
    )
    environment = acceptance.get("environment")
    if not isinstance(environment, dict) or environment.get("baseline") != "Ubuntu 22.04 x86-64 under WSL2/WSLg":
        raise legacy.ReleaseError("Linux acceptance does not prove the declared WSL2/WSLg baseline")
    required_environment = {
        "distribution": "ubuntu",
        "distribution_version": "22.04",
        "filesystem": "ext4",
        "wsl_interop": True,
        "wslg": True,
        "dxg": True,
    }
    for field, expected in required_environment.items():
        legacy.require_equal(environment.get(field), expected, f"Linux acceptance environment {field}")
    routes = acceptance.get("routes")
    expected_routes = {
        "kazan": "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory",
        "manhattan": "/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase",
    }
    if not isinstance(routes, list) or len(routes) != len(expected_routes):
        raise legacy.ReleaseError("Linux acceptance does not prove both required product routes")
    actual_routes: dict[str, dict[str, Any]] = {}
    for item in routes:
        if not isinstance(item, dict) or item.get("route") not in expected_routes:
            raise legacy.ReleaseError("Linux acceptance contains an unknown product route")
        route_id = item["route"]
        if route_id in actual_routes:
            raise legacy.ReleaseError(f"Linux acceptance duplicates product route: {route_id}")
        legacy.require_equal(item.get("map"), expected_routes[route_id], f"Linux acceptance {route_id} map")
        rhi = str(item.get("rhi", ""))
        adapter = str(item.get("gpu_adapter", ""))
        if "vulkan" not in rhi.lower() or not adapter or any(
            marker in adapter.lower() for marker in ("llvmpipe", "software", "cpu")
        ):
            raise legacy.ReleaseError(f"Linux acceptance {route_id} did not prove GPU-backed Vulkan")
        actual_routes[route_id] = item
    if set(actual_routes) != set(expected_routes):
        raise legacy.ReleaseError("Linux acceptance does not prove both required product routes")
    return report, acceptance, paths


def write_guide(
    output: Path,
    version: str,
    tag: str,
    windows_report: dict[str, Any],
    linux_report: dict[str, Any],
    developer_manifest: dict[str, Any],
    notices_name: str,
    copied: dict[str, Path],
) -> None:
    windows_parts = [item["name"] for item in windows_report["parts"]]
    linux_parts = [item["name"] for item in linux_report["parts"]]
    linux_archive = linux_report["logical_name"]
    developer_parts = [item["name"] for item in developer_manifest["archive"]["parts"]]
    lines = [
        f"ALIS {version}",
        "",
        "World Reborn",
        "",
        "WHAT'S NEW " + version,
        "",
        "- Play the current Kazan and Manhattan worlds on Windows x86-64.",
        "- Linux x86-64 is tested on Ubuntu 22.04 under WSL2/WSLg.",
        "- Develop and contribute from the exact public source tag.",
        "",
        "PLAY ON WINDOWS",
        "",
        "Download these archive parts into one folder:",
        *[f"  {name}" for name in windows_parts],
        "Extract the first part with 7-Zip, then run Alis.exe.",
        "INSTALL_ALIS_PLAYER.bat is an optional convenience.",
        "",
        "PLAY ON LINUX X86-64",
        "",
        "Download these archive parts into one folder:",
        *[f"  {name}" for name in linux_parts],
        *(
            [f"Run: cat {' '.join(linux_parts)} > {linux_archive}"]
            if len(linux_parts) > 1
            else []
        ),
        f"Run: tar -xf {linux_archive}",
        "Run: ./Alis.sh",
        "The archive preserves the required executable modes.",
        "",
        "DEVELOP OR CONTRIBUTE",
        "",
        f"Clone the exact {tag} tag and download:",
        *[f"  {name}" for name in developer_parts],
        f"Follow https://github.com/fallintodusk/alis/blob/{tag}/docs/quickstart/developer/README.md",
        "",
        "VERIFY THE RELEASE",
        "",
        "Windows: run VERIFY_RELEASE.bat.",
        "Linux: run ./VERIFY_RELEASE.sh.",
        "Both routes verify the same public key, detached signature, and checksums.",
        "",
        "LICENSES AND TERMS",
        "",
        "Product terms: PRODUCT_TERMS.txt",
        f"Data and third-party notices: {notices_name}",
        f"Open-source license: https://github.com/fallintodusk/alis/blob/{tag}/LICENSE",
    ]
    legacy.write_release_text(output, "README.txt", lines, copied)


def prepare(
    inputs: legacy.ReleaseInputs,
    output: Path,
    windows_report_path: Path,
    linux_game_root: Path,
    linux_report_path: Path,
    linux_acceptance_path: Path,
) -> Path:
    output = output.resolve()
    windows_report_path = windows_report_path.resolve()
    linux_report_path = linux_report_path.resolve()
    validated = legacy.validate_inputs(inputs, windows_report_path)
    windows_report = validated["archive_report"]
    windows_paths = validate_parts(windows_report, windows_report_path, "Windows player")
    linux_report, _, linux_paths = validate_linux(
        inputs,
        validated,
        linux_game_root,
        linux_report_path,
        linux_acceptance_path,
    )
    prepared_inputs = {
        *(path.resolve() for path in windows_paths),
        *(path.resolve() for path in linux_paths),
        windows_report_path,
        linux_report_path,
    }
    allowed_existing = {path for path in prepared_inputs if path.parent == output}
    if output.exists():
        existing = {path.resolve() for path in output.iterdir()}
        if existing != allowed_existing or any(not path.is_file() for path in output.iterdir()):
            raise legacy.ReleaseError("Release output may contain only prepared player archives and reports")

    sources: list[tuple[Path, str]] = [(path, path.name) for path in windows_paths + linux_paths]
    for source in sorted(validated["developer_files"], key=lambda item: item.name.lower()):
        if source.name not in legacy.DEVELOPER_INPUT_ONLY_FILES:
            sources.append((source, source.name))
    sources.extend(
        [
            (inputs.component_manifest, "effective-component-manifest.json"),
            (inputs.product_terms, "PRODUCT_TERMS.txt"),
            (linux_acceptance_path, "linux-player-acceptance.json"),
        ]
    )
    names: dict[str, Path] = {}
    for source, name in sources:
        if name in names and legacy.sha256_file(source) != legacy.sha256_file(names[name]):
            raise legacy.ReleaseError(f"Conflicting release asset name: {name}")
        names[name] = source

    output.mkdir(parents=True, exist_ok=True)
    copied: dict[str, Path] = {}
    try:
        for source, name in sources:
            legacy.copy_asset(source, output, name, copied)
        package_scripts = Path(__file__).resolve().parent
        mirror_scripts = package_scripts.parents[1] / "git" / "mirror"
        legacy.write_player_installer(
            package_scripts / "install_player_release.ps1",
            output,
            windows_report,
            copied,
        )
        legacy.copy_asset(package_scripts / "install_player_release.bat", output, "INSTALL_ALIS_PLAYER.bat", copied)
        legacy.copy_asset(mirror_scripts / "bootstrap_developer_release.ps1", output, "INSTALL_ALIS_DEVELOPER.ps1", copied)
        legacy.copy_asset(mirror_scripts / "bootstrap_developer_release.bat", output, "INSTALL_ALIS_DEVELOPER.bat", copied)
        write_guide(
            output,
            inputs.release_version,
            inputs.release_tag,
            windows_report,
            linux_report,
            validated["developer_manifest"],
            inputs.attribution_notice.name,
            copied,
        )
        for report_path in (windows_report_path, linux_report_path):
            if report_path.is_relative_to(output):
                report_path.unlink()
        artifacts = [
            {"name": name, "byte_size": path.stat().st_size, "sha256": legacy.sha256_file(path)}
            for name, path in sorted(copied.items())
        ]
        manifest = {
            "schema": MANIFEST_SCHEMA,
            "status": "pending_owner_approval",
            "release_version": inputs.release_version,
            "release_tag": inputs.release_tag,
            "public_source": {
                "revision": validated["public_revision"],
                "tree": validated["public_tree"],
            },
            "player_sources": {
                "windows-x86_64": {
                    "revision": validated["private_revision"],
                    "source_state_sha256": validated["private_state"],
                    "operation_id": validated["player"]["operation_id"],
                    "runtime_payload_tree_sha256": windows_report["game_tree_sha256"],
                    "shipping_executable": "Alis/Binaries/Win64/Alis-Win64-Shipping.exe",
                    "shipping_executable_sha256": validated["player"]["shipping_executable_sha256"],
                },
                "linux-x86_64": {
                    "revision": validated["private_revision"],
                    "source_state_sha256": validated["private_state"],
                    "runtime_payload_tree_sha256": linux_report["game_tree_sha256"],
                    "shipping_executable": linux_report["shipping_executable"],
                    "shipping_executable_sha256": linux_report["shipping_executable_sha256"],
                    "acceptance_artifact": "linux-player-acceptance.json",
                    "acceptance_sha256": legacy.sha256_file(linux_acceptance_path),
                },
            },
            "unresolved_count": 0,
            "product_review": {"status": validated["player"]["product_review"]},
            "rights_review": {"status": "pending_owner_approval"},
            "release_documents": {
                "component_manifest_artifact": "effective-component-manifest.json",
                "developer_guide_url": f"https://github.com/fallintodusk/alis/blob/{inputs.release_tag}/docs/quickstart/developer/README.md",
                "notices_artifact": inputs.attribution_notice.name,
                "product_terms_artifact": "PRODUCT_TERMS.txt",
                "source_license_url": f"https://github.com/fallintodusk/alis/blob/{inputs.release_tag}/LICENSE",
            },
            "artifacts": artifacts,
        }
        manifest_path = output / "release_manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        legacy.verify_release_manifest(output)
        return manifest_path
    except Exception:
        shutil.rmtree(output, ignore_errors=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-version", required=True)
    parser.add_argument("--release-tag", required=True)
    parser.add_argument("--private-source-root", type=Path, required=True)
    parser.add_argument("--public-source-root", type=Path, required=True)
    parser.add_argument("--windows-package-root", type=Path, required=True)
    parser.add_argument("--windows-evidence", type=Path, required=True)
    parser.add_argument("--windows-archive-report", type=Path, required=True)
    parser.add_argument("--linux-game-root", type=Path, required=True)
    parser.add_argument("--linux-archive-report", type=Path, required=True)
    parser.add_argument("--linux-acceptance", type=Path, required=True)
    parser.add_argument("--developer-release-dir", type=Path, required=True)
    parser.add_argument("--developer-payload-manifest", type=Path, required=True)
    parser.add_argument("--component-manifest", type=Path, required=True)
    parser.add_argument("--dependency-report", type=Path, required=True)
    parser.add_argument("--privacy-report", type=Path, required=True)
    parser.add_argument("--map-load-report", type=Path, required=True)
    parser.add_argument("--attribution-notice", type=Path, required=True)
    parser.add_argument("--product-terms", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    inputs = legacy.ReleaseInputs(
        args.release_version,
        args.release_tag,
        args.private_source_root,
        args.public_source_root,
        args.windows_package_root,
        args.windows_evidence,
        args.developer_release_dir,
        args.developer_payload_manifest,
        args.component_manifest,
        args.dependency_report,
        args.privacy_report,
        args.map_load_report,
        args.attribution_notice,
        args.product_terms,
    )
    try:
        result = prepare(
            inputs,
            args.output_dir,
            args.windows_archive_report,
            args.linux_game_root,
            args.linux_archive_report,
            args.linux_acceptance,
        )
    except (legacy.ReleaseError, OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    print(f"[OK] prepare-v4: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
