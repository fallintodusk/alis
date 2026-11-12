"""Boundary-crossing full-width floor composition and same-owner refusal controls."""
from collections import Counter
from copy import deepcopy
import json
from pathlib import Path
import unittest
from unittest.mock import patch

from World.CanonicalCompilation.app.contracts import CompilerError, canonical_hash
from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from ..refinement import ground_path_floor as operation
from .ground_path_floor_fixture import fixture

ROOT = Path(__file__).resolve().parents[4]


class GroundPathFloorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.baseline = cls('test_captured_complete_width_field_source_states_semantics_and_determinism').build(fixture())

    def build(self, values, sampler=None, budget=4096):
        def captured(point):
            return deepcopy(values['samples'].get(tuple(point), {'state': 'uncaptured', 'height_m': None}))
        return operation.build_identified_path_floor(ROOT, [values['reference']], values['land_edge'],
            values['axis'], values['width_m'], values['control_points'], sampler or captured,
            values['evidence'], values['limits'], canonical_crs='EPSG:32618', maximum_triangles=budget)

    def test_captured_complete_width_field_source_states_semantics_and_determinism(self):
        values = fixture(); before = deepcopy(values)
        result = self.baseline
        fresh = self.build(values)
        self.assertEqual(before, values)
        self.assertEqual(result, fresh)
        self.assertEqual(result, json.loads(json.dumps(result, allow_nan=False)))
        self.assertEqual('unpublished_model_space', result['publication'])
        self.assertNotIn('$schema', result)
        metrics = result['metrics']
        self.assertAlmostEqual(150.01742168118315, metrics['protected_floor_area_m2'], places=9)
        self.assertLessEqual(metrics['protected_floor_whole_field_error_m'], 1e-8)
        self.assertLessEqual(metrics['protected_floor_whole_field_grade'], .05)
        self.assertLessEqual(metrics['protected_floor_whole_field_cross_slope'], .02)
        self.assertLessEqual(metrics['whole_original_nonLand_reference_error_m'], 1e-8)
        self.assertEqual(790, metrics['faces'])
        self.assertEqual(429, metrics['vertices'])
        self.assertEqual(66, metrics['boundary_edges'])
        self.assertEqual(1152, metrics['shared_edges'])
        self.assertTrue(any(row['source']['state'] == 'unknown' for row in result['nodes']))
        self.assertTrue(all(row['source']['height_m'] is None for row in result['nodes'] if row['source']['state'] == 'unknown'))
        quad = result['surfaces'][values['reference']['cell_id']]['quads'][0]
        for point in quad['vertices']:
            self.assertTrue(all(0 <= v <= 1 for v in point[3:]))
            self.assertAlmostEqual(1., sum(point[3:]), places=12)
        inside = [row for row in result['side_observations'] if row['model_state'] == 'inside_domain']
        outside = [row for row in result['side_observations'] if row['model_state'] == 'outside_domain']
        self.assertEqual((22, 2), (len(inside), len(outside)))
        self.assertTrue(all(row['model_height_m'] is None for row in outside))
        self.assertGreater(max(abs(row['signed_departure_m']) for row in inside), 2.)

    def test_captured_low_reference_shore_join_fails_the_same_floor_proof(self):
        values = fixture(); result = self.baseline
        with self.assertRaisesRegex(CompilerError, 'floor'):
            operation._prove_floor(values['bad_faces'], result['floor_specification'], values['limits'])

    def test_axis_unknown_missing_query_and_unknown_with_invented_zero_refuse(self):
        values = fixture(); result = self.baseline
        point = tuple(result['axis_observations'][0]['xy'])
        values['samples'][point] = {'state': 'unknown', 'height_m': None}
        with self.assertRaisesRegex(CompilerError, 'axis'):
            self.build(values)
        with self.assertRaisesRegex(CompilerError, 'source'):
            self.build(fixture(), lambda _: {'state': 'uncaptured', 'height_m': None})
        with self.assertRaisesRegex(CompilerError, 'source'):
            self.build(fixture(), lambda _: {'state': 'unknown', 'height_m': 0.})

    def test_consumed_interior_unknown_keeps_reference_while_absent_or_unreadable_refuse(self):
        point = (590323.41, 4516872.45)
        values = fixture(); values['samples'][point] = {'state': 'unknown', 'height_m': None}
        result = self.build(values)
        node = next(row for row in result['nodes'] if tuple(row['xy']) == point)
        self.assertEqual('identified_reference_at_UNKNOWN_control', node['branch'])
        self.assertEqual({'state': 'unknown', 'height_m': None}, node['source'])
        self.assertEqual(values['reference']['vertices'][2][2], node['z'])
        self.assertNotEqual(0., node['z'])
        missing = fixture(); missing['samples'].pop(point)
        with self.assertRaisesRegex(CompilerError, 'source'):
            self.build(missing)
        def unreadable(p):
            if tuple(p) == point: raise OSError('Captured source control is unreadable')
            return fixture_values['samples'].get(tuple(p), {'state': 'uncaptured', 'height_m': None})
        fixture_values = fixture()
        with self.assertRaisesRegex(OSError, 'unreadable'):
            self.build(fixture_values, unreadable)

    def test_actual_floor_height_and_cross_slope_faults_refuse_in_constructor(self):
        original = operation._assign_heights
        for fault in ('height', 'cross'):
            def corrupt(*args, **kwargs):
                rows = original(*args, **kwargs)
                floor = [row for row in rows if row['branch'] == 'identified_original_width_path_floor']
                if fault == 'height':
                    floor[len(floor)//2]['z'] += .001
                else:
                    center = sum(row['xy'][0] for row in floor)/len(floor)
                    for row in floor: row['z'] += .03*(row['xy'][0]-center)
                return rows
            with self.subTest(fault=fault), patch.object(operation, '_assign_heights', side_effect=corrupt):
                with self.assertRaisesRegex(CompilerError, 'floor'):
                    self.build(fixture())

    def test_missing_floor_breaklines_refuse_even_with_same_source_controls(self):
        def source(point):
            return {'state': 'known', 'height_m': 4.+.02*abs(point[1]-4516887.45)}
        values = fixture(); self.build(values, source)
        native = operation.triangulate_constraints
        def omit_breaklines(repo, polygon, segments, points, maximum_triangles):
            # Keep the original diagonal, Land cut and complete floor boundary.
            return native(repo, polygon, [*segments[:2], *segments[-4:]], points, maximum_triangles)
        with patch.object(operation, 'triangulate_constraints', side_effect=omit_breaklines):
            with self.assertRaisesRegex(CompilerError, 'floor'):
                self.build(values, source)

    def test_balanced_duplicate_hole_and_conflicting_shared_height_refuse(self):
        values = fixture(); result = self.baseline
        faces = deepcopy(result['whole_ground_triangles'])
        counts = Counter(tuple(p[:2]) for face in faces for p in face)
        groups = {}
        for i, face in enumerate(faces):
            if all(counts[tuple(p[:2])] >= 3 for p in face):
                groups.setdefault(abs(operation._cross(*[[operation.F(v) for v in p] for p in face])), []).append(i)
        indices = next(rows for rows in groups.values() if len(rows) >= 2)
        broken = deepcopy(faces); broken[indices[0]] = deepcopy(broken[indices[1]])
        with self.assertRaisesRegex(CompilerError, 'topology|cover'):
            operation._prove_topology(broken, values['reference']['vertices'],
                [[values['reference']['vertices'][i][:3] for i in face] for face in ((0, 2, 1), (1, 2, 3))])
        faces[0][0][2] += .001
        with self.assertRaisesRegex(CompilerError, 'height|continuity'):
            operation._prove_topology(faces, values['reference']['vertices'],
                [[values['reference']['vertices'][i][:3] for i in face] for face in ((0, 2, 1), (1, 2, 3))])

    def test_model_dependency_locality_and_closed_inputs(self):
        values = fixture(); result = self.baseline
        unused = fixture(); unused['samples'][(0., 0.)] = {'state': 'known', 'height_m': 999.}
        self.assertEqual(result, self.build(unused))
        changed = fixture(); point = tuple(result['axis_observations'][0]['xy'])
        changed['samples'][point]['height_m'] += .001
        fresh = self.build(changed)
        self.assertNotEqual(result['consumed']['boundary_trace_sha256'], fresh['consumed']['boundary_trace_sha256'])
        self.assertNotEqual(result['consumed']['source_queries_sha256'], fresh['consumed']['source_queries_sha256'])
        for key, value in [('maximum_grade', .06), ('maximum_cross_slope', .03), ('maximum_numeric_error_m', .001)]:
            invalid = fixture(); invalid['limits'][key] = value
            with self.subTest(key=key), self.assertRaises(CompilerError): self.build(invalid)
        with self.assertRaises((CompilerError, ExecutionEnvironmentError)):
            self.build(fixture(), budget=32)
        invalid = fixture(); invalid['evidence']['vertical_datum'] = 'unknown'
        with self.assertRaises(CompilerError): self.build(invalid)


if __name__ == '__main__':
    unittest.main()
