"""Captured incoming-PWA continuation and its original semantic fields."""
import importlib
import importlib.util
from copy import deepcopy
from pathlib import Path
import unittest
from unittest.mock import patch

from ..app.contracts import CompilerError, canonical_hash
from ..refinement.app.geometry import _point
from World.ExecutionEnvironment.geometry.affine import measure_affine_cover
from .ground_continuation_fixture import fixture

ROOT = Path(__file__).resolve().parents[4]
OWNER = 'World.CanonicalCompilation.refinement.ground_continuation'


def triangles(surfaces, field=2):
    return [[[quad['vertices'][i][0], quad['vertices'][i][1], quad['vertices'][i][field]] for i in row]
        for cell in surfaces.values() for quad in cell['quads'] for row in quad['triangles']]


class GroundContinuationTests(unittest.TestCase):
    def owner(self):
        self.assertIsNotNone(importlib.util.find_spec(OWNER), 'The demonstrated incoming-PWA continuation needs its optional owner')
        return importlib.import_module(OWNER)

    def small(self, height=0.):
        values = fixture()
        identifier, cell = next(iter(values['cells'].items()))
        for quad in values['incoming']['surfaces'][identifier]['quads']:
            x, y = quad['coordinate']
            quad['vertices'] = [_point(cell, x+i, y+j) for i, j in ((0, 0), (1, 0), (0, 1), (1, 1))]
            for vertex in quad['vertices']:
                vertex[2] = height
            quad['triangles'], quad['water_triangles'] = [[0, 2, 1], [1, 2, 3]], []
        values['grid']['samples'] = [[height for _ in row] for row in values['grid']['samples']]
        values['evidence']['grid_content_sha256'] = canonical_hash(values['grid'])
        values['evidence']['processing_sha256'] = canonical_hash({'mechanical_control': height})
        return values

    def build(self, values):
        return self.owner().build_ground_continuation(ROOT, **values, canonical_crs='EPSG:32618', maximum_triangles=4096)

    def test_oversized_incoming_number_has_owned_refusal(self):
        values = self.small()
        next(iter(values['incoming']['surfaces'].values()))['quads'][0]['vertices'][0][2] = 10**400
        before = deepcopy(values)
        with self.assertRaisesRegex(CompilerError, 'finite'):
            self.build(values)
        self.assertEqual(before, values)

    def test_original_neighbor_corner_change_invalidates_consumed_provenance(self):
        values = self.small()
        changed = deepcopy(values)
        cell = next(iter(changed['cells'].values()))
        cell['surface_semantics']['core_samples']['ground'][0][-1] += .001
        cell['surface_semantics']['core_samples']['hydro_transition'][0][-1] -= .001
        first, second = self.build(values), self.build(changed)
        self.assertNotEqual(first['surfaces'], second['surfaces'])
        self.assertNotEqual(first['consumed'], second['consumed'])

    def test_unselected_original_corner_does_not_invalidate_the_model(self):
        values = self.small()
        changed = deepcopy(values)
        cell = next(iter(changed['cells'].values()))
        cell['bounds'][2] += 30.
        for row in cell['core_samples']:
            row.append(123.4)
        for role, extra in (('ground', .8), ('hydro_transition', .2)):
            for row in cell['surface_semantics']['core_samples'][role]:
                row.append(extra)
        self.assertEqual(self.build(values), self.build(changed))

    def test_reversed_incoming_face_is_refused(self):
        values = self.small()
        next(iter(values['incoming']['surfaces'].values()))['quads'][0]['triangles'][0].reverse()
        before = deepcopy(values)
        with self.assertRaisesRegex(CompilerError, 'winding'):
            self.build(values)
        self.assertEqual(before, values)

    def test_reversed_emitted_face_is_refused(self):
        operation = self.owner()
        original = operation._pack
        def reversed_face(coordinate, records, corners, retained):
            quad = original(coordinate, records, corners, retained)
            if coordinate == [2, 0]:
                quad['triangles'][0].reverse()
            return quad
        values = self.small()
        before = deepcopy(values)
        with patch.object(operation, '_pack', side_effect=reversed_face):
            with self.assertRaisesRegex(CompilerError, 'winding'):
                self.build(values)
        self.assertEqual(before, values)

    def test_actual_emitted_neighbor_height_fault_is_refused(self):
        operation = self.owner()
        original = operation._pack
        values = self.small()
        before = deepcopy(values)
        def altered(coordinate, records, corners, retained):
            quad = original(coordinate, records, corners, retained)
            if coordinate == [2, 0]:
                point = next(p for p in quad['vertices'] if corners[0][0] < p[0] < corners[1][0] and corners[2][1] < p[1] < corners[0][1])
                point[2] += .001
            return quad
        with patch.object(operation, '_pack', side_effect=altered):
            with self.assertRaisesRegex(CompilerError, 'reference field'):
                operation.build_ground_continuation(ROOT, **values, canonical_crs='EPSG:32618', maximum_triangles=4096)
        self.assertEqual(before, values)

    def test_declared_wet_roles_survive_subdivision_and_dry_zero_stays_dry(self):
        values = self.small(7.4)
        identifier = next(iter(values['cells']))
        values['incoming']['surfaces'][identifier]['quads'][1]['water_triangles'] = [1]
        before = deepcopy(values)
        result = self.build(values)
        quads = result['surfaces'][identifier]['quads']
        wet = [q for q in quads if q['water_triangles']]
        self.assertEqual(len(wet), 1)
        self.assertEqual(wet[0]['coordinate'], [1, 0])
        self.assertGreater(len(wet[0]['water_triangles']), 1)
        area = 0.
        for index in wet[0]['water_triangles']:
            a, b, c = [wet[0]['vertices'][i] for i in wet[0]['triangles'][index]]
            for height in (a[2], b[2], c[2]):
                self.assertAlmostEqual(height, 7.4, places=8)
            area += abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/2
        self.assertEqual(area, 450.)
        self.assertEqual(before, values)
        dry = self.build(self.small())
        self.assertTrue(all(not q['water_triangles'] for cell in dry['surfaces'].values() for q in cell['quads']))

    def test_wrong_semantic_diagonal_is_refused_against_whole_original_fields(self):
        operation = self.owner()
        native = operation.triangulate_constraints
        def wrong(repo, polygon, segments, points, capacity):
            modified = deepcopy(segments)
            ring = polygon['coordinates'][0]
            modified[-1] = [ring[3], ring[1]]
            return native(repo, polygon, modified, points, capacity)
        values = self.small()
        before = deepcopy(values)
        with patch.object(operation, 'triangulate_constraints', side_effect=wrong):
            with self.assertRaisesRegex(CompilerError, 'semantic field'):
                self.build(values)
        self.assertEqual(before, values)

    def test_missing_unknown_contradictory_and_oversized_inputs_refuse_before_native(self):
        operation = self.owner()
        for case in ('unknown_model', 'version', 'frame', 'nodata', 'stale_grid', 'index', 'role', 'gap', 'two_neighbors', 'shared_height', 'capacity', 'winding'):
            values = self.small()
            identifier = next(iter(values['cells']))
            incoming = values['incoming']
            quads = incoming['surfaces'][identifier]['quads']
            if case == 'unknown_model': incoming['observed'] = True
            elif case == 'version': incoming['version'] = True
            elif case == 'frame': incoming['vertical_datum'] = 'EPSG:6360'
            elif case == 'nodata':
                values['grid']['samples'][0][0] = values['grid']['nodata']
                values['evidence']['grid_content_sha256'] = canonical_hash(values['grid'])
            elif case == 'stale_grid': values['grid']['samples'][0][0] += 1
            elif case == 'index': quads[0]['triangles'][0][0] = []
            elif case == 'role': quads[0]['water_triangles'] = [999]
            elif case == 'gap': quads.pop()
            elif case == 'two_neighbors': values['selected'][identifier].append([1, 0])
            elif case == 'shared_height': quads[1]['vertices'][0][2] += .01
            elif case == 'winding': quads[0]['triangles'][0].reverse()
            else:
                values['grid'].update(size=[36, 36], spacing=[30/35, -30/35], samples=[[0.]*36 for _ in range(36)])
                values['evidence']['grid_content_sha256'] = canonical_hash(values['grid'])
            before = deepcopy(values)
            with self.subTest(case=case), patch.object(operation, 'triangulate_points') as first, patch.object(
                    operation, 'triangulate_constraints') as second:
                with self.assertRaises(CompilerError):
                    self.build(values)
                first.assert_not_called()
                second.assert_not_called()
            self.assertEqual(before, values)

    def test_small_cold_rebuild_and_unmodified_quad_preserve_exact_values(self):
        values = self.small()
        before = deepcopy(values)
        result = self.build(values)
        self.assertEqual(result, self.build(values))
        identifier = next(iter(values['cells']))
        self.assertEqual(values['incoming']['surfaces'][identifier]['quads'][0], result['surfaces'][identifier]['quads'][0])
        self.assertEqual(result['metrics']['unchanged_incoming_quads'], 1)
        self.assertEqual(result['metrics']['rewritten_incoming_quads'], [{'cell_id': identifier, 'coordinate': [1, 0]}])
        self.assertEqual(before, values)

    def test_optional_leaf_identity_is_local_to_consumers(self):
        operation = self.owner()
        from ..refinement.app.identity import implementation_identity as shared_identity
        before, selected = shared_identity(), operation.implementation_identity()
        real = operation.file_hash
        def changed(path):
            return canonical_hash({'new_leaf': real(path)}) if Path(path).resolve() == Path(operation.__file__).resolve() else real(path)
        with patch.object(operation, 'file_hash', side_effect=changed):
            self.assertNotEqual(selected, operation.implementation_identity())
            self.assertEqual(before, shared_identity())

    def test_captured_strip_preserves_seed_and_original_weight_fields(self):
        values = fixture()
        result = self.owner().build_ground_continuation(ROOT, **values, canonical_crs='EPSG:32618', maximum_triangles=4096)
        self.assertEqual(result['publication'], 'unpublished_model_space')
        self.assertNotIn('$schema', result)
        self.assertEqual(len(triangles(result['surfaces'])), 883)
        proof = measure_affine_cover(triangles(result['surfaces']), triangles(values['incoming']['surfaces']))
        self.assertLessEqual(proof['maximum_height_error_upper_m'], 1e-8)
        self.assertLessEqual(result['metrics']['maximum_original_semantic_field_error_upper'], 1e-8)
        self.assertEqual(result['metrics']['shared_stations'], 31)
