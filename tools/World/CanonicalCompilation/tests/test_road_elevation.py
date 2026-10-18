"""Disposable bridge profile behavior, independent of native Road tessellation."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError
from World.CanonicalCompilation.app.surface_context import surface_context
from World.CanonicalCompilation.refinement.api import build_bridge_profile
from World.ExecutionEnvironment.geometry.api import partition_triangles


def fixture():
    road = {'feature_id': 'alis:fixture:way:1', 'feature_class': 'road',
            'geometry': {'type': 'LineString', 'coordinates': [[5., 15.], [25., 15.]]},
            'attributes': {'width_m': 4., 'surface_context': surface_context({'bridge': 'yes'}, 'road')}}
    cell = {'cell_id': 'cell_0', 'grid_id': 'grid_' + '0' * 16, 'bounds': [0., 0., 30., 30.],
            'sample_spacing': [10., 10.], 'core_samples': [[10., 0., 0., 12.]] * 4,
            'surface_semantics': {'roles': ['ground', 'hydro_transition'],
                                  'core_samples': {'ground': [[1.] * 4] * 4, 'hydro_transition': [[0.] * 4] * 4}}}
    return road, {'cell_0': cell}


class RoadElevationTests(unittest.TestCase):
    def test_affine_interior_clearance_with_nonuniform_abutments(self):
        road, cells = fixture()
        rows = [[10.+y*.4, 15.+y*.2, y*.6, 12.+y*.9] for y in range(4)]
        cells['cell_0']['core_samples'] = rows
        before = deepcopy((road, cells))
        profile = build_bridge_profile(ROOT, road, cells)
        def determinant(a, b, c):
            return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
        def elevation(face, xy):
            a, b, c = face
            return (determinant(xy, b, c)*a[2]+determinant(a, xy, c)*b[2]+
                    determinant(a, b, xy)*c[2])/determinant(a, b, c)
        terrain = []
        for x in range(3):
            a, b = [x*10., 20., rows[1][x]], [(x+1)*10., 20., rows[1][x+1]]
            c, d = [x*10., 10., rows[2][x]], [(x+1)*10., 10., rows[2][x+1]]
            terrain.extend(((a, b, c), (b, d, c)))
        endpoint_heights = {0.: [], 20.: []}
        witnesses = 0
        for face in profile['triangles']:
            world = [[5.+s, 15.+side, z] for s, side, z in face]
            for ground in terrain:
                polygon = {'type': 'Polygon', 'coordinates': [[*[p[:2] for p in ground], ground[0][:2]]]}
                clipped = partition_triangles(ROOT, [p[:2] for p in world], polygon, 16)
                for part in clipped['wet']:
                    for xy in part:
                        self.assertGreaterEqual(elevation(world, xy)-elevation(ground, xy), -1e-8)
                        witnesses += 1
            for s, side, z in face:
                if s not in endpoint_heights:
                    continue
                xy = [5.+s, 15.+side]
                ground = next(g for g in terrain if all(
                    determinant(g[i], g[(i+1) % 3], xy)*determinant(*g) >= -1e-8 for i in range(3)))
                self.assertAlmostEqual(elevation(ground, xy), z, delta=1e-8)
                endpoint_heights[s].append(z)
        self.assertGreater(witnesses, 20)
        for heights in endpoint_heights.values():
            self.assertGreater(max(heights)-min(heights), .01)
        self.assertEqual(before, (road, cells))

    def test_source_only_profile_is_deterministic_preserves_ends_and_inputs(self):
        road, cells = fixture()
        before = deepcopy((road, cells))
        profile = build_bridge_profile(ROOT, road, cells)
        self.assertEqual(profile, build_bridge_profile(ROOT, road, cells))
        self.assertEqual(before, (road, cells))
        self.assertEqual('affine_uv_triangles_v1', profile['interpolation'])
        self.assertEqual('canonical_absolute_m_without_presentation_offset', profile['height_rule'])
        self.assertEqual(20., profile['length_m'])
        self.assertEqual({'cell_0'}, set(profile['consumed_terrain']))
        for face in profile['triangles']:
            for s, side, height in face:
                self.assertGreaterEqual(height, 0.)
                if s == 0.:
                    self.assertAlmostEqual(5., height)
                if s == 20.:
                    self.assertAlmostEqual(6., height)

    def test_missing_conflicting_or_unsupported_source_refuses(self):
        road, cells = fixture()
        for tags in ({}, {'bridge': 'yes', 'tunnel': 'yes'}, {'bridge': 'yes', 'ford': 'yes'},
                     {'bridge': 'yes', 'tunnel': 'maybe'}, {'bridge': 'yes', 'ford': 'maybe'}):
            bad = deepcopy(road)
            bad['attributes']['surface_context'] = surface_context(tags, 'road')
            with self.subTest(tags=tags), self.assertRaises(CompilerError):
                build_bridge_profile(ROOT, bad, cells)
        bad = deepcopy(road)
        bad['geometry'] = {'type': 'MultiLineString', 'coordinates': [road['geometry']['coordinates']]}
        with self.assertRaises(CompilerError):
            build_bridge_profile(ROOT, bad, cells)

    def test_interior_support_is_lifted_and_layer_never_becomes_metres(self):
        road, cells = fixture()
        cells['cell_0']['core_samples'] = [[10., 15., 0., 12.]] * 4
        before = deepcopy((road, cells))
        road['attributes']['surface_context'] = surface_context({'bridge': 'yes', 'layer': '1'}, 'road')
        profile = build_bridge_profile(ROOT, road, cells)
        self.assertGreaterEqual(max(p[2] for f in profile['triangles'] for p in f), 15.)
        road['attributes']['surface_context'] = surface_context({'bridge': 'yes', 'layer': '99'}, 'road')
        changed = build_bridge_profile(ROOT, road, cells)
        self.assertEqual(profile['triangles'], changed['triangles'])
        self.assertNotEqual(profile['consumed_feature_sha256'], changed['consumed_feature_sha256'])
        self.assertEqual(before[1], cells)

    def test_incomplete_terrain_support_and_unknown_values_refuse(self):
        road, cells = fixture()
        for bad in ({}, {'cell_0': {**cells['cell_0'], 'core_samples': [[float('nan')] * 4] * 4}}):
            with self.subTest(cells=bool(bad)), self.assertRaises(CompilerError):
                build_bridge_profile(ROOT, road, bad)
        road['geometry']['coordinates'].insert(1, [5., 15.])
        with self.assertRaises(CompilerError):
            build_bridge_profile(ROOT, road, cells)

    def test_consumed_numeric_types_refuse_as_owned_errors(self):
        road, cells = fixture()
        before = deepcopy((road, cells))
        for value in (True, 'bad', None, float('nan'), float('inf')):
            bad = deepcopy(cells)
            bad['cell_0']['core_samples'][0][0] = value
            with self.subTest(kind='terrain', value=value), self.assertRaises(CompilerError):
                build_bridge_profile(ROOT, road, bad)
            bad_road = deepcopy(road)
            bad_road['geometry']['coordinates'][0][0] = value
            with self.subTest(kind='coordinate', value=value), self.assertRaises(CompilerError):
                build_bridge_profile(ROOT, bad_road, cells)
        for key, value in (('sample_spacing', []), ('core_samples', [None]), ('bounds', [True, 0, 30, 30])):
            bad = deepcopy(cells)
            bad['cell_0'][key] = value
            with self.subTest(key=key), self.assertRaises(CompilerError):
                build_bridge_profile(ROOT, road, bad)
        self.assertEqual(before, (road, cells))


if __name__ == '__main__':
    unittest.main()
