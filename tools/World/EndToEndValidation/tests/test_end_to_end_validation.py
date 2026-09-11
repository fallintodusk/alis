from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[4]
TOOLS_ROOT = REPO_ROOT / "tools"
sys.path.insert(0, str(TOOLS_ROOT))

from World.EndToEndValidation.app.contracts import ValidationFailure, load_profile, profile_path
from World.EndToEndValidation.app import checks, execution
from World.EndToEndValidation.app import package_gate
from World.EndToEndValidation.app.package_gate import _required_map_cooked
from World.EndToEndValidation.app.validation import _determinism, _forbidden_files, _validate_bootstrap, _validate_realization


class EndToEndValidationTests(unittest.TestCase):
    PRESENTATION = {"profile_id": "presentation_v1", "sha256": "profile-hash"}
    RUNTIME = {"profile_id": "runtime_v1", "sha256": "runtime-hash"}
    PROFILE_ROOT = (
        REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
        / "Profiles" / "EndToEndValidation"
    )

    def _write(self, path: Path, value: dict) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(value), encoding="utf-8")

    def _compile_root(self, root: Path, semantic: str = "semantic", terrain_hash: str = "terrain") -> None:
        outputs = [
            {"kind": "compiler_document", "path": "canonical/coverage.json", "sha256": "coverage", "byte_size": 1},
            {"kind": "compiler_document", "path": "canonical/terrain/cell.json", "sha256": terrain_hash, "byte_size": 1},
            {"kind": "compiler_document", "path": "canonical/features/cell.json", "sha256": "features", "byte_size": 1},
            {"kind": "compiler_document", "path": "reports/metrics.json", "sha256": "observational", "byte_size": 1},
        ]
        self._write(root / "compile_result.json", {
            "$schema": "https://alis.world/schemas/world-compiler/compile-result-v1.json",
            "schema_version": 1,
            "status": "accepted",
            "outputs": outputs,
        })
        self._write(root / "canonical" / "coverage.json", {"semantic_hash": semantic})

    def _realization(
        self,
        path: Path,
        fingerprint: str = "same",
        enrolled: bool = False,
        artifact_paths: tuple[str, ...] = ("Content/A.uasset", "Content/B.uasset"),
    ) -> None:
        legs = {"first.json": ("enroll", "1" * 64, "none"), "second.json": ("apply", "2" * 64, "1" * 64)}
        route, active_sha, prior_sha = legs.get(path.name, ("reconstruct", "3" * 64, "2" * 64))
        manifest_root = path.parent / "manifests"
        generation = {"first": 1, "second": 2, "incremental": 3, "clean": 4}.get(path.stem, 5)
        manifest_path = f"scopes/map_test.{generation}.json"
        self._write(manifest_root / manifest_path, {
            "scope_id": "map_test",
            "artifacts": [{"path": value} for value in artifact_paths],
        })
        self._write(Path(f"{path}.manifests.json"), {
            "manifest_root": str(manifest_root),
            "route": route,
            "active_set_sha256": active_sha,
            "prior_active_set_sha256": prior_sha,
            "enrolled": enrolled,
            "scopes": [{
                "scope_id": "map_test",
                "manifest_path": manifest_path,
                "manifest_sha256": "a" * 64,
            }],
        })
        self._write(path, {
            "$schema": "https://alis.world/schemas/world-realization/realization-result-v1.json",
            "schema_version": 1,
            "status": "accepted",
            "duration_seconds": 1.0,
            "semantic_fingerprint": fingerprint,
            "generated_source_bytes": 100,
            "verified_output_count": 10,
            "authored_correction_layer_preserved": True,
            "georeferencing_placement_error_m": 0,
            "presentation_profile": self.PRESENTATION["profile_id"],
            "presentation_profile_sha256": self.PRESENTATION["sha256"],
            "runtime_profile": self.RUNTIME["profile_id"],
            "runtime_profile_sha256": self.RUNTIME["sha256"],
            "runtime_route_collision_probed": True,
            "runtime_collision_probe_count": 2,
            "runtime_route_collision_orientation_probed": True,
            "runtime_collision_orientation_probe_count": 2,
            "runtime_navigation_probed": True,
            "runtime_streaming_policy_probed": True,
            "runtime_nanite_policy_probed": True,
            "runtime_instancing_policy_probed": True,
            "runtime_hlod_policy_probed": True,
            "hlod_proxy_actor_count": 0,
            "hlod_layer_reference_count": 0,
            "hlod_eligible_generated_actor_count": 0,
            "runtime_structural_budgets_passed": True,
            "changes": {
                "road_sections": 2,
                "building_sections": 4,
                "cross_cell_road_shared_boundary_points": 1,
                "updated_landscape_components": 0,
            },
        })

    def test_p0_profile_is_valid(self) -> None:
        profile = load_profile(self.PROFILE_ROOT / "p0.validation.json")
        self.assertEqual("p0", profile["profile_id"])

    def test_validation_profile_accepts_an_explicit_repository_path(self) -> None:
        relative = "Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/p0.validation.json"
        self.assertEqual((REPO_ROOT / relative).resolve(), profile_path(relative))

    def test_common_check_rejects_contract_changes_during_execution(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            with patch.object(checks, "common_contract_hash", side_effect=["a" * 64, "b" * 64]), patch.object(
                checks, "read_json", return_value={}
            ), patch.object(checks, "validate_against"), patch.object(
                checks, "_run"
            ), patch.object(checks, "_run_test_suites", return_value=[]), patch.object(
                checks, "_powershell", return_value="powershell"
            ):
                with self.assertRaises(ValidationFailure) as raised:
                    checks.execute_checks(root, root / "preflight.json")
        self.assertEqual("common_contract_changed_during_check", raised.exception.code)

    def test_representative_profile_is_separate_from_frozen_p0(self) -> None:
        root = self.PROFILE_ROOT
        p0 = load_profile(root / "p0.validation.json")
        representative = load_profile(root / "representative_v1.validation.json")
        self.assertEqual(5, p0["profiles"]["synthetic"]["expected_features"])
        self.assertEqual(1074, p0["profiles"]["kazan"]["expected_features"])
        self.assertEqual("synthetic_representative_v1", representative["profiles"]["synthetic"]["source_profile"])
        self.assertEqual("kazan_representative_v1", representative["profiles"]["kazan"]["source_profile"])
        for profile in (p0, representative):
            for name, settings in profile["profiles"].items():
                contract = execution._presentation_profile_contract(
                    settings["presentation_profile"], settings["world_data_plugin"]
                )
                expected_id = "synthetic_representative_v1" if name == "synthetic" else "kazan_representative_v1"
                self.assertEqual(expected_id, contract["profile_id"])
                self.assertEqual(64, len(contract["sha256"]))
        runtime = execution._runtime_profile_contract(
            representative["profiles"]["kazan"]["runtime_profile"], "ProjectWorldData"
        )
        self.assertEqual("kazan_representative_playable_v1", runtime["profile_id"])
        self.assertEqual(64, len(runtime["sha256"]))
        self.assertNotIn("runtime_profile", representative["profiles"]["synthetic"])

    def test_one_owner_cannot_pin_two_presentation_profiles(self) -> None:
        source = self.PROFILE_ROOT / "p0.validation.json"
        profile = json.loads(source.read_text(encoding="utf-8"))
        duplicate = dict(profile["profiles"]["synthetic"])
        duplicate["map_package"] = "/ProjectWorldTestData/Generated/P0/L_SecondSynthetic"
        profile["profiles"]["synthetic_second"] = duplicate
        data_root = REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Data"
        with tempfile.TemporaryDirectory(dir=data_root) as directory:
            other = Path(directory) / "other.presentation.json"
            self._write(other, {"profile_id": "other_profile"})
            duplicate["presentation_profile"] = other.relative_to(REPO_ROOT).as_posix()
            with tempfile.TemporaryDirectory(dir=self.PROFILE_ROOT) as profile_directory:
                path = Path(profile_directory) / "conflict.validation.json"
                self._write(path, profile)
                with self.assertRaises(ValidationFailure) as raised:
                    load_profile(path)
        self.assertEqual("presentation_profile_owner_conflict", raised.exception.code)

    def test_validation_profile_supports_a_separate_world_data_owner(self) -> None:
        source = self.PROFILE_ROOT / "representative_v1.validation.json"
        profile = json.loads(source.read_text(encoding="utf-8"))
        kazan = profile["profiles"]["kazan"]
        kazan["world_data_plugin"] = "ProjectWorldData"
        kazan["map_package"] = "/ProjectWorldData/Generated/Representative/L_Kazan"
        profile["package"]["required_map"] = kazan["map_package"]
        data_root = REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
        with tempfile.TemporaryDirectory(dir=data_root) as directory:
            owned = Path(directory)
            presentation = owned / "kazan_v1.presentation.json"
            runtime = owned / "kazan_v1.runtime.json"
            source_profile = owned / "kazan_v1.source.json"
            compiler_profile = owned / "kazan_v1.compile.json"
            for path in (presentation, runtime):
                self._write(path, {})
            self._write(source_profile, {"profile_id": kazan["source_profile"]})
            # The compiler document is loaded through the real compiler contract, which
            # requires a resolvable "$schema" and a schema-valid body. Deriving this fixture
            # from the shipped profile keeps it valid as that contract evolves; a hand-rolled
            # stub silently rots into a contract_violation the moment a field is added.
            template_path = (
                REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
                / "Profiles" / "CanonicalCompilation" / "kazan_territory_v1.compile.json"
            )
            template = json.loads(template_path.read_text(encoding="utf-8"))
            schema_path = (template_path.parent / template["$schema"]).resolve()
            template["$schema"] = Path(
                os.path.relpath(schema_path, compiler_profile.parent)).as_posix()
            template["profile_id"] = kazan["compiler_profile"]
            template["world_data_plugin"] = "ProjectWorldData"
            template["source_profile_id"] = kazan["source_profile"]
            template["source_profile"] = source_profile.relative_to(REPO_ROOT).as_posix()
            self._write(compiler_profile, template)
            kazan["presentation_profile"] = presentation.relative_to(REPO_ROOT).as_posix()
            kazan["runtime_profile"] = runtime.relative_to(REPO_ROOT).as_posix()
            kazan["source_profile_path"] = source_profile.relative_to(REPO_ROOT).as_posix()
            kazan["compiler_profile_path"] = compiler_profile.relative_to(REPO_ROOT).as_posix()
            path = owned / "production.validation.json"
            self._write(path, profile)
            accepted = load_profile(path)
        self.assertEqual("ProjectWorldData", accepted["profiles"]["kazan"]["world_data_plugin"])
        roots = execution._world_data_roots("ProjectWorldData")
        self.assertEqual("ProjectWorldData", roots[0].parent.name)

    def test_world_profile_requires_real_owned_paths_and_matching_pair(self) -> None:
        source = self.PROFILE_ROOT / "p0.validation.json"
        profile = json.loads(source.read_text(encoding="utf-8"))
        settings = profile["profiles"]["kazan"]
        settings["world_data_plugin"] = "ProjectWorldData"
        settings["map_package"] = "/ProjectWorldData/Generated/P0/L_Kazan"
        profile["package"]["required_map"] = settings["map_package"]
        data_root = REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
        with tempfile.TemporaryDirectory(dir=data_root) as directory:
            owned = Path(directory)
            presentation = owned / "presentation.json"
            runtime = owned / "runtime.json"
            self._write(presentation, {})
            self._write(runtime, {})
            settings["presentation_profile"] = presentation.relative_to(REPO_ROOT).as_posix()
            settings["runtime_profile"] = runtime.relative_to(REPO_ROOT).as_posix()
            settings.pop("source_profile_path", None)
            settings.pop("compiler_profile_path", None)
            path = owned / "missing-paths.validation.json"
            self._write(path, profile)
            with self.assertRaises(ValidationFailure) as raised:
                load_profile(path)
        self.assertEqual("owned_profile_path_missing", raised.exception.code)

    def test_acceptance_audit_command_is_pinned_to_required_map_owner(self) -> None:
        from World.EndToEndValidation.app import acceptance

        captured: list[str] = []

        def record_run(name: str, command: list[str], logs: Path, timeout: int) -> float:
            captured.extend(command)
            return 0.0

        with patch.object(acceptance, "_run", side_effect=record_run), patch.object(
            acceptance,
            "read_json",
            return_value={"status": "accepted"},
        ):
            acceptance._run_audit(
                "powershell",
                REPO_ROOT / "tmp",
                "pre_package_audit",
                REPO_ROOT / "tmp" / "audit.json",
                "ProjectWorldData",
            )
        owner_index = captured.index("-WorldDataPlugin")
        self.assertEqual("ProjectWorldData", captured[owner_index + 1])

    def test_enrollment_can_inspect_an_explicit_rejected_audit_receipt(self) -> None:
        from World.EndToEndValidation.app import acceptance

        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            receipt = Path(directory) / "audit.json"
            receipt.write_text(json.dumps({"status": "rejected"}), encoding="utf-8")
            failure = ValidationFailure(
                "stage_failed",
                "audit rejected",
                stage="enrollment_pre_audit",
                returncode=1,
            )
            with patch.object(acceptance, "_run", side_effect=failure):
                document = acceptance._run_audit(
                    "powershell",
                    Path(directory),
                    "enrollment_pre_audit",
                    receipt,
                    "ProjectWorldData",
                    allow_rejected_receipt=True,
                )

        self.assertEqual("rejected", document["status"])

    def test_owner_name_substring_cannot_fake_data_root_confinement(self) -> None:
        source = self.PROFILE_ROOT / "p0.validation.json"
        profile = json.loads(source.read_text(encoding="utf-8"))
        settings = profile["profiles"]["kazan"]
        settings["world_data_plugin"] = "ProjectWorldData"
        settings["map_package"] = "/ProjectWorldData/Generated/P0/L_Kazan"
        profile["package"]["required_map"] = settings["map_package"]
        data_root = REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
        with tempfile.TemporaryDirectory(dir=data_root) as owned_directory, tempfile.TemporaryDirectory(
            dir=REPO_ROOT / "tmp"
        ) as outside_directory:
            owned = Path(owned_directory)
            lookalike = Path(outside_directory) / "ProjectWorldData" / "Data" / "fake.source.json"
            presentation = owned / "presentation.json"
            runtime = owned / "runtime.json"
            compiler = owned / "compiler.json"
            for path in (presentation, runtime, compiler, lookalike):
                self._write(path, {})
            settings["presentation_profile"] = presentation.relative_to(REPO_ROOT).as_posix()
            settings["runtime_profile"] = runtime.relative_to(REPO_ROOT).as_posix()
            settings["source_profile_path"] = lookalike.relative_to(REPO_ROOT).as_posix()
            settings["compiler_profile_path"] = compiler.relative_to(REPO_ROOT).as_posix()
            validation = owned / "lookalike.validation.json"
            self._write(validation, profile)
            with self.assertRaises(ValidationFailure) as raised:
                load_profile(validation)
        self.assertEqual("world_data_owner_mismatch", raised.exception.code)

    def test_realization_evidence_and_generated_material_stay_in_owned_lifecycles(self) -> None:
        evidence = execution._realization_evidence_path("run-identity", "kazan")
        self.assertEqual(
            Path("run-identity/kazan/unreal_emitted.json"),
            evidence.relative_to(execution.REALIZATION_EVIDENCE_ROOT),
        )
        with self.assertRaises(ValidationFailure):
            execution._realization_evidence_path("../escape", "kazan")

        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            content = root / "Content"
            presentation = content / "Generated" / "Presentation"
            map_path = content / "Generated" / "Representative" / "L_Test.umap"
            map_path.parent.mkdir(parents=True)
            presentation.mkdir(parents=True)
            map_path.write_bytes(b"original-map")
            (presentation / "MI_Test.uasset").write_bytes(b"original-material")
            moves = execution._backup_generated(
                ["/ProjectWorldTestData/Generated/Representative/L_Test"],
                root / "backup", content, presentation, "ProjectWorldTestData",
            )
            map_path.parent.mkdir(parents=True, exist_ok=True)
            presentation.mkdir(parents=True, exist_ok=True)
            map_path.write_bytes(b"replacement-map")
            (presentation / "MI_Test.uasset").write_bytes(b"replacement-material")
            execution._restore_generated(
                moves,
                ["/ProjectWorldTestData/Generated/Representative/L_Test"],
                content, presentation, "ProjectWorldTestData",
            )
            self.assertEqual(b"original-map", map_path.read_bytes())
            self.assertEqual(b"original-material", (presentation / "MI_Test.uasset").read_bytes())
            self.assertTrue((root / "backup" / "Generated" / "Representative" / "L_Test.umap").is_file())
            self.assertTrue((root / "backup" / "Generated" / "Presentation" / "MI_Test.uasset").is_file())

    def test_map_backup_preserves_same_prefix_siblings(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            content = root / "Content"
            map_root = content / "Generated" / "Representative"
            map_root.mkdir(parents=True)
            owned = (
                map_root / "L_Test.umap",
                map_root / "L_Test_BuiltData.uasset",
                map_root / "L_Test_HLODLayer_Merged.uasset",
            )
            siblings = (
                map_root / "L_TestNight.umap",
                map_root / "L_Test_Blockout.umap",
            )
            for path in (*owned, *siblings):
                path.write_bytes(path.name.encode("ascii"))

            execution._backup_maps(
                ["/ProjectWorldTestData/Generated/Representative/L_Test"],
                root / "backup", content, "ProjectWorldTestData",
            )

            self.assertTrue(all(not path.exists() for path in owned))
            self.assertTrue(all(path.is_file() for path in siblings))

    def test_package_command_pins_the_profile_required_map(self) -> None:
        required_map = "/ProjectWorldTestData/Generated/Test/L_RequiredCookMap"
        command = package_gate._package_command(
            "powershell",
            REPO_ROOT / "tmp" / "package",
            required_map,
        )
        self.assertEqual("-RequiredCookMap", command[-2])
        self.assertEqual(required_map, command[-1])

    def test_d0_d1_and_d2_accept_equal_isolated_outputs(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            first = Path(directory) / "first"
            second = Path(directory) / "second"
            self._compile_root(first)
            self._compile_root(second)
            result = _determinism(first, second)
            self.assertEqual("semantic", result["D0"])
            self.assertEqual(2, result["D2_artifact_count"])

    def test_d2_change_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            first = Path(directory) / "first"
            second = Path(directory) / "second"
            self._compile_root(first)
            self._compile_root(second, terrain_hash="changed")
            with self.assertRaises(ValidationFailure) as raised:
                _determinism(first, second)
            self.assertEqual("d1_hash_mismatch", raised.exception.code)

    def test_d3_accepts_semantic_identity_not_uasset_bytes(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            first = Path(directory) / "first.json"
            second = Path(directory) / "second.json"
            clean = Path(directory) / "clean.json"
            self._realization(first, enrolled=True)
            self._realization(second)
            self._realization(clean)
            result = _validate_realization(
                first,
                second,
                clean,
                {"expected_road_fragments": 2, "expected_buildings": 4, "require_landscape": True},
                self.PRESENTATION,
                180.0,
            )
            self.assertEqual("same", result["D3"])

    def test_d3_semantic_change_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            first = Path(directory) / "first.json"
            second = Path(directory) / "second.json"
            clean = Path(directory) / "clean.json"
            self._realization(first, enrolled=True)
            self._realization(second, fingerprint="changed")
            self._realization(clean)
            with self.assertRaises(ValidationFailure) as raised:
                _validate_realization(
                    first,
                    second,
                    clean,
                    {"expected_road_fragments": 2, "expected_buildings": 4, "require_landscape": True},
                    self.PRESENTATION,
                    180.0,
                )
            self.assertEqual("d3_mismatch", raised.exception.code)

    def test_d3_clean_rebuild_change_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            first = Path(directory) / "first.json"
            second = Path(directory) / "second.json"
            clean = Path(directory) / "clean.json"
            self._realization(first, enrolled=True)
            self._realization(second)
            self._realization(clean, fingerprint="volatile")
            with self.assertRaises(ValidationFailure) as raised:
                _validate_realization(
                    first,
                    second,
                    clean,
                    {"expected_road_fragments": 2, "expected_buildings": 4, "require_landscape": True},
                    self.PRESENTATION,
                    180.0,
                )
            self.assertEqual("d3_mismatch", raised.exception.code)

    def test_d3_generated_package_path_churn_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            first = root / "first.json"
            second = root / "second.json"
            clean = root / "clean.json"
            self._realization(first, enrolled=True)
            self._realization(second)
            self._realization(clean, artifact_paths=("Content/A.uasset", "Content/C.uasset"))
            with self.assertRaises(ValidationFailure) as raised:
                _validate_realization(
                    first,
                    second,
                    clean,
                    {"expected_road_fragments": 2, "expected_buildings": 4, "require_landscape": True},
                    self.PRESENTATION,
                    180.0,
                )
            self.assertEqual("generated_package_path_churn", raised.exception.code)

    def test_d3_rejects_evidence_from_an_unpinned_presentation_profile(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            first = root / "first.json"
            second = root / "second.json"
            clean = root / "clean.json"
            self._realization(first, enrolled=True)
            self._realization(second)
            self._realization(clean)
            with self.assertRaises(ValidationFailure) as raised:
                _validate_realization(
                    first,
                    second,
                    clean,
                    {"expected_road_fragments": 2, "expected_buildings": 4, "require_landscape": True},
                    {"profile_id": "other", "sha256": "other-hash"},
                    180.0,
                )
            self.assertEqual("presentation_profile_mismatch", raised.exception.code)

    def test_d3_rejects_evidence_from_an_unpinned_runtime_profile(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            paths = [root / name for name in ("first.json", "second.json", "clean.json")]
            for path in paths:
                self._realization(path, enrolled=(path.name == "first.json"))
            with self.assertRaises(ValidationFailure) as raised:
                _validate_realization(
                    *paths,
                    {"expected_road_fragments": 2, "expected_buildings": 4, "require_landscape": True},
                    self.PRESENTATION,
                    180.0,
                    {"profile_id": "other", "sha256": "other-hash"},
                )
            self.assertEqual("runtime_profile_mismatch", raised.exception.code)

    def test_d3_rejects_runtime_evidence_without_executable_policy_proof(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            paths = [root / name for name in ("first.json", "second.json", "clean.json")]
            for path in paths:
                self._realization(path, enrolled=(path.name == "first.json"))
            evidence = json.loads(paths[1].read_text(encoding="utf-8"))
            evidence["runtime_hlod_policy_probed"] = False
            self._write(paths[1], evidence)
            with self.assertRaises(ValidationFailure) as raised:
                _validate_realization(
                    *paths,
                    {"expected_road_fragments": 2, "expected_buildings": 4, "require_landscape": True},
                    self.PRESENTATION,
                    180.0,
                    self.RUNTIME,
                )
            self.assertEqual("runtime_route_unproven", raised.exception.code)

    def test_acceptance_chain_requires_full_evidence_when_accepted(self) -> None:
        from World.EndToEndValidation.app.contracts import validate_against

        base = {
            "$schema": "https://alis.world/schemas/world-validation/acceptance-chain-v1.json",
            "schema_version": 1,
            "operation_id": "accept-20260807T151235Z",
            "status": "rejected",
            "e2e_runs": {},
            "errors": [{"code": "durable_authority_rejected", "message": "audit refused"}],
        }
        # A rejected chain is a valid document: a refusal must leave evidence.
        validate_against(base, "acceptance-chain.schema.json")
        # An accepted chain missing its audits/package/gate is NOT.
        accepted = dict(base, status="accepted", errors=[])
        with self.assertRaises(ValidationFailure):
            validate_against(accepted, "acceptance-chain.schema.json")

    def test_acceptance_refuses_a_run_supplied_as_the_wrong_profile(self) -> None:
        from World.EndToEndValidation.app import acceptance

        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            run_dir = root / "run-20260101T000000Z"
            common_path = root / "check-20260101T000000Z" / "result.json"
            validation_path = self.PROFILE_ROOT / "representative_v1.validation.json"
            contract_hash = acceptance.common_contract_hash()
            self._write(common_path, {
                "$schema": "https://alis.world/schemas/world-validation/validation-result-v1.json",
                "schema_version": 1,
                "operation_id": "check:common:20260101T000000Z",
                "operation": "world_pipeline_validation",
                "status": "accepted",
                "profile_id": "common_checks",
                "checks": [],
                "metrics": {},
                "evidence": {"common_contract_sha256": contract_hash},
                "errors": [],
            })

            def matrix_result(profile_id: str, profile_path_value: Path) -> dict:
                return {
                    "status": "accepted",
                    "profile_id": profile_id,
                    "evidence": {
                        "profiles": {},
                        "validation_profile": {
                            "path": profile_path_value.relative_to(REPO_ROOT).as_posix(),
                            "sha256": acceptance.file_hash(profile_path_value),
                        },
                        "profile_input_contract": acceptance.validation_profile_contract(
                            profile_path_value
                        ),
                        "common_checks": {
                            "receipt": str(common_path),
                            "receipt_sha256": acceptance.file_hash(common_path),
                            "common_contract_sha256": contract_hash,
                        },
                    },
                }
            self._write(run_dir / "result.json", matrix_result("representative_v1", validation_path))
            wrong_dir = root / "run-20260101T000001Z"
            self._write(wrong_dir / "result.json", matrix_result("p0", common_path))
            wrong_identity_dir = root / "run-20260101T000002Z"
            self._write(wrong_identity_dir / "result.json", matrix_result("p0", validation_path))
            with patch.object(acceptance, "PIPELINE_ROOT", root):
                # Correctly typed: accepted.
                evidence = acceptance.accepted_run_evidence("run-20260101T000000Z", "representative_v1")
                self.assertEqual(64, len(evidence["result_sha256"]))
                # Same run passed as the p0 argument: refused, not relabelled.
                with self.assertRaises(ValidationFailure) as raised:
                    acceptance.accepted_run_evidence("run-20260101T000000Z", "p0")
                self.assertEqual("e2e_run_profile_mismatch", raised.exception.code)
                with self.assertRaises(ValidationFailure) as raised:
                    acceptance.accepted_run_evidence("run-20260101T000001Z", "p0")
                self.assertEqual("validation_profile_evidence_invalid", raised.exception.code)
                with self.assertRaises(ValidationFailure) as raised:
                    acceptance.accepted_run_evidence("run-20260101T000002Z", "p0")
                self.assertEqual("validation_profile_identity_mismatch", raised.exception.code)
                common_path.write_text("{}", encoding="utf-8")
                with self.assertRaises(ValidationFailure) as raised:
                    acceptance.accepted_run_evidence("run-20260101T000000Z", "representative_v1")
                self.assertEqual("common_checks_evidence_changed", raised.exception.code)

    LEG_RECORDS = {
        "/ProjectWorldData/Generated/P0/L_ProjectWorldKazan": {
            "compile_result_sha256": "a" * 64,
            "presentation_profile_sha256": "e" * 64,
            "runtime_profile_sha256": None,
        },
        "/ProjectWorldTestData/Generated/Representative/L_ProjectWorldSynthetic_Representative": {
            "compile_result_sha256": "d" * 64,
            "presentation_profile_sha256": "e" * 64,
            "runtime_profile_sha256": None,
        },
    }

    def _map_scope(self, scope_id: str, map_package: str, **identity: str) -> dict:
        base = {"compile_result_sha256": "none", "presentation_profile_sha256": "none",
                "runtime_profile_sha256": "none", "map_package": map_package}
        base.update(identity)
        return {"scope_id": scope_id, "owning_layer": "map", "input_identity": base}

    def test_manifest_provenance_binds_each_scope_to_its_own_leg(self) -> None:
        from World.EndToEndValidation.app.acceptance import verify_manifest_provenance

        good = [self._map_scope(
            "map_p0_kazan", "/ProjectWorldData/Generated/P0/L_ProjectWorldKazan",
            compile_result_sha256="a" * 64, presentation_profile_sha256="e" * 64)]
        self.assertEqual([], verify_manifest_provenance(good, self.LEG_RECORDS))

        # SABOTAGE: P0 Kazan carries a compile hash that is perfectly VALID -
        # it belongs to the representative synthetic leg. A union-of-hashes
        # check passes this; binding by map_package must reject it.
        laundered = [self._map_scope(
            "map_p0_kazan", "/ProjectWorldData/Generated/P0/L_ProjectWorldKazan",
            compile_result_sha256="d" * 64, presentation_profile_sha256="e" * 64)]
        problems = verify_manifest_provenance(laundered, self.LEG_RECORDS)
        self.assertEqual(1, len(problems))
        self.assertIn("compile_result_sha256", problems[0])

        # A map nobody gated is refused rather than ignored.
        unknown = [self._map_scope("map_ghost", "/ProjectWorldTestData/Generated/P0/L_Ghost",
                                   compile_result_sha256="a" * 64)]
        self.assertIn("no accepted leg produced", verify_manifest_provenance(unknown, self.LEG_RECORDS)[0])

    def test_presentation_scope_provenance_comes_from_its_consumers(self) -> None:
        from World.EndToEndValidation.app.acceptance import verify_manifest_provenance

        consumer = self._map_scope(
            "map_p0_kazan", "/ProjectWorldData/Generated/P0/L_ProjectWorldKazan",
            compile_result_sha256="a" * 64, presentation_profile_sha256="e" * 64)
        presentation = {
            "scope_id": "presentation_x",
            "owning_layer": "presentation",
            "consumer_references": ["map_p0_kazan"],
            "input_identity": {"compile_result_sha256": "none", "runtime_profile_sha256": "none",
                               "map_package": "shared", "presentation_profile_sha256": "e" * 64},
        }
        self.assertEqual([], verify_manifest_provenance([consumer, presentation], self.LEG_RECORDS))

        # A presentation digest nobody's consumers were gated with is refused,
        # even though the sentinels around it are legitimate.
        drifted = dict(presentation, input_identity=dict(presentation["input_identity"],
                                                         presentation_profile_sha256="f" * 64))
        problems = verify_manifest_provenance([consumer, drifted], self.LEG_RECORDS)
        self.assertEqual(1, len(problems))
        self.assertIn("presentation_profile_sha256", problems[0])

    def test_acceptance_rejects_a_realization_receipt_mutated_after_the_run_was_accepted(self) -> None:
        from World.EndToEndValidation.app.acceptance import gated_leg_records
        from World.EndToEndValidation.app.contracts import file_hash

        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            receipt = root / "unreal_first.json"
            self._write(receipt, {
                "map_package": "/ProjectWorldData/Generated/P0/L_ProjectWorldKazan",
                "input_sha256": "a" * 64,
                "presentation_profile_sha256": "e" * 64,
            })
            run = {"run_id": "run-x", "document": {"evidence": {"profiles": {"kazan": {
                "unreal_first": str(receipt),
                "unreal_first_sha256": file_hash(receipt),
            }}}}}
            records = gated_leg_records([run])
            self.assertEqual("a" * 64,
                             records["/ProjectWorldData/Generated/P0/L_ProjectWorldKazan"]["compile_result_sha256"])

            # SABOTAGE: mutate the child receipt while the parent run document
            # (and its own hash) stay untouched. Following the path and
            # trusting today's bytes would launder this into provenance.
            self._write(receipt, {
                "map_package": "/ProjectWorldData/Generated/P0/L_ProjectWorldKazan",
                "input_sha256": "c" * 64,
                "presentation_profile_sha256": "e" * 64,
            })
            with self.assertRaises(ValidationFailure) as raised:
                gated_leg_records([run])
            self.assertEqual("e2e_realization_evidence_tampered", raised.exception.code)

            # An accepted run that never pinned its children is refused too.
            unpinned = {"run_id": "run-y", "document": {"evidence": {"profiles": {"kazan": {
                "unreal_first": str(receipt)}}}}}
            with self.assertRaises(ValidationFailure) as raised:
                gated_leg_records([unpinned])
            self.assertEqual("e2e_evidence_unauthenticated", raised.exception.code)

    def test_content_mutation_lock_owns_lifecycle_and_delegates_by_token(self) -> None:
        import os

        # Point at a private lock file: asserting on the REAL project-global
        # lock would fail whenever a legitimate world operation is running,
        # which is a flaky test rather than a real signal.
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            lock_path = Path(directory) / "content_mutation.lock"
            with patch.object(execution, "CONTENT_LOCK_PATH", lock_path):
                self.assertNotIn(execution.CONTENT_LOCK_TOKEN_ENV, os.environ)
                with execution._content_mutation_lock() as token:
                    self.assertEqual(token, os.environ.get(execution.CONTENT_LOCK_TOKEN_ENV))
                    self.assertEqual(token, lock_path.read_text(encoding="ascii"))
                    with self.assertRaises(ValidationFailure) as raised:
                        with execution._content_mutation_lock():
                            self.fail("A second owner must never acquire the held content lock")
                    self.assertEqual("content_lock_unavailable", raised.exception.code)
                self.assertNotIn(execution.CONTENT_LOCK_TOKEN_ENV, os.environ)

    def test_d3_rejects_runtime_evidence_without_collision_orientation_proof(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            paths = [root / name for name in ("first.json", "second.json", "clean.json")]
            for path in paths:
                self._realization(path, enrolled=(path.name == "first.json"))
            evidence = json.loads(paths[2].read_text(encoding="utf-8"))
            evidence["runtime_route_collision_orientation_probed"] = False
            evidence["runtime_collision_orientation_probe_count"] = 1
            self._write(paths[2], evidence)
            with self.assertRaises(ValidationFailure) as raised:
                _validate_realization(
                    *paths,
                    {"expected_road_fragments": 2, "expected_buildings": 4, "require_landscape": True},
                    self.PRESENTATION,
                    180.0,
                    self.RUNTIME,
                )
            self.assertEqual("runtime_route_unproven", raised.exception.code)

    def test_distribution_audit_rejects_raw_provider_payload(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            (root / "leaked.pbf").write_bytes(b"provider")
            matches = _forbidden_files([root], {".pbf"}, set())
            self.assertEqual([str(root / "leaked.pbf")], matches)

    def test_cook_proof_requires_a_cook_marker_not_a_map_name(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            log = root / "package.log"
            required = "/ProjectWorldData/Generated/P0/L_ProjectWorldKazan"
            log.write_text("Configured L_ProjectWorldKazan", encoding="utf-8")
            self.assertFalse(_required_map_cooked(log, required))
            log.write_text(f"LogCook: Splitting Package {required} with splitter", encoding="utf-8")
            self.assertTrue(_required_map_cooked(log, required))

    def test_clean_bootstrap_requires_created_environment_and_reacquired_cache(self) -> None:
        environment = {
            "bootstrap_preflight": {
                "all_absent": True,
                "observations": [{"present": False}] * 11,
            },
            "python_environment_bytes": 1,
            "native_tools_bytes": 1,
            "immutable_cache_bytes": 1,
            "network_transfer_bytes": 1,
        }
        self.assertEqual("clean-bootstrap", _validate_bootstrap(environment)["mode"])

    def test_clean_bootstrap_fails_without_source_reacquisition(self) -> None:
        environment = {
            "bootstrap_preflight": {
                "all_absent": True,
                "observations": [{"present": False}] * 11,
            },
            "python_environment_bytes": 1,
            "native_tools_bytes": 1,
            "immutable_cache_bytes": 1,
            "network_transfer_bytes": 0,
        }
        with self.assertRaises(ValidationFailure) as raised:
            _validate_bootstrap(environment)
        self.assertEqual("source_network_evidence_missing", raised.exception.code)


if __name__ == "__main__":
    unittest.main()
