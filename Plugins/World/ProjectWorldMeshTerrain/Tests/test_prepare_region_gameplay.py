"""Source-faithful local route refusals without starting Unreal or a city pipeline."""
import math
import unittest

from prepare_region_gameplay import compose_axis, sample, slice_axis, top_support


class RegionalGameplayPreparationTests(unittest.TestCase):
    def test_captured_kazan_footway_submerged_interval_refuses_supported_bank_stays_explicit(self):
        ground = [
            [[380990., 6185670., 50.], [380990., 6185640., 50.], [381020., 6185670., 50.]],
            [[381020., 6185700., 59.9], [380990., 6185670., 50.], [381020., 6185670., 50.]],
        ]
        water = [[[381018.85, 6185692.59, 50.25], [380985.14, 6185683.76, 50.25], [381011.72, 6185639.42, 50.25]]]
        with self.assertRaisesRegex(ValueError, 'submerged'):
            top_support(ground, [], water, [381001.3582193308, 6185660.353813214])
        height, stage, structural = top_support(ground, [], water, [380998.89, 6185672.84])
        self.assertGreater(height-stage, .68)
        self.assertFalse(structural)

    def test_separate_support_uses_actual_varying_water_and_cannot_conceal_missing_ground(self):
        ground = [[[0., 0., 0.], [2., 0., 0.], [0., 2., 0.]]]
        deck = [[[0., 0., 1.], [2., 0., 1.], [0., 2., 1.]]]
        water = [[[0., 0., .25], [2., 0., .75], [0., 2., .25]]]
        height, stage, structural = top_support(ground, deck, water, [.5, .5])
        self.assertEqual((height, stage, structural), (1., .375, True))
        with self.assertRaisesRegex(ValueError, 'submerged'):
            top_support(ground, [], water, [.5, .5])
        with self.assertRaisesRegex(ValueError, 'Ground'):
            top_support([], deck, water, [.5, .5])
        self.assertIsNone(sample(water, [3., 3.]))

    def test_source_axis_slicing_keeps_vertices_and_refuses_an_unbound_extension(self):
        axis = [[0., 0.], [3., 0.], [3., 4.]]
        self.assertEqual(slice_axis(axis, 1., 6.), [[1., 0.], [3., 0.], [3., 3.]])
        self.assertEqual(slice_axis(axis, 0., 3.), [[0., 0.], [3., 0.]])
        self.assertEqual(slice_axis(axis, 0., math.nextafter(7., math.inf))[-1], axis[-1])
        with self.assertRaisesRegex(ValueError, 'escapes'):
            slice_axis(axis, 1., 8.)

    def test_two_real_source_legs_join_without_inventing_a_connector(self):
        features = {key: {'feature_id': key, 'feature_class': 'road', 'geometry': {'type': 'LineString', 'coordinates': axis},
                         'attributes': {'width_m': 2.}} for key, axis in {'a': [[0., 0.], [3., 0.]], 'b': [[3., 0.], [3., 4.]]}.items()}
        request = {'legs': [{'source_feature_id': key, 'start_m': 0., 'end_m': length} for key, length in [('a', 3.), ('b', 4.)]]}
        points, _, width = compose_axis(request, features)
        self.assertEqual((points, width), ([[0., 0.], [3., 0.], [3., 4.]], 2.))
        features['b']['geometry']['coordinates'][0] = [3.1, 0.]
        with self.assertRaisesRegex(ValueError, 'exact original endpoint'):
            compose_axis(request, features)


if __name__ == '__main__':
    unittest.main()
