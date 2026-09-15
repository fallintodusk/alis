import importlib.util
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "prepare_release.py"
SPEC = importlib.util.spec_from_file_location("prepare_release", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def init_repo(path: Path, tag: str | None = None) -> tuple[str, str]:
    path.mkdir(parents=True)
    subprocess.run(["git", "init", "-q"], cwd=path, check=True)
    subprocess.run(["git", "config", "user.name", "test"], cwd=path, check=True)
    subprocess.run(["git", "config", "user.email", "test@localhost"], cwd=path, check=True)
    (path / "tracked.txt").write_text("tracked\n", encoding="utf-8")
    subprocess.run(["git", "add", "."], cwd=path, check=True)
    subprocess.run(["git", "commit", "-q", "-m", "fixture"], cwd=path, check=True)
    revision = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=path, check=True, capture_output=True, text=True
    ).stdout.strip()
    tree = subprocess.run(
        ["git", "rev-parse", "HEAD^{tree}"], cwd=path, check=True, capture_output=True, text=True
    ).stdout.strip()
    if tag:
        subprocess.run(["git", "tag", tag], cwd=path, check=True)
    return revision, tree


class PrepareReleaseTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.private = self.root / "private"
        self.public = self.root / "public"
        self.private_revision, _ = init_repo(self.private)
        self.public_revision, self.public_tree = init_repo(self.public, "v2.0.0")
        self.candidate = self.root / "candidate"
        executable = self.candidate / "Windows/Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"shipping")
        (self.candidate / "package_summary.txt").write_text("accepted\n", encoding="ascii")
        self.composite = self.root / "composite.json"
        write_json(self.composite, {"schema_version": 1, "status": "accepted"})
        self.acceptance = self.root / "operator-acceptance.json"
        write_json(
            self.acceptance,
            {
                "schema_version": 1,
                "status": "operator_accepted",
                "product_decision": "accepted",
                "package_root": self.candidate.as_posix(),
                "package_tree_sha256": MODULE.package_tree_digest(self.candidate),
                "shipping_executable": executable.as_posix(),
                "shipping_executable_sha256": MODULE.sha256_file(executable),
                "source_revision": self.private_revision,
                "source_state_sha256": MODULE.source_state_digest(self.private),
                "release_operation_id": "fixture-operation",
                "release_composite": self.composite.as_posix(),
                "release_composite_sha256": MODULE.sha256_file(self.composite),
            },
        )
        self.developer = self.root / "developer"
        self.developer.mkdir()
        (self.developer / "developer.zip").write_bytes(b"developer")
        self.payload_id = "fixture-payload"
        self.developer_manifest = self.developer / "developer.developer-payload.json"
        write_json(
            self.developer_manifest,
            {
                "schema_version": 2,
                "release_version": "v2.0.0",
                "payload_id": self.payload_id,
                "public_source": {
                    "tag": "v2.0.0",
                    "revision": self.public_revision,
                    "branch": "main",
                },
                "archive": {
                    "logical_name": "developer.zip",
                    "byte_size": 9,
                    "sha256": MODULE.sha256_file(self.developer / "developer.zip"),
                    "parts": [
                        {
                            "name": "developer.zip",
                            "byte_size": 9,
                            "sha256": MODULE.sha256_file(self.developer / "developer.zip"),
                        }
                    ]
                },
            },
        )
        self.attribution = self.developer / "developer.notices.json"
        write_json(self.attribution, {"schema": "test-attribution", "payload_id": self.payload_id})
        for name in MODULE.DEVELOPER_HELPER_FILES:
            (self.developer / name).write_text(name + "\n", encoding="ascii")
        self.component = self.root / "effective-component-manifest.json"
        write_json(
            self.component,
            {
                "schema": "alis-effective-component-manifest-v1",
                "source_commit": self.public_revision,
                "source_tree": self.public_tree,
                "source_tag": "v2.0.0",
                "entry_count": 1,
                "entries": [{}],
            },
        )
        self.dependency = self.root / "developer-dependency-report.json"
        write_json(
            self.dependency,
            {
                "schema_version": 1,
                "status": "accepted",
                "source_revision": self.public_revision,
                "source_tree": self.public_tree,
                "hlod_package_count": 0,
                "issues": [],
            },
        )
        self.privacy = self.root / "public-source-privacy.json"
        write_json(
            self.privacy,
            {
                "schema": "alis-public-source-privacy-v1",
                "status": "accepted",
                "source_revision": self.public_revision,
                "source_tree": self.public_tree,
                "issue_count": 0,
            },
        )
        self.map_load = self.root / "public-world-map-load.json"
        write_json(
            self.map_load,
            {
                "schema": "alis-public-world-map-load-v1",
                "status": "accepted",
                "maps": [
                    "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory",
                    "/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase",
                ],
            },
        )
        self.terms = self.root / "PRODUCT_TERMS.txt"
        self.terms.write_text("ALIS Product Terms\n", encoding="utf-8")
        self.player_archive = self.root / "ALIS_Win64_v2.0.0.zip"
        self.player_archive.write_bytes(b"player")
        self.archive_report = self.root / "player-archive.json"
        write_json(
            self.archive_report,
            {
                "schema": "alis-player-archive-v1",
                "status": "accepted",
                "package_tree_sha256": MODULE.package_tree_digest(self.candidate),
                "parts": [
                    {
                        "name": self.player_archive.name,
                        "byte_size": self.player_archive.stat().st_size,
                        "sha256": MODULE.sha256_file(self.player_archive),
                    }
                ],
            },
        )

    def tearDown(self) -> None:
        self.temp.cleanup()

    def inputs(self) -> MODULE.ReleaseInputs:
        return MODULE.ReleaseInputs(
            release_version="2.0.0",
            release_tag="v2.0.0",
            private_source_root=self.private,
            public_source_root=self.public,
            player_package_root=self.candidate,
            player_evidence=self.acceptance,
            developer_release_dir=self.developer,
            developer_payload_manifest=self.developer_manifest,
            component_manifest=self.component,
            dependency_report=self.dependency,
            privacy_report=self.privacy,
            map_load_report=self.map_load,
            attribution_notice=self.attribution,
            product_terms=self.terms,
        )

    def test_prepare_and_approve_bind_exact_release(self) -> None:
        output = self.root / "release"
        manifest_path = MODULE.prepare_release(
            self.inputs(), output, [self.player_archive], self.archive_report
        )
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual("pending_owner_approval", manifest["status"])
        self.assertEqual(self.public_revision, manifest["public_source"]["revision"])
        self.assertEqual(self.private_revision, manifest["player_source"]["revision"])

        MODULE.approve_release(output, approve=True)
        approved = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual("ready_for_signature", approved["status"])
        self.assertEqual("accepted", approved["product_review"]["status"])
        self.assertEqual("accepted", approved["rights_review"]["status"])
        self.assertNotIn("identity", json.dumps(approved).lower())

        MODULE.verify_release_manifest(output, require_ready=True)

    def test_machine_accepted_candidate_prepares_reviewable_unsigned_release(self) -> None:
        evidence = self.root / "machine-acceptance.json"
        write_json(
            evidence,
            {
                "schema_version": 1,
                "status": "accepted",
                "operation_id": "fixture-operation",
                "revision": self.private_revision,
                "source_state_sha256": MODULE.source_state_digest(self.private),
                "final_package": self.candidate.as_posix(),
                "shipping_package_sha256": MODULE.package_tree_digest(self.candidate),
                "shipping_executable_sha256": MODULE.sha256_file(
                    self.candidate / "Windows/Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
                ),
            },
        )
        inputs = replace(self.inputs(), player_evidence=evidence)
        output = self.root / "release"

        manifest_path = MODULE.prepare_release(
            inputs, output, [self.player_archive], self.archive_report
        )

        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual("pending_owner_approval", manifest["status"])
        self.assertEqual("pending_owner_approval", manifest["product_review"]["status"])
        self.assertFalse((output / "player-machine-acceptance.json").exists())
        self.assertFalse((output / "player-acceptance.json").exists())

    def test_unsigned_release_is_the_minimal_flat_github_asset_set(self) -> None:
        output = self.root / "release"

        MODULE.prepare_release(
            self.inputs(), output, [self.player_archive], self.archive_report
        )

        self.assertTrue((output / "README.txt").is_file())
        self.assertTrue((output / "INSTALL_ALIS_PLAYER.bat").is_file())
        self.assertTrue((output / "INSTALL_ALIS_PLAYER.ps1").is_file())
        self.assertTrue((output / self.player_archive.name).is_file())
        self.assertTrue((output / "PRODUCT_TERMS.txt").is_file())
        self.assertTrue((output / "INSTALL_ALIS_DEVELOPER.bat").is_file())
        self.assertTrue((output / "INSTALL_ALIS_DEVELOPER.ps1").is_file())
        self.assertTrue((output / "developer.zip").is_file())
        self.assertTrue((output / "developer.developer-payload.json").is_file())
        self.assertTrue((output / "developer.notices.json").is_file())
        self.assertTrue((output / "effective-component-manifest.json").is_file())
        self.assertFalse((output / "INSTALL_ALIS_DEVELOPER_PROJECT.bat").exists())
        self.assertFalse((output / "player-archive.json").exists())
        self.assertFalse((output / "player-machine-acceptance.json").exists())
        self.assertFalse((output / "developer-dependency-report.json").exists())
        self.assertFalse((output / "public-source-privacy.json").exists())
        self.assertFalse((output / "public-world-map-load.json").exists())
        self.assertFalse((output / "LICENSE.txt").exists())
        self.assertFalse((output / "LICENSE_MPL-2.0.txt").exists())
        self.assertFalse(any(path.is_dir() for path in output.iterdir()))

        root_entries = {path.name for path in output.iterdir() if path.is_file()}
        self.assertEqual(
            {
                "README.txt",
                "release_manifest.json",
                "PRODUCT_TERMS.txt",
                self.player_archive.name,
                "INSTALL_ALIS_PLAYER.bat",
                "INSTALL_ALIS_PLAYER.ps1",
                "developer.zip",
                "developer.developer-payload.json",
                "developer.notices.json",
                "effective-component-manifest.json",
                "INSTALL_ALIS_DEVELOPER.bat",
                "INSTALL_ALIS_DEVELOPER.ps1",
            },
            root_entries,
        )

        root_readme = (output / "README.txt").read_text(encoding="ascii")
        self.assertIn("WHAT'S NEW 2.0.0", root_readme)
        self.assertIn("Choose Kazan or Manhattan from the main menu", root_readme)
        self.assertIn("deterministic world pipeline", root_readme)
        self.assertIn("PLAY ON WINDOWS", root_readme)
        self.assertIn("With 7-Zip, extract ALIS_Win64_v2.0.0.zip", root_readme)
        self.assertIn("Optional convenience", root_readme)
        self.assertIn("DEVELOP OR CONTRIBUTE", root_readme)
        self.assertIn("Clone the exact v2.0.0 tag", root_readme)
        self.assertIn("developer/README.md", root_readme)
        self.assertIn("Product terms: PRODUCT_TERMS.txt", root_readme)
        self.assertIn("Data and third-party notices: developer.notices.json", root_readme)
        self.assertNotIn("player/README.md", root_readme)
        self.assertNotIn("README_PLAYER.txt", root_readme)
        self.assertNotIn("README_DEVELOPER.txt", root_readme)
        self.assertNotIn("developer source", root_readme.lower())

        manifest = json.loads(
            (output / "release_manifest.json").read_text(encoding="utf-8")
        )
        artifact_names = {item["name"] for item in manifest["artifacts"]}
        self.assertIn("README.txt", artifact_names)
        self.assertIn(self.player_archive.name, artifact_names)
        self.assertIn("developer.zip", artifact_names)
        self.assertTrue(all(Path(name).name == name for name in artifact_names))

        player_installer = (output / "INSTALL_ALIS_PLAYER.ps1").read_text(encoding="utf-8")
        self.assertIn(self.player_archive.name, player_installer)
        self.assertIn(MODULE.sha256_file(self.player_archive), player_installer)
        self.assertNotIn("__ALIS_PLAYER_ARCHIVE_MANIFEST_JSON__", player_installer)

    def test_prepare_accepts_player_archive_already_in_output(self) -> None:
        output = self.root / "release"
        output.mkdir()
        archive = output / self.player_archive.name
        archive.write_bytes(self.player_archive.read_bytes())
        report = output / "player-archive.json"
        archive_report = json.loads(self.archive_report.read_text(encoding="utf-8"))
        archive_report["parts"][0]["sha256"] = MODULE.sha256_file(archive)
        write_json(report, archive_report)

        manifest = MODULE.prepare_release(self.inputs(), output, [archive], report)

        self.assertTrue(manifest.is_file())
        self.assertTrue((output / archive.name).is_file())
        self.assertFalse(report.exists())

    def test_archive_player_rejects_part_at_github_asset_limit(self) -> None:
        output = self.root / "player-archive"
        original_stat = Path.stat

        def run_7zip(command: list[str], check: bool = False) -> subprocess.CompletedProcess:
            if command[1] == "a":
                (output / "ALIS_Win64_v2.0.0.zip.001").write_bytes(b"part")
            return subprocess.CompletedProcess(command, 0)

        def report_oversized_part(path: Path, *args: object, **kwargs: object) -> object:
            if path.parent == output and path.name.endswith(".001"):
                return SimpleNamespace(st_size=2 * 1024 * 1024 * 1024)
            return original_stat(path, *args, **kwargs)

        with (
            mock.patch.object(MODULE, "resolve_7zip", return_value=sys.executable),
            mock.patch.object(MODULE.subprocess, "run", side_effect=run_7zip),
            mock.patch.object(Path, "stat", report_oversized_part),
            self.assertRaisesRegex(MODULE.ReleaseError, "GitHub release asset limit"),
        ):
            MODULE.archive_player(self.candidate, output, "2.0.0", 1900, None)
        self.assertFalse(output.exists())

    def test_dependency_rejection_fails_before_output(self) -> None:
        dependency = json.loads(self.dependency.read_text(encoding="utf-8"))
        dependency["status"] = "rejected"
        dependency["issues"] = [{"code": "missing"}]
        write_json(self.dependency, dependency)
        output = self.root / "release"
        with self.assertRaisesRegex(MODULE.ReleaseError, "dependency report"):
            MODULE.prepare_release(self.inputs(), output, [self.player_archive], self.archive_report)
        self.assertFalse(output.exists())

    def test_privacy_rejection_fails_before_output(self) -> None:
        privacy = json.loads(self.privacy.read_text(encoding="utf-8"))
        privacy["issue_count"] = 1
        write_json(self.privacy, privacy)
        output = self.root / "release"
        with self.assertRaisesRegex(MODULE.ReleaseError, "privacy report"):
            MODULE.prepare_release(self.inputs(), output, [self.player_archive], self.archive_report)
        self.assertFalse(output.exists())

    def test_stale_player_package_is_rejected(self) -> None:
        (self.candidate / "package_summary.txt").write_text("changed\n", encoding="ascii")
        with self.assertRaisesRegex(MODULE.ReleaseError, "package tree"):
            MODULE.prepare_release(
                self.inputs(), self.root / "release", [self.player_archive], self.archive_report
            )

    def test_public_revision_mismatch_is_rejected(self) -> None:
        dependency = json.loads(self.dependency.read_text(encoding="utf-8"))
        dependency["source_revision"] = "0" * 40
        write_json(self.dependency, dependency)
        with self.assertRaisesRegex(MODULE.ReleaseError, "source revision"):
            MODULE.prepare_release(
                self.inputs(), self.root / "release", [self.player_archive], self.archive_report
            )

    def test_corrupted_developer_part_is_rejected_before_output(self) -> None:
        (self.developer / "developer.zip").write_bytes(b"corrupted")
        output = self.root / "release"
        with self.assertRaisesRegex(MODULE.ReleaseError, "developer archive part"):
            MODULE.prepare_release(self.inputs(), output, [self.player_archive], self.archive_report)
        self.assertFalse(output.exists())

    def test_unlisted_developer_file_is_rejected_before_output(self) -> None:
        (self.developer / "private.bin").write_bytes(b"private")
        output = self.root / "release"
        with self.assertRaisesRegex(MODULE.ReleaseError, "Developer release inventory mismatch"):
            MODULE.prepare_release(self.inputs(), output, [self.player_archive], self.archive_report)
        self.assertFalse(output.exists())

    def test_unlisted_release_file_blocks_approval(self) -> None:
        output = self.root / "release"
        MODULE.prepare_release(self.inputs(), output, [self.player_archive], self.archive_report)
        (output / "private.bin").write_bytes(b"private")
        with self.assertRaisesRegex(MODULE.ReleaseError, "Release directory inventory mismatch"):
            MODULE.approve_release(output, approve=True)

    def test_post_prepare_mutation_blocks_approval(self) -> None:
        output = self.root / "release"
        MODULE.prepare_release(self.inputs(), output, [self.player_archive], self.archive_report)
        (output / self.player_archive.name).write_bytes(b"mutated")
        with self.assertRaisesRegex(MODULE.ReleaseError, "mismatch"):
            MODULE.approve_release(output, approve=True)

    def test_approval_requires_explicit_flag(self) -> None:
        output = self.root / "release"
        MODULE.prepare_release(self.inputs(), output, [self.player_archive], self.archive_report)
        with self.assertRaisesRegex(MODULE.ReleaseError, "explicit approval"):
            MODULE.approve_release(output, approve=False)

    def test_source_state_hashes_tracked_and_untracked_binary_bytes(self) -> None:
        repo = self.root / "source-state"
        init_repo(repo)
        (repo / "tracked.txt").write_bytes(b"changed \xe2\x80\x94 bytes\n")
        (repo / "untracked.txt").write_bytes(b"untracked\n")
        initial = MODULE.source_state_digest(repo)

        (repo / "tracked.txt").write_bytes(b"different tracked bytes\n")
        self.assertNotEqual(MODULE.source_state_digest(repo), initial)
        (repo / "tracked.txt").write_bytes(b"changed \xe2\x80\x94 bytes\n")

        (repo / "untracked.txt").write_bytes(b"different untracked bytes\n")
        self.assertNotEqual(MODULE.source_state_digest(repo), initial)

    def test_source_state_is_stable_when_new_file_is_staged(self) -> None:
        repo = self.root / "source-state-staging"
        init_repo(repo)
        (repo / "new.txt").write_bytes(b"same source bytes\n")
        untracked_digest = MODULE.source_state_digest(repo)

        subprocess.run(
            ["git", "-C", str(repo), "add", "--", "new.txt"],
            check=True,
            capture_output=True,
        )

        self.assertEqual(MODULE.source_state_digest(repo), untracked_digest)

    def test_source_state_is_stable_when_same_effective_tree_is_committed(self) -> None:
        repo = self.root / "source-state-commit"
        init_repo(repo)
        (repo / "new.txt").write_bytes(b"same source bytes\n")
        before_commit = MODULE.source_state_digest(repo)

        subprocess.run(
            ["git", "-C", str(repo), "add", "--", "new.txt"],
            check=True,
            capture_output=True,
        )
        subprocess.run(
            ["git", "-C", str(repo), "commit", "-q", "-m", "same effective tree"],
            check=True,
            capture_output=True,
        )

        self.assertEqual(MODULE.source_state_digest(repo), before_commit)


if __name__ == "__main__":
    unittest.main()
