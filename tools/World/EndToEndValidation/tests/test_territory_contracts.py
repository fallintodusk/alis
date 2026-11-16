from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.contracts import ValidationFailure, read_json, validate_against


class TerritoryContractTests(unittest.TestCase):
    def test_empty_layer_inventory_has_no_placeholder_artifact(self) -> None:
        path = REPO_ROOT / ("Plugins/World/ProjectWorldData/Data/Profiles/"
                            "EndToEndValidation/kazan_territory_v1.validation.json")
        profile = read_json(path)
        layer = copy.deepcopy(profile["profiles"]["manhattan"]["expected_layers"][0])
        layer.pop("producer_artifact_count", None)
        layer.update(layer_id="constructed", generator_id="project_constructed_surface",
                     artifact_count=0, metrics={"physical_units": 0})
        profile["profiles"]["manhattan"]["expected_layers"].append(layer)
        validate_against(profile, "validation-profile.schema.json")
        for field in ("artifact_count", "canonical_input_count"):
            invalid = copy.deepcopy(profile)
            invalid["profiles"]["manhattan"]["expected_layers"][-1][field] = -1 if field == "artifact_count" else 0
            with self.assertRaises(ValidationFailure):
                validate_against(invalid, "validation-profile.schema.json")

    def test_territory_budget_is_frozen_before_realization(self) -> None:
        path = (
            REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
            / "Profiles" / "Budgets" / "kazan_territory_v1.budget.json"
        )
        budget = read_json(path)
        validate_against(budget, "territory-budget.schema.json")
        self.assertEqual(210, budget["ceilings"]["cells"]["max"])
        self.assertEqual("hard", budget["ceilings"]["canonical_bytes"]["kind"])
        self.assertEqual(16.67, budget["ceilings"]["p95_frame_time_ms"]["max"])
        self.assertEqual("provisional", budget["ceilings"]["streaming_hitch_ms"]["kind"])


if __name__ == "__main__":
    unittest.main()
