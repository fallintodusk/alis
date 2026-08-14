import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = REPO_ROOT / "scripts" / "git" / "mirror" / "compose_developer_payload.py"
SPEC = importlib.util.spec_from_file_location("compose_developer_payload", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class DeveloperPayloadTests(unittest.TestCase):
    def test_active_authority_selects_only_production_generated_assets(self):
        entries = {}
        scopes = MODULE.collect_manifest_authority(REPO_ROOT, "ProjectWorldData", entries)
        canonical = MODULE.collect_canonical_authority(REPO_ROOT, "ProjectWorldData", entries)

        self.assertTrue(scopes)
        self.assertTrue(canonical)
        self.assertTrue(any(entry.kind == "canonical_bundle" for entry in entries.values()))
        for entry in entries.values():
            lowered = entry.path.lower()
            self.assertNotIn("projectworldtestdata", lowered)
            self.assertNotIn("hlod", lowered)

    def test_public_asset_authority_selects_every_generated_definition_pair(self):
        entries = {}
        authorities = MODULE.collect_public_asset_authority(REPO_ROOT, entries)

        self.assertEqual(82, authorities[0]["asset_count"])
        self.assertEqual(82, sum(entry.kind == "generated_definition_asset" for entry in entries.values()))
        self.assertEqual(82, sum(entry.kind == "generated_definition_source" for entry in entries.values()))
        self.assertTrue(all("thirdparty" not in path.lower() for path in entries))
        self.assertTrue(all("projectworldtestdata" not in path.lower() for path in entries))

    def test_public_asset_authority_rejects_asset_hash_drift(self):
        manifest = json.loads(
            (REPO_ROOT / "Plugins/Resources/ProjectObject/Data/Manifests/public_generated_definitions.json").read_text()
        )
        target = REPO_ROOT / Path(manifest["assets"][0]["artifact_path"])
        original = MODULE.sha256_file

        def sabotaged(path):
            return "0" * 64 if Path(path).resolve() == target.resolve() else original(path)

        with mock.patch.object(MODULE, "sha256_file", side_effect=sabotaged):
            with self.assertRaises(MODULE.PayloadError):
                MODULE.collect_public_asset_authority(REPO_ROOT, {})

    def test_archive_split_is_bounded_and_byte_exact(self):
        with tempfile.TemporaryDirectory() as temp_value:
            archive = Path(temp_value) / "payload.zip"
            expected = bytes(range(35))
            archive.write_bytes(expected)

            parts = MODULE.split_archive(archive, 10)

            self.assertEqual([10, 10, 10, 5], [part.stat().st_size for part in parts])
            self.assertEqual(expected, b"".join(part.read_bytes() for part in parts))
            self.assertFalse(archive.exists())

    def test_unsafe_project_path_is_rejected(self):
        for value in ("../outside", "/rooted", ""):
            with self.subTest(value=value):
                with self.assertRaises(MODULE.PayloadError):
                    MODULE.safe_relative(value)

    def test_compose_rejects_non_public_revision_shape(self):
        with tempfile.TemporaryDirectory() as temp_value:
            with self.assertRaises(MODULE.PayloadError):
                MODULE.compose(
                    REPO_ROOT, Path(temp_value) / "release", "v-test",
                    ["ProjectWorldData"], 1700, True, "private-head", "main",
                )

    def test_mirror_refuses_to_claim_payload_publication_on_direct_push(self):
        git_bash = Path("C:/Program Files/Git/bin/bash.exe")
        bash = str(git_bash) if git_bash.is_file() else shutil.which("bash")
        if not bash:
            self.skipTest("Bash is required")
        result = subprocess.run(
            [
                bash, (REPO_ROOT / "scripts/git/mirror/mirror_to_github.sh").as_posix(),
                "--push", "--remote-url", "https://example.invalid/alis.git",
                "--developer-release-dir", "unused", "--developer-version", "v-test",
            ],
            capture_output=True, text=True, check=False,
        )
        self.assertNotEqual(0, result.returncode)
        self.assertIn("Developer payload publication is not implemented", result.stderr)

    @unittest.skipUnless(shutil.which("powershell.exe") or shutil.which("pwsh"), "PowerShell is required")
    def test_installer_is_complete_and_conflict_safe(self):
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        with tempfile.TemporaryDirectory() as temp_value:
            temp_root = Path(temp_value)
            seed = temp_root / "seed"
            revision = self._write_project_checkout(seed)
            release = temp_root / "release"
            MODULE.compose(REPO_ROOT, release, "installer-test", ["ProjectWorldData"], 1, True, revision, "main")
            subprocess.run(["git", "tag", "installer-test"], cwd=seed, check=True)
            manifest_path = next(release.glob("*.developer-payload.json"))
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(2, manifest["schema_version"])
            self.assertEqual(revision, manifest["public_source"]["revision"])
            self.assertGreater(len(manifest["archive"]["parts"]), 1)
            project = temp_root / "project"
            subprocess.run(["git", "clone", "-q", str(seed), str(project)], check=True)
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(project / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(project), "-ReleaseDir", str(release),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertTrue(all((project / Path(entry["path"])).is_file() for entry in manifest["entries"]))

            conflict = temp_root / "conflict"
            self._write_project_checkout(conflict)
            bad_path = conflict / Path(manifest["entries"][0]["path"])
            bad_path.parent.mkdir(parents=True, exist_ok=True)
            bad_path.write_bytes(b"conflict")
            subprocess.run(["git", "add", "."], cwd=conflict, check=True)
            subprocess.run(["git", "commit", "-q", "-m", "conflicting public source"], cwd=conflict, check=True)
            conflict_revision = subprocess.run(
                ["git", "rev-parse", "HEAD"], cwd=conflict, check=True, capture_output=True, text=True
            ).stdout.strip()
            subprocess.run(["git", "tag", "conflict-test"], cwd=conflict, check=True)
            conflict_release = temp_root / "conflict-release"
            MODULE.compose(
                REPO_ROOT, conflict_release, "conflict-test", ["ProjectWorldData"], 1700,
                True, conflict_revision, "main",
            )
            conflict_manifest = json.loads(next(conflict_release.glob("*.developer-payload.json")).read_text())
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(conflict / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(conflict), "-ReleaseDir", str(conflict_release),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertNotEqual(0, result.returncode)
            present = [entry for entry in conflict_manifest["entries"] if (conflict / Path(entry["path"])).exists()]
            self.assertEqual(1, len(present))

    @unittest.skipUnless(shutil.which("powershell.exe") or shutil.which("pwsh"), "PowerShell is required")
    def test_installer_rejects_wrong_public_revision_before_copy(self):
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            project = root / "project"
            revision = self._write_project_checkout(project)
            subprocess.run(["git", "tag", "revision-test"], cwd=project, check=True)
            release = root / "release"
            MODULE.compose(REPO_ROOT, release, "revision-test", ["ProjectWorldData"], 1700, True, "f" * 40, "main")
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(project / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(project), "-ReleaseDir", str(release),
                ],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertIn(revision, result.stderr + result.stdout)
            manifest = json.loads(next(release.glob("*.developer-payload.json")).read_text())
            self.assertFalse(any((project / Path(entry["path"])).exists() for entry in manifest["entries"]))

    @unittest.skipUnless(shutil.which("powershell.exe") or shutil.which("pwsh"), "PowerShell is required")
    def test_source_installer_never_executes_downloaded_verifier(self):
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            project = root / "project"
            revision = self._write_project_checkout(project)
            subprocess.run(["git", "tag", "trust-test"], cwd=project, check=True)
            release = root / "release"
            MODULE.compose(REPO_ROOT, release, "trust-test", ["ProjectWorldData"], 1700, True, revision, "main")
            marker = root / "downloaded-verifier-executed"
            (release / "VERIFY_RELEASE.ps1").write_text(f"Set-Content -Path '{marker}' -Value bad\n", encoding="utf-8")
            (release / "SHA256SUMS.txt").write_text("invalid\n", encoding="ascii")
            (release / "SHA256SUMS.txt.asc").write_text("invalid\n", encoding="ascii")
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(project / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(project), "-ReleaseDir", str(release), "-RequireReleaseSignature",
                ],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertFalse(marker.exists())

    @unittest.skipUnless(shutil.which("powershell.exe") or shutil.which("pwsh"), "PowerShell is required")
    def test_installer_rejects_manifest_outside_authenticated_release(self):
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            project = root / "project"
            authenticated = root / "authenticated-release"
            outside = root / "outside" / "untrusted.developer-payload.json"
            verifier_marker = root / "trusted-verifier-ran"
            outside.parent.mkdir()
            outside.write_text("not valid json", encoding="ascii")
            marker_value = str(verifier_marker).replace("'", "''")
            verifier = (
                "param([string]$ReleaseDir)\n"
                f"Set-Content -LiteralPath '{marker_value}' -Value verified\n"
                "& $env:ComSpec /c exit 0\n"
            )
            revision = self._write_project_checkout(project, verifier)
            subprocess.run(["git", "tag", "boundary-test"], cwd=project, check=True)
            MODULE.compose(
                REPO_ROOT, authenticated, "boundary-test", ["ProjectWorldData"], 1700,
                True, revision, "main",
            )
            (authenticated / "SHA256SUMS.txt").write_text("test fixture\n", encoding="ascii")
            (authenticated / "SHA256SUMS.txt.asc").write_text("test fixture\n", encoding="ascii")
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(project / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(project), "-ReleaseDir", str(authenticated),
                    "-ManifestPath", str(outside), "-RequireReleaseSignature",
                ],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertTrue(verifier_marker.is_file())
            self.assertIn("ManifestPath must be inside", result.stderr + result.stdout)
            manifest = json.loads(next(authenticated.glob("*.developer-payload.json")).read_text())
            self.assertFalse(any((project / Path(entry["path"])).exists() for entry in manifest["entries"]))

    @unittest.skipUnless(shutil.which("powershell.exe") or shutil.which("pwsh"), "PowerShell is required")
    def test_installer_rejects_missing_public_tag_before_copy(self):
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            project = root / "project"
            revision = self._write_project_checkout(project)
            release = root / "release"
            MODULE.compose(REPO_ROOT, release, "missing-tag", ["ProjectWorldData"], 1700, True, revision, "main")
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(project / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(project), "-ReleaseDir", str(release),
                ],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertIn("missing-tag", result.stderr + result.stdout)
            manifest = json.loads(next(release.glob("*.developer-payload.json")).read_text())
            self.assertFalse(any((project / Path(entry["path"])).exists() for entry in manifest["entries"]))

    @staticmethod
    def _write_project_checkout(project: Path, verifier_text: str | None = None) -> str:
        project.mkdir(parents=True)
        shutil.copy2(REPO_ROOT / "Alis.uproject", project / "Alis.uproject")
        plugin = project / "Plugins" / "World" / "ProjectWorldData"
        plugin.mkdir(parents=True)
        shutil.copy2(
            REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "ProjectWorldData.uplugin",
            plugin / "ProjectWorldData.uplugin",
        )
        object_plugin = project / "Plugins" / "Resources" / "ProjectObject"
        object_plugin.mkdir(parents=True)
        shutil.copy2(
            REPO_ROOT / "Plugins" / "Resources" / "ProjectObject" / "ProjectObject.uplugin",
            object_plugin / "ProjectObject.uplugin",
        )
        mirror = project / "scripts/git/mirror"
        package = project / "scripts/ue/package"
        mirror.mkdir(parents=True)
        package.mkdir(parents=True)
        shutil.copy2(REPO_ROOT / "scripts/git/mirror/install_developer_payload.ps1", mirror)
        if verifier_text is None:
            shutil.copy2(REPO_ROOT / "scripts/ue/package/verify_release.ps1", package)
        else:
            (package / "verify_release.ps1").write_text(verifier_text, encoding="ascii")
        subprocess.run(["git", "init", "-q"], cwd=project, check=True)
        subprocess.run(["git", "config", "core.autocrlf", "false"], cwd=project, check=True)
        subprocess.run(["git", "config", "user.name", "payload-test"], cwd=project, check=True)
        subprocess.run(["git", "config", "user.email", "payload-test@localhost"], cwd=project, check=True)
        subprocess.run(["git", "add", "."], cwd=project, check=True)
        subprocess.run(["git", "commit", "-q", "-m", "public source fixture"], cwd=project, check=True)
        return subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=project, check=True, capture_output=True, text=True
        ).stdout.strip()



if __name__ == "__main__":
    unittest.main()
