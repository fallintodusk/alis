"""Unpublished provider-boundary composition, protected creases and safe refusals."""
from copy import deepcopy
from pathlib import Path
import unittest
from unittest.mock import patch

from World.CanonicalCompilation.app.contracts import CompilerError, canonical_hash
from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.api import validate_triangle_cover
from ..refinement import ground, ground_composite as operation
from ..refinement.app.road_elevation import _height
from .test_ground_surface import fixture as source_fixture
from .ground_composite_fixture import fixture as captured_fixture

ROOT = Path(__file__).resolve().parents[4]


def fixture():
    cells, grid, evidence, selected = source_fixture()
    a, b, c, d, center = [20., 10., 11.], [40., 10., 11.], [40., 20., 11.], [20., 20., 11.], [30., 15., 10.]
    inner = {'version': 1, 'basis': 'identified_model', 'canonical_crs': 'EPSG:32618', 'vertical_datum': 'EPSG:3855',
        'model_sha256': 'f'*64, 'footprint': {'type': 'Polygon', 'coordinates': [[a[:2], b[:2], c[:2], d[:2], a[:2]]]},
        'triangles': [[a, b, center], [b, c, center], [c, d, center], [d, a, center]]}
    return cells, grid, evidence, selected, inner


def triangles(result):
    return [[quad['vertices'][i][:3] for i in face] for cell in result['surfaces'].values()
            for quad in cell['quads'] for face in quad['triangles']]


