"""Complete captured inferred basin, source separation and bounded refusal paths."""
from copy import deepcopy
import math
from pathlib import Path
import unittest
from unittest.mock import patch

from World.ExecutionEnvironment.app.contracts import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.affine import measure_affine_cover
from ..app.contracts import CompilerError, canonical_hash
from ..refinement import identified_basin as operation
from ..refinement.app.contained_water import build_contained_water
from .identified_basin_fixture import fixture

ROOT = Path(__file__).resolve().parents[4]


def build(values, budget=4096):
    return operation.build_identified_basin(ROOT, **values, canonical_crs='EPSG:32618', maximum_triangles=budget)


class IdentifiedBasinTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.values = fixture()
        before = deepcopy(cls.values)
        cls.result = build(cls.values)
        assert before == cls.values

    def test_complete_captured_joint_field_and_explicit_inference(self):
        result = self.result
        self.assertEqual(result['publication'], 'unpublished_model_space')
        self.assertNotIn('$schema', result)
        self.assertNotIn('feature', result)
        self.assertEqual(result['modeled_water']['function_id'], 'identified_static_lined_basin')
        self.assertEqual(result['modeled_water']['original_mapped_vertices_preserved'], 19)
        self.assertEqual(result['modeled_water']['modeled_boundary_vertices'], 45)
        self.assertEqual(len(result['whole_ground_triangles']), 1606)
        self.assertAlmostEqual(result['metrics']['maximum_gradient'], .3228030381943, places=11)
        self.assertAlmostEqual(result['metrics']['modeled_spill_margin_m'], .15, places=12)
        self.assertGreater(result['metrics']['modeled_wet_volume_m3'], 8.)
        self.assertGreater(result['boundary_encoding_certificates']['wet']['vertices_off_exact_source_edges'], 0)
        self.assertEqual(self.values, fixture())
        # Real source tags never become the observed fountain/kerb context.
        with self.assertRaisesRegex(CompilerError, 'context'):
            build_contained_water(ROOT, self.values['water'], [], {}, {}, quantile=.5,
                height_step=.1, canonical_crs='EPSG:32618')

    def test_cold_rebuild_and_depth_dependency_locality(self):
        self.assertEqual(self.result, build(fixture()))
        changed = fixture()
        changed['model']['parameters']['depth_m'] = .3
        result = build(changed)
        self.assertNotEqual(result['consumed']['model_sha256'], self.result['consumed']['model_sha256'])
        self.assertEqual(result['consumed']['boundary_trace_sha256'], self.result['consumed']['boundary_trace_sha256'])
        self.assertEqual(result['modeled_water'], self.result['modeled_water'])
        wet = set(result['region_face_indices']['wet'])
        self.assertEqual(result['region_face_indices'], self.result['region_face_indices'])
        for i, face in enumerate(result['whole_ground_triangles']):
            previous = self.result['whole_ground_triangles'][i]
            self.assertEqual([p[:2] for p in face], [p[:2] for p in previous])
            if i not in wet:
                self.assertEqual(face, previous)
        self.assertAlmostEqual(result['metrics']['modeled_wet_volume_m3'],
            2*self.result['metrics']['modeled_wet_volume_m3'], places=9)

    def test_actual_quad_consumption_and_shared_semantics(self):
        result = self.result
        faces, by_edge = [], []
        for key in sorted(result['surfaces']):
            quad = result['surfaces'][key]['quads'][0]
            faces.extend([[quad['vertices'][i][:3] for i in face] for face in quad['triangles']])
            by_edge.append(sorted(p for p in quad['vertices'] if p[1] == 4512912.45))
            self.assertTrue(all(0 <= v <= 1 for p in quad['vertices'] for v in p[3:]))
            self.assertTrue(all(abs(sum(p[3:])-1) < 1e-12 for p in quad['vertices']))
        self.assertEqual(by_edge[0], by_edge[1])
        normalize = lambda ff: sorted(tuple(sorted(tuple(p) for p in face)) for face in ff)
        self.assertEqual(normalize(faces), normalize(result['whole_ground_triangles']))

    def test_wrong_generated_exterior_height_refuses_in_constructor(self):
        result = self.result
        reference_nodes = {tuple(p[:2]) for face in result['provider_reference_triangles'] for p in face}
        inside = set(result['region_face_indices']['influence'])
        candidates = {tuple(p[:2]) for i, face in enumerate(result['whole_ground_triangles'])
            if i not in inside for p in face}-reference_nodes
        # Pick a well-conditioned real contour/grid intersection so the
        # perturbation passes the separate gradient and containment screens.
        def altitude(xy):
            return min(2*operation._area(face)/max(
                (sum((a[k]-b[k])**2 for k in (0, 1)))**.5
                for a, b in zip(face, (*face[1:], face[0])))
                for face in result['whole_ground_triangles'] if any(tuple(p[:2]) == xy for p in face))
        target = max(sorted(candidates), key=altitude)
        reference = operation._reference
        def changed(grid, xy):
            return reference(grid, xy)+(.001 if tuple(xy) == target else 0.)
        values = fixture()
        before = deepcopy(values)
        with patch.object(operation, '_reference', changed):
            with self.assertRaisesRegex(CompilerError, 'outside-influence field'):
                build(values)
        self.assertEqual(values, before)

    def test_unknown_contradictory_stale_and_unsupported_inputs_refuse_before_native(self):
        cases = []
        for field, value in (('depth_m', 0), ('level_m', float('nan')), ('rim_freeboard_m', -1)):
            values = fixture()
            values['model']['parameters'][field] = value
            cases.append(values)
        for field, value in (('source_geometry_sha256', '0'*64), ('basis', 'qualified_ground_tin'),
                ('representation', 'exact_unchanged_footprint'), ('role', 'observed_fountain')):
            values = fixture()
            values['model'][field] = value
            cases.append(values)
        for field, value in (('known_canonical_under_routes', ['captured_under_route']),
                ('unmapped_under_routes', 'absent'), ('basis', 'observed_ground')):
            values = fixture()
            values['model']['support'][field] = value
            cases.append(values)
        values = fixture()
        values['evidence']['grid_content_sha256'] = '0'*64
        cases.append(values)
        for i, values in enumerate(cases):
            before = deepcopy(values)
            with self.subTest(case=i), patch.object(operation, 'triangulate_constraints') as native:
                with self.assertRaises((CompilerError, ExecutionEnvironmentError)):
                    build(values)
                native.assert_not_called()
            # NaN is intentionally retained by deepcopy; compare content identities.
            self.assertEqual(canonical_hash(before), canonical_hash(values))

    def test_complete_contour_crossings_contacts_and_missing_domain_refuse(self):
        for kind in ('core_contact', 'core_crossing', 'influence_leaves_domain', 'lost_quad'):
            values = fixture()
            if kind == 'core_contact':
                values['model']['contours']['core'] = deepcopy(values['water']['geometry']['coordinates'][0][0])
            elif kind == 'core_crossing':
                values['model']['contours']['core'] = deepcopy(values['model']['contours']['influence'])
            elif kind == 'influence_leaves_domain':
                for point in values['model']['contours']['influence']:
                    point[0] += 30.
            else:
                del values['selected']['captured_basin_quad_1']
            before = deepcopy(values)
            with self.subTest(case=kind), self.assertRaises((CompilerError, ExecutionEnvironmentError)):
                build(values)
            self.assertEqual(values, before)

    def test_reflex_contour_refuses_instead_of_building_a_basin(self):
        values = fixture()
        ring = values['model']['contours']['influence']
        center = [math.fsum(p[k] for p in ring[:-1])/(len(ring)-1) for k in (0, 1)]
        midpoint = [(ring[10][k]+ring[11][k])/2 for k in (0, 1)]
        direction = [center[k]-midpoint[k] for k in (0, 1)]
        length = math.hypot(*direction)
        ring.insert(11, [midpoint[k]+.001*direction[k]/length for k in (0, 1)])
        before = deepcopy(values)
        with patch.object(operation, 'triangulate_constraints', wraps=operation.triangulate_constraints) as native:
            with self.assertRaisesRegex(CompilerError, 'strictly convex'):
                build(values)
            native.assert_not_called()
        self.assertEqual(values, before)

    def test_actual_gradient_spill_screens_and_capacity_refuse_without_mutation(self):
        for kind in ('gradient', 'spill', 'old_dsm_stage', 'capacity'):
            values = fixture()
            if kind == 'gradient':
                values['model']['parameters']['maximum_gradient'] = .01
            elif kind == 'spill':
                values['model']['parameters']['minimum_spill_margin_m'] = .2
            elif kind == 'old_dsm_stage':
                values['model']['parameters']['level_m'] = 168.3
            before = deepcopy(values)
            with self.subTest(case=kind), self.assertRaises((CompilerError, ExecutionEnvironmentError)):
                build(values, budget=32 if kind == 'capacity' else 4096)
            self.assertEqual(values, before)

    def test_breached_rim_and_missing_face_cannot_claim_containment(self):
        result = self.result
        faces = deepcopy(result['whole_ground_triangles'])
        outer = operation._boundary(faces, range(len(faces)))
        # Lower every rim and transition node to the stage; this opens a real
        # connected sublevel route to the low exterior without changing XY.
        wet = set(result['region_face_indices']['wet'])
        level = result['modeled_water']['level_m']
        for i, face in enumerate(faces):
            if i not in wet:
                for p in face:
                    p[2] = min(p[2], level)
        with self.assertRaises(CompilerError):
            operation._containment(faces, wet, level, outer, .1)
        with self.assertRaises(CompilerError):
            operation._boundary(result['whole_ground_triangles'], [0, 0, 1])
        omitted = next(iter(wet))
        with self.assertRaises(CompilerError):
            operation._boundary(result['whole_ground_triangles'],
                [i for i in range(len(faces)) if i != omitted])

    def test_complete_reference_cover_and_whole_outside_field(self):
        result = self.result
        flat = lambda faces: [[[p[0], p[1], 0.] for p in face] for face in faces]
        faces, reference = result['whole_ground_triangles'], result['provider_reference_triangles']
        cover = measure_affine_cover(flat(faces), flat(reference))
        self.assertEqual(cover['maximum_height_error_upper_m'], 0.)
        reverse = measure_affine_cover(flat(reference), flat(faces))
        self.assertEqual(reverse['maximum_height_error_upper_m'], 0.)
        inside = set(result['region_face_indices']['influence'])
        outside = [face for i, face in enumerate(faces) if i not in inside]
        proof = measure_affine_cover(reference, outside)
        self.assertLess(proof['maximum_height_error_upper_m'], 1e-8)
        self.assertEqual(proof, result['metrics']['whole_outside_reference_field'])
        changed = deepcopy(faces)
        i = next(i for i in range(len(faces)) if i not in inside)
        changed[i][0][2] += .001
        with self.assertRaises(CompilerError):
            operation._prove_exterior(reference, changed, inside)
