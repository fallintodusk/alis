from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.SourceIngestion.app.contracts import IngestionError, read_json
from World.SourceIngestion.app.coverage import projected_edge_points, raster_union_covers
from World.SourceIngestion.app.profiles import build_plan, validate_profile


class SourceCoverageTests(unittest.TestCase):
    PROFILE_PATH = (
        REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
        / "Profiles" / "SourceIngestion" / "kazan_p0.source.json"
    )

    def test_projected_edge_points_include_expanded_bounds(self) -> None:
        points = projected_edge_points({
            "technical_bounds": [374180, 6178590, 388130, 6191610],
            "terrain_resampling_halo_m": 30,
            "minimum_source_margin_m": 350,
            "edge_sample_step_m": 30,
        })
        self.assertIn([373800.0, 6178210.0], points)
        self.assertIn([388510.0, 6191990.0], points)
        self.assertEqual(373800.0, min(point[0] for point in points))
        self.assertEqual(6191990.0, max(point[1] for point in points))

    def test_adjacent_rasters_cover_without_hiding_a_gap(self) -> None:
        required = [48.5, 55.2, 49.5, 55.8]
        self.assertTrue(raster_union_covers(required, [
            [47.9, 55.0, 49.0, 56.0],
            [49.0, 55.0, 50.0, 56.0],
        ]))
        self.assertFalse(raster_union_covers(required, [
            [47.9, 55.0, 48.99, 56.0],
            [49.01, 55.0, 50.0, 56.0],
        ]))

    def test_copernicus_rights_and_boundaries_fail_closed(self) -> None:
        profile = read_json(self.PROFILE_PATH)
        validate_profile(profile, self.PROFILE_PATH, REPO_ROOT)
        plan = build_plan(profile)
        copernicus = next(item for item in plan["inputs"] if item["adapter"] == "copernicus_dem")
        self.assertIn("liability_notice", copernicus["license"])
        self.assertEqual(profile["sources"][1]["hashes"], copernicus["hashes"])
        self.assertEqual(profile["sources"][1]["admission"], copernicus["admission"])

        missing_notice = copy.deepcopy(profile)
        missing_notice["sources"][1]["license"].pop("liability_notice")
        with self.assertRaises(IngestionError) as raised:
            validate_profile(missing_notice, self.PROFILE_PATH, REPO_ROOT)
        self.assertEqual("contract_violation", raised.exception.code)

        closed_package = copy.deepcopy(profile)
        closed_package["sources"][1]["admission"]["boundaries"][
            "packaged_commercial_distribution"
        ] = "not_distributed"
        with self.assertRaises(IngestionError) as raised:
            validate_profile(closed_package, self.PROFILE_PATH, REPO_ROOT)
        self.assertEqual("incomplete_admission", raised.exception.code)


if __name__ == "__main__":
    unittest.main()
