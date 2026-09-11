from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.planning import plan_for_paths


class WorldPipelinePlanningTests(unittest.TestCase):
    def test_shipping_config_change_selects_every_executable_matrix(self) -> None:
        plan = plan_for_paths(["Config/DefaultGame.ini"])
        self.assertEqual(["p0", "representative", "kazan_territory_v1"], plan["l2_matrices"])
        self.assertTrue(plan["l4_required"])
        self.assertEqual([], plan["l3_candidate_owners"])

    def test_generator_change_selects_only_persistent_production_owner(self) -> None:
        plan = plan_for_paths([
            "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimeNavigation.cpp"
        ])
        self.assertTrue(plan["l1_required"])
        self.assertEqual(["p0", "representative", "kazan_territory_v1"], plan["l2_matrices"])
        self.assertEqual(["ProjectWorldData"], plan["l3_candidate_owners"])
        self.assertTrue(plan["l4_required"])

    def test_test_only_edit_does_not_request_durable_regeneration(self) -> None:
        plan = plan_for_paths([
            "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/Tests/ProjectWorldTests.cpp"
        ])
        self.assertEqual(["p0", "representative", "kazan_territory_v1"], plan["l2_matrices"])
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

    def test_representative_transitive_input_selects_only_representative(self) -> None:
        plan = plan_for_paths([
            "Plugins/World/ProjectWorldData/Data/Runtime/kazan_representative_playable_v1.json"
        ])
        self.assertEqual(["representative"], plan["l2_matrices"])
        self.assertEqual([], plan["l3_candidate_owners"])


if __name__ == "__main__":
    unittest.main()
