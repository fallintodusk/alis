"""A mapped platform owns its surface role without changing Ground or Water."""
from copy import deepcopy
from pathlib import Path
import unittest

from ..app.contracts import CompilerError, canonical_hash
from ..app.surface_context import surface_context
from ..refinement.constructed_surface import build_constructed_road_surface

ROOT = Path(__file__).resolve().parents[4]


def fixture():
    road = {'feature_id': 'fixture:path', 'feature_class': 'road',
            'geometry': {'type': 'LineString', 'coordinates': [[3., 4.], [7., 5.]]},
            'attributes': {'width_m': 2.}}
    parent = {'type': 'Polygon', 'coordinates': [[[0., 0.], [10., 0.], [10., 10.], [0., 10.], [0., 0.]]]}
    points = [[float(x), float(y), 3.5+x*.01+y*.02] for x in (1, 5, 9) for y in (1, 5, 9)]
    context = {'man_made': 'pier', 'floating': 'no', 'geometry_sha256': canonical_hash(parent),
               'source_sha256': 'a'*64}
    evidence = {'classification': 2, 'canonical_crs': 'EPSG:32618', 'vertical_datum': 'EPSG:3855',
                'point_content_sha256': canonical_hash(points), 'source_sha256': 'b'*64,
                'processing_sha256': 'c'*64}
    selection = {'rule': 'explicit_query_window', 'bounds': [0., 0., 10., 10.]}
    return road, points, parent, context, evidence, selection


