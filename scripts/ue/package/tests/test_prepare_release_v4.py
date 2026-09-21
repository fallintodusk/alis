import importlib.util
import json
import shutil
import sys
import unittest
import uuid
from pathlib import Path
from unittest import mock


PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = Path(__file__).resolve().parents[4]
for name in (
    "prepare_release",
    "release_platforms",
    "prepare_release_v4",
    "release_workspace",
    "refresh_github_release_v4",
):
    path = PACKAGE_ROOT / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader
    sys.modules[name] = module
    spec.loader.exec_module(module)

legacy = sys.modules["prepare_release"]
platforms = sys.modules["release_platforms"]
release_v4 = sys.modules["prepare_release_v4"]
workspace = sys.modules["release_workspace"]
refresh_v4 = sys.modules["refresh_github_release_v4"]


class PrepareReleaseV4Tests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = PROJECT_ROOT / "tmp/release/tests" / uuid.uuid4().hex
        self.root.mkdir(parents=True)

    def tearDown(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)

    def make_fixture(self) -> tuple[legacy.ReleaseInputs, dict, Path, Path, Path, Path]:
        windows_package = self.root / "windows-package"
        windows_game = windows_package / "Windows"
        windows_executable = windows_game / "Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
        windows_executable.parent.mkdir(parents=True)
        windows_executable.write_bytes(b"windows")

        windows_archive = self.root / "windows-archive"
        windows_archive.mkdir()
        windows_part = windows_archive / "ALIS_Win64_v2.1.0.zip.001"
        windows_part.write_bytes(b"windows-archive")
        windows_report = {
            "schema": "alis-player-archive-v1",
            "status": "accepted",
            "package_tree_sha256": legacy.package_tree_digest(windows_package),
            "game_tree_sha256": legacy.package_tree_digest(windows_game),
            "parts": [
                {
                    "name": windows_part.name,
                    "byte_size": windows_part.stat().st_size,
                    "sha256": legacy.sha256_file(windows_part),
                }
            ],
        }
        windows_report_path = windows_archive / "player-archive.json"
        windows_report_path.write_text(json.dumps(windows_report), encoding="utf-8")

        linux_game = self.root / "linux-game"
        linux_executable = linux_game / "Alis/Binaries/Linux/Alis-Linux-Shipping"
        linux_executable.parent.mkdir(parents=True)
        linux_executable.write_bytes(b"\x7fELFlinux")
        (linux_game / "Alis.sh").write_text("#!/bin/sh\n", encoding="ascii")
        linux_report_path = platforms.archive_linux_game(
            linux_game,
            self.root / "linux-archive",
            "2.1.0",
            1,
            "a" * 40,
            "b" * 64,
        )
        linux_report = legacy.read_json(linux_report_path)
        acceptance = {
            "schema": "alis-linux-player-acceptance-v1",
            "status": "accepted",
            "platform": "linux-x86_64",
            "environment": {
                "baseline": "Ubuntu 22.04 x86-64 under WSL2/WSLg",
                "distribution": "ubuntu",
                "distribution_version": "22.04",
                "filesystem": "ext4",
                "wsl_interop": True,
                "wslg": True,
                "dxg": True,
            },
            "routes": [
                {
                    "route": "kazan",
                    "map": "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory",
                    "rhi": "Vulkan",
                    "gpu_adapter": "NVIDIA RTX fixture",
                },
                {
                    "route": "manhattan",
                    "map": "/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase",
                    "rhi": "Vulkan",
                    "gpu_adapter": "NVIDIA RTX fixture",
                },
            ],
            **{
                key: linux_report[key]
                for key in (
                    "release_version",
                    "source_revision",
                    "source_state_sha256",
                    "archive_sha256",
                    "archive_byte_size",
                    "shipping_executable_sha256",
                )
            },
            "runtime_payload_tree_sha256": linux_report["game_tree_sha256"],
        }
        acceptance_path = self.root / "linux-player-acceptance.json"
        acceptance_path.write_text(json.dumps(acceptance), encoding="utf-8")

        developer_root = self.root / "developer"
        developer_root.mkdir()
        developer_part = developer_root / "developer.zip"
        developer_part.write_bytes(b"developer")
        developer_payload = developer_root / "developer.developer-payload.json"
        developer_payload.write_text(
            json.dumps({"archive": {"parts": [{"name": developer_part.name}]}}),
            encoding="utf-8",
        )
        notices = developer_root / "fixture.notices.json"
        notices.write_text("{}\n", encoding="ascii")
        component = self.root / "component.json"
        component.write_text("{}\n", encoding="ascii")
        terms = self.root / "terms.txt"
        terms.write_text("terms\n", encoding="ascii")
        inputs = legacy.ReleaseInputs(
            "2.1.0",
            "v2.1.0",
            self.root,
            self.root,
            windows_package,
            self.root / "windows-evidence.json",
            developer_root,
            developer_payload,
            component,
            self.root / "dependency.json",
            self.root / "privacy.json",
            self.root / "maps.json",
            notices,
            terms,
        )
        validated = {
            "private_revision": "a" * 40,
            "private_state": "b" * 64,
            "public_revision": "c" * 40,
            "public_tree": "d" * 40,
            "player": {
                "operation_id": "fixture-operation",
                "package_tree_sha256": windows_report["package_tree_sha256"],
                "shipping_executable_sha256": legacy.sha256_file(windows_executable),
                "product_review": "pending_owner_approval",
            },
            "archive_report": windows_report,
            "developer_manifest": {
                "archive": {"parts": [{"name": developer_part.name}]},
            },
            "developer_files": [developer_part, developer_payload, notices],
        }
        return inputs, validated, windows_report_path, linux_game, linux_report_path, acceptance_path

    def test_prepare_writes_v4_and_workspace_v2_accepts_both_platforms(self) -> None:
        inputs, validated, windows_report, linux_game, linux_report, acceptance = self.make_fixture()
        output = self.root / "workspace/github"
        output.mkdir(parents=True)
        windows_value = json.loads(windows_report.read_text(encoding="utf-8"))
        for part in windows_value["parts"]:
            shutil.copy2(windows_report.parent / part["name"], output / part["name"])
        windows_report = output / "player-archive.json"
        windows_report.write_text(json.dumps(windows_value), encoding="utf-8")
        with mock.patch.object(legacy, "validate_inputs", return_value=validated):
            manifest_path = release_v4.prepare(
                inputs,
                output,
                windows_report,
                linux_game,
                linux_report,
                acceptance,
            )
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual("alis-release-manifest-v4", manifest["schema"])
        self.assertEqual(set(workspace.PLATFORM_KEYS), set(manifest["player_sources"]))
        legacy.verify_release_manifest(output)

        windows_game = inputs.player_package_root / "Windows"
        workspace.initialize_workspace_v2(
            self.root / "workspace",
            {"windows-x86_64": windows_game, "linux-x86_64": linux_game},
            "2.1.0",
        )
        workspace.adopt_platforms(
            self.root / "workspace",
            {"windows-x86_64": windows_game, "linux-x86_64": linux_game},
        )
        workspace.verify_workspace(self.root / "workspace")

    def test_prepare_rejects_acceptance_missing_required_route(self) -> None:
        inputs, validated, windows_report, linux_game, linux_report, acceptance = self.make_fixture()
        value = json.loads(acceptance.read_text(encoding="utf-8"))
        value["routes"] = [{"route": "kazan"}]
        acceptance.write_text(json.dumps(value), encoding="utf-8")
        with mock.patch.object(legacy, "validate_inputs", return_value=validated):
            with self.assertRaisesRegex(legacy.ReleaseError, "both required product routes"):
                release_v4.prepare(
                    inputs,
                    self.root / "github",
                    windows_report,
                    linux_game,
                    linux_report,
                    acceptance,
                )

    def test_v4_manifest_rejects_platform_without_source_identity(self) -> None:
        inputs, validated, windows_report, linux_game, linux_report, acceptance = self.make_fixture()
        output = self.root / "github"
        with mock.patch.object(legacy, "validate_inputs", return_value=validated):
            manifest_path = release_v4.prepare(
                inputs,
                output,
                windows_report,
                linux_game,
                linux_report,
                acceptance,
            )
        manifest = legacy.read_json(manifest_path)
        del manifest["player_sources"]["linux-x86_64"]["source_state_sha256"]
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

        with self.assertRaisesRegex(legacy.ReleaseError, "source_state_sha256"):
            legacy.verify_release_manifest(output)

    def test_v4_manifest_rejects_cross_platform_source_identity_mismatch(self) -> None:
        inputs, validated, windows_report, linux_game, linux_report, acceptance = self.make_fixture()
        output = self.root / "github"
        with mock.patch.object(legacy, "validate_inputs", return_value=validated):
            manifest_path = release_v4.prepare(
                inputs,
                output,
                windows_report,
                linux_game,
                linux_report,
                acceptance,
            )
        manifest = legacy.read_json(manifest_path)
        manifest["player_sources"]["linux-x86_64"]["source_state_sha256"] = "e" * 64
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

        with self.assertRaisesRegex(legacy.ReleaseError, "cross-platform source state"):
            legacy.verify_release_manifest(output)

    def test_refresh_replaces_both_unsigned_archives_with_signed_game_archives(self) -> None:
        inputs, validated, windows_report, linux_game, linux_report, acceptance = self.make_fixture()
        source = self.root / "github"
        with mock.patch.object(legacy, "validate_inputs", return_value=validated):
            release_v4.prepare(
                inputs,
                source,
                windows_report,
                linux_game,
                linux_report,
                acceptance,
            )
        legacy.approve_release(source, True)
        windows_game = inputs.player_package_root / "Windows"
        for game in (windows_game, linux_game):
            verification = game / "Verification"
            verification.mkdir()
            (verification / "SHA256SUMS.txt").write_text("manifest\n", encoding="ascii")
            (verification / "SHA256SUMS.txt.asc").write_text("signature\n", encoding="ascii")

        windows_signed_dir = self.root / "windows-signed"
        linux_signed_dir = self.root / "linux-signed"
        windows_signed_dir.mkdir()
        linux_signed_dir.mkdir()
        windows_part = windows_signed_dir / "ALIS_Win64_v2.1.0.zip.001"
        linux_part = linux_signed_dir / "ALIS_Linux_x86_64_v2.1.0.tar.001"
        windows_part.write_bytes(b"signed-windows")
        linux_part.write_bytes(b"signed-linux")
        windows_signed_report = windows_signed_dir / "player-archive.json"
        linux_signed_report = linux_signed_dir / "linux-player-archive.json"
        windows_signed_report.write_text(
            json.dumps(
                {
                    "schema": "alis-game-archive-v1",
                    "status": "accepted",
                    "game_tree_sha256": legacy.package_tree_digest(windows_game),
                    "parts": [
                        {
                            "name": windows_part.name,
                            "byte_size": windows_part.stat().st_size,
                            "sha256": legacy.sha256_file(windows_part),
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
        linux_signed_report.write_text(
            json.dumps(
                {
                    "schema": "alis-player-archive-v2",
                    "status": "accepted",
                    "platform": "linux-x86_64",
                    "logical_name": "ALIS_Linux_x86_64_v2.1.0.tar",
                    "archive_sha256": "f" * 64,
                    "archive_byte_size": linux_part.stat().st_size,
                    "game_tree_sha256": platforms.platform_tree_digest(linux_game, include_modes=True),
                    "parts": [
                        {
                            "name": linux_part.name,
                            "byte_size": linux_part.stat().st_size,
                            "sha256": legacy.sha256_file(linux_part),
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )

        tampered_linux_game = self.root / "tampered-linux-game"
        shutil.copytree(linux_game, tampered_linux_game)
        (tampered_linux_game / "Alis.sh").write_text("#!/bin/sh\necho changed\n", encoding="ascii")
        tampered_report = legacy.read_json(linux_signed_report)
        tampered_report["game_tree_sha256"] = platforms.platform_tree_digest(
            tampered_linux_game, include_modes=True
        )
        tampered_report_path = self.root / "tampered-linux-report.json"
        tampered_report_path.write_text(json.dumps(tampered_report), encoding="utf-8")
        with self.assertRaisesRegex(legacy.ReleaseError, "accepted runtime payload tree"):
            refresh_v4.refresh(
                source,
                self.root / "tampered-refresh",
                windows_game,
                windows_signed_report,
                tampered_linux_game,
                tampered_report_path,
            )

        output = self.root / "refreshed"
        result = refresh_v4.refresh(
            source,
            output,
            windows_game,
            windows_signed_report,
            linux_game,
            linux_signed_report,
        )

        refreshed = legacy.read_json(result)
        self.assertTrue((output / windows_part.name).is_file())
        self.assertTrue((output / linux_part.name).is_file())
        self.assertEqual(set(workspace.PLATFORM_KEYS), set(refreshed["player_distribution"]))
        self.assertEqual(
            refreshed["player_sources"]["linux-x86_64"]["runtime_payload_tree_sha256"],
            refreshed["player_distribution"]["linux-x86_64"]["runtime_payload_tree_sha256"],
        )
        self.assertNotEqual(
            refreshed["player_sources"]["linux-x86_64"]["runtime_payload_tree_sha256"],
            refreshed["player_distribution"]["linux-x86_64"]["game_tree_sha256"],
        )
        legacy.verify_release_manifest(output, require_ready=True)
        refreshed["player_distribution"]["linux-x86_64"]["archive_sha256"] = "invalid"
        result.write_text(json.dumps(refreshed), encoding="utf-8")
        with self.assertRaisesRegex(legacy.ReleaseError, "archive_sha256"):
            legacy.verify_release_manifest(output, require_ready=True)


if __name__ == "__main__":
    unittest.main()
