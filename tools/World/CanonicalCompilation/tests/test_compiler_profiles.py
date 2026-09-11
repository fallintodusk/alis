from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.pipeline import load_profile


class CompilerProfileTests(unittest.TestCase):
    def test_territory_range_expands_to_the_frozen_210_cells(self) -> None:
        profile = load_profile(
            REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
            / "Profiles" / "CanonicalCompilation" / "kazan_territory_v1.compile.json"
        )
        representative = load_profile(
            REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
            / "Profiles" / "CanonicalCompilation" / "kazan_representative_v1.compile.json"
        )
        cells = {(item["x"], item["y"]) for item in profile["target_cells"]}
        self.assertEqual(210, len(cells))
        self.assertIn((-6, -6), cells)
        self.assertIn((8, 7), cells)
        self.assertEqual([381409, 6185051], profile["engine_georeference_origin"])
        self.assertEqual([379760.0, 6184170.0], profile["grid"]["origin"])
        self.assertEqual([1.0, 1.0], profile["raster_sampling"]["bilinear_warp_scale"])
        self.assertEqual(representative["grid"], profile["grid"])
        self.assertEqual(representative["water_semantics"], profile["water_semantics"])


if __name__ == "__main__":
    unittest.main()
