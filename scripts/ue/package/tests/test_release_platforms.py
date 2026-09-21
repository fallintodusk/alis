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
MODULE_PATH = PACKAGE_ROOT / "release_platforms.py"


class ReleasePlatformTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        spec = importlib.util.spec_from_file_location("release_platforms", MODULE_PATH)
        cls.module = importlib.util.module_from_spec(spec)
        assert spec.loader
        sys.modules["release_platforms"] = cls.module
        spec.loader.exec_module(cls.module)

    def setUp(self) -> None:
        self.root = PROJECT_ROOT / "tmp/release/tests" / uuid.uuid4().hex
        self.game = self.root / "game"
        executable = self.game / "Alis/Binaries/Linux/Alis-Linux-Shipping"
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"\x7fELF" + b"shipping")
        (self.game / "Alis.sh").write_text("#!/bin/sh\nexec ./Alis/Binaries/Linux/Alis-Linux-Shipping\n", encoding="ascii")
        (self.game / "payload.bin").write_bytes(b"x" * (1024 * 1024 + 17))

    def tearDown(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)

    def test_linux_archive_preserves_launch_modes_and_splits_below_limit(self) -> None:
        output = self.root / "archive"
        report_path = self.module.archive_linux_game(
            self.game,
            output,
            "2.1.0",
            1,
            "a" * 40,
            "b" * 64,
        )
        report = json.loads(report_path.read_text(encoding="utf-8"))

        self.assertEqual("alis-player-archive-v2", report["schema"])
        self.assertEqual("linux-x86_64", report["platform"])
        self.assertEqual("tar", report["format"])
        self.assertEqual("ALIS_Linux_x86_64_v2.1.0.tar", report["logical_name"])
        self.assertGreater(len(report["parts"]), 1)
        self.assertTrue(all(part["byte_size"] <= 1024 * 1024 for part in report["parts"]))

        archive_bytes = b"".join((output / part["name"]).read_bytes() for part in report["parts"])
        self.assertEqual(report["archive_byte_size"], len(archive_bytes))
        self.assertEqual(report["archive_sha256"], self.module.sha256_bytes(archive_bytes))
        with tarfile.open(fileobj=io.BytesIO(archive_bytes), mode="r:") as archive:
            self.assertEqual(0o755, archive.getmember("Alis.sh").mode)
            self.assertEqual(0o755, archive.getmember("Alis/Binaries/Linux/Alis-Linux-Shipping").mode)
            self.assertEqual(0o644, archive.getmember("payload.bin").mode)

    def test_linux_archive_rejects_non_elf_shipping_executable(self) -> None:
        executable = self.game / "Alis/Binaries/Linux/Alis-Linux-Shipping"
        executable.write_bytes(b"not-elf")
        with self.assertRaisesRegex(self.module.ReleasePlatformError, "ELF"):
            self.module.archive_linux_game(
                self.game,
                self.root / "archive",
                "2.1.0",
                1,
                "a" * 40,
                "b" * 64,
            )


if __name__ == "__main__":
    unittest.main()
