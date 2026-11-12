"""Finite arrangement coverage, source constraints, locality and refusal controls."""
from collections import Counter
from copy import deepcopy
from pathlib import Path
import unittest
from unittest.mock import patch
import math

from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from .. import constrained
from ..api import execution_identity, triangulate_points, validate_triangle_cover
from ..app.kernel import _roundtrip_area_bound
from .constraints_native_fixture import ALL_SOURCE_SEGMENTS, PARENT, RESTORATION_STATION, SEGMENTS

ROOT = Path(__file__).resolve().parents[5]


def box(x=0., y=0., size=10.):
    return {'type': 'Polygon', 'coordinates': [[[x, y], [x+size, y], [x+size, y+size], [x, y+size], [x, y]]]}


def crossing_count(faces, segments):
    def side(a, b, c):
        return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    return sum(side(a, b, u)*side(a, b, v) < 0 and side(u, v, a)*side(u, v, b) < 0
               for face in faces for a, b in zip(face, (*face[1:], face[0])) for u, v in segments)


class ConstraintTriangulationTests(unittest.TestCase):
    def test_closed_cells_do_not_gain_scaffold_stations(self):
        # A closed projected quad with a required non-Delaunay diagonal reduces
        # the coastal retessellation case without its height or cooking policy.
        a, b, c, d = [[583393.41, 4508292.45], [583396.41, 4508292.45],
                      [583395.41, 4508294.45], [583393.41, 4508293.45]]
        domain = {'type': 'Polygon', 'coordinates': [[a, b, c, d, a]]}
        result = constrained.triangulate_constraints(ROOT, domain, [[b, d]], [], 32)
        validate_triangle_cover(ROOT, domain, result)
        self.assertEqual({tuple(p) for p in (a, b, c, d)}, {tuple(p) for f in result for p in f})
        self.assertEqual(2, len(result))
        self.assertEqual(0, crossing_count(result, [[b, d]]))

    def test_closed_cycles_connected_by_a_cut_edge_keep_the_bridge(self):
        rings = [box(2., 2., 2.)['coordinates'][0], box(6., 2., 2.)['coordinates'][0]]
        segments = [[a, b] for ring in rings for a, b in zip(ring, ring[1:])]
        bridge = [[4., 3.], [6., 3.]]
        segments.append(bridge)
        result = constrained.triangulate_constraints(ROOT, box(), segments, [], 128)
        validate_triangle_cover(ROOT, box(), result)
        self.assertEqual(0, crossing_count(result, segments))
        edges = {_edge for f in result for a, b in zip(f, (*f[1:], f[0]))
                 for _edge in [tuple(sorted((tuple(a), tuple(b))))]}
        self.assertIn(tuple(map(tuple, bridge)), edges)

    def test_closed_holes_and_disconnected_components_need_no_artificial_nodes(self):
        first = box()['coordinates']
        first.append([[4., 4.], [4., 6.], [6., 6.], [6., 4.], [4., 4.]])
        domain = {'type': 'MultiPolygon', 'coordinates': [first, box(20., 0., 2.)['coordinates']]}
        result = constrained.triangulate_constraints(ROOT, domain, [], [], 128)
        validate_triangle_cover(ROOT, domain, result)
        self.assertEqual({tuple(p) for part in domain['coordinates'] for ring in part for p in ring},
                         {tuple(p) for face in result for p in face})

    def test_closed_cells_still_refuse_a_sub_budget_missing_face(self):
        ring = [[5., 5.], [5.0000001, 5.], [5., 5.0000001], [5., 5.]]
        segments = [[a, b] for a, b in zip(ring, ring[1:])]
        original = constrained._Geometry.triangles
        removed = []
        def lose(native, region, capacity):
            result = []
            for face in original(native, region, capacity):
                a, b, c = face
                area = abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/2
                if not removed and 0 < area < 1e-12:
                    removed.append(face)
                else:
                    result.append(face)
            return result
        with patch.object(constrained._Geometry, 'triangles', lose):
            with self.assertRaisesRegex(ExecutionEnvironmentError, 'nonconforming cell boundary'):
                constrained.triangulate_constraints(ROOT, box(), segments, [], 128)
        self.assertEqual(1, len(removed))

    def test_owned_origin_restoration_uses_corresponding_vertices(self):
        origin = min(p for ring in PARENT['coordinates'] for p in ring)
        shift = lambda p: [p[i]-origin[i] for i in (0, 1)]
        local_parent = {'type': 'Polygon', 'coordinates': [[shift(p) for p in ring] for ring in PARENT['coordinates']]}
        local = constrained.triangulate_constraints(ROOT, local_parent,
            [[shift(p) for p in segment] for segment in ALL_SOURCE_SEGMENTS], [shift(RESTORATION_STATION)], 512)
        validate_triangle_cover(ROOT, local_parent, local)
        bounds, displacements = [], []
        def measure(before, after):
            value = _roundtrip_area_bound(before, after)
            bounds.append(value)
            displacements.extend(math.dist(a, b) for left, right in zip(before, after, strict=True)
                                 for a, b in zip(left, right, strict=True))
            return value
        with patch.object(constrained, '_roundtrip_area_bound', measure, create=True):
            actual = constrained.triangulate_constraints(ROOT, PARENT, ALL_SOURCE_SEGMENTS, [RESTORATION_STATION], 512)
        self.assertEqual(1, len(bounds))
        self.assertGreater(bounds[0], 0)
        self.assertLess(bounds[0], 1e-6)
        self.assertLess(max(displacements), 1e-9)
        expected = [[[p[i]+origin[i] for i in (0, 1)] for p in face] for face in local]
        canonical = lambda faces: sorted(tuple(sorted(tuple(p) for p in face)) for face in faces)
        self.assertEqual(canonical(expected), canonical(actual))
        with self.assertRaises(ExecutionEnvironmentError):
            validate_triangle_cover(ROOT, PARENT, actual)

    def test_complete_source_axes_share_one_noding_operation(self):
        before = deepcopy((PARENT, ALL_SOURCE_SEGMENTS))
        faces = constrained.triangulate_constraints(ROOT, PARENT, ALL_SOURCE_SEGMENTS, [], 512)
        validate_triangle_cover(ROOT, PARENT, faces)
        self.assertEqual(before, (PARENT, ALL_SOURCE_SEGMENTS))
        required = {tuple(p) for ring in PARENT['coordinates'] for p in ring}
        required.update(tuple(p) for segment in ALL_SOURCE_SEGMENTS for p in segment)
        self.assertTrue(required <= {tuple(p) for face in faces for p in face})

    def test_projected_concave_parent_with_original_interior_axis(self):
        before = deepcopy((PARENT, SEGMENTS))
        faces = constrained.triangulate_constraints(ROOT, PARENT, SEGMENTS, [], 512)
        validate_triangle_cover(ROOT, PARENT, faces)
        self.assertEqual(before, (PARENT, SEGMENTS))
        required = {tuple(p) for ring in PARENT['coordinates'] for p in ring}
        required.update(tuple(p) for segment in SEGMENTS for p in segment)
        self.assertTrue(required <= {tuple(p) for face in faces for p in face})

    def test_sub_budget_interior_crack_refuses_with_all_nodes_and_edges_retained(self):
        domain = box()
        points = [[5., 5.], [5.0000001, 5.], [5., 5.0000001]]
        healthy = constrained.triangulate_constraints(ROOT, domain, [], points, 128)
        validate_triangle_cover(ROOT, domain, healthy)
        original = constrained._Geometry.triangles
        removed, emitted = [], []
        def lose_tiny_face(native, region, capacity):
            result = []
            for face in original(native, region, capacity):
                a, b, c = face
                area = abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/2
                if not removed and 0 < area < 1e-12 and all(4 < v < 6 for p in face for v in p):
                    removed.append(face)
                else:
                    result.append(face)
            emitted.extend(result)
            return result
        with patch.object(constrained._Geometry, 'triangles', lose_tiny_face):
            with self.assertRaisesRegex(ExecutionEnvironmentError, 'nonconforming cell boundary'):
                constrained.triangulate_constraints(ROOT, domain, [], points, 128)
        self.assertEqual(1, len(removed))
        # Area checks cannot distinguish this real crack; topology must reject it.
        validate_triangle_cover(ROOT, domain, emitted)
        self.assertTrue({tuple(p) for p in points} <= {tuple(p) for face in emitted for p in face})
        edges = {tuple(sorted((tuple(a), tuple(b)))) for face in emitted for a, b in zip(face, (*face[1:], face[0]))}
        self.assertTrue(all(tuple(sorted((tuple(a), tuple(b)))) in edges
                            for a, b in zip(removed[0], (*removed[0][1:], removed[0][0]))))

    def test_unclosed_native_scaffold_is_not_silently_discarded(self):
        original = constrained._Geometry.read
        injected = []
        def dangling_scaffold(native, value):
            if value.get('type') == 'GeometryCollection':
                value = deepcopy(value)
                value['geometries'].append({'type': 'LineString', 'coordinates': [[2., 3.], [2.25, 3.1]]})
                injected.append(True)
            return original(native, value)
        with patch.object(constrained._Geometry, 'read', dangling_scaffold):
            with self.assertRaisesRegex(ExecutionEnvironmentError, 'unclosed source or scaffold edge'):
                constrained.triangulate_constraints(ROOT, box(), [], [], 128)
        self.assertTrue(injected, 'The native fault must actually exercise the refusal path')

    def test_crossing_constraints_and_source_stations_survive_together(self):
        domain = box()
        segments = [[[0., 4.], [10., 4.]], [[2., 0.], [2., 10.]], [[0., 0.], [10., 10.]]]
        points = [[1., 1.], [8., 7.], [5., 2.], [6., 9.]]
        before = deepcopy((domain, segments, points))
        cloud = sorted({tuple(p) for p in domain['coordinates'][0]+points+[p for s in segments for p in s]})
        cloud = [list(p) for p in cloud]
        naive = [[cloud[i] for i in face] for face in triangulate_points(ROOT, cloud)]
        self.assertGreater(crossing_count(naive, segments), 0, 'Point Delaunay must expose the missing constrained seam')
        faces = constrained.triangulate_constraints(ROOT, domain, segments, points, 128)
        self.assertEqual(0, crossing_count(faces, segments))
        self.assertEqual(before, (domain, segments, points))
        self.assertEqual(faces, constrained.triangulate_constraints(ROOT, domain, list(reversed(segments)), list(reversed(points)), 128))
        vertices = {tuple(p) for face in faces for p in face}
        self.assertTrue({tuple(p) for p in cloud} <= vertices)
        self.assertTrue({(2., 4.), (4., 4.), (2., 2.)} <= vertices)
        validate_triangle_cover(ROOT, domain, faces)
        with self.assertRaises(ExecutionEnvironmentError):
            validate_triangle_cover(ROOT, domain, faces[1:])

    def test_projected_inner_creases_and_original_half_diagonal(self):
        # Reduced original-quad and inner-face constraints, with their captured XY.
        domain = box(583423.41, 4508172.45, 30.)
        inner = [[583425.756997617, 4508174.243002383], [583424.728460444, 4508174.243002383],
                 [583424.728460444, 4508175.271539556], [583428., 4508176.],
                 [583428., 4508174.243002383], [583424.728460444, 4508176.]]
        faces = [[inner[i] for i in indices] for indices in ((0, 1, 2), (3, 4, 0), (0, 2, 5))]
        segments = [[a, b] for face in faces for a, b in zip(face, (*face[1:], face[0]))]
        segments.append([[583453.41, 4508202.45], [583423.41, 4508172.45]])
        points = [[583423.41+3*x, 4508172.45+3*y] for x in range(11) for y in range(11)]
        result = constrained.triangulate_constraints(ROOT, domain, segments, points, 1024)
        validate_triangle_cover(ROOT, domain, result)
        vertices = {tuple(p) for face in result for p in face}
        self.assertTrue({tuple(p) for p in inner+points} <= vertices)
        edges = Counter(tuple(sorted((tuple(a), tuple(b)))) for face in result for a, b in zip(face, (*face[1:], face[0])))
        self.assertTrue(all(count in (1, 2) for count in edges.values()))
        self.assertTrue(all(a[0] == b[0] and a[0] in (583423.41, 583453.41) or
                            a[1] == b[1] and a[1] in (4508172.45, 4508202.45)
                            for (a, b), count in edges.items() if count == 1))

    def test_holes_and_disconnected_components_are_preserved(self):
        first = box()['coordinates']
        first.append([[4., 4.], [4., 6.], [6., 6.], [6., 4.], [4., 4.]])
        domain = {'type': 'MultiPolygon', 'coordinates': [first, box(20., 0., 2.)['coordinates']]}
        result = constrained.triangulate_constraints(ROOT, domain, [[[0., 2.], [10., 2.]]], [[21., 1.]], 128)
        validate_triangle_cover(ROOT, domain, result)
        self.assertEqual(100., sum(abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/2 for a, b, c in result))

    def test_unknown_outside_hole_crossing_and_capacity_refuse_without_mutation(self):
        for case in ('nan', 'boolean', 'outside_point', 'outside_segment', 'hole_point', 'hole_crossing', 'collapsed', 'budget', 'input_limit', 'open_ring'):
            domain, segments, points, budget = box(), [], [], 32
            if case == 'nan': points = [[float('nan'), 2.]]
            elif case == 'boolean': points = [[True, 2.]]
            elif case == 'outside_point': points = [[11., 2.]]
            elif case == 'outside_segment': segments = [[[0., 0.], [11., 2.]]]
            elif case in ('hole_point', 'hole_crossing'):
                domain['coordinates'].append([[4., 4.], [4., 6.], [6., 6.], [6., 4.], [4., 4.]])
                if case == 'hole_point': points = [[5., 5.]]
                else: segments = [[[0., 5.], [10., 5.]]]
            elif case == 'collapsed': segments = [[[1., 1.], [1., 1.]]]
            elif case == 'budget': budget = 1
            elif case == 'input_limit': points = [[1., 1.]]*4097
            else: domain['coordinates'][0].pop()
            before = repr((domain, segments, points, budget))
            with self.subTest(case=case), self.assertRaises(ExecutionEnvironmentError):
                constrained.triangulate_constraints(ROOT, domain, segments, points, budget)
            self.assertEqual(before, repr((domain, segments, points, budget)))

    def test_optional_identity_does_not_change_unconsumed_parent(self):
        parent = execution_identity(ROOT, True)
        own = constrained.execution_identity(ROOT, True)
        self.assertNotEqual(parent['identity_sha256'], own['identity_sha256'])
        read_bytes = Path.read_bytes
        def changed(path):
            result = read_bytes(path)
            return result+b'\n' if path.resolve() == Path(constrained.__file__).resolve() else result
        with patch.object(Path, 'read_bytes', changed):
            self.assertEqual(parent, execution_identity(ROOT, True))
            self.assertNotEqual(own['identity_sha256'], constrained.execution_identity(ROOT, True)['identity_sha256'])


if __name__ == '__main__': unittest.main()
