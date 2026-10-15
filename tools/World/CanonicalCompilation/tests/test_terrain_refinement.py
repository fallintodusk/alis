from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from World.CanonicalCompilation import api
from World.CanonicalCompilation.app.contracts import CompilerError, canonical_hash


def fixture():
    grid = "grid_0000000000000000"
    cells = {}
    for column in range(2):
        identifier = f"{grid}:x{column}:y0"
        cells[identifier] = {
            "grid_id": grid, "cell_id": identifier,
            "bounds": [30 * column, 0, 30 * (column + 1), 30],
            "sample_spacing": [30, 30], "core_samples": [[12, 12], [12, 12]],
            "height_quantization": 0.1,
            "surface_semantics": {"roles": ["ground", "hydro_transition"],
                                  "core_samples": {"ground": [[1, 1], [1, 1]],
                                                   "hydro_transition": [[0, 0], [0, 0]]}},
        }
    water = {"feature_id": "fixture:water", "feature_class": "water",
             "geometry": {"type": "Polygon", "coordinates": [
                 [[8, 12], [52, 12], [52, 18], [8, 18], [8, 12]]]},
             "attributes": {"surface_function": {"level_m": 10, "accuracy": "fixture"}}}
    policy = {"accepted_accuracy": ["fixture"], "maximum_cut_m": 2,
              "maximum_fill_m": 0, "maximum_dry_distance_m": 45,
              "maximum_bank_gradient": 1, "maximum_triangles": 128,
              "water_clearance_m": 0}
    selected = {key: [(0, 0)] for key in cells}
    return cells, water, policy, selected


class TerrainRefinementTests(unittest.TestCase):
    def refine(self, cells, water, policy, selected):
        operation = getattr(api, "refine_terrain_cells", None)
        self.assertTrue(callable(operation), "Canonical authority cannot publish a constrained final surface")
        return operation(ROOT, cells, water, policy, selected)

    def test_between_samples_and_shared_edge(self):
        cells, water, policy, selected = fixture()
        before = deepcopy(cells)
        result = self.refine(cells, water, policy, selected)
        self.assertEqual(cells, before)
        self.assertEqual(set(cells), set(result))
        seams = []
        for key, surface in sorted(result.items()):
            self.assertGreater(len(surface["quads"][0]["triangles"]), 2)
            vertices = surface["quads"][0]["vertices"]
            seams.append(sorted(v for v in vertices if v[0] == 30))
            for triangle in surface["quads"][0]["water_triangles"]:
                self.assertTrue(all(vertices[i][2] <= 10 for i in surface["quads"][0]["triangles"][triangle]))
            self.assertEqual(2, surface["metrics"]["maximum_cut_m"])
        self.assertEqual(seams[0], seams[1])
        self.assertEqual(canonical_hash(result), canonical_hash(self.refine(cells, water, policy, selected)))

    def test_excluded_source_and_policy_refuse_unchanged(self):
        for change in ("low_confidence_point_fallback", "cut", "gradient", "flowing", "neighbor", "authored"):
            cells, water, policy, selected = fixture()
            if change == "cut":
                policy["maximum_cut_m"] = 1
            elif change == "gradient":
                policy["maximum_bank_gradient"] = 0.01
            elif change == "flowing":
                water["attributes"]["surface_function"] = {"knots": [[0, 0, 10], [60, 30, 11]], "accuracy": "fixture"}
            elif change == "neighbor":
                selected.pop(next(reversed(selected)))
            elif change == "authored":
                cells[next(iter(cells))]["authored_patch_ids"] = ["fixture:authored"]
            else:
                water["attributes"]["surface_function"]["accuracy"] = change
            before = deepcopy(cells)
            with self.subTest(change=change), self.assertRaises(CompilerError):
                self.refine(cells, water, policy, selected)
            self.assertEqual(before, cells)

    def test_hole_and_remote_context(self):
        cells, water, policy, selected = fixture()
        water["geometry"]["coordinates"].append([[12, 14], [12, 16], [18, 16], [18, 14], [12, 14]])
        result = self.refine(cells, water, policy, selected)
        left = next(iter(cells))
        surface = result[left]
        for index in surface["quads"][0]["water_triangles"]:
            vertices = surface["quads"][0]["vertices"]
            triangle = [vertices[i] for i in surface["quads"][0]["triangles"][index]]
            center = [sum(v[i] for v in triangle) / 3 for i in range(2)]
            self.assertFalse(12 < center[0] < 18 and 14 < center[1] < 16)
        remote = deepcopy(cells[left])
        remote.update(cell_id="grid_0000000000000000:x10:y0", bounds=[300, 0, 330, 30])
        cells[remote["cell_id"]] = remote
        selected[remote["cell_id"]] = [(0, 0)]
        again = self.refine(cells, water, policy, selected)
        self.assertEqual(canonical_hash(result[left]), canonical_hash(again[left]))

    def test_mismatched_neighbor_refuses(self):
        cells, water, policy, selected = fixture()
        cells[next(reversed(cells))]["core_samples"] = [[13, 13], [13, 13]]
        policy["maximum_cut_m"] = 3
        with self.assertRaises(CompilerError) as error:
            self.refine(cells, water, policy, selected)
        self.assertEqual(error.exception.code, "refinement_seam_conflict")


if __name__ == "__main__":
    unittest.main()
