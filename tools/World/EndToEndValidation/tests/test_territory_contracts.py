from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.contracts import read_json, validate_against


class TerritoryContractTests(unittest.TestCase):
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
