"""Source-faithful neighboring crop failure and one-field owner-preserving correction."""
from collections import Counter
from copy import deepcopy
from fractions import Fraction as F
import json
from pathlib import Path
import unittest
from unittest.mock import patch

from World.CanonicalCompilation.app.contracts import CompilerError
from ..refinement import ground_path_floor as operation
from .ground_path_neighbor_fixture import fixture

ROOT = Path(__file__).resolve().parents[4]


class GroundPathNeighborTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.baseline = cls('test_common_field_keeps_complete_width_and_exact_shared_xyz_semantics').build(fixture())

    def build(self, values, budget=4096):
        def sample(point):
            return deepcopy(values['samples'].get(tuple(point), {'state': 'uncaptured', 'height_m': None}))
        return operation.build_identified_path_floor(ROOT, values['references'], values['land_edge'], values['axis'],
            values['width_m'], values['control_points'], sample, values['evidence'], values['limits'],
            canonical_crs='EPSG:32618', maximum_triangles=budget)

    def trace(self, result):
        y = fixture()['references'][0]['vertices'][2][1]
        quads = [q for cell in result['surfaces'].values() for q in cell['quads']]
        return [sorted(p for p in q['vertices'] if p[1] == y) for q in quads]

    def test_independent_crops_reproduce_actual_whole_edge_height_gap(self):
        values = fixture(); crops = []; faces = []
        for reference in values['references']:
            crop = deepcopy(values); crop['references'] = [reference]
            a = reference['vertices'][0]
            crop['control_points'] = [[a[0]+3*x, a[1]-3*y] for y in range(11) for x in range(11)]
            result = self.build(crop)
            crops.append(self.trace(result)[0]); faces.extend(result['whole_ground_triangles'])
        self.assertEqual([p[:2] for p in crops[0]], [p[:2] for p in crops[1]])
        # Both traces are affine between these identical complete edge knots.
        gap = max(abs(F(a[2])-F(b[2])) for a, b in zip(*crops))
        self.assertAlmostEqual(.0025577366110609567, float(gap), places=15)
        self.assertGreater(float(gap), values['limits']['maximum_numeric_error_m'])
        corners = [*values['references'][0]['vertices'][:2], *values['references'][1]['vertices'][2:]]
        reference = [[r['vertices'][i][:3] for i in face] for r in values['references'] for face in ((0, 2, 1), (1, 2, 3))]
        with self.assertRaisesRegex(CompilerError, 'continuity'):
            operation._prove_topology(faces, corners, reference)

    def test_common_field_keeps_complete_width_and_exact_shared_xyz_semantics(self):
        result = self.baseline; values = fixture(); before = deepcopy(values)
        fresh = self.build(values, budget=788)
        self.assertEqual(before, values)
        self.assertEqual(result['surfaces'], fresh['surfaces'])
        self.assertEqual(result, json.loads(json.dumps(result, allow_nan=False)))
        traces = self.trace(result)
        self.assertEqual(14, len(traces[0])); self.assertEqual(*traces)
        metrics = result['metrics']
        self.assertEqual((1576, 842), (metrics['faces'], metrics['vertices']))
        self.assertEqual(908, result['consumed']['source_query_count'])
        self.assertAlmostEqual(300.0348433623663, metrics['protected_floor_area_m2'], places=9)
        self.assertLessEqual(metrics['protected_floor_whole_field_error_m'], 1e-8)
        self.assertLessEqual(metrics['protected_floor_whole_field_grade'], .05)
        self.assertLessEqual(metrics['protected_floor_whole_field_cross_slope'], .02)
        self.assertLessEqual(metrics['whole_original_nonLand_reference_error_m'], 1e-8)
        self.assertEqual(4, metrics['whole_cover']['query_triangles'])
        joined = []
        for reference, quad in zip(values['references'], result['surfaces'][values['references'][0]['cell_id']]['quads']):
            self.assertEqual(reference['coordinate'], quad['coordinate'])
            self.assertEqual(788, len(quad['triangles']))
            for p in quad['vertices']:
                self.assertEqual(operation._semantic(reference['vertices'], p[:2]), p[3:])
            joined.extend(tuple(tuple(quad['vertices'][i][:3]) for i in tri) for tri in quad['triangles'])
        self.assertEqual(Counter(tuple(tuple(p) for p in face) for face in result['whole_ground_triangles']), Counter(joined))

    def test_original_owner_split_budget_coverage_and_seam_faults_refuse(self):
        values = fixture(); faces = deepcopy(self.baseline['whole_ground_triangles'])
        with self.assertRaisesRegex(CompilerError, 'capacity'):
            operation._split_original_quads(faces, values['references'], 787)
        a, b, c, d = values['references'][0]['vertices']
        crossing = [[a[:3], b[:3], values['references'][1]['vertices'][2][:3]]]
        with self.assertRaisesRegex(CompilerError, 'crosses'):
            operation._split_original_quads(crossing, values['references'], 4096)
        traces = self.trace(self.baseline); shared = traces[0][len(traces[0])//2][:2]
        for face in faces:
            point = next((p for p in face if p[:2] == shared), None)
            if point is not None:
                point[2] += .001
                break
        corners = [*values['references'][0]['vertices'][:2], *values['references'][1]['vertices'][2:]]
        reference = [[r['vertices'][i][:3] for i in face] for r in values['references'] for face in ((0, 2, 1), (1, 2, 3))]
        with self.assertRaisesRegex(CompilerError, 'continuity'):
            operation._prove_topology(faces, corners, reference)
        semantic = operation._semantic
        def corrupt(points, xy):
            weights = semantic(points, xy)
            return [weights[0]-.001, weights[1]+.001] if points == values['references'][1]['vertices'] and xy[1] == shared[1] else weights
        with patch.object(operation, '_semantic', side_effect=corrupt), self.assertRaisesRegex(CompilerError, 'semantic'):
            operation._split_original_quads(self.baseline['whole_ground_triangles'], values['references'], 4096)

    def test_shared_unknown_is_explicit_and_missing_query_refuses(self):
        values = fixture(); point = (590323.41, 4516872.45)
        values['samples'][point] = {'state': 'unknown', 'height_m': None}
        result = self.build(values)
        node = next(row for row in result['nodes'] if tuple(row['xy']) == point)
        self.assertEqual('identified_reference_at_UNKNOWN_control', node['branch'])
        self.assertEqual(3.7, node['z']); self.assertIsNone(node['source']['height_m'])
        self.assertEqual(*self.trace(result))
        values['samples'].pop(point)
        with self.assertRaisesRegex(CompilerError, 'source'):
            self.build(values)

    def test_dependency_local_change_and_order_are_deterministic(self):
        values = fixture(); values['references'].reverse()
        self.assertEqual(self.baseline, self.build(values))
        node = next(row for row in self.baseline['nodes'] if row['branch'] == 'strict_source_control' and
            590329 < row['xy'][0] < 590340 and 4516850 < row['xy'][1] < 4516865)
        values = fixture(); values['samples'][tuple(node['xy'])]['height_m'] += .001
        changed = self.build(values)
        key = values['references'][0]['cell_id']
        self.assertEqual(self.baseline['surfaces'][key]['quads'][0], changed['surfaces'][key]['quads'][0])
        self.assertNotEqual(self.baseline['surfaces'][key]['quads'][1], changed['surfaces'][key]['quads'][1])
        self.assertEqual(self.baseline['consumed']['boundary_trace_sha256'], changed['consumed']['boundary_trace_sha256'])
        self.assertNotEqual(self.baseline['consumed']['source_queries_sha256'], changed['consumed']['source_queries_sha256'])

    def test_disconnected_conflicting_duplicate_and_unbounded_references_refuse(self):
        for kind in ('position', 'height', 'semantic', 'owner', 'count'):
            values = fixture()
            if kind == 'position':
                for p in values['references'][1]['vertices']: p[1] -= 1.
            elif kind == 'height': values['references'][1]['vertices'][0][2] += .001
            elif kind == 'semantic': values['references'][1]['vertices'][0][3:] = [.9, .1]
            elif kind == 'owner': values['references'][1]['coordinate'] = values['references'][0]['coordinate'][:]
            else: values['references'].append(deepcopy(values['references'][-1]))
            with self.subTest(kind=kind), self.assertRaises(CompilerError), patch.object(operation, 'triangulate_constraints') as native:
                self.build(values)
            native.assert_not_called()


if __name__ == '__main__':
    unittest.main()
