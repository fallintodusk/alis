import importlib.util
import io
import json
import shutil
import sys
import tarfile
import unittest
import uuid
from pathlib import Path


PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = PACKAGE_ROOT / "accept_linux_player.py"


class LinuxPlayerAcceptanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        spec = importlib.util.spec_from_file_location("accept_linux_player", MODULE_PATH)
        cls.module = importlib.util.module_from_spec(spec)
        assert spec.loader
        sys.modules["accept_linux_player"] = cls.module
        spec.loader.exec_module(cls.module)

    def setUp(self) -> None:
        if sys.platform.startswith("linux"):
            self.root = Path.home() / "alis-linux-acceptance-tests" / uuid.uuid4().hex
        else:
            self.root = PROJECT_ROOT / "tmp/release/tests" / uuid.uuid4().hex
        self.root.mkdir(parents=True)

    def tearDown(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)

    def make_archive(self, launcher_mode: int = 0o755, member_name: str = "Alis.sh") -> tuple[Path, dict]:
        archive_path = self.root / "player.tar"
        executable_name = "Alis/Binaries/Linux/Alis-Linux-Shipping"
        with tarfile.open(archive_path, "w") as archive:
            for name, content, mode in (
                (member_name, b"#!/bin/sh\n", launcher_mode),
                (executable_name, b"\x7fELFshipping", 0o755),
            ):
                info = tarfile.TarInfo(name)
                info.size = len(content)
                info.mode = mode
                archive.addfile(info, io.BytesIO(content))
        report = {
            "required_executable_modes": {
                "Alis.sh": "0755",
                executable_name: "0755",
            }
        }
        return archive_path, report

    def test_extract_preserves_required_executable_modes(self) -> None:
        if not sys.platform.startswith("linux"):
            self.skipTest("extracted POSIX mode proof runs on the WSL Linux filesystem")
        archive, report = self.make_archive()
        destination = self.root / "game"
        self.module.extract_archive(archive, destination, report)
        self.assertEqual(0o755, (destination / "Alis.sh").stat().st_mode & 0o777)
        self.assertEqual(
            0o755,
            (destination / "Alis/Binaries/Linux/Alis-Linux-Shipping").stat().st_mode & 0o777,
        )

    def test_extract_rejects_removed_launcher_execute_mode(self) -> None:
        archive, report = self.make_archive(launcher_mode=0o644)
        with self.assertRaisesRegex(self.module.AcceptanceError, "archive mode for Alis.sh"):
            self.module.extract_archive(archive, self.root / "game", report)

    def test_extract_rejects_unsafe_member_before_writing_outside_root(self) -> None:
        archive, report = self.make_archive(member_name="../Alis.sh")
        with self.assertRaisesRegex(self.module.AcceptanceError, "Unsafe Linux archive member"):
            self.module.extract_archive(archive, self.root / "game", report)
        self.assertFalse((self.root.parent / "Alis.sh").exists())

    def test_report_requires_closed_linux_platform(self) -> None:
        report = {
            "schema": "alis-player-archive-v2",
            "status": "accepted",
            "platform": "windows-x86_64",
            "format": "tar",
            "shipping_executable": "Alis/Binaries/Linux/Alis-Linux-Shipping",
            "launcher": "Alis.sh",
            "parts": [{"name": "part.001", "byte_size": 1, "sha256": "0" * 64}],
        }
        with self.assertRaisesRegex(self.module.AcceptanceError, "platform mismatch"):
            self.module.validate_report(report)

    def test_declared_environment_rejects_a_different_ubuntu_version(self) -> None:
        with self.assertRaisesRegex(self.module.AcceptanceError, "distribution version"):
            self.module.validate_acceptance_environment(
                {"ID": "ubuntu", "VERSION_ID": "24.04"},
                "Linux",
                "x86_64",
                "6.6.87.2-microsoft-standard-WSL2",
                True,
                True,
                True,
            )

    def test_report_rejects_missing_source_identity(self) -> None:
        report = {
            "schema": "alis-player-archive-v2",
            "status": "accepted",
            "platform": "linux-x86_64",
            "format": "tar",
            "shipping_executable": "Alis/Binaries/Linux/Alis-Linux-Shipping",
            "launcher": "Alis.sh",
            "release_version": "2.1.0",
            "logical_name": "ALIS_Linux_x86_64_v2.1.0.tar",
        }
        with self.assertRaisesRegex(self.module.AcceptanceError, "source_revision"):
            self.module.validate_report(report)


if __name__ == "__main__":
    unittest.main()
