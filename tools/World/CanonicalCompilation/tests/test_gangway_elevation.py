"""Constructed endpoint contracts, independent of source-survey or city acceptance."""
from copy import deepcopy
import math
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))

from ..app.contracts import CompilerError, canonical_hash
from ..refinement.api import build_bridge_profile, build_gangway_profile
from ..refinement.app import gangway_elevation as gangway
from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.api import partition_triangles
from .gangway_fixture import fixture


class GangwayElevationTests(unittest.TestCase):
    def test_complete_floating_body_remains_flat_before_the_modeled_hinge(self):
        values = fixture(freeboard=1.2)
        profile = self.build(values)
        self.assertAlmostEqual(1.2, gangway._height(profile['triangles'], [1., 0.]), delta=1e-9)

    def test_complete_assembly_refutes_endpoint_only_scenario_feasibility(self):
        for water_level, limit in ((0., 1/8), (.810, 1/12)):
            values = fixture(water_level=water_level, freeboard=1.)
            values[-1]['maximum_longitudinal_grade'] = limit
            with self.subTest(water_level=water_level), self.assertRaisesRegex(CompilerError, 'maximum_longitudinal_grade'):
                self.build(values)

    def test_assembly_coverage_identity_and_height_continuity_are_required(self):
        baseline = self.build(fixture())
        changed = fixture(); changed[4]['source_sha256'] = 'e'*64
        self.assertNotEqual(baseline['support_model_sha256'], self.build(changed)['support_model_sha256'])
        for kind in ('unknown', 'frame', 'missing', 'duplicate', 'crack'):
            values = list(fixture()); complete = values[4]
            if kind == 'unknown': values[4] = None
            if kind == 'frame': complete['vertical_datum'] = 'EPSG:5703'
            if kind == 'missing': complete['triangles'].pop()
            if kind == 'duplicate': complete['triangles'].append(deepcopy(complete['triangles'][0]))
            if kind == 'crack': complete['triangles'][0][0][2] += .01
            before = canonical_hash(values)
            with self.subTest(kind=kind), self.assertRaises((CompilerError, ExecutionEnvironmentError)): self.build(values)
            self.assertEqual(before, canonical_hash(values))

    def build(self, values):
        road, cells, supports, water, complete, limits = values
        return build_gangway_profile(ROOT, road, cells, endpoint_supports=supports,
            assembly_surface=complete, water_model=water, quality_limits=limits, canonical_crs='EPSG:32618')

    def test_captured_connection_joins_complete_fixed_and_floating_sections(self):
        values = fixture(); before = deepcopy(values)
        result = self.build(values)
        self.assertEqual(before, values)
        self.assertEqual(result, self.build(values))
        length = result['length_m']
        sections = {0.: [], length: []}
        for face in result['triangles']:
            for station, lateral, z in face:
                if station in sections:
                    sections[station].append((lateral, z))
        for station, index in ((0., 0), (length, 1)):
            with self.subTest(station=station):
                self.assertEqual(-1., min(p[0] for p in sections[station]))
                self.assertEqual(1., max(p[0] for p in sections[station]))
                point = values[0]['geometry']['coordinates'][index]
                a, b = values[0]['geometry']['coordinates']
                normal = [-(b[1]-a[1])/length, (b[0]-a[0])/length]
                for side, z in sections[station]:
                    xy = [point[i]+side*normal[i] for i in (0, 1)]
                    self.assertAlmostEqual(gangway._height(values[2][index]['triangles'], xy), z, delta=1e-8)
        self.assertAlmostEqual(.1234963690194, result['quality']['maximum_longitudinal_grade'], delta=1e-8)
        self.assertEqual('constructed_surface', result['endpoint_relation'])

    def test_original_terrain_abutment_candidate_fails_the_join_quality_gate(self):
        road, cells, supports, water, complete, limits = fixture()
        old = build_bridge_profile(ROOT, road, cells)
        points, width, length, tangent, normal, faces, terrain, consumed = gangway._inputs(
            ROOT, road, cells, supports, water, limits, 'EPSG:32618')
        world = [[[points[0][i]+u*tangent[i]+v*normal[i] for i in (0, 1)]+[z]
                  for u, v, z in face] for face in old['triangles']]
        with self.assertRaisesRegex(CompilerError, 'join-step'):
            gangway._measure(ROOT, old['triangles'], world, points, width, length, tangent, normal,
                             faces, terrain, water, limits)

    def test_predeclared_water_scenarios_and_changed_support_identity(self):
        low = fixture(); mean = fixture(water_level=.810)
        mean[-1]['maximum_longitudinal_grade'] = 1/12
        low_profile, mean_profile = self.build(low), self.build(mean)
        self.assertLess(low_profile['quality']['maximum_longitudinal_grade'], 1/8)
        self.assertLess(mean_profile['quality']['maximum_longitudinal_grade'], 1/12)
        self.assertNotEqual(low_profile['consumed']['water_model_sha256'], mean_profile['consumed']['water_model_sha256'])
        self.assertNotEqual(low_profile['support_model_sha256'], mean_profile['support_model_sha256'])
        self.assertNotEqual(low_profile['triangles'], mean_profile['triangles'])
        low[-1]['maximum_longitudinal_grade'] = 1/12
        with self.assertRaisesRegex(CompilerError, 'maximum_longitudinal_grade'): self.build(low)
        changed = fixture(); changed[2][0]['source_sha256'] = 'f'*64
        self.assertNotEqual(low_profile['support_model_sha256'], self.build(changed)['support_model_sha256'])
        stale = fixture(); stale[3]['level_m'] += .1
        with self.assertRaisesRegex(CompilerError, 'Water and freeboard'): self.build(stale)

    def test_exact_and_exceeded_full_face_limits(self):
        values = fixture()
        measured = self.build(values)['quality']
        for key in ('maximum_longitudinal_grade', 'maximum_cross_slope',
                    'minimum_terrain_clearance_m', 'minimum_water_clearance_m'):
            exact = deepcopy(values); exact[-1][key] = measured[key]
            self.build(exact)
            failed = deepcopy(exact)
            failed[-1][key] = math.nextafter(measured[key], -math.inf if key.startswith('maximum') else math.inf)
            with self.subTest(limit=key), self.assertRaisesRegex(CompilerError, key): self.build(failed)

    def test_hidden_endpoint_hole_refuses_without_width_loss_or_extrapolation(self):
        values = fixture(); support = values[2][0]
        a, b = values[0]['geometry']['coordinates']; length = math.dist(a, b)
        normal = [-(b[1]-a[1])/length, (b[0]-a[0])/length]
        x, y = [a[i]+.4*normal[i] for i in (0, 1)]
        support['footprint']['coordinates'].append([[x-.05, y-.05], [x-.05, y+.05],
                                                   [x+.05, y+.05], [x+.05, y-.05], [x-.05, y-.05]])
        original = deepcopy(support['triangles'])
        support['triangles'] = [[gangway._affine(face, xy) for xy in piece] for face in original for piece in
            partition_triangles(ROOT, [p[:2] for p in face], support['footprint'], 16)['wet']]
        before = canonical_hash(values)
        with self.assertRaisesRegex(CompilerError, 'structural support'): self.build(values)
        self.assertEqual(before, canonical_hash(values))

    def test_interior_terrain_collision_refuses_instead_of_arching(self):
        values = list(fixture()); cells = values[1]
        grid = [[0.] * 15 for _ in range(11)]; grid[5][6] = 4.
        cells.clear(); cells['injected'] = {'bounds': [583298., 4508120., 583326., 4508140.],
            'sample_spacing': [2., 2.], 'core_samples': grid,
            'surface_semantics': {'core_samples': {'ground': [[1.] * 15 for _ in range(11)],
                                                  'hydro_transition': [[0.] * 15 for _ in range(11)]}}}
        before = canonical_hash(values)
        with self.assertRaisesRegex(CompilerError, 'minimum_terrain_clearance_m'): self.build(values)
        self.assertEqual(before, canonical_hash(values))

    def test_unknown_and_contradictory_inputs_refuse_without_mutation(self):
        for kind in ('role', 'role_type', 'node', 'basis', 'frame', 'epoch', 'epoch_order', 'water',
                     'limit', 'nan', 'budget', 'small_budget', 'source_nodes', 'crossing', 'footprint', 'cell_type'):
            values = list(fixture()); road, cells, supports, water, complete, limits = values
            if kind == 'role': supports[0]['role'] = 'unknown'
            if kind == 'role_type': supports[0]['role'] = []
            if kind == 'node': supports[0]['provider_node_id'] = 'node/123'
            if kind == 'basis': supports[1]['basis'] = 'unknown'
            if kind == 'frame': water['vertical_datum'] = 'EPSG:5703'
            if kind == 'epoch': supports[0]['source_epoch'] = None
            if kind == 'epoch_order': water['source_epoch'] = {'start': '2024-01-01', 'end': '2000-01-01'}
            if kind == 'water': water['model'] = 'unknown'
            if kind == 'limit': limits['maximum_cross_slope'] = None
            if kind == 'nan': supports[1]['triangles'][0][0][2] = float('nan')
            if kind == 'budget': limits['maximum_triangles'] = True
            if kind == 'small_budget': limits['maximum_triangles'] = 2
            if kind == 'source_nodes': road['attributes']['surface_context']['source_nodes'] = []
            if kind == 'crossing': road['attributes']['surface_context']['unresolved'].append('unsupported_tunnel')
            if kind == 'footprint': supports[0]['footprint'] = None
            if kind == 'cell_type': cells['reduced'] = None
            before = canonical_hash(values)
            with self.subTest(kind=kind), self.assertRaises(CompilerError): self.build(values)
            self.assertEqual(before, canonical_hash(values))

    def test_remote_context_does_not_change_geometry_or_consumed_support(self):
        values = fixture(); baseline = self.build(values)
        other = deepcopy(values[1]['reduced'])
        other['bounds'][0] += 1000.; other['bounds'][2] += 1000.
        values[1]['remote'] = other
        self.assertEqual(baseline, self.build(values))


if __name__ == '__main__': unittest.main()
