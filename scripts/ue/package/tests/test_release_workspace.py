import importlib.util
import json
import shutil
import sys
import unittest
import uuid
from pathlib import Path


PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = Path(__file__).resolve().parents[4]
for name in ("prepare_release", "release_workspace"):
    path = PACKAGE_ROOT / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader
    sys.modules[name] = module
    spec.loader.exec_module(module)

release = sys.modules["prepare_release"]
workspace = sys.modules["release_workspace"]


class ReleaseWorkspaceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = PROJECT_ROOT / "tmp/release/tests" / uuid.uuid4().hex
        self.root.mkdir(parents=True)

    def tearDown(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)

    def make_fixture(self, name: str) -> tuple[Path, Path]:
        candidate = self.root / f"{name}-candidate"
        executable = candidate / "Windows/Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"shipping")
        (candidate / "package_summary.txt").write_text("accepted\n", encoding="ascii")
        package_digest = release.package_tree_digest(candidate)
        release_root = self.root / name
        github = release_root / "github"
        github.mkdir(parents=True)
        readme = github / "README.txt"
        readme.write_text("fixture\n", encoding="ascii")
        manifest = {
            "schema": "alis-release-manifest-v3",
            "status": "pending_owner_approval",
            "release_version": "9.8.7",
            "release_tag": "v9.8.7",
            "unresolved_count": 0,
            "player_source": {
                "package_tree_sha256": package_digest,
                "shipping_executable_sha256": release.sha256_file(executable),
            },
            "artifacts": [
                {
                    "name": readme.name,
                    "byte_size": readme.stat().st_size,
                    "sha256": release.sha256_file(readme),
                }
            ],
        }
        (github / "release_manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        workspace.initialize_workspace(release_root, candidate, "9.8.7")
        return release_root, candidate

    def test_adoption_moves_candidate_and_verifies_exact_game(self) -> None:
        release_root, candidate = self.make_fixture("normal")
        workspace.adopt_game(release_root, candidate)
        workspace.verify_workspace(release_root)
        self.assertFalse(candidate.exists())
        self.assertTrue((release_root / "game/Alis/Binaries/Win64/Alis-Win64-Shipping.exe").is_file())

        verification = release_root / "game/Verification"
        verification.mkdir()
        (release_root / "game/VERIFY_ALIS.bat").write_text("verify\n", encoding="ascii")
        (verification / "VERIFY_ALIS.ps1").write_text("verify\n", encoding="ascii")
        (verification / "ALIS_PUBLIC_KEY.asc").write_text("key\n", encoding="ascii")
        (verification / "SHA256SUMS.txt").write_text("signed\n", encoding="ascii")
        (verification / "SHA256SUMS.txt.asc").write_text("signature\n", encoding="ascii")
        workspace.verify_workspace(release_root)
        (verification / "unexpected.bin").write_bytes(b"unexpected")
        with self.assertRaisesRegex(release.ReleaseError, "package tree mismatch"):
            workspace.verify_workspace(release_root)
        (verification / "unexpected.bin").unlink()
        (release_root / "game/Alis/Binaries/Win64/Alis-Win64-Shipping.exe").write_bytes(b"changed")
        with self.assertRaisesRegex(release.ReleaseError, "package tree mismatch"):
            workspace.verify_workspace(release_root)

    def test_adoption_resumes_after_game_directory_move(self) -> None:
        release_root, candidate = self.make_fixture("resume")
        (candidate / "Windows").replace(release_root / "game")
        workspace.adopt_game(release_root, candidate)
        workspace.verify_workspace(release_root)
        self.assertFalse(candidate.exists())

    def test_recovery_restores_the_only_interrupted_github_backup(self) -> None:
        release_root, candidate = self.make_fixture("recover-missing")
        workspace.adopt_game(release_root, candidate)
        backup = release_root / ("github-previous-" + "1" * 32)
        (release_root / "github").replace(backup)

        workspace.recover_github_projection(release_root)

        self.assertTrue((release_root / "github").is_dir())
        self.assertFalse(backup.exists())
        workspace.verify_workspace(release_root)

    def test_recovery_keeps_valid_github_and_removes_stale_backup(self) -> None:
        release_root, candidate = self.make_fixture("recover-current")
        workspace.adopt_game(release_root, candidate)
        backup = release_root / ("github-previous-" + "2" * 32)
        shutil.copytree(release_root / "github", backup)

        workspace.recover_github_projection(release_root)

        self.assertTrue((release_root / "github").is_dir())
        self.assertFalse(backup.exists())
        workspace.verify_workspace(release_root)

    def test_recovery_rejects_ambiguous_backups(self) -> None:
        release_root, candidate = self.make_fixture("recover-ambiguous")
        workspace.adopt_game(release_root, candidate)
        for marker in ("3", "4"):
            shutil.copytree(
                release_root / "github",
                release_root / ("github-previous-" + marker * 32),
            )

        with self.assertRaisesRegex(release.ReleaseError, "ambiguous"):
            workspace.recover_github_projection(release_root)

    def test_recovery_preserves_backup_when_current_github_is_invalid(self) -> None:
        release_root, candidate = self.make_fixture("recover-invalid-current")
        workspace.adopt_game(release_root, candidate)
        backup = release_root / ("github-previous-" + "5" * 32)
        shutil.copytree(release_root / "github", backup)
        (release_root / "github/README.txt").write_text("changed\n", encoding="ascii")

        with self.assertRaisesRegex(release.ReleaseError, "hash.*mismatch"):
            workspace.recover_github_projection(release_root)

        self.assertTrue((release_root / "github").is_dir())
        self.assertTrue(backup.is_dir())

    def test_recovery_does_not_restore_backup_from_another_release(self) -> None:
        release_root, candidate = self.make_fixture("recover-wrong-release")
        workspace.adopt_game(release_root, candidate)
        backup = release_root / ("github-previous-" + "6" * 32)
        (release_root / "github").replace(backup)
        manifest_path = backup / "release_manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["release_version"] = "9.8.6"
        manifest["release_tag"] = "v9.8.6"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

        with self.assertRaisesRegex(release.ReleaseError, "workspace version"):
            workspace.recover_github_projection(release_root)

        self.assertFalse((release_root / "github").exists())
        self.assertTrue(backup.is_dir())


if __name__ == "__main__":
    unittest.main()
