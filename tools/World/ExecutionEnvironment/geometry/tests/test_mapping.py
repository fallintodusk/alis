"""Mapping proves its own numeric restoration, never excuses an invalid source cover."""
from copy import deepcopy
from pathlib import Path
import unittest

from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.api import map_triangle_cover, validate_triangle_cover

ROOT = Path(__file__).resolve().parents[5]


class TriangleMappingTests(unittest.TestCase):
    def test_native_arithmetic_projected_restoration_and_reversed_orientation(self):
        source = [[0., 0.], [20., 0.], [0., 2.]]
        target = [[583301.069599373, 4508131.510208816], [583322.349599373, 4508128.510208816],
                  [583300.790400627, 4508129.529791184]]
        a, b, c = source; middle = [10., 1.]
        faces = [[a, b, middle], [a, middle, c]]
        before = deepcopy((source, target, faces))
        result = map_triangle_cover(ROOT, source, target, faces)
        self.assertEqual(before, (source, target, faces))
        self.assertEqual(result, map_triangle_cover(ROOT, source, target, faces))
        for original, mapped in zip(faces, result['triangles']):
            for (x, y), p in zip(original, mapped):
                expected = [target[0][i]+(target[1][i]-target[0][i])*(x/20.)
                            +(target[2][i]-target[0][i])*(y/2.) for i in (0, 1)]
                self.assertEqual(expected, p)
        self.assertLess(result['maximum_vertex_displacement_m'], 1e-9)
        self.assertLess(result['roundtrip_area_bound_m2'], 1e-6)
        reverse = map_triangle_cover(ROOT, list(reversed(source)), list(reversed(target)), faces)
        self.assertEqual(len(result['triangles']), len(reverse['triangles']))

    def test_missing_duplicate_exterior_and_invalid_transforms_refuse(self):
        source = [[0., 0.], [20., 0.], [0., 2.]]
        target = [[583300., 4508130.], [583320., 4508127.], [583300., 4508132.]]
        faces = [[source[0], source[1], [10., 1.]], [source[0], [10., 1.], source[2]]]
        for kind in ('missing', 'duplicate', 'exterior', 'nan', 'boolean', 'collapsed', 'unknown', 'empty'):
            a, b, mesh = deepcopy((source, target, faces))
            if kind == 'missing': mesh.pop()
            if kind == 'duplicate': mesh.append(deepcopy(mesh[0]))
            if kind == 'exterior': mesh[1][2] = [-1., 2.]
            if kind == 'nan': b[0][0] = float('nan')
            if kind == 'boolean': a[0][0] = True
            if kind == 'collapsed': b[2] = b[0]
            if kind == 'unknown': b = None
            if kind == 'empty': mesh.clear()
            with self.subTest(kind=kind), self.assertRaises(ExecutionEnvironmentError):
                map_triangle_cover(ROOT, a, b, mesh)

    def test_general_cover_remains_strict_without_owned_restoration(self):
        region = {'type': 'Polygon', 'coordinates': [[[0., 0.], [10., 0.], [0., 10.], [0., 0.]]]}
        with self.assertRaises(ExecutionEnvironmentError):
            validate_triangle_cover(ROOT, region, [[[-.001, 0.], [10., 0.], [0., 10.]]])


if __name__ == '__main__': unittest.main()
