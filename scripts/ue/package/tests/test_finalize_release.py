import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path


PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PREPARE_PATH = PACKAGE_ROOT / "prepare_release.py"
FINALIZE_PATH = PACKAGE_ROOT / "finalize_release.py"

PREPARE_SPEC = importlib.util.spec_from_file_location("prepare_release", PREPARE_PATH)
PREPARE = importlib.util.module_from_spec(PREPARE_SPEC)
assert PREPARE_SPEC.loader
sys.modules[PREPARE_SPEC.name] = PREPARE
PREPARE_SPEC.loader.exec_module(PREPARE)

FINALIZE_SPEC = importlib.util.spec_from_file_location("finalize_release", FINALIZE_PATH)
FINALIZE = importlib.util.module_from_spec(FINALIZE_SPEC)
assert FINALIZE_SPEC.loader
FINALIZE_SPEC.loader.exec_module(FINALIZE)

try:
    from . import test_prepare_release as fixture_module
except ImportError:
    import test_prepare_release as fixture_module


class FinalizeReleaseTests(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture = fixture_module.PrepareReleaseTests(
            "test_prepare_and_approve_bind_exact_release"
        )
        self.fixture.setUp()
        self.__dict__.update(self.fixture.__dict__)
        payload = json.loads(self.developer_manifest.read_text(encoding="utf-8"))
        payload["owners"] = ["ProjectWorldData"]
        payload["manifest_authority"] = {"ProjectWorldData": []}
        payload["entries"] = []
        self.developer_manifest.write_text(json.dumps(payload), encoding="utf-8")
        self.release = self.root / "release"
        PREPARE.prepare_release(
            self.fixture.inputs(), self.release, [self.player_archive], self.archive_report
        )

    def test_identical_tree_new_revision_rebinds_metadata_without_changing_payload_bytes(self) -> None:
        archive = self.release / "developer.zip"
        before_archive = PREPARE.sha256_file(archive)
        subprocess.run(["git", "commit", "--allow-empty", "-q", "-m", "metadata"], cwd=self.public, check=True)
        new_revision = PREPARE.git_value(self.public, "rev-parse", "HEAD")

        FINALIZE.rebind_release(self.release, self.public, "main")

        manifest = PREPARE.read_json(self.release / "release_manifest.json")
        self.assertEqual(new_revision, manifest["public_source"]["revision"])
        self.assertEqual(self.public_tree, manifest["public_source"]["tree"])
        self.assertEqual("content_identical", manifest["public_source_rebind"]["status"])
        rebound_archive = next(self.release.glob("ALIS_DeveloperProject_*.zip"))
        self.assertEqual(before_archive, PREPARE.sha256_file(rebound_archive))
        readme = (self.release / "README.txt").read_text(encoding="ascii")
        rebound_payload = next(self.release.glob("ALIS_DeveloperProject_*.developer-payload.json"))
        self.assertIn(rebound_archive.name, readme)
        self.assertIn(rebound_payload.name, readme)
        self.assertNotIn("developer.zip", readme)
        self.assertNotIn("developer.developer-payload.json", readme)
        PREPARE.verify_release_manifest(self.release)

    def test_changed_public_tree_refuses_without_mutating_release(self) -> None:
        before = {
            path.relative_to(self.release).as_posix(): PREPARE.sha256_file(path)
            for path in self.release.rglob("*")
            if path.is_file()
        }
        (self.public / "tracked.txt").write_text("changed\n", encoding="utf-8")
        subprocess.run(["git", "add", "tracked.txt"], cwd=self.public, check=True)
        subprocess.run(["git", "commit", "-q", "-m", "changed"], cwd=self.public, check=True)

        with self.assertRaisesRegex(PREPARE.ReleaseError, "differs from the reviewed"):
            FINALIZE.rebind_release(self.release, self.public, "main")

        after = {
            path.relative_to(self.release).as_posix(): PREPARE.sha256_file(path)
            for path in self.release.rglob("*")
            if path.is_file()
        }
        self.assertEqual(before, after)

    def test_v4_rebind_preserves_each_platform_game_identity(self) -> None:
        manifest_path = self.release / "release_manifest.json"
        manifest = PREPARE.read_json(manifest_path)
        player = manifest.pop("player_source")
        manifest["schema"] = "alis-release-manifest-v4"
        manifest["release_version"] = "2.1.0"
        manifest["release_tag"] = "v2.1.0"
        manifest["player_sources"] = {
            "windows-x86_64": {
                "revision": "a" * 40,
                "source_state_sha256": "b" * 64,
                "runtime_payload_tree_sha256": "1" * 64,
                "shipping_executable": "Alis/Binaries/Win64/Alis-Win64-Shipping.exe",
                "shipping_executable_sha256": player["shipping_executable_sha256"],
            },
            "linux-x86_64": {
                "revision": "a" * 40,
                "source_state_sha256": "b" * 64,
                "runtime_payload_tree_sha256": "2" * 64,
                "shipping_executable": "Alis/Binaries/Linux/Alis-Linux-Shipping",
                "shipping_executable_sha256": "3" * 64,
            },
        }
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        subprocess.run(["git", "commit", "--allow-empty", "-q", "-m", "metadata"], cwd=self.public, check=True)

        FINALIZE.rebind_release(self.release, self.public, "main")

        rebound = PREPARE.read_json(manifest_path)
        self.assertEqual(
            {
                "windows-x86_64": "1" * 64,
                "linux-x86_64": "2" * 64,
            },
            rebound["public_source_rebind"]["player_runtime_payload_tree_sha256"],
        )
        self.assertNotIn("player_package_tree_sha256", rebound["public_source_rebind"])
        PREPARE.verify_release_manifest(self.release)

    def tearDown(self) -> None:
        self.fixture.tearDown()


if __name__ == "__main__":
    unittest.main()
