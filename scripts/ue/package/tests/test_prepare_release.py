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
sys.path.insert(0, str(SCRIPT.parent))
WRAPPER = SCRIPT.with_suffix(".ps1")
TEST_TMP = SCRIPT.parents[3] / "tmp/release/tests"
SPEC = importlib.util.spec_from_file_location("prepare_release", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
REFRESH_SCRIPT = SCRIPT.with_name("refresh_github_release.py")
REFRESH_SPEC = importlib.util.spec_from_file_location("refresh_github_release", REFRESH_SCRIPT)
REFRESH = importlib.util.module_from_spec(REFRESH_SPEC)
assert REFRESH_SPEC.loader
REFRESH_SPEC.loader.exec_module(REFRESH)


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
        TEST_TMP.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=TEST_TMP)
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

    def test_game_only_approval_records_bounded_scope(self) -> None:
        output = self.root / "release"
        manifest_path = MODULE.prepare_release(
            self.inputs(), output, [self.player_archive], self.archive_report
        )

        MODULE.approve_release(output, approve=True, approval_scope="game")

        approved = json.loads(manifest_path.read_text(encoding="utf-8"))
        rights = json.loads((output / "release-rights-review.json").read_text(encoding="utf-8"))
        self.assertEqual("game", approved["approval_scope"])
        self.assertEqual("game", rights["approval_scope"])
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
        self.assertIn("docs/quickstart/developer/README.md", root_readme)
        self.assertIn("Product terms: PRODUCT_TERMS.txt", root_readme)
        self.assertIn("Data and third-party notices: developer.notices.json", root_readme)
        self.assertNotIn("docs/quickstart/player/README.md", root_readme)
        self.assertNotIn("README_PLAYER.txt", root_readme)
        self.assertNotIn("README_DEVELOPER.txt", root_readme)
        self.assertNotIn("developer source", root_readme.lower())

        manifest = json.loads(
            (output / "release_manifest.json").read_text(encoding="utf-8")
        )
        artifact_names = {item["name"] for item in manifest["artifacts"]}
        self.assertEqual(
            "https://github.com/fallintodusk/alis/blob/v2.0.0/docs/quickstart/developer/README.md",
            manifest["release_documents"]["developer_guide_url"],
        )
        self.assertIn("README.txt", artifact_names)
        self.assertIn(self.player_archive.name, artifact_names)
        self.assertIn("developer.zip", artifact_names)
        self.assertTrue(all(Path(name).name == name for name in artifact_names))

        player_installer = (output / "INSTALL_ALIS_PLAYER.ps1").read_text(encoding="utf-8")
        self.assertIn(self.player_archive.name, player_installer)
        self.assertIn(MODULE.sha256_file(self.player_archive), player_installer)
        self.assertNotIn("__ALIS_PLAYER_ARCHIVE_MANIFEST_JSON__", player_installer)

    def waived_machine(self) -> Path:
        aggregate = {
            "schema_version": 1, "status": "rejected", "operation_id": "fixture-operation",
            "source_revision": self.private_revision,
            "source_state_sha256": MODULE.source_state_digest(self.private),
            "runtime_profile_sha256": "a" * 64,
            "development_executable_sha256": "b" * 64,
            "development_package_sha256": "c" * 64,
            "execution_count": 3, "children": [{}, {}, {}], "total_sample_count": 900,
            "base_frame_p95_budget_ms": 16.67, "frame_p95_budget_ms": 18.337,
            "frame_p95_ms": 24.35, "streaming_failures": 0,
            "acceptance_reason": "Pooled Frame p95 exceeded the budget.",
        }
        kazan = self.root / "kazan-performance.json"
        manhattan = self.root / "manhattan-performance.json"
        write_json(kazan, aggregate)
        write_json(manhattan, {"performance": aggregate})
        machine = {
            "schema_version": 1, "status": "accepted_with_performance_waiver",
            "operation_id": "fixture-operation", "revision": self.private_revision,
            "source_state_sha256": MODULE.source_state_digest(self.private),
            "runtime_profile_sha256": "a" * 64,
            "development_executable_sha256": "b" * 64,
            "development_package_sha256": "c" * 64,
            "final_package": self.candidate.as_posix(),
            "shipping_package_sha256": MODULE.package_tree_digest(self.candidate),
            "shipping_executable_sha256": MODULE.sha256_file(
                self.candidate / "Windows/Alis/Binaries/Win64/Alis-Win64-Shipping.exe"),
            "artifacts": {"development_performance_aggregate": kazan.as_posix(),
                          "manhattan_development_performance": manhattan.as_posix()},
            "artifact_sha256": {"development_performance_aggregate": MODULE.sha256_file(kazan),
                                "manhattan_development_performance": MODULE.sha256_file(manhattan)},
        }
        from release_performance import performance_decision, validate_machine_performance
        nested = {}
        for index in range(1, 4):
            for suffix in ("correctness", "product_screenshot", "performance", "samples",
                           "csv", "playable_screenshot", "log"):
                key = f"run-{index:02d}_{suffix}"
                path = self.root / "manhattan" / key
                path.parent.mkdir(exist_ok=True)
                path.write_text("fixture evidence", encoding="ascii")
                nested[key] = path.as_posix()
        write_json(manhattan, {"performance": aggregate, "runtime_profile_sha256": "a" * 64,
                              "performance_review": performance_decision(aggregate, True),
                              "artifacts": nested,
                              "artifact_sha256": {key: MODULE.sha256_file(Path(path)) for key, path in nested.items()}})
        machine["artifact_sha256"]["manhattan_development_performance"] = MODULE.sha256_file(manhattan)
        machine["performance_review"] = validate_machine_performance(machine, True, self.private, False)
        write_json(self.composite, machine)
        return self.composite

    def test_waiver_requires_explicit_release_acceptance_before_output_creation(self) -> None:
        evidence = self.waived_machine()
        output = self.root / "release"
        with self.assertRaises(MODULE.ReleaseError):
            MODULE.prepare_release(replace(self.inputs(), player_evidence=evidence), output,
                                   [self.player_archive], self.archive_report)
        self.assertFalse(output.exists())

    def test_explicit_waiver_retains_inconclusive_measurement_in_release_manifest(self) -> None:
        evidence = self.waived_machine()
        inputs = SimpleNamespace(**{**vars(self.inputs()), "player_evidence": evidence,
                                   "accept_inconclusive_performance": True})
        output = self.root / "release"
        manifest_path = MODULE.prepare_release(inputs, output, [self.player_archive], self.archive_report)
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual("inconclusive", manifest["performance_review"]["status"])
        self.assertEqual("accepted_residual_risk", manifest["performance_review"]["operator_decision"])
        self.assertEqual("rejected", json.loads((self.root / "kazan-performance.json").read_text())["status"])
        self.assertEqual(self.private_revision, manifest["performance_review"]["source_revision"])

    def test_waiver_refuses_changed_artifact_or_candidate_before_output_creation(self) -> None:
        for mutation in ("artifact_bytes", "source_revision", "runtime_profile_sha256",
                         "development_executable_sha256", "shipping_package_sha256", "decision"):
            with self.subTest(mutation=mutation):
                evidence = self.waived_machine()
                machine = json.loads(evidence.read_text())
                if mutation == "artifact_bytes":
                    with (self.root / "kazan-performance.json").open("a") as stream:
                        stream.write(" ")
                elif mutation == "source_revision":
                    machine["revision"] = "d" * 40
                elif mutation == "decision":
                    machine["performance_review"]["status"] = "accepted"
                else:
                    machine[mutation] = "d" * 64
                write_json(evidence, machine)
                inputs = replace(self.inputs(), player_evidence=evidence, accept_inconclusive_performance=True)
                output = self.root / "release"
                with self.assertRaises(MODULE.ReleaseError):
                    MODULE.prepare_release(inputs, output, [self.player_archive], self.archive_report)
                self.assertFalse(output.exists())

    def test_waiver_cannot_accept_unknown_measurement_or_non_budget_failure(self) -> None:
        from release_performance import performance_decision
        self.waived_machine()
        original = json.loads((self.root / "kazan-performance.json").read_text())
        for key, value in (("frame_p95_ms", None), ("frame_p95_ms", float("nan")),
                           ("status", "accepted"), ("streaming_failures", 1),
                           ("execution_count", 1), ("total_sample_count", 0),
                           ("base_frame_p95_budget_ms", 25), ("frame_p95_budget_ms", 25)):
            with self.subTest(key=key, value=value):
                with self.assertRaises(ValueError):
                    performance_decision({**original, key: value}, True)
        accepted = {**original, "status": "accepted", "frame_p95_ms": 12.96, "acceptance_reason": ""}
        self.assertEqual("accepted", performance_decision(accepted, True)["status"])
        self.assertEqual("not_required", performance_decision(accepted, True)["operator_decision"])

    def test_manifest_verification_preserves_waiver_identity_and_inconclusive_status(self) -> None:
        evidence = self.waived_machine()
        inputs = replace(self.inputs(), player_evidence=evidence, accept_inconclusive_performance=True)
        output = self.root / "release"
        path = MODULE.prepare_release(inputs, output, [self.player_archive], self.archive_report)
        MODULE.verify_release_manifest(output)
        original = json.loads(path.read_text())
        for field, value in (("source_revision", "d" * 40), ("status", "accepted"),
                              ("operator_decision", "not_required")):
            with self.subTest(field=field):
                manifest = json.loads(json.dumps(original))
                manifest["performance_review"][field] = value
                write_json(path, manifest)
                with self.assertRaises(MODULE.ReleaseError):
                    MODULE.verify_release_manifest(output)

    def test_operator_receipt_cannot_hide_waived_machine_acceptance(self) -> None:
        self.waived_machine()
        acceptance = json.loads(self.acceptance.read_text())
        acceptance["release_composite_sha256"] = MODULE.sha256_file(self.composite)
        write_json(self.acceptance, acceptance)
        output = self.root / "release"
        with self.assertRaises(MODULE.ReleaseError):
            MODULE.prepare_release(self.inputs(), output, [self.player_archive], self.archive_report)
        self.assertFalse(output.exists())
        inputs = replace(self.inputs(), accept_inconclusive_performance=True)
        path = MODULE.prepare_release(inputs, output, [self.player_archive], self.archive_report)
        self.assertEqual("inconclusive", json.loads(path.read_text())["performance_review"]["status"])

    def test_waiver_reauthenticates_nested_manhattan_evidence_before_output_creation(self) -> None:
        for mutation in ("changed_samples", "deleted_samples", "missing_inventory"):
            with self.subTest(mutation=mutation):
                evidence = self.waived_machine()
                if mutation == "missing_inventory":
                    path = self.root / "manhattan-performance.json"
                    document = json.loads(path.read_text())
                    del document["artifacts"]
                    del document["artifact_sha256"]
                    write_json(path, document)
                    machine = json.loads(evidence.read_text())
                    digest = MODULE.sha256_file(path)
                    machine["artifact_sha256"]["manhattan_development_performance"] = digest
                    machine["performance_review"]["cities"]["manhattan"]["aggregate_sha256"] = digest
                    write_json(evidence, machine)
                else:
                    path = self.root / "manhattan/run-01_samples"
                    path.unlink() if mutation == "deleted_samples" else path.write_text("changed")
                inputs = replace(self.inputs(), player_evidence=evidence, accept_inconclusive_performance=True)
                output = self.root / ("release-" + mutation)
                with self.assertRaises(MODULE.ReleaseError):
                    MODULE.prepare_release(inputs, output, [self.player_archive], self.archive_report)
                self.assertFalse(output.exists())

    def test_interrupted_waived_projection_refuses_recovery_without_mutation(self) -> None:
        import shutil
        import release_workspace as workspace
        evidence = self.waived_machine()
        inputs = replace(self.inputs(), player_evidence=evidence, accept_inconclusive_performance=True)
        output = self.root / "workspace"
        MODULE.prepare_release(inputs, output / "github", [self.player_archive], self.archive_report)
        workspace.initialize_workspace(output, self.candidate, "2.0.0")
        workspace.adopt_game(output, self.candidate)
        backup = output / ("github-previous-" + "a" * 32)
        (output / "github").rename(backup)
        for current_exists in (False, True):
            with self.subTest(current_exists=current_exists):
                if current_exists:
                    shutil.copytree(output / "github", backup)
                before = {path.relative_to(output).as_posix(): MODULE.sha256_file(path)
                          for path in output.rglob("*") if path.is_file()}
                with self.assertRaises(MODULE.ReleaseError):
                    workspace.recover_github_projection(output)
                self.assertEqual(before, {path.relative_to(output).as_posix(): MODULE.sha256_file(path)
                                         for path in output.rglob("*") if path.is_file()})
                workspace.recover_github_projection(output, accept_inconclusive_performance=True)
                self.assertFalse(backup.exists())
                workspace.verify_workspace(output)

    def test_windows_300_wrapper_assembles_and_resumes_waived_workspace_only_explicitly(self) -> None:
        self.private = SCRIPT.parents[3]
        self.private_revision = MODULE.git_value(self.private, "rev-parse", "HEAD")
        evidence = self.waived_machine()
        subprocess.run(["git", "-C", str(self.public), "tag", "v3.0.0"], check=True)
        for path, key in ((self.developer_manifest, "release_version"), (self.component, "source_tag")):
            document = json.loads(path.read_text())
            document[key] = "v3.0.0"
            if path == self.developer_manifest:
                document["public_source"]["tag"] = "v3.0.0"
            write_json(path, document)
        output = self.root / "release"
        arguments = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(WRAPPER),
                     "-ReleaseVersion", "3.0.0", "-ReleaseDir", str(output),
                     "-PublicSourceRoot", str(self.public), "-PlayerPackageRoot", str(self.candidate),
                     "-PlayerEvidence", str(evidence), "-DeveloperReleaseDir", str(self.developer),
                     "-DeveloperPayloadManifest", str(self.developer_manifest), "-ComponentManifest", str(self.component),
                     "-DependencyReport", str(self.dependency), "-PrivacyReport", str(self.privacy),
                     "-MapLoadReport", str(self.map_load), "-AttributionNotice", str(self.attribution),
                     "-ProductTerms", str(self.terms)]
        refused = subprocess.run(arguments, capture_output=True, text=True)
        self.assertNotEqual(0, refused.returncode)
        self.assertFalse(output.exists())
        accepted = subprocess.run([*arguments, "-AcceptInconclusivePerformance"], capture_output=True, text=True)
        self.assertEqual(0, accepted.returncode, accepted.stdout + accepted.stderr)
        manifest = MODULE.verify_release_manifest(output / "github")
        self.assertEqual("inconclusive", manifest["performance_review"]["status"])
        self.assertFalse((output / "game/linux-x86_64").exists())
        resume = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                  str(SCRIPT.with_name("release.ps1")), "-ReleaseVersion", "3.0.0",
                  "-ReleaseDir", str(output), "-SkipSigning"]
        before = {path.relative_to(output).as_posix(): MODULE.sha256_file(path)
                  for path in output.rglob("*") if path.is_file()}
        refused = subprocess.run(resume, capture_output=True, text=True)
        self.assertNotEqual(0, refused.returncode)
        self.assertIn("requires -AcceptInconclusivePerformance", refused.stdout + refused.stderr)
        self.assertEqual(before, {path.relative_to(output).as_posix(): MODULE.sha256_file(path)
                                 for path in output.rglob("*") if path.is_file()})
        accepted = subprocess.run([*resume, "-AcceptInconclusivePerformance"], capture_output=True, text=True)
        self.assertEqual(0, accepted.returncode, accepted.stdout + accepted.stderr)
        self.assertIn("Absolute performance INCONCLUSIVE", accepted.stdout)
        self.candidate.mkdir()
        (output / "game").rename(self.candidate / "Windows")
        (output / "package_summary.txt").rename(self.candidate / "package_summary.txt")
        state_path = output / "release-workspace.json"
        state = json.loads(state_path.read_text())
        state["status"] = "awaiting_game_adoption"
        write_json(state_path, state)
        resume.extend(["-PlayerPackageRoot", str(self.candidate)])
        refused = subprocess.run(resume, capture_output=True, text=True)
        self.assertNotEqual(0, refused.returncode)
        self.assertIn("requires -AcceptInconclusivePerformance", refused.stdout + refused.stderr)
        self.assertTrue((self.candidate / "Windows").is_dir())
        self.assertEqual("awaiting_game_adoption", json.loads(state_path.read_text())["status"])
        accepted = subprocess.run([*resume, "-AcceptInconclusivePerformance"], capture_output=True, text=True)
        self.assertEqual(0, accepted.returncode, accepted.stdout + accepted.stderr)

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

    def test_wrapper_creates_game_and_github_workspace_from_absent_output(self) -> None:
        self.assert_wrapper_creates_workspace("2.0.0")

    def test_wrapper_creates_windows_3_0_without_linux_inputs(self) -> None:
        self.assert_wrapper_creates_workspace("3.0.0")

    def test_3_0_guide_has_concrete_player_and_developer_highlights(self) -> None:
        output = self.root / "guide"
        output.mkdir()
        (output / "developer.developer-payload.json").write_text("{}")
        (output / "developer.notices.json").write_text("{}")
        MODULE.write_release_guide(
            output, "3.0.0", "v3.0.0",
            {"parts": [{"name": "ALIS_Win64_v3.0.0.zip"}]},
            {"archive": {"parts": [{"name": "developer.zip"}]}}, {},
        )
        guide = (output / "README.txt").read_text(encoding="utf-8")
        self.assertIn("WHAT'S NEW 3.0.0", guide)
        players, developers = guide.split("\nPlayers\n", 1)[1].split(
            "\nDevelopers and contributors\n", 1
        )
        self.assertIn("Kazan and Manhattan", players)
        self.assertIn("Mesh Terrain", players)
        self.assertIn("generated terrain material", players)
        self.assertIn("common World pipeline", developers.split("\nPLAY ON WINDOWS\n", 1)[0])
        self.assertNotIn("See the Git history", guide)

    def test_wrapper_requires_explicit_version_before_output_creation(self) -> None:
        self.assert_wrapper_creates_workspace("2.0.0", omit_version=True)

    def assert_wrapper_creates_workspace(self, version: str, omit_version: bool = False) -> None:
        if version != "2.0.0":
            subprocess.run(["git", "tag", f"v{version}"], cwd=self.public, check=True, capture_output=True)
            developer = MODULE.read_json(self.developer_manifest)
            developer["release_version"] = f"v{version}"
            developer["public_source"]["tag"] = f"v{version}"
            write_json(self.developer_manifest, developer)
            component = MODULE.read_json(self.component)
            component["source_tag"] = f"v{version}"
            write_json(self.component, component)
        project_root = SCRIPT.parents[3]
        release = self.root / "workspace"
        evidence = self.root / "wrapper-machine-acceptance.json"
        write_json(
            evidence,
            {
                "schema_version": 1,
                "status": "accepted",
                "operation_id": "fixture-operation",
                "revision": subprocess.run(
                    ["git", "rev-parse", "HEAD"],
                    cwd=project_root,
                    check=True,
                    capture_output=True,
                    text=True,
                ).stdout.strip(),
                "source_state_sha256": MODULE.source_state_digest(project_root),
                "final_package": self.candidate.as_posix(),
                "shipping_package_sha256": MODULE.package_tree_digest(self.candidate),
                "shipping_executable_sha256": MODULE.sha256_file(
                    self.candidate / "Windows/Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
                ),
            },
        )
        result = subprocess.run(
            [
                "powershell.exe", "-NoLogo", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
                "-File", str(WRAPPER),
                "-ReleaseDir", str(release),
                "-PublicSourceRoot", str(self.public),
                "-PlayerPackageRoot", str(self.candidate),
                "-PlayerEvidence", str(evidence),
                "-DeveloperReleaseDir", str(self.developer),
                "-DeveloperPayloadManifest", str(self.developer_manifest),
                "-ComponentManifest", str(self.component),
                "-DependencyReport", str(self.dependency),
                "-PrivacyReport", str(self.privacy),
                "-MapLoadReport", str(self.map_load),
                "-AttributionNotice", str(self.attribution),
                "-ProductTerms", str(self.terms),
                "-SplitSizeMiB", "1",
            ] + ([] if omit_version else ["-ReleaseVersion", version]),
            cwd=project_root,
            capture_output=True,
            text=True,
        )
        if omit_version:
            self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("ReleaseVersion", result.stderr)
            self.assertFalse(release.exists())
            self.assertEqual(MODULE.read_json(evidence)["shipping_package_sha256"],
                             MODULE.package_tree_digest(self.candidate))
            return
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertTrue((release / "game/Alis/Binaries/Win64/Alis-Win64-Shipping.exe").is_file())
        self.assertTrue((release / "github/release_manifest.json").is_file())
        self.assertEqual("alis-release-workspace-v1", MODULE.read_json(release / "release-workspace.json")["schema"])
        self.assertEqual("alis-release-manifest-v3", MODULE.read_json(release / "github/release_manifest.json")["schema"])
        self.assertEqual(f"v{version}", MODULE.read_json(release / "release-workspace.json")["release_tag"])
        self.assertEqual(f"v{version}", MODULE.read_json(release / "github/release_manifest.json")["release_tag"])
        self.assertFalse((release / "game/linux-x86_64").exists())

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

    def test_signed_game_refreshes_only_player_transport_in_github_projection(self) -> None:
        output = self.root / "github"
        MODULE.prepare_release(self.inputs(), output, [self.player_archive], self.archive_report)
        MODULE.approve_release(output, approve=True)
        old_part = self.player_archive.name

        game = self.root / "game"
        verification = game / "Verification"
        verification.mkdir(parents=True)
        (game / "Alis.exe").write_bytes(b"game")
        (verification / "SHA256SUMS.txt").write_bytes(b"game hashes")
        (verification / "SHA256SUMS.txt.asc").write_bytes(b"game signature")
        archive_dir = self.root / "signed-archive"
        archive_dir.mkdir()
        part = archive_dir / "ALIS_Win64_v2.0.0.zip.001"
        part.write_bytes(b"signed game archive")
        report_path = archive_dir / "player-archive.json"
        write_json(
            report_path,
            {
                "schema": "alis-game-archive-v1",
                "status": "accepted",
                "game_tree_sha256": MODULE.package_tree_digest(game),
                "parts": [{
                    "name": part.name,
                    "byte_size": part.stat().st_size,
                    "sha256": MODULE.sha256_file(part),
                }],
            },
        )

        refreshed = self.root / "refreshed"
        REFRESH.refresh(output, refreshed, game, report_path)
        self.assertFalse((refreshed / old_part).exists())
        self.assertTrue((refreshed / part.name).is_file())
        rendered_installer = (refreshed / "INSTALL_ALIS_PLAYER.ps1").read_text(encoding="utf-8")
        self.assertIn('"schema":"alis-game-archive-v1"', rendered_installer)
        manifest = MODULE.verify_release_manifest(refreshed, require_ready=True)
        self.assertEqual(
            MODULE.sha256_file(game / "Verification/SHA256SUMS.txt.asc"),
            manifest["player_distribution"]["signature_sha256"],
        )

    def test_archive_game_packages_directly_runnable_root(self) -> None:
        game = self.root / "signed-game"
        executable = game / "Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"shipping")
        (game / "Alis.exe").write_bytes(b"launcher")
        output = self.root / "signed-game-archive"

        report_path = MODULE.archive_game(game, output, "2.0.0", 1, None)
        report = json.loads(report_path.read_text(encoding="utf-8"))
        self.assertEqual("alis-game-archive-v1", report["schema"])
        self.assertEqual(MODULE.package_tree_digest(game), report["game_tree_sha256"])
        self.assertGreaterEqual(len(report["parts"]), 1)


if __name__ == "__main__":
    unittest.main()
