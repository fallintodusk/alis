from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.territory_admission import admit_territory


class TerritoryAdmissionTests(unittest.TestCase):
    PROFILE_PATH = (
        REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
        / "Profiles" / "CanonicalCompilation" / "kazan_territory_v1.compile.json"
    )

    def test_production_contract_set_is_accepted_as_one_boundary(self) -> None:
        receipt, path = admit_territory(REPO_ROOT, self.PROFILE_PATH)
        self.assertEqual("accepted", receipt["status"])
        self.assertEqual(210, receipt["cell_count"])
        self.assertEqual([374180.0, 6178590.0, 388130.0, 6191610.0], receipt["technical_bounds"])
        self.assertEqual(1902, receipt["coverage"]["edge_samples_checked"])
        self.assertTrue(receipt["coverage"]["raster_union_complete"])
        self.assertEqual(receipt["contract_hash"], path.parent.name)


if __name__ == "__main__":
    unittest.main()