class ConstructedSurfaceTests(unittest.TestCase):
    def build(self, values, **kw):
        road, points, parent, context, evidence, selection = values
        return build_constructed_road_surface(ROOT, road, points, parent, context, evidence,
            canonical_crs='EPSG:32618', selection=selection, **kw)

    def test_oblique_platform_path_keeps_observed_heights_and_complete_width(self):
        values = fixture(); before = deepcopy(values)
        result = self.build(values)
        self.assertEqual(result, self.build(values))
        self.assertEqual(values, before)
        self.assertEqual('observed_constructed_tin_candidate_not_surveyed', result['model'])
        self.assertEqual('constructed_surface', result['endpoint_relation'])
        self.assertGreater(result['minimum_height_m'], 3.5)
        self.assertEqual(canonical_hash(values[0]), result['consumed']['feature_sha256'])
        for face in result['world_triangles']:
            for x, y, z in face:
                self.assertAlmostEqual(3.5+x*.01+y*.02, z, delta=1e-10)
        self.assertEqual(len(result['world_triangles']), len(result['uv_triangles']))
        for world, uv in zip(result['world_triangles'], result['uv_triangles']):
            for a, b in zip(world, uv):
                self.assertEqual(a[2], b[2])
        changed = deepcopy(values); changed[-1]['bounds'][0] = -.1
        self.assertNotEqual(result['consumed']['selection_sha256'], self.build(changed)['consumed']['selection_sha256'])

    def test_unqualified_structure_overhang_and_parent_hole_refuse(self):
        for kind in ('missing', 'floating', 'metric', 'wrong_geometry', 'overhang', 'hole'):
            values = list(deepcopy(fixture())); road, points, parent, context, evidence, selection = values
            if kind == 'missing': context.pop('man_made')
            if kind == 'floating': context['floating'] = 'yes'
            if kind == 'metric': context['ele'] = '10'
            if kind == 'wrong_geometry': context['geometry_sha256'] = '0'*64
            if kind == 'overhang': road['attributes']['width_m'] = 20.
            if kind == 'hole':
                parent['coordinates'].append([[4., 3.], [4., 6.], [6., 6.], [6., 3.], [4., 3.]])
                context['geometry_sha256'] = canonical_hash(parent)
            before = deepcopy(values)
            with self.subTest(kind=kind), self.assertRaises(CompilerError): self.build(values)
            self.assertEqual(values, before)

    def test_outside_parent_observations_do_not_supply_platform_support(self):
        values = list(fixture())
        values[1] = [[-1., -1., 3.5], [11., -1., 3.5], [11., 11., 3.5], [-1., 11., 3.5]]
        values[4]['point_content_sha256'] = canonical_hash(values[1])
        values[5]['bounds'] = [-1., -1., 11., 11.]
        with self.assertRaises(CompilerError): self.build(values)
        values = list(fixture()); values[1] = [[1., 1., 3.5], [2., 1., 3.5], [1., 2., 3.5]]
        values[4]['point_content_sha256'] = canonical_hash(values[1])
        with self.assertRaises(CompilerError): self.build(values)

    def test_single_native_multipolygon_and_captured_boundary_partition(self):
        values = list(fixture()); length = 4.278142120057872
        values[0]['geometry']['coordinates'] = [[0., 0.], [length, 0.]]
        parent = {'type': 'MultiPolygon', 'coordinates': [[[[-2., -3.], [6., -3.], [6., 3.], [-2., 3.], [-2., -3.]]]]}
        values[2] = parent; values[3]['geometry_sha256'] = canonical_hash(parent)
        # Actual source face in parameter space: it crosses the complete Road's outer edge.
        xy = [[.3352652329034024, 1.0980023773761842], [.4820111761280862, .7965631187162479],
              [.6762543722466176, 1.2507322278369155], [-1., -2.], [5., -2.], [5., 2.], [-1., 2.]]
        values[1] = [[x, y, 3.5+x*.01] for x, y in xy]
        values[4]['point_content_sha256'] = canonical_hash(values[1])
        values[5]['bounds'] = [-1., -2., 5., 2.]
        result = self.build(values)
        self.assertGreater(len(result['uv_triangles']), 2)
        for face in result['uv_triangles']:
            for s, lateral, z in face: self.assertAlmostEqual(3.5+s*.01, z, delta=1e-10)

    def test_frame_pins_conflicts_and_unknown_values_refuse_without_mutation(self):
        for kind in ('hash', 'frame', 'classification', 'conflict', 'nan', 'boolean', 'window', 'model', 'geometry', 'attributes'):
            values = list(deepcopy(fixture()))
            if kind == 'hash': values[4]['point_content_sha256'] = '0'*64
            if kind == 'frame': values[4]['vertical_datum'] = 'EPSG:5703'
            if kind == 'classification': values[4]['classification'] = 9
            if kind == 'conflict':
                values[1].append([*values[1][4][:2], values[1][4][2]+.05])
                values[4]['point_content_sha256'] = canonical_hash(values[1])
            if kind == 'nan': values[1][0][2] = float('nan')
            if kind == 'boolean': values[0]['attributes']['width_m'] = True
            if kind == 'window': values[5]['bounds'] = [2., 2., 8., 8.]
            if kind == 'model': values[5]['rule'] = 'unknown'
            if kind == 'geometry': values[0]['geometry'] = None
            if kind == 'attributes': values[0]['attributes'] = None
            with self.subTest(kind=kind), self.assertRaises(CompilerError): self.build(values)
        with self.assertRaises(CompilerError): self.build(fixture(), maximum_triangles=True)
        with self.assertRaises(CompilerError): self.build(fixture(), maximum_triangles=2)

    def test_projected_coordinates_preserve_affine_source_and_reject_missing_face(self):
        values = list(fixture()); ox, oy = 583300., 4508100.
        values[0]['geometry']['coordinates'] = [[x+ox, y+oy] for x, y in values[0]['geometry']['coordinates']]
        values[1] = [[x+ox, y+oy, z] for x, y, z in values[1]]
        values[2]['coordinates'] = [[[x+ox, y+oy] for x, y in ring] for ring in values[2]['coordinates']]
        values[3]['geometry_sha256'] = canonical_hash(values[2]); values[4]['point_content_sha256'] = canonical_hash(values[1])
        values[5]['bounds'] = [ox, oy, ox+10., oy+10.]
        result = self.build(values)
        for face in result['world_triangles']:
            for x, y, z in face: self.assertAlmostEqual(3.5+(x-ox)*.01+(y-oy)*.02, z, delta=1e-10)
        from World.ExecutionEnvironment.geometry.api import validate_triangle_cover
        from World.ExecutionEnvironment.api import ExecutionEnvironmentError
        with self.assertRaises(ExecutionEnvironmentError):
            validate_triangle_cover(ROOT, result['footprint'], [[p[:2] for p in f] for f in result['world_triangles'][1:]])

    def test_finite_observations_cannot_publish_overflowed_interpolation(self):
        values = list(fixture())
        values[1] = [[x, y, 1e308 if x == 1 else -1e308] for x, y, z in values[1]]
        values[4]['point_content_sha256'] = canonical_hash(values[1])
        before = deepcopy(values)
        with self.assertRaises(CompilerError): self.build(values)
        self.assertEqual(before, values)

    def test_parent_containment_does_not_override_explicit_crossing_evidence(self):
        for tags in ({'tunnel': 'yes'}, {'ford': 'yes'}, {'bridge': 'yes'},
                     {'bridge': 'yes', 'tunnel': 'yes'}, {'tunnel': 'maybe'}):
            values = list(fixture())
            values[0]['attributes']['surface_context'] = surface_context(tags, 'road')
            before = deepcopy(values)
            with self.subTest(tags=tags), self.assertRaises(CompilerError): self.build(values)
            self.assertEqual(before, values)


if __name__ == '__main__': unittest.main()
