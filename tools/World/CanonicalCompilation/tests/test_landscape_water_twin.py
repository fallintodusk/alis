from __future__ import annotations

import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
TEST_DATA = REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Data"
SOURCE_PROFILE = TEST_DATA / "Profiles" / "SourceIngestion" / "synthetic_landscape_water_twin.source.json"
COMPILER_PROFILE = TEST_DATA / "Profiles" / "CanonicalCompilation" / "synthetic_landscape_water_twin.compile.json"
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.contracts import read_json
from World.CanonicalCompilation.app.pipeline import compile_world
from World.SourceIngestion.app.cli import main as source_main


class LandscapeWaterTwinTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        with redirect_stdout(StringIO()):
            if source_main(["run", "--profile", str(SOURCE_PROFILE)]) != 0:
                raise RuntimeError("Landscape/water twin source ingestion failed")

    @staticmethod
    def _features(root: Path) -> list[dict]:
        return [
            feature
            for path in sorted((root / "canonical" / "features").glob("*.json"))
            for feature in read_json(path)["features"]
        ]

    @staticmethod
    def _canonical_hashes(result: dict) -> dict[str, str]:
        return {
            output["path"]: output["sha256"]
            for output in result["outputs"]
            if output["path"].startswith("canonical/")
        }

    def test_compiler_backed_landscape_water_twin_is_deterministic(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            first_result, _ = compile_world(str(COMPILER_PROFILE), output_root_value=root / "first")
            second_result, _ = compile_world(str(COMPILER_PROFILE), output_root_value=root / "second")
            self.assertEqual(self._canonical_hashes(first_result), self._canonical_hashes(second_result))

            features = self._features(root / "first")
            by_source = {feature["source_refs"][0]["provider_feature_id"]: feature for feature in features}
            self.assertEqual(
                {"relation/1002", "way/1001", "way/1003", "way/1004", "way/1008", "way/2001"},
                set(by_source),
            )

            building = by_source["way/2001"]
            self.assertEqual("building", building["feature_class"])
            self.assertEqual("Polygon", building["geometry"]["type"])
            self.assertEqual(12.0, building["attributes"]["height_m"])
            self.assertEqual(2, len(building["intersecting_cell_ids"]))
            self.assertEqual(2, len(building["geometry"]["coordinates"]))

            road = by_source["way/1008"]
            self.assertEqual("primary", road["attributes"]["road_class"])
            self.assertEqual(10.0, road["attributes"]["width_m"])
            self.assertEqual(2, len(road["representations"]))
            self.assertEqual(
                road["representations"][0]["geometry"]["coordinates"][-1],
                road["representations"][1]["geometry"]["coordinates"][0],
            )

            lake = by_source["way/1001"]
            self.assertEqual(2, len(lake["geometry"]["coordinates"]))
            self.assertEqual("standing", lake["attributes"]["surface_behavior"])
            level = lake["attributes"]["surface_function"]["level_m"]
            terrain = read_json(root / "first" / "canonical" / "terrain" / "cell_x0_y0.json")
            samples = [
                terrain["core_samples"][63 - y_value][x_value]
                for y_value in range(8, 23)
                for x_value in range(10, 56)
                if not (25.25 <= x_value <= 40.25 and 12.25 <= y_value <= 18.25)
            ]
            self.assertGreater(max(samples) - min(samples), 0.5)
            self.assertIsInstance(level, (int, float))

            river = by_source["relation/1002"]
            axis = by_source["way/1003"]
            self.assertEqual(river["attributes"]["surface_group_id"], axis["attributes"]["surface_group_id"])
            knots = river["attributes"]["surface_function"]["knots"]
            self.assertTrue(all(left[2] >= right[2] for left, right in zip(knots, knots[1:], strict=False)))
            self.assertEqual(1, len({knot[2] for knot in knots if knot[0] == 65.0}))
            tails = axis["geometry"]["coordinates"]
            self.assertTrue(all(max(point[0] for point in tail) < 35.0 or
                                min(point[0] for point in tail) > 110.0 for tail in tails))
            self.assertEqual(0.0, axis["attributes"]["polygon_overlap_area_m2"])

            footprint = by_source["way/1004"]
            self.assertEqual(2, len(footprint["intersecting_cell_ids"]))
            self.assertTrue(any(
                item["representation"] == "water_ribbon_reference" and "geometry" not in item
                for item in footprint["representations"]
            ))
            rejections = read_json(root / "first" / "reports" / "rejections.json")["rejections"]
            self.assertEqual(
                ["water_hidden", "water_non_surface", "water_temporal"],
                sorted(item["reason_code"] for item in rejections),
            )


if __name__ == "__main__":
    unittest.main()
