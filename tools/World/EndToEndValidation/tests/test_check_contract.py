from __future__ import annotations

import json
import tempfile
import unittest
import sys
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app import checks
from World.EndToEndValidation.app.checks import common_contract_hash
from World.EndToEndValidation.app.contracts import ValidationFailure
from World.EndToEndValidation.app.profile_inputs import ProfileInputError, profile_input_contract
from World.EndToEndValidation.tests import proof_input_fixture as proof_fixture


class CommonCheckContractTests(unittest.TestCase):
    def test_compiled_process_owner_changes_invalidate_common_evidence(self) -> None:
        parent = REPO_ROOT / 'tmp/world/check_contract_test/process-owner'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as directory:
            root = Path(directory)
            proof_fixture.write_producers(root)
            proof_fixture.write_target_export(root)
            relative = 'scripts/ue/test/lib/owned_test_process.cs'
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('class OwnedProcess {}\n')
            self._assert_change_stales_receipt(root, relative)

    def test_target_export_precedes_first_contract_hash(self) -> None:
        calls: list[str] = []
        with mock.patch.object(checks, "_powershell", return_value="powershell"), \
             mock.patch.object(checks, "_run", side_effect=lambda name, *_: calls.append(name)), \
             mock.patch.object(checks, "_preflight_verify_revisions", side_effect=lambda: calls.append("verify_revisions")), \
             mock.patch.object(checks, "common_contract_hash", side_effect=lambda: calls.append("hash") or "same"), \
             mock.patch.object(checks, "read_json", return_value={}), \
             mock.patch.object(checks, "validate_against"), \
             mock.patch.object(checks, "tree_size", return_value=0), \
             mock.patch.object(checks, "_run_test_suites", return_value=[]):
            checks.execute_checks(REPO_ROOT / "tmp/world/check_contract_test", REPO_ROOT / "unused.json")
        self.assertEqual(calls[:3], ["export_editor_target", "verify_revisions", "hash"])

    def test_declared_and_embedded_verify_revision_drift_refuse_before_suites(self) -> None:
        parent=REPO_ROOT/'tmp/world/verify_revision_tests';parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            root=Path(temporary);proof_fixture.write_producers(root)
            descriptors=checks.load_producer_descriptors(root)
            for item in descriptors:
                document=json.loads((root/item.path).read_text())
                identity=({'pipeline_revision':document['pipeline_revision'],
                           'embedded':['project_water_mesh:v1|1|1']} if item.is_pipeline else
                          {'producer_id':item.label,'output_revision':document['output_revision']})
                baseline=json.loads((root/item.baseline_path).read_text())
                baseline['identity']=identity
                (root/item.baseline_path).write_text(json.dumps(baseline))
            checks._preflight_verify_revisions(root)
            for identifier in ('map','project_water_mesh'):
                item=next(row for row in descriptors if row.generator_id==identifier)
                path=root/item.path;document=json.loads(path.read_text());document['output_revision']=2
                path.write_text(json.dumps(document))
                with self.assertRaises(ValidationFailure) as error:
                    checks._preflight_verify_revisions(root)
                self.assertEqual('verify_baseline_revision_stale',error.exception.code)
                if identifier=='project_water_mesh':self.assertIn('embedded',str(error.exception.details))
                document['output_revision']=1;path.write_text(json.dumps(document))

    def test_twin_verify_is_a_required_common_suite(self) -> None:
        commands: dict[str, list[str]] = {}
        with mock.patch.object(checks, "_run", side_effect=lambda name, command, _: commands.setdefault(name, command) or 0):
            checks._run_test_suites(REPO_ROOT / "tmp/world/check_contract_test", "powershell")
        self.assertIn("twin_realization", checks.REQUIRED_TEST_SUITE_NAMES)
        self.assertIn("verify_twin_realization.ps1", commands["test_twin_realization"][-1])
        self.assertEqual(["-t", "tools"], commands["test_canonical_compilation"][-2:])
        self.assertEqual(["-t", "tools"], commands["test_source_ingestion"][-2:])
        self.assertLess(list(commands).index("test_world_architecture"), list(commands).index("test_canonical_compilation"))
        native = commands["test_unreal_realization"]
        self.assertIn("verify_native_realization.ps1", native[-3])
        self.assertEqual("-EvidenceDirectory", native[-2])
        self.assertTrue(Path(native[-1]).is_relative_to(REPO_ROOT / "tmp"))

    def test_no_gate_passes_verify_record_mode(self) -> None:
        gate_files = [
            REPO_ROOT / "tools/World/EndToEndValidation/app/checks.py",
            REPO_ROOT / "scripts/ue/world/test/run_all.ps1",
            *(REPO_ROOT / "scripts/ue/test/unit").glob("*.ps1"),
        ]
        self.assertGreater(len(gate_files), 2)
        forbidden = ("ProjectWorldVerifyRecord", "record_verify_baseline", "-Record")
        known_bad = "& record_verify_baseline.ps1 -Record"
        self.assertTrue(any(token in known_bad for token in forbidden))
        for path in gate_files:
            content = path.read_text(encoding="utf-8-sig")
            self.assertFalse(
                any(token in content for token in forbidden),
                f"Common gate passes verify record mode: {path.relative_to(REPO_ROOT)}",
            )

    def _write(self, root: Path, relative: str, value: bytes = b"baseline") -> Path:
        return proof_fixture.write(root, relative, value)

    def _fitness_failure(self, root: Path) -> ValidationFailure:
        with self.assertRaises(ValidationFailure) as raised:
            common_contract_hash(root)
        return raised.exception

    def _assert_change_stales_receipt(self, root: Path, relative: str) -> None:
        path = root / relative
        original = path.read_bytes()
        baseline = common_contract_hash(root)
        path.write_bytes(original + b"\n")
        self.assertNotEqual(baseline, common_contract_hash(root), relative)
        path.write_bytes(original)
        self.assertEqual(baseline, common_contract_hash(root), relative)

    def test_missing_target_export_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proof_fixture.write_producers(root)
            self.assertEqual("proof_input_export_missing", self._fitness_failure(root).code)

    def test_target_export_older_than_a_build_rules_file_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proof_fixture.write_producers(root)
            proof_fixture.write_target_export(root)
            common_contract_hash(root)
            rules = "Plugins/Resources/ProjectObject/Source/ProjectObject/ProjectObject.Build.cs"
            proof_fixture.make_newer_than_export(root, rules)
            failure = self._fitness_failure(root)
            self.assertEqual("proof_input_export_stale", failure.code)
            self.assertEqual(rules, failure.details["newer"])

    def test_descriptor_module_absent_from_export_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proof_fixture.write_producers(root)
            proof_fixture.write_target_export(root, omitted=("ProjectWorldMeshTerrainEditor",))
            failure = self._fitness_failure(root)
            self.assertEqual("proof_input_module_missing", failure.code)
            self.assertEqual(["ProjectWorldMeshTerrainEditor"], failure.details["modules"])

    def test_producing_module_dependency_outside_roots_fails_closed(self) -> None:
        # M22: a descriptor-named module gains a first-party dependency no root covers.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proof_fixture.write_producers(root)
            proof_fixture.write_target_export(root)
            common_contract_hash(root)
            proof_fixture.write_target_export(
                root,
                extra_modules={"ProjectInventory": "Plugins/Features/ProjectInventory/Source/ProjectInventory"},
                extra_dependencies={"ProjectWorldMeshTerrainEditor": ["ProjectInventory"]},
            )
            failure = self._fitness_failure(root)
            self.assertEqual("proof_input_root_uncovered", failure.code)
            self.assertEqual(
                {"ProjectInventory": "Plugins/Features/ProjectInventory/Source/ProjectInventory"},
                failure.details["modules"],
            )

    def test_descriptors_are_keyed_by_generator_id_and_version(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proof_fixture.write_producers(root)
            proof_fixture.write_target_export(root)
            other = proof_fixture.descriptor_path("Plugins/World/ProjectWorldTerrainNext", "project_mesh_terrain")
            document = {
                "$schema": "schema.json", "schema_version": 1, "kind": "producer",
                "generator_id": "project_mesh_terrain", "generator_version": 2, "output_revision": 1,
                "data_inputs": [], "modules": ["ProjectWorldMeshTerrainEditor"],
            }
            self._write(root, other, json.dumps(document).encode("utf-8"))
            common_contract_hash(root)
            document["generator_version"] = 1
            self._write(root, other, json.dumps(document).encode("utf-8"))
            failure = self._fitness_failure(root)
            self.assertEqual("proof_input_descriptor_invalid", failure.code)
            self.assertIn("project_mesh_terrain:v1", str(failure))

    def test_adapter_only_edit_stales_common_check_receipt(self) -> None:
        # M23: the Mesh Terrain adapter is producing code outside ProjectWorld.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proof_fixture.write_producers(root)
            self._write(root, proof_fixture.ADAPTER_SOURCE)
            proof_fixture.write_target_export(root)
            self._assert_change_stales_receipt(root, proof_fixture.ADAPTER_SOURCE)

    def test_producer_records_verifier_and_reached_code_stale_common_check_receipt(self) -> None:
        # M25 plus the code producers reach outside ProjectWorld.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proof_fixture.write_producers(root)
            paths = (
                proof_fixture.descriptor_path("Plugins/World/ProjectWorldMeshTerrain", "project_mesh_terrain"),
                proof_fixture.descriptor_path("Plugins/World/ProjectWorld", "realization_pipeline"),
                proof_fixture.baseline_path("Plugins/World/ProjectWorldMeshTerrain", "project_mesh_terrain"),
                proof_fixture.baseline_path("Plugins/World/ProjectWorld", "realization_pipeline"),
                proof_fixture.MPD,
                "scripts/ue/world/record_verify_baseline.ps1",
                "scripts/ue/world/test/integration/verify_twin_realization.ps1",
                "scripts/ue/world/realization_layer_operation.ps1",
                "scripts/ue/generated_content/generated_content_mutation_lock.ps1",
                "Plugins/Resources/ProjectObject/Source/ProjectObject/Private/ObjectSpawnServiceImpl.cpp",
                "Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Pickup.h",
                "Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/ProjectSha256.h",
                "Plugins/World/ProjectBuildingAssembly/Source/ProjectBuildingAssemblyEditor/Private/ProjectBuildingAssemblyRealization.cpp",
            )
            for relative in paths:
                if not (root / relative).exists():
                    self._write(root, relative)
            proof_fixture.write_target_export(root)
            for relative in paths:
                with self.subTest(path=relative):
                    self._assert_change_stales_receipt(root, relative)

    def test_hash_tracks_immutable_inputs_but_excludes_generated_authority(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proof_fixture.write_producers(root)
            game_config = self._write(root, "Config/DefaultGame.ini")
            self._write(root, "Config/DefaultEngine.ini")
            self._write(root, "Alis.uproject")
            self._write(root, "Plugins/World/ProjectWorld/ProjectWorld.uplugin")
            self._write(root, "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Builder.cpp")
            self._write(root, "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/ProjectWorldEditor.Build.cs")
            self._write(root, "Plugins/World/ProjectWorld/Data/Schemas/world.schema.json")
            self._write(root, "tools/World/ExecutionEnvironment/requirements.lock.txt")
            self._write(root, "scripts/ue/world/realize.ps1")

            source_profile = self._write(
                root,
                "Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_territory_v1.source.json",
                b'{"sources": []}',
            )
            compiler_profile = self._write(
                root,
                "Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/kazan_territory_v1.compile.json",
                b'{}',
            )
            validation_profile = self._write(
                root,
                "Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/kazan_territory_v1.validation.json",
                (
                    '{"profile_id":"kazan_territory_v1","profiles":{"kazan":{'
                    f'"source_profile_path":"{source_profile.relative_to(root).as_posix()}",'
                    f'"compiler_profile_path":"{compiler_profile.relative_to(root).as_posix()}"'
                    '}}}'
                ).encode("utf-8"),
            )
            unrelated = self._write(
                root,
                "Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_future_v2.source.json",
                b'{"sources": []}',
            )
            future_validation = self._write(
                root,
                "Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/kazan_future_v2.validation.json",
                (
                    '{"profile_id":"kazan_future_v2","profiles":{"kazan":{'
                    '"source_profile_path":"Plugins/World/ProjectWorldData/Data/Profiles/'
                    'SourceIngestion/not_created_yet.source.json"}}}'
                ).encode("utf-8"),
            )

            fixture = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Data/Fixtures/Provider/source.osm",
            )
            generated_map = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Content/Generated/P0/L_Test.umap",
            )
            manifest = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Data/Manifests/active_set.json",
            )
            proof_fixture.write_target_export(root)
            baseline = common_contract_hash(root)
            profile_baseline = profile_input_contract(validation_profile, root)

            game_config.write_bytes(b"changed")
            self.assertNotEqual(baseline, common_contract_hash(root))
            game_config.write_bytes(b"baseline")
            self.assertEqual(baseline, common_contract_hash(root))

            fixture.write_bytes(b"changed")
            self.assertNotEqual(baseline, common_contract_hash(root))
            fixture.write_bytes(b"baseline")
            self.assertEqual(baseline, common_contract_hash(root))

            generated_map.write_bytes(b"changed")
            self.assertEqual(baseline, common_contract_hash(root))
            manifest.write_bytes(b"changed")
            self.assertEqual(baseline, common_contract_hash(root))

            source_profile.write_bytes(b'{"sources": [{"source_id": "changed"}]}')
            self.assertNotEqual(baseline, common_contract_hash(root))
            self.assertNotEqual(profile_baseline, profile_input_contract(validation_profile, root))
            source_profile.write_bytes(b'{"sources": []}')
            self.assertEqual(baseline, common_contract_hash(root))

            unrelated.write_bytes(b'{"profile_id": "future"}')
            self.assertEqual(baseline, common_contract_hash(root))
            future_validation.write_bytes(future_validation.read_bytes() + b" ")
            self.assertEqual(baseline, common_contract_hash(root))

    def test_profile_contract_tracks_authored_package_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self._write(root, "Plugins/World/ProjectWorldTestData/ProjectWorldTestData.uplugin")
            authored_map = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Content/Authored/Fixtures/L_Marker.umap",
            )
            authored_profile = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Data/Authored/synthetic.json",
                (
                    '{"world_data_plugin":"ProjectWorldTestData","overlays":['
                    '{"authored_package":"/ProjectWorldTestData/Authored/Fixtures/L_Marker"}]}'
                ).encode("utf-8"),
            )
            validation_profile = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Data/Profiles/EndToEndValidation/synthetic.validation.json",
                (
                    '{"profile_id":"synthetic","profiles":{"synthetic":{'
                    f'"authored_overlay_profile":"{authored_profile.relative_to(root).as_posix()}"'
                    '}}}'
                ).encode("utf-8"),
            )

            baseline = profile_input_contract(validation_profile, root)
            authored_map.write_bytes(b"changed")
            self.assertNotEqual(baseline, profile_input_contract(validation_profile, root))
            authored_map.unlink()
            with self.assertRaises(ProfileInputError):
                profile_input_contract(validation_profile, root)


if __name__ == "__main__":
    unittest.main()
