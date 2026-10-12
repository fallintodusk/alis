from __future__ import annotations

from copy import deepcopy
import json
from pathlib import Path
from types import SimpleNamespace
import unittest

import jsonschema

from .test_terrain_surface import CELL_ID, GRID_ID, _compiled, _overlay, _profile, _terrain
from World.CanonicalCompilation.app.contracts import CompilerError
from World.CanonicalCompilation.app.terrain_surface import finalize_terrain_cells, water_surface_dirty_cells
from World.CanonicalCompilation.app.terrain_water_coverage import support_bounds


def fixture():
    profile = _profile()
    profile["grid"].update(sample_spacing=[30.0, 30.0], cell_quads=[1, 1])
    profile["terrain_surface"]["water_conditioning_limits"] = {
        "maximum_cut_m": 2.0, "maximum_support_distance_m": 45.0,
        "accepted_accuracy": ["fixture"],
    }
    terrain = _terrain()
    cell = terrain[CELL_ID]
    cell.update(bounds=[0, 0, 30, 30], sample_spacing=[30, 30])
    cell["water_fit"] = {"core_samples": [[12.0] * 2 for _ in range(2)],
                         "halo_window": {"samples": [[12.0] * 4 for _ in range(4)]}}
    cell["halo_window"] = deepcopy(cell["water_fit"]["halo_window"])
    water = _compiled().by_owner[CELL_ID][0]
    water["geometry"] = {"type": "LineString", "coordinates": [[0, 15], [30, 15]]}
    water["attributes"].update(width_m=3)
    water["attributes"]["surface_function"].update(level_m=10, accuracy="fixture")
    overlay = _overlay()
    overlay["terrain_patches"] = []
    return terrain, SimpleNamespace(by_owner={CELL_ID: [water]}), profile, overlay


class WaterCoverageTests(unittest.TestCase):
    def test_polygon_inside_quad_with_no_corner_coverage(self):
        terrain, compiled, profile, _ = fixture()
        water = compiled.by_owner[CELL_ID][0]
        water["geometry"] = {"type": "Polygon", "coordinates": [
            [[14, 14], [16, 14], [16, 16], [14, 16], [14, 14]]]}
        bounds = support_bounds(terrain[CELL_ID], [water], profile["terrain_surface"], 0.1, 1)
        self.assertEqual([[10, 10], [10, 10]], [row[1:3] for row in bounds[1:3]])

    def test_between_samples_neighbor_dirty_closure(self):
        terrain, current, profile, _ = fixture()
        neighbor = f"{GRID_ID}:x1:y0"
        water = current.by_owner[CELL_ID][0]
        water["geometry"]["coordinates"] = [[30, 15], [32, 15]]
        water["intersecting_cell_ids"] = [neighbor]
        previous = deepcopy(current)
        previous.by_owner[CELL_ID][0]["attributes"]["surface_function"]["level_m"] = 11
        profile["terrain_surface"]["hydro_transition_radius_m"] = 0.1
        self.assertIn(CELL_ID, water_surface_dirty_cells(current, previous, terrain, profile))

    def test_quality_and_authored_refusal_leave_all_cells_unchanged(self):
        for change, code in (
            (lambda p, w, o: p["terrain_surface"].pop("water_conditioning_limits"), "water_conditioning_policy_required"),
            (lambda p, w, o: p["terrain_surface"]["water_conditioning_limits"].update(maximum_cut_m=1), "water_conditioning_quality_exception"),
            (lambda p, w, o: p["terrain_surface"]["water_conditioning_limits"].update(maximum_support_distance_m=1), "water_conditioning_quality_exception"),
            (lambda p, w, o: w["attributes"]["surface_function"].update(accuracy="unknown"), "water_conditioning_quality_exception"),
            (lambda p, w, o: o["terrain_patches"].append({**_overlay(1)["terrain_patches"][0], "center": [0, 30]}), "authored_height_breaks_water_clearance"),
        ):
            with self.subTest(code=code):
                terrain, compiled, profile, overlay = fixture()
                change(profile, compiled.by_owner[CELL_ID][0], overlay)
                before = deepcopy(terrain)
                with self.assertRaises(CompilerError) as raised:
                    finalize_terrain_cells(terrain, compiled, profile, GRID_ID, overlay)
                self.assertEqual(code, raised.exception.code)
                self.assertEqual(before, terrain)

    def test_hole_flowing_and_shared_halo_support(self):
        terrain, compiled, profile, _ = fixture()
        cell, water = terrain[CELL_ID], compiled.by_owner[CELL_ID][0]
        # A curved, monotonic flowing function: realized triangle interiors
        # need not follow the nearest-segment function across its breaklines.
        water["attributes"]["surface_function"] = {
            "knots": [[0, 15, 10], [15, 25, 11], [30, 15, 12]], "accuracy": "fixture"}
        bounds = support_bounds(cell, [water], profile["terrain_surface"], 0.1, 1)
        self.assertEqual(10, bounds[1][1])
        neighbor = deepcopy(cell)
        neighbor["bounds"] = [30, 0, 60, 30]
        adjacent = support_bounds(neighbor, [water], profile["terrain_surface"], 0.1, 1)
        for row in range(4):
            self.assertEqual(bounds[row][2:], adjacent[row][:2])
        water["geometry"] = {"type": "Polygon", "coordinates": [
            [[-100, -100], [100, -100], [100, 100], [-100, 100], [-100, -100]],
            [[-95, -95], [95, -95], [95, 95], [-95, 95], [-95, -95]]]}
        water["attributes"]["surface_function"] = {"level_m": 10, "accuracy": "fixture"}
        hole = support_bounds(cell, [water], profile["terrain_surface"], 0.1, 1)
        self.assertTrue(all(v == float("inf") for row in hole for v in row))

    def test_policy_schema(self):
        schema_path = Path(__file__).resolve().parents[1] / "contracts/compiler-profile.schema.json"
        schema = json.loads(schema_path.read_text())
        validator = jsonschema.Draft202012Validator(schema["properties"]["terrain_surface"])
        _, _, profile, _ = fixture()
        validator.validate(profile["terrain_surface"])
        for invalid in (-1, "2", None):
            contract = deepcopy(profile["terrain_surface"])
            contract["water_conditioning_limits"]["maximum_cut_m"] = invalid
            self.assertTrue(list(validator.iter_errors(contract)))


if __name__ == "__main__":
    unittest.main()