class GroundCompositeTests(unittest.TestCase):
    def build(self, values, budget=256):
        return operation.build_ground_composite(ROOT, *values, canonical_crs='EPSG:32618', maximum_triangles=budget)

    def test_source_only_misses_inner_field_and_composite_preserves_complete_crease(self):
        values = fixture()
        cells, grid, evidence, selected, inner = values
        original = ground.build_ground_surface(ROOT, cells, grid, evidence, selected, maximum_triangles=32, canonical_crs='EPSG:32618')
        original_faces = triangles(original)
        self.assertGreater(abs(_height(original_faces, [20., 10.])-11.), .1)
        before = deepcopy(values)
        result = self.build(values)
        self.assertEqual(before, values)
        self.assertEqual(result, self.build(values))
        self.assertEqual('unpublished_model_space', result['publication'])
        self.assertEqual('provider_node_trace_requires_neighbor_continuation', result['boundary_condition'])
        self.assertNotIn('$schema', result)
        output = triangles(result)
        for face in inner['triangles']:
            for point in [*face, [sum(p[i] for p in face)/3 for i in range(3)]]:
                self.assertAlmostEqual(point[2], _height(output, point[:2]), places=10)
        left, right = [result['surfaces'][key]['quads'][0] for key in cells]
        self.assertEqual(sorted(v for v in left['vertices'] if v[0] == 30.), sorted(v for v in right['vertices'] if v[0] == 30.))
        self.assertTrue(any(v[:2] == [0., 15.] for v in left['vertices']), 'Provider trace retains stations forbidden on historical boundaries')
        for quad in (left, right):
            self.assertTrue(all(0 <= v <= 1 for p in quad['vertices'] for v in p[3:]))
            self.assertTrue(all(abs(sum(p[3:])-1) < 1e-12 for p in quad['vertices']))

    def test_equal_affine_fields_remain_equal_everywhere(self):
        values = fixture()
        cells, grid, evidence, selected, inner = values
        field = lambda x, y: 3.+.01*x+.02*y
        for y, row in enumerate(grid['samples']):
            for x in range(len(row)):
                row[x] = field(grid['origin'][0]+x*15, grid['origin'][1]-y*15)
        evidence['grid_content_sha256'] = canonical_hash(grid)
        for face in inner['triangles']:
            for p in face:
                p[2] = field(*p[:2])
        result = self.build(values)
        for face in triangles(result):
            for p in face:
                self.assertAlmostEqual(field(*p[:2]), p[2], places=12)
        self.assertNotEqual(result['consumed']['boundary_trace_sha256'], self.build(fixture())['consumed']['boundary_trace_sha256'])

    def test_captured_quad_preserves_all_inner_planes_and_provider_boundary(self):
        values = captured_fixture()
        result = self.build(values, budget=1024)
        cell = values[0]['captured_quad']
        quad = result['surfaces']['captured_quad']['quads'][0]
        output = triangles(result)
        for face in values[-1]['triangles']:
            for p in [*face, [sum(v[i] for v in face)/3 for i in range(3)]]:
                self.assertAlmostEqual(p[2], _height(output, p[:2]), places=9)
        west, south, east, north = cell['bounds']
        boundary = [p for p in quad['vertices'] if p[0] in (west, east) or p[1] in (south, north)]
        self.assertEqual(40, len(boundary))
        for p in boundary:
            self.assertAlmostEqual(operation._provider(values[1], p[:2]), p[2], places=12)
        a, b, c, d = [[*xy, cell['core_samples'][y][x],
                      cell['surface_semantics']['core_samples']['ground'][y][x],
                      cell['surface_semantics']['core_samples']['hydro_transition'][y][x]]
                     for x, y, xy in ((0, 0, [west, north]), (1, 0, [east, north]), (0, 1, [west, south]), (1, 1, [east, south]))]
        from ..refinement.app.geometry import _interpolate
        for p in quad['vertices']:
            parent = (a, c, b) if (p[0]-west)/30+(north-p[1])/30 <= 1 else (b, c, d)
            for actual, expected in zip(p[3:], _interpolate(parent, p[:2])[3:]):
                self.assertAlmostEqual(expected, actual, places=14)
        validate_triangle_cover(ROOT, {'type': 'Polygon', 'coordinates': [[[west, south], [east, south], [east, north], [west, north], [west, south]]]},
                                [[p[:2] for p in face] for face in output])
        with self.assertRaises(CompilerError):
            ground.build_ground_surface(ROOT, *values[:4], maximum_triangles=1024, canonical_crs='EPSG:32618')

    def test_unknown_conflicting_missing_and_malformed_inputs_refuse_unchanged(self):
        for case in ('nodata', 'stale_grid', 'basis', 'frame', 'missing_face', 'duplicate_face', 'height_conflict', 'outer_contact', 'budget', 'inner_pin', 'geometry_crs'):
            values = fixture()
            cells, grid, evidence, selected, inner = values
            budget = 256
            if case == 'nodata':
                grid['samples'][1][1] = grid['nodata']
                evidence['grid_content_sha256'] = canonical_hash(grid)
            elif case == 'stale_grid': evidence['grid_content_sha256'] = '0'*64
            elif case == 'basis': inner['basis'] = 'observed'
            elif case == 'frame': inner['vertical_datum'] = 'unknown'
            elif case == 'missing_face': inner['triangles'].pop()
            elif case == 'duplicate_face': inner['triangles'].append(deepcopy(inner['triangles'][0]))
            elif case == 'height_conflict': inner['triangles'][0] = deepcopy(inner['triangles'][0]); inner['triangles'][0][2][2] += 1.
            elif case == 'outer_contact':
                for face in inner['triangles']:
                    for p in face: p[0] -= 20.
                inner['footprint']['coordinates'][0] = [[p[0]-20., p[1]] for p in inner['footprint']['coordinates'][0]]
            elif case == 'budget': budget = 2
            elif case == 'inner_pin': inner['model_sha256'] = 'unknown'
            else: inner['canonical_crs'] = 'EPSG:32638'
            before = deepcopy(values)
            with self.subTest(case=case), self.assertRaises((CompilerError, ExecutionEnvironmentError)):
                self.build(values, budget)
            self.assertEqual(before, values)

    def test_lost_constraint_and_duplicated_output_refuse(self):
        for case in ('missing_constraint', 'duplicated_output'):
            original = operation.triangulate_constraints
            def altered(*args):
                if case == 'missing_constraint':
                    repo, region, segments, stations, budget = args
                    # Omitting inner creases can still produce a complete XY cover.
                    return original(repo, region, segments[-10:], stations, budget)
                faces = original(*args)
                return [*faces, faces[0]]
            with self.subTest(case=case), patch.object(operation, 'triangulate_constraints', side_effect=altered):
                with self.assertRaises((CompilerError, ExecutionEnvironmentError)):
                    self.build(fixture())

    def test_lost_internal_crease_refuses_even_when_every_inner_node_survives(self):
        values = fixture()
        a, b, c, d = [4., 5., 0.], [24., 4., 1.], [23., 24., 0.], [5., 23., 0.]
        inner = values[-1]
        inner['footprint'] = {'type': 'Polygon', 'coordinates': [[a[:2], b[:2], c[:2], d[:2], a[:2]]]}
        inner['triangles'] = [[a, b, c], [a, c, d]]
        original = operation.triangulate_constraints
        def omit_only_internal_crease(repo, region, segments, stations, budget):
            crease = {tuple(a[:2]), tuple(c[:2])}
            retained = [s for s in segments if {tuple(v) for v in s} != crease]
            self.assertEqual(len(segments)-2, len(retained))
            result = original(repo, region, retained, stations, budget)
            self.assertTrue({tuple(p[:2]) for p in (a, b, c, d)} <= {tuple(p) for face in result for p in face})
            return result
        with patch.object(operation, 'triangulate_constraints', side_effect=omit_only_internal_crease):
            with self.assertRaisesRegex(CompilerError, 'protected inner field'):
                self.build(values)

    def test_identity_consumes_inner_provider_and_only_selected_domain(self):
        values = fixture(); before = self.build(values)
        values[0]['unselected'] = deepcopy(next(iter(values[0].values())))
        self.assertEqual(before, self.build(values))
        values[-1]['model_sha256'] = 'e'*64
        self.assertNotEqual(before['consumed']['inner_sha256'], self.build(values)['consumed']['inner_sha256'])
        values[2]['source_sha256'] = 'd'*64
        self.assertNotEqual(before['consumed']['ground_evidence'], self.build(values)['consumed']['ground_evidence'])

    def test_optional_implementation_identity_and_production_refusal(self):
        before, parent = operation.implementation_identity(), ground.implementation_identity()
        original = operation.file_hash
        with patch.object(operation, 'file_hash', side_effect=lambda p: '0'*64 if Path(p) == Path(operation.__file__) else original(p)):
            self.assertNotEqual(before, operation.implementation_identity())
            self.assertEqual(parent, ground.implementation_identity())
        from ..refinement.app.contracts import validate_document
        result = self.build(fixture())
        for schema in (None, 'https://alis.world/schemas/world-compiler/terrain-refinement-v1.json'):
            candidate = deepcopy(result)
            if schema is not None:
                candidate['$schema'] = schema
            with self.subTest(schema=schema), self.assertRaises(CompilerError):
                validate_document(candidate, ROOT/'tmp/world/ground-composite-refusal.json')


if __name__ == '__main__': unittest.main()
