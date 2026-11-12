"""Real dry-ground controls without promoting a source or geographic choice."""
from copy import deepcopy
import csv
import math
import unittest
from unittest.mock import patch

from . import madison_ground_replay as model

from .madison_ground_replay import EDITED, FIXTURE, INCIDENT, faces, inputs, native_payload, point, replay
from ..app.contracts import CompilerError
from ..app.terrain_surface import finalize_terrain_cells
from ..refinement.api import refine_terrain_cells
from .madison_ground_replay import ROOT, window
from types import SimpleNamespace


class MadisonGroundTests(unittest.TestCase):
    def test_changed_finalizer_semantics_cannot_be_silently_discarded(self):
        finalize = model.finalize_terrain_cells

        def changed_semantics(terrain, *args):
            finalize(terrain, *args)
            cell = next(iter(terrain.values()))
            cell['surface_semantics']['core_samples']['ground'][0][0] = .5
            cell['surface_semantics']['core_samples']['hydro_transition'][0][0] = .5

        with patch.object(model, 'finalize_terrain_cells', side_effect=changed_semantics):
            with self.assertRaisesRegex(ValueError, 'semantic weights'):
                replay()

    def test_native_export_retains_original_grid_as_stale_control(self):
        base, candidate, _, _ = replay()
        payload = native_payload(candidate, base)
        for quad in payload['quads']:
            x, y = quad['coordinate']
            for vertex, (i, j) in zip(quad['vertices'], ((x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1))):
                original = payload['base_heights'][j * 4 + i]
                delta = 5.1 if (i + 6, j + 21) in EDITED else 0.
                self.assertAlmostEqual(delta, vertex[2] - original, places=12)

    def test_authored_fill_source_gain_is_distinct_from_local_shape_quality(self):
        base, candidate, _, report = replay()
        reference = {}
        with (FIXTURE / 'reference.csv').open(newline='', encoding='ascii') as stream:
            for row in csv.DictReader(stream):
                reference[float(row['x_m']), float(row['y_m'])] = float(row['ground_m'])
        residuals = sorted(reference[tuple(point(base, x, y)[:2])] - point(base, x, y)[2]
                           for x, y in EDITED)
        delta = round((residuals[1] + residuals[2]) / 2, 1)
        self.assertEqual(5.1, delta)
        errors = [abs(point(candidate, x, y)[2] - reference[tuple(point(base, x, y)[:2])])
                  for x, y in EDITED]
        self.assertLess(max(errors), 1., 'Selected authored control remains far from the provider Ground model')
        self.assertEqual(delta, inputs()['overlay']['terrain_patches'][0]['delta_m'])
        self.assertLess(report['reference_after']['mean_absolute_m'], report['reference_before']['mean_absolute_m'])
        self.assertEqual(2116, report['reference_after']['sample_count'])

    def test_face_observer_detects_changes_hidden_by_unchanged_global_maximum(self):
        _, _, _, report = replay()
        quality = report['face_quality']
        self.assertEqual(18, len(quality['faces']))
        self.assertEqual(6, quality['steepened_face_count'])
        self.assertEqual(report['maximum_gradient_before'], report['maximum_gradient_after'])
        faces_by_id = {tuple(row['face']): row for row in quality['faces']}
        self.assertAlmostEqual(2.403700850, faces_by_id[7, 23, 0]['before_slope_percent'], places=6)
        self.assertAlmostEqual(18.442101138, faces_by_id[7, 23, 0]['after_slope_percent'], places=6)
        self.assertTrue(any(row['after_normal_angle_degrees'] > row['before_normal_angle_degrees']+10
                            for row in quality['incident_edges']))
        self.assertTrue(any([8, 20, 1] in row['faces'] for row in quality['incident_edges']))

    def test_retained_base_fails_the_authored_control_and_repeat_does_not_accumulate(self):
        data = inputs()
        empty = deepcopy(data['overlay'])
        empty['terrain_patches'] = []
        _, retained, _, unchanged_report = replay(empty)
        original, first, local, corrected_report = replay()
        _, second, again, _ = replay()
        self.assertEqual(original['core_samples'], retained['core_samples'])
        self.assertEqual(first, second)
        self.assertEqual(local, again)
        self.assertEqual([[12.7, 13.1], [13.4, 14.0]], local['core_samples'])
        self.assertEqual(original['water_fit'], first['water_fit'])
        self.assertEqual([], unchanged_report['edited_nodes'])
        self.assertEqual([], unchanged_report['incident_quads'])
        self.assertEqual(unchanged_report['full_cell_refusal'], corrected_report['full_cell_refusal'])
        self.assertTrue(unchanged_report['full_cell_after_refusal_equals_input'])

    def test_exact_nine_quad_scope_and_unchanged_boundary_context(self):
        base, candidate, _, report = replay()
        changed = {(x, y) for y, row in enumerate(base['core_samples'])
                   for x, value in enumerate(row) if candidate['core_samples'][y][x] != value}
        incident = {(i, j) for x, y in changed for i in (x - 1, x) for j in (y - 1, y)}
        self.assertEqual(EDITED, changed)
        self.assertEqual(INCIDENT, incident)
        self.assertEqual([[1, 0]], report['dirty_cells'])
        self.assertEqual([], report['changed_feature_ids'])
        self.assertEqual([], report['water_intersections'])
        self.assertEqual(base['surface_semantics'], candidate['surface_semantics'])
        for y, row in enumerate(base['halo_window']['samples']):
            for x, height in enumerate(row):
                if (x - 1, y - 1) not in EDITED:
                    self.assertEqual(height, candidate['halo_window']['samples'][y][x])
        area = 0.
        for x, y in incident:
            for a, b, c in faces(candidate, x, y):
                determinant = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
                self.assertGreater(determinant, 0.)
                self.assertTrue(all(math.isfinite(value) for p in (a, b, c) for value in p))
                area += determinant / 2
        self.assertEqual(8100., area)

    def test_full_cell_still_refuses_unqualified_water_without_mutation(self):
        _, _, _, report = replay()
        self.assertIsNotNone(report['full_cell_refusal'])
        self.assertEqual('water_conditioning_policy_required', report['full_cell_refusal']['code'])
        self.assertEqual('alis:osm:way:452617299', report['full_cell_refusal']['details']['feature_id'])
        self.assertTrue(report['full_cell_after_refusal_equals_input'])

    def test_point_fallback_cannot_be_promoted_to_a_terrain_cut(self):
        data = inputs()
        for water in data['water']['features']:
            self.assertEqual('low_confidence_point_fallback', water['attributes']['surface_function']['accuracy'])
            with self.subTest(water=water['feature_id']), self.assertRaises(CompilerError) as error:
                refine_terrain_cells(ROOT, {}, water, {
                    'maximum_cut_m': 0, 'maximum_fill_m': 0, 'maximum_dry_distance_m': 0,
                    'maximum_bank_gradient': 0, 'water_clearance_m': 0, 'maximum_triangles': 2,
                    'accepted_accuracy': ['low_confidence_point_fallback']}, {})
            self.assertEqual('refinement_source_excluded', error.exception.code)

    def test_missing_authored_provenance_keeps_reduced_input_unchanged(self):
        data = inputs()
        cell = window(data['terrain'])
        before = deepcopy(cell)
        overlay = data['overlay']
        overlay['provenance'] = []
        profile = data['profile']
        profile['grid']['halo_samples'] = 0
        with self.assertRaises(CompilerError) as error:
            finalize_terrain_cells({cell['cell_id']: cell}, SimpleNamespace(by_owner={}),
                                   profile, cell['grid_id'], overlay)
        self.assertEqual('authored_overlay_provenance_missing', error.exception.code)
        self.assertEqual(before, cell)
