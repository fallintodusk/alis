import importlib.util
import json
import shutil
import sys
import unittest
import uuid
from pathlib import Path


PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = Path(__file__).resolve().parents[4]
for name in ("prepare_release", "release_platforms", "release_workspace"):
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
            "release_version": "2.0.99",
            "release_tag": "v2.0.99",
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
        workspace.initialize_workspace(release_root, candidate, "2.0.99")
        return release_root, candidate

    def make_v2_fixture(self, name: str, initialize: bool = True) -> tuple[Path, Path]:
        candidate = self.root / f"{name}-candidate"
        windows_executable = candidate / "windows-x86_64/Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
        linux_executable = candidate / "linux-x86_64/Alis/Binaries/Linux/Alis-Linux-Shipping"
        windows_executable.parent.mkdir(parents=True)
        linux_executable.parent.mkdir(parents=True)
        windows_executable.write_bytes(b"windows-shipping")
        linux_executable.write_bytes(b"\x7fELFlinux-shipping")
        (candidate / "linux-x86_64/Alis.sh").write_text("#!/bin/sh\n", encoding="ascii")

        release_root = self.root / name
        github = release_root / "github"
        github.mkdir(parents=True)
        readme = github / "README.txt"
        readme.write_text("fixture\n", encoding="ascii")
        manifest = {
            "schema": "alis-release-manifest-v4",
            "status": "pending_owner_approval",
            "release_version": "9.8.7",
            "release_tag": "v9.8.7",
            "unresolved_count": 0,
            "player_sources": {
                platform: {
                    "revision": "a" * 40,
                    "source_state_sha256": "b" * 64,
                    "runtime_payload_tree_sha256": workspace.platform_game_tree_digest(
                        candidate / platform, platform
                    ),
                    "shipping_executable": workspace.PLATFORM_EXECUTABLES[platform].as_posix(),
                    "shipping_executable_sha256": release.sha256_file(
                        candidate / platform / workspace.PLATFORM_EXECUTABLES[platform]
                    ),
                }
                for platform in workspace.PLATFORM_KEYS
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
        if initialize:
            workspace.initialize_workspace_v2(
                release_root,
                {platform: candidate / platform for platform in workspace.PLATFORM_KEYS},
                "9.8.7",
            )
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
        manifest["release_version"] = "2.0.98"
        manifest["release_tag"] = "v2.0.98"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

        with self.assertRaisesRegex(release.ReleaseError, "workspace version"):
            workspace.recover_github_projection(release_root)

        self.assertFalse((release_root / "github").exists())
        self.assertTrue(backup.is_dir())

    def test_v1_rejects_release_version_that_requires_v2(self) -> None:
        release_root, candidate = self.make_fixture("v1-wrong-generation")
        workspace.adopt_game(release_root, candidate)
        for path in (
            release_root / "release-workspace.json",
            release_root / "github/release_manifest.json",
        ):
            value = json.loads(path.read_text(encoding="utf-8"))
            value["release_version"] = "2.1.0"
            value["release_tag"] = "v2.1.0"
            path.write_text(json.dumps(value), encoding="utf-8")

        with self.assertRaisesRegex(release.ReleaseError, "manifest schema mismatch"):
            workspace.verify_workspace(release_root)

    def test_v2_adopts_exact_closed_platform_map(self) -> None:
        release_root, candidate = self.make_v2_fixture("v2-normal")
        state = json.loads((release_root / "release-workspace.json").read_text(encoding="utf-8"))
        self.assertEqual("alis-release-workspace-v2", state["schema"])

        workspace.adopt_platforms(
            release_root,
            {platform: candidate / platform for platform in workspace.PLATFORM_KEYS},
        )
        workspace.verify_workspace(release_root)

        self.assertFalse(candidate.exists())
        self.assertTrue(
            (release_root / "game/windows-x86_64/Alis/Binaries/Win64/Alis-Win64-Shipping.exe").is_file()
        )
        self.assertTrue(
            (release_root / "game/linux-x86_64/Alis/Binaries/Linux/Alis-Linux-Shipping").is_file()
        )

    def test_v2_rejects_platform_tamper(self) -> None:
        release_root, candidate = self.make_v2_fixture("v2-tamper")
        workspace.adopt_platforms(
            release_root,
            {platform: candidate / platform for platform in workspace.PLATFORM_KEYS},
        )
        (release_root / "game/linux-x86_64/Alis/Binaries/Linux/Alis-Linux-Shipping").write_bytes(
            b"\x7fELFchanged"
        )
        with self.assertRaisesRegex(release.ReleaseError, "linux-x86_64 runtime payload tree mismatch"):
            workspace.verify_workspace(release_root)

    def test_v2_does_not_hide_a_foreign_platform_verifier(self) -> None:
        release_root, candidate = self.make_v2_fixture("v2-foreign-verifier")
        workspace.adopt_platforms(
            release_root,
            {platform: candidate / platform for platform in workspace.PLATFORM_KEYS},
        )
        (release_root / "game/linux-x86_64/VERIFY_ALIS.bat").write_text(
            "unexpected\n", encoding="ascii"
        )
        with self.assertRaisesRegex(release.ReleaseError, "linux-x86_64 runtime payload tree mismatch"):
            workspace.verify_workspace(release_root)

    def test_v2_rejects_missing_platform_before_adoption(self) -> None:
        release_root, candidate = self.make_v2_fixture("v2-missing", initialize=False)
        shutil.rmtree(candidate / "linux-x86_64")
        with self.assertRaisesRegex(release.ReleaseError, "Candidate is missing for linux-x86_64"):
            workspace.initialize_workspace_v2(
                release_root,
                {platform: candidate / platform for platform in workspace.PLATFORM_KEYS},
                "9.8.7",
            )


if __name__ == "__main__":
    unittest.main()
