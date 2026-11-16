"""Characterize a rejected mechanism; GREEN here never accepts the shoreline."""
from copy import deepcopy
import unittest

from . import pier_shore_fixture as scenario


class PierShoreDiagnosticTests(unittest.TestCase):
    def test_existing_sparse_clipping_lowers_dry_ground_and_does_not_close_outer_trace(self):
        before = deepcopy(scenario.CAPTURE)
        result = scenario.compare_sparse()
        self.assertEqual('NOT_ADMISSIBLE',result['disposition'])
        self.assertGreater(result['dry_area_m2'],0.)
        self.assertGreater(result['dry_cut_volume_m3'],1.)
        self.assertGreater(result['outer_refusals'].get('refinement_domain_open',0),0)
        self.assertGreater(max(p['dry_ground_departure_m'] for p in result['contacts']),3.)
        self.assertGreater(max(p['boundary_height_conflict_if_all_dry_preserved_m']
                               for p in result['contacts']),3.)
        self.assertGreater(max(p['minimum_transition_gradient_if_source_contact_retained']
                               for p in result['contacts']),57.)
        self.assertEqual(before,scenario.CAPTURE)

    def test_lower_ground_control_does_not_falsely_report_a_transition_conflict(self):
        value = scenario.fixture()
        stage = value['original_water']['attributes']['surface_function']['level_m']
        value['cell']['water_fit']['halo_window']['samples'] = [
            [stage for _ in row] for row in value['cell']['water_fit']['halo_window']['samples']]
        value['expected_original_core'] = [[stage for _ in row] for row in value['expected_original_core']]
        result = scenario.compare_sparse(value)
        # Equal heights remove the C0 conflict; new outer subdivisions still
        # require their neighbors and do not certify a closed candidate.
        self.assertEqual('NOT_ADMISSIBLE',result['disposition'])
        self.assertNotIn('refinement_domain_open',result['outer_refusals'])
        self.assertGreater(result['outer_refusals'].get('refinement_neighbor_required',0),0)
        self.assertEqual(0.,result['dry_cut_volume_m3'])
        self.assertTrue(all(p['dry_ground_departure_m']==0. for p in result['contacts']))
        self.assertTrue(all(p['boundary_height_conflict_if_all_dry_preserved_m']==0.
                            for p in result['contacts']))
        self.assertTrue(any(abs(p['source_to_old_deck_difference_m'])>1. for p in result['contacts']))

    def test_wet_only_cut_is_not_reported_as_dry_ground_loss(self):
        value = scenario.fixture()
        stage = value['original_water']['attributes']['surface_function']['level_m']
        value['cell']['water_fit']['halo_window']['samples'] = [
            [stage for _ in row] for row in value['cell']['water_fit']['halo_window']['samples']]
        value['expected_original_core'] = [[stage for _ in row] for row in value['expected_original_core']]
        # This west-boundary vertex has three wholly wet affine half-quads.
        value['cell']['water_fit']['halo_window']['samples'][3][1] = 2.
        result = scenario.compare_sparse(value)
        self.assertAlmostEqual(1350.,result['total_cut_volume_m3'],delta=1e-6)
        self.assertEqual(0.,result['dry_cut_volume_m3'])
        self.assertNotIn('refinement_domain_open',result['outer_refusals'])


if __name__=='__main__': unittest.main()
