"""Direct native polygon triangulation preserves source boundaries and strict coverage."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[5]
sys.path.insert(0, str(ROOT/'tools'))
from World.ExecutionEnvironment.geometry.api import triangulate_polygon, validate_triangle_cover
from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from .polygon_fixture import fixture


class PolygonTriangulationTests(unittest.TestCase):
    def test_captured_projected_parent_preserves_vertices_and_strict_cover(self):
        parent = fixture(); before = deepcopy(parent)
        triangles = triangulate_polygon(ROOT, parent, 32)
        self.assertEqual(before, parent)
        self.assertEqual(triangles, triangulate_polygon(ROOT, parent, 32))
        vertices = {tuple(p) for poly in parent['coordinates'] for ring in poly for p in ring}
        self.assertTrue(all(tuple(p) in vertices for face in triangles for p in face))
        validate_triangle_cover(ROOT, parent, triangles)
        with self.assertRaises(ExecutionEnvironmentError): validate_triangle_cover(ROOT, parent, triangles[1:])
        with self.assertRaises(ExecutionEnvironmentError): triangulate_polygon(ROOT, parent, 2)

    def test_hole_and_disconnected_components_keep_exact_area(self):
        first = [[[0., 0.], [10., 0.], [10., 10.], [0., 10.], [0., 0.]],
                 [[4., 4.], [4., 6.], [6., 6.], [6., 4.], [4., 4.]]]
        second = [[[20., 0.], [22., 0.], [22., 2.], [20., 2.], [20., 0.]]]
        for polygons, expected in (([first], 96.), ([first, second], 100.)):
            parent = {'type': 'MultiPolygon', 'coordinates': polygons}
            triangles = triangulate_polygon(ROOT, parent, 32)
            area = sum(abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/2 for a, b, c in triangles)
            self.assertEqual(expected, area)
            validate_triangle_cover(ROOT, parent, triangles)

    def test_invalid_shape_nonfinite_coordinates_and_unknown_budget_refuse(self):
        for kind in ('none', 'empty', 'open', 'nonfinite', 'budget', 'boolean'):
            parent, budget = fixture(), 32
            if kind == 'none': parent = None
            if kind == 'empty': parent['coordinates'] = []
            if kind == 'open': parent['coordinates'][0][0].pop()
            if kind == 'nonfinite': parent['coordinates'][0][0][1][0] = float('nan')
            if kind == 'budget': budget = 0
            if kind == 'boolean': budget = True
            with self.subTest(kind=kind), self.assertRaises(ExecutionEnvironmentError):
                triangulate_polygon(ROOT, parent, budget)


if __name__ == '__main__': unittest.main()
