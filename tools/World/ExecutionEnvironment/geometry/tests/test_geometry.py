from pathlib import Path
from copy import deepcopy
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[5]
sys.path.insert(0, str(ROOT / 'tools'))

from World.ExecutionEnvironment.geometry.api import partition_triangles, validate_triangle_cover
from World.ExecutionEnvironment.api import ExecutionEnvironmentError


def area(triangles):
    return sum(abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / 2
               for a, b, c in triangles)


def polygon(points):
    return {'type': 'Polygon', 'coordinates': [[*points, points[0]]]}


class NativeGeometryPrecisionTests(unittest.TestCase):
    def test_source_shaped_partition_survives_collapsed_coordinate_roundtrip(self):
        face = [[379600.4131159258, 6185265.473115926],
                [379600.70150859066, 6185266.302864549],
                [379463.1731159258, 6185313.173115926]]
        clip = polygon([[379460., 6185340.], [379490., 6185340.], [379460., 6185310.]])
        before = deepcopy((face, clip))
        result = partition_triangles(ROOT, face, clip, 16)
        emitted = result['wet'] + result['dry']
        self.assertTrue(emitted)
        self.assertTrue(all(area([triangle]) > 0 for triangle in emitted))
        self.assertAlmostEqual(area([face]), area(emitted), delta=1e-8)
        self.assertEqual([], result['wet'], 'The sub-ULP intersection has no representable area')
        with self.assertRaises(ExecutionEnvironmentError):
            validate_triangle_cover(ROOT, polygon(face), [])
        self.assertEqual(before, (face, clip))

    def test_collapsed_roundtrip_cannot_hide_real_area_or_native_failure(self):
        from World.ExecutionEnvironment.geometry.app.kernel import _Geometry
        parent = [[379600., 6185260.], [379610., 6185260.], [379600., 6185270.]]
        before = deepcopy(parent)
        bind = _Geometry.bind
        for value, code in ((0, 'native_geometry_coverage'), (2, 'native_geometry_failed'),
                            (float('nan'), 'native_geometry_failed')):
            def instrument(native, name, arguments, result):
                return (lambda *unused: value) if name == 'GEOSOrientationIndex_r' else bind(native, name, arguments, result)
            with self.subTest(orientation=value), patch.object(_Geometry, 'bind', instrument):
                with self.assertRaises(ExecutionEnvironmentError) as error:
                    partition_triangles(ROOT, parent, polygon(parent), 16)
                self.assertEqual(code, error.exception.code)
        self.assertEqual(before, parent)

    def test_source_shaped_collinear_cut_keeps_original_domain(self):
        face = [[381048.96245729626, 6185538.77415762],
                [381049.2372796766, 6185538.765088406],
                [381048.3629969157, 6185569.228527602]]
        ground = [[381050., 6185550.], [381050., 6185520.], [381020., 6185520.]]
        before = deepcopy((face, ground))
        result = partition_triangles(ROOT, face, polygon(ground), 16)
        self.assertAlmostEqual(area([face]), area(result['wet']+result['dry']), delta=1e-8)
        self.assertGreater(area(result['dry']), 1.8)
        validate_triangle_cover(ROOT, polygon(face), result['wet']+result['dry'])
        self.assertEqual(before, (face, ground))

    def test_thin_intersection_is_translation_equivalent_at_map_coordinates(self):
        face = [[381033.86088205234, 6185623.860857243],
                [381033.8608788545, 6185623.860878855],
                [381036.821015625, 6185626.821015625]]
        ground = [[381050., 6185640.], [381050., 6185610.], [381020., 6185610.]]
        origin = [381020, 6185610]
        local_face = [[x - origin[0], y - origin[1]] for x, y in face]
        local_ground = [[x - origin[0], y - origin[1]] for x, y in ground]
        reference = partition_triangles(ROOT, local_face, polygon(local_ground), 16)
        result = partition_triangles(ROOT, face, polygon(ground), 16)
        self.assertGreater(area(result['wet']), 3.6e-5, 'Map coordinates lost a real wet intersection')
        self.assertAlmostEqual(area(reference['wet']), area(result['wet']), delta=2e-9)
        self.assertLess(area(result['dry']), 2e-9)
        self.assertAlmostEqual(area([face]), area(result['wet']) + area(result['dry']), delta=2e-9)
        validate_triangle_cover(ROOT, polygon(face), result['wet'] + result['dry'])

    def test_failed_convex_distance_proof_refuses_without_mutation(self):
        face = [[381048.96245729626, 6185538.77415762], [381049.2372796766, 6185538.765088406],
                [381048.3629969157, 6185569.228527602]]
        ground = [[381050., 6185550.], [381050., 6185520.], [381020., 6185520.]]
        before = deepcopy((face, ground))
        for value in (float('nan'), float('inf'), -1, 1):
            with self.subTest(distance=value), patch(
                    'World.ExecutionEnvironment.geometry.app.kernel._Geometry.distance_to', return_value=value):
                with self.assertRaises(ExecutionEnvironmentError):
                    partition_triangles(ROOT, face, polygon(ground), 16)
        self.assertEqual(before, (face, ground))

    def test_equal_area_hole_and_overlap_cannot_cancel(self):
        parent = [[0., 0.], [2., 0.], [0., 2.]]
        faces = [[[0., 0.], [1., 0.], [0., 1.]], [[1., 0.], [2., 0.], [1., 1.]],
                 [[0., 1.], [1., 1.], [0., 2.]], [[1., 0.], [1., 1.], [0., 1.]]]
        validate_triangle_cover(ROOT, polygon(parent), faces)
        faces[-1] = faces[0]
        self.assertEqual(area([parent]), area(faces))
        with self.assertRaises(ExecutionEnvironmentError):
            validate_triangle_cover(ROOT, polygon(parent), faces)

    def test_convex_certificate_is_not_used_across_a_hole(self):
        parent = polygon([[0., 0.], [10., 0.], [0., 10.]])
        parent['coordinates'].append([[1., 1.], [1., 2.], [2., 1.], [1., 1.]])
        # Every triangle vertex belongs to the polygon; its interior crosses the hole.
        with self.assertRaises(ExecutionEnvironmentError):
            validate_triangle_cover(ROOT, parent, [[[0., 0.], [10., 0.], [0., 10.]]])

    def test_native_distance_error_and_empty_values_are_not_zero(self):
        from World.ExecutionEnvironment.geometry.app.kernel import _Geometry
        native = _Geometry(ROOT)
        self.addCleanup(native.close)
        region = native.read(polygon([[0, 0], [1, 0], [0, 1]]))
        for status, value in ((0, 0), (1, float('nan')), (1, float('inf')), (1, -1)):
            def result(context, point, target, output):
                output._obj.value = value
                return status
            with self.subTest(status=status, value=value), patch.object(native, 'distance', result):
                with self.assertRaises(ExecutionEnvironmentError):
                    native.distance_to([2., 2.], region)

    def test_translated_hole_and_coverage_refusal_keep_the_same_contract(self):
        triangle = [[381000, 6185000], [381010, 6185000], [381000, 6185010]]
        domain = polygon(triangle)
        footprint = polygon([[381000, 6185000], [381010, 6185000], [381010, 6185010], [381000, 6185010]])
        footprint['coordinates'].append([[381001, 6185001], [381001, 6185002],
                                          [381002, 6185002], [381002, 6185001], [381001, 6185001]])
        before = deepcopy((triangle, footprint))
        result = partition_triangles(ROOT, triangle, footprint, 32)
        self.assertAlmostEqual(49, area(result['wet']))
        self.assertAlmostEqual(1, area(result['dry']))
        validate_triangle_cover(ROOT, domain, result['wet'] + result['dry'])
        for defect in (result['wet'], result['wet'] + result['dry'] + result['dry'],
                       result['wet'] + result['dry'] + [[[0, 0], [1, 0], [0, 1]]]):
            with self.subTest(defect=len(defect)), self.assertRaises(ExecutionEnvironmentError) as error:
                validate_triangle_cover(ROOT, domain, defect)
            self.assertEqual('native_geometry_coverage', error.exception.code)
        self.assertEqual(before, (triangle, footprint))

    def test_large_domain_keeps_absolute_exterior_refusal(self):
        triangle = [[0, 0], [1000000, 0], [0, 1000000]]
        exterior = [[-1, -1], [-.99, -1], [-1, -.99]]
        with self.assertRaises(ExecutionEnvironmentError) as error:
            validate_triangle_cover(ROOT, polygon(triangle), [triangle, exterior])
        self.assertEqual('native_geometry_coverage', error.exception.code)

    def test_translated_concave_multipolygon_keeps_disconnected_parts(self):
        ox, oy = 381000, 6185000
        def shift(points):
            return [[x + ox, y + oy] for x, y in points]
        triangle = shift([[0, 0], [6, 0], [0, 6]])
        footprint = {'type': 'MultiPolygon', 'coordinates': [
            [shift([[0, 0], [4, 0], [4, 1], [1, 1], [1, 4], [0, 4], [0, 0]])],
            [shift([[2, 2], [3, 2], [2, 3], [2, 2]])]]}
        result = partition_triangles(ROOT, triangle, footprint, 32)
        self.assertAlmostEqual(7.5, area(result['wet']))
        self.assertAlmostEqual(10.5, area(result['dry']))
        validate_triangle_cover(ROOT, polygon(triangle), result['wet'] + result['dry'])

    def test_unknown_numeric_allowance_cannot_accept_partition(self):
        triangle = [[0, 0], [10, 0], [0, 10]]
        before = deepcopy(triangle)
        for value in (float('nan'), float('inf'), -1):
            with self.subTest(value=value), patch(
                    'World.ExecutionEnvironment.geometry.app.kernel._roundtrip_area_bound', return_value=value):
                with self.assertRaises(ExecutionEnvironmentError) as error:
                    partition_triangles(ROOT, triangle, polygon(triangle), 16)
                self.assertEqual('native_geometry_coverage', error.exception.code)
        self.assertEqual(before, triangle)


if __name__ == '__main__':
    unittest.main()
