from __future__ import annotations

import sys
import json
import copy
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.planning import changed_paths, format_plan, plan_for_paths
from World.EndToEndValidation.app.canonical_mode import (
    CanonicalModeError, _profile_upstream_projection, require_world_only_changes,
)
from World.EndToEndValidation.tests import proof_input_fixture as proof_fixture


EVERY_FIXTURE_VERIFY = [
    proof_fixture.VERIFY + name for name in ("Map", "MeshTerrain", "Pipeline", "Water")
]


class WorldVerifySelectionTests(unittest.TestCase):
    def _plan(self, paths: list[str], baselines: bool = True) -> dict[str, object]:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proof_fixture.write_producers(root, baselines=baselines)
            return plan_for_paths(paths, root)

    def test_adapter_only_edit_selects_only_the_terrain_verify(self) -> None:
        # M23: an adapter path is under a module only the terrain descriptor names.
        plan = self._plan([proof_fixture.ADAPTER_SOURCE])
        self.assertEqual([proof_fixture.VERIFY + "MeshTerrain"], plan["verifies"])
        self.assertEqual([], plan["unrecorded_verifies"])
        self.assertEqual([], plan["l3_candidate_owners"])
        self.assertEqual(
            [{
                "path": proof_fixture.ADAPTER_SOURCE,
                "rule": "producer module ProjectWorldMeshTerrainEditor",
                "producers": ["project_mesh_terrain:v1"],
            }],
            plan["verify_rules"],
        )
        text = format_plan(plan, "HEAD")
        self.assertIn(f"Verifies: {proof_fixture.VERIFY}MeshTerrain", text)
        self.assertIn(f"{proof_fixture.ADAPTER_SOURCE}: producer module ProjectWorldMeshTerrainEditor", text)

    def test_module_shared_by_several_descriptors_selects_every_verify(self) -> None:
        plan = self._plan([proof_fixture.WORLD_EDITOR_SOURCE])
        self.assertEqual(EVERY_FIXTURE_VERIFY, plan["verifies"])
        self.assertEqual("shared module ProjectWorldEditor", plan["verify_rules"][0]["rule"])

    def test_ambiguous_producer_module_selects_every_verify(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proof_fixture.write_producers(root)
            path = root / proof_fixture.descriptor_path("Plugins/World/ProjectWorld", "project_water_mesh")
            document = json.loads(path.read_text(encoding="utf-8"))
            document["modules"].append("ProjectWorldMeshTerrainEditor")
            path.write_text(json.dumps(document), encoding="utf-8")
            plan = plan_for_paths([proof_fixture.ADAPTER_SOURCE], root)
        self.assertEqual(EVERY_FIXTURE_VERIFY, plan["verifies"])
        self.assertEqual("ambiguous module ProjectWorldMeshTerrainEditor", plan["verify_rules"][0]["rule"])

    def test_other_proof_inputs_select_every_verify(self) -> None:
        paths = [
            "Plugins/Resources/ProjectObject/Source/ProjectObject/Private/ObjectSpawnServiceImpl.cpp",
            proof_fixture.baseline_path("Plugins/World/ProjectWorld", "project_water_mesh"),
            "scripts/ue/world/realization_layer_operation.ps1",
        ]
        plan = self._plan(paths)
        self.assertEqual(EVERY_FIXTURE_VERIFY, plan["verifies"])
        self.assertEqual(sorted(paths), [rule["path"] for rule in plan["verify_rules"]])
        self.assertEqual({"proof input"}, {rule["rule"] for rule in plan["verify_rules"]})

    def test_descriptor_revision_bump_selects_its_verify_and_l3_owner(self) -> None:
        path = proof_fixture.descriptor_path("Plugins/World/ProjectWorldMeshTerrain", "project_mesh_terrain")
        plan = self._plan([path])
        self.assertEqual([proof_fixture.VERIFY + "MeshTerrain"], plan["verifies"])
        self.assertEqual(["producer descriptor"], [rule["rule"] for rule in plan["verify_rules"]])
        self.assertEqual(["ProjectWorldData"], plan["l3_candidate_owners"])

    def test_data_input_change_selects_declaring_producer_and_l3_owner(self) -> None:
        plan = self._plan([proof_fixture.MPD])
        self.assertEqual([proof_fixture.VERIFY + "MeshTerrain"], plan["verifies"])
        self.assertEqual("declared data input", plan["verify_rules"][0]["rule"])
        self.assertEqual(["ProjectWorldData"], plan["l3_candidate_owners"])

    def test_engine_config_change_selects_every_verify_and_l3_owner(self) -> None:
        plan = self._plan(["scripts/config/ue_path.conf"])
        self.assertEqual(EVERY_FIXTURE_VERIFY, plan["verifies"])
        self.assertEqual("engine configuration", plan["verify_rules"][0]["rule"])
        self.assertEqual(["ProjectWorldData"], plan["l3_candidate_owners"])

    def test_pipeline_descriptor_selects_every_verify_and_l3_owner(self) -> None:
        plan = self._plan([proof_fixture.descriptor_path("Plugins/World/ProjectWorld", "realization_pipeline")])
        self.assertEqual(EVERY_FIXTURE_VERIFY, plan["verifies"])
        self.assertEqual(["ProjectWorldData"], plan["l3_candidate_owners"])

    def test_path_outside_the_proof_input_set_selects_no_verify(self) -> None:
        plan = self._plan([
            "Plugins/World/ProjectWorldMeshTerrain/README.md",
            "docs/testing/world_pipeline_layers.md",
        ])
        self.assertEqual([], plan["verifies"])
        self.assertEqual([], plan["verify_rules"])

    def test_selected_producer_without_a_baseline_is_reported(self) -> None:
        plan = self._plan([proof_fixture.ADAPTER_SOURCE], baselines=False)
        self.assertEqual([], plan["verifies"])
        self.assertEqual(["project_mesh_terrain:v1"], plan["unrecorded_verifies"])

    def test_repository_descriptors_route_adapter_and_generic_code(self) -> None:
        adapter = (
            "Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/"
            "ProjectWorldMeshTerrainProducer.cpp"
        )
        generic = "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldLayerInventory.cpp"
        rules = {rule["path"]: rule for rule in plan_for_paths([adapter, generic])["verify_rules"]}
        self.assertEqual(["project_mesh_terrain:v1"], rules[adapter]["producers"])
        self.assertEqual("shared module ProjectWorldEditor", rules[generic]["rule"])
        self.assertIn("realization_pipeline", rules[generic]["producers"])


class WorldPipelinePlanningTests(unittest.TestCase):
    def test_renamed_upstream_source_keeps_both_git_endpoints(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            source = root / "tools/World/CanonicalCompilation/app/producing_rule.py"
            target = root / "tools/World/Shared/producing_rule.py"
            source.parent.mkdir(parents=True)
            source.write_text("rule = 1\n", encoding="utf-8")
            for command in (
                ("init", "-q"),
                ("-c", "user.name=WorldTest", "-c", "user.email=world@example.invalid", "add", "."),
                ("-c", "user.name=WorldTest", "-c", "user.email=world@example.invalid", "commit", "-qm", "base"),
            ):
                subprocess.run(("git", *command), cwd=root, check=True, capture_output=True)
            target.parent.mkdir(parents=True)
            source.rename(target)
            subprocess.run(("git", "add", "-A"), cwd=root, check=True, capture_output=True)
            paths = changed_paths("HEAD", root)
            self.assertEqual([
                "tools/World/CanonicalCompilation/app/producing_rule.py",
                "tools/World/Shared/producing_rule.py",
            ], paths)
            with self.assertRaises(CanonicalModeError):
                require_world_only_changes(paths, "HEAD", root)

    def test_shipping_config_change_selects_every_executable_matrix(self) -> None:
        plan = plan_for_paths(["Config/DefaultGame.ini"])
        self.assertEqual(["kazan_territory_v1"], plan["l2_matrices"])
        self.assertTrue(plan["l4_required"])
        self.assertEqual([], plan["l3_candidate_owners"])

    def test_generator_source_change_selects_no_durable_regeneration(self) -> None:
        plan = plan_for_paths([
            "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimeNavigation.cpp"
        ])
        self.assertTrue(plan["l1_required"])
        self.assertEqual(["kazan_territory_v1"], plan["l2_matrices"])
        self.assertEqual([], plan["l3_candidate_owners"])
        self.assertTrue(plan["l4_required"])
        self.assertEqual({"kazan_territory_v1": "canonical_authority"}, plan["l2_matrix_modes"])

    def test_upstream_change_requires_full_replay(self) -> None:
        path = "tools/World/CanonicalCompilation/app/pipeline.py"
        plan = plan_for_paths([path])
        self.assertEqual({"kazan_territory_v1": "full"}, plan["l2_matrix_modes"])
        with self.assertRaises(CanonicalModeError) as rejected:
            require_world_only_changes([path])
        self.assertEqual("canonical_mode_upstream_changed", rejected.exception.code)

    def test_promoted_authority_change_requires_full_replay(self) -> None:
        path = "Plugins/World/ProjectWorldData/Data/Canonical/kazan_territory_v1/active.json"
        plan = plan_for_paths([path])
        self.assertEqual({"kazan_territory_v1": "full"}, plan["l2_matrix_modes"])
        with self.assertRaises(CanonicalModeError):
            require_world_only_changes([path])

    def test_world_change_allows_canonical_boundary(self) -> None:
        path = "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedActorLifecycle.cpp"
        require_world_only_changes([path])
        self.assertEqual("canonical_authority", plan_for_paths([path])["l2_matrix_modes"]["kazan_territory_v1"])

    def test_source_documentation_does_not_force_provider_replay(self) -> None:
        require_world_only_changes(["tools/World/SourceIngestion/README.md"])

    def test_validation_profile_world_assertion_keeps_canonical_mode_but_source_edit_requires_full(self) -> None:
        path = "Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/kazan_territory_v1.validation.json"
        original = json.loads((REPO_ROOT / path).read_text(encoding="utf-8"))
        world_only = copy.deepcopy(original)
        world_only["profiles"]["kazan"]["expected_layers"][0]["artifact_count"] += 1
        self.assertEqual(_profile_upstream_projection(original), _profile_upstream_projection(world_only))
        upstream = copy.deepcopy(original)
        upstream["profiles"]["kazan"]["source_profile_path"] += ".changed"
        self.assertNotEqual(_profile_upstream_projection(original), _profile_upstream_projection(upstream))
        upstream = copy.deepcopy(original)
        upstream["thresholds"]["full_compile_seconds"] += 1
        self.assertNotEqual(_profile_upstream_projection(original), _profile_upstream_projection(upstream))
        with mock.patch("World.EndToEndValidation.app.canonical_mode._validation_profile_upstream_changed", return_value=False):
            require_world_only_changes([path], "HEAD")
            self.assertEqual("canonical_authority", plan_for_paths([path], base="HEAD")["l2_matrix_modes"]["kazan_territory_v1"])
        with mock.patch("World.EndToEndValidation.app.canonical_mode._validation_profile_upstream_changed", return_value=True):
            with self.assertRaises(CanonicalModeError):
                require_world_only_changes([path], "HEAD")
            self.assertEqual("full", plan_for_paths([path], base="HEAD")["l2_matrix_modes"]["kazan_territory_v1"])

    def test_test_only_edit_does_not_request_durable_regeneration(self) -> None:
        plan = plan_for_paths([
            "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/Tests/ProjectWorldTests.cpp"
        ])
        self.assertEqual(["kazan_territory_v1"], plan["l2_matrices"])
        self.assertEqual([], plan["l3_candidate_owners"])
        self.assertFalse(plan["l4_required"])

    def test_world_script_readme_does_not_impersonate_generator_code(self) -> None:
        plan = plan_for_paths(["scripts/ue/world/README.md"])
        self.assertFalse(plan["l1_required"])
        self.assertEqual([], plan["l2_matrices"])
        self.assertEqual([], plan["l3_candidate_owners"])
        self.assertFalse(plan["l4_required"])

    def test_generated_fixture_path_never_requests_durable_test_authority(self) -> None:
        plan = plan_for_paths([
            "Plugins/World/ProjectWorldTestData/Content/Generated/P0/L_Test.umap"
        ])
        self.assertFalse(plan["l1_required"])
        self.assertEqual([], plan["l2_matrices"])
        self.assertEqual([], plan["l3_candidate_owners"])
        self.assertFalse(plan["l4_required"])

    def test_new_control_does_not_select_existing_matrix_or_l3(self) -> None:
        plan = plan_for_paths(["Plugins/World/ProjectWorldData/Data/Controls/foo.json"])
        self.assertEqual([], plan["l2_matrices"])
        self.assertEqual([], plan["l3_candidate_owners"])

    def test_future_territory_profiles_do_not_select_existing_matrix_or_l3(self) -> None:
        plan = plan_for_paths([
            "Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_future_v2.source.json",
            "Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/kazan_future_v2.compile.json",
        ])
        self.assertEqual([], plan["l2_matrices"])
        self.assertEqual([], plan["l3_candidate_owners"])

    def test_territory_transitive_input_selects_only_territory(self) -> None:
        plan = plan_for_paths([
            "Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json"
        ])
        self.assertEqual(["kazan_territory_v1"], plan["l2_matrices"])
        self.assertEqual([], plan["l3_candidate_owners"])

    def test_retired_representative_input_selects_no_matrix(self) -> None:
        plan = plan_for_paths([
            "Plugins/World/ProjectWorldData/Data/Runtime/kazan_representative_playable_v1.json"
        ])
        self.assertEqual([], plan["l2_matrices"])
        self.assertEqual([], plan["l3_candidate_owners"])


if __name__ == "__main__":
    unittest.main()
