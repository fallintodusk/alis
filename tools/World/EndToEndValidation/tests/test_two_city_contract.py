from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.canonical_mode import _profile_upstream_projection
from World.EndToEndValidation.app.contracts import load_profile
from World.EndToEndValidation.app.planning import plan_for_paths
from World.EndToEndValidation.app.profile_inputs import profile_input_files


PROFILE = REPO_ROOT / (
    "Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/"
    "kazan_territory_v1.validation.json"
)


class TwoCityValidationContractTests(unittest.TestCase):
    def test_profile_authenticates_both_production_authorities(self) -> None:
        profile = load_profile(PROFILE)
        self.assertEqual({"synthetic", "kazan", "manhattan"}, set(profile["profiles"]))
        for city, compiler in (("kazan", "kazan_territory_v1"),
                               ("manhattan", "manhattan_showcase_v1")):
            settings = profile["profiles"][city]
            self.assertTrue(settings["canonical_authority"])
            self.assertEqual(compiler, settings["compiler_profile"])
            self.assertEqual(210, settings["expected_topology"]["canonical_cells"])
        self.assertEqual(profile["profiles"]["kazan"]["presentation_profile"],
                         profile["profiles"]["manhattan"]["presentation_profile"])

    def test_manhattan_upstream_edit_selects_full_matrix(self) -> None:
        path = "Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/manhattan_showcase_v1.compile.json"
        plan = plan_for_paths([path])
        self.assertEqual({"kazan_territory_v1": "full"}, plan["l2_matrix_modes"])

    def test_manhattan_inputs_are_in_the_authenticated_profile_closure(self) -> None:
        inputs = {path.relative_to(REPO_ROOT).as_posix()
                  for path in profile_input_files(PROFILE, REPO_ROOT)}
        prefix = "Plugins/World/ProjectWorldData/Data/"
        for path in (
            "Profiles/SourceIngestion/manhattan_showcase_v1.source.json",
            "Profiles/CanonicalCompilation/manhattan_showcase_v1.compile.json",
            "Profiles/Realization/manhattan_showcase_v1.realization.json",
            "Profiles/Budgets/manhattan_showcase_v1.budget.json",
            "Authored/manhattan_showcase_sabotage_v1.json",
            "Runtime/manhattan_showcase_512_1536_v1.json",
        ):
            self.assertIn(prefix + path, inputs)

    def test_manhattan_sabotage_targets_missing_feature_in_the_correct_grid(self) -> None:
        settings = load_profile(PROFILE)["profiles"]["manhattan"]
        valid = json.loads((REPO_ROOT / settings["authored_overlay_profile"]).read_text())
        sabotage_path = REPO_ROOT / settings["authored_overlay_sabotage_profile"]
        sabotage = json.loads(sabotage_path.read_text())
        schema = json.loads((REPO_ROOT / "Plugins/World/ProjectWorld/Data/Schemas/"
                             "project_world_authored_overlay.schema.json").read_text())
        Draft202012Validator(schema).validate(sabotage)
        self.assertEqual(valid["grid_id"], sabotage["grid_id"])
        self.assertEqual(valid["world_data_plugin"], sabotage["world_data_plugin"])
        self.assertEqual(valid["resolver_version"], sabotage["resolver_version"])
        anchor = sabotage["overlays"][0]["anchor"]
        self.assertEqual("feature", anchor["kind"])
        self.assertEqual("alis:osm:way:missing", anchor["feature_id"])
        compiler = json.loads((REPO_ROOT / settings["compiler_profile_path"]).read_text())
        bounds = compiler["target_cell_range"]
        grid = compiler["grid"]
        for axis, coordinate in (("x", "easting"), ("y", "northing")):
            index = 0 if axis == "x" else 1
            span = grid["sample_spacing"][index] * grid["cell_quads"][index]
            value = anchor[f"expected_{coordinate}_m"]
            self.assertGreaterEqual(value, grid["origin"][index] + bounds[f"min_{axis}"] * span)
            self.assertLess(value, grid["origin"][index] + (bounds[f"max_{axis}"] + 1) * span)

    def test_removing_manhattan_changes_the_upstream_acceptance_contract(self) -> None:
        profile = load_profile(PROFILE)
        kazan_only = copy.deepcopy(profile)
        del kazan_only["profiles"]["manhattan"]
        self.assertNotEqual(_profile_upstream_projection(profile),
                            _profile_upstream_projection(kazan_only))


if __name__ == "__main__":
    unittest.main()
