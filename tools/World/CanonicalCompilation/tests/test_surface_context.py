from copy import deepcopy
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))

from World.CanonicalCompilation.app.adapters import adapt_feature
from World.CanonicalCompilation.app.surface_context import surface_context
from World.CanonicalCompilation.app.contracts import CompilerError, canonical_hash
from World.CanonicalCompilation.tests.test_water_semantics import _bundle, _feature, _profile
from World.CanonicalCompilation.app.features import compile_features


class SurfaceContextTests(unittest.TestCase):
    def adapt(self, properties, feature_class='highway'):
        feature = _feature('way/1', {'type': 'LineString', 'coordinates': [[0, 0], [.01, 0]]}, **properties)
        feature['provider_class'] = feature_class
        original = deepcopy(feature)
        result, reason = adapt_feature(feature, 'fixture', _profile()['grid'],
                                       {'snapshot_id': 'fixture', 'provider': 'fixture', 'release': '1'})
        self.assertIsNone(reason)
        self.assertEqual(feature, original)
        return result

    def test_bridge_semantics_survive_without_fabricated_height(self):
        context = self.adapt({'highway': 'primary', 'bridge': 'yes', 'layer': '1'})['attributes'].get('surface_context')
        self.assertIsInstance(context, dict, 'Canonical road lost the source crossing evidence')
        self.assertEqual('above', context['ground_relation'])
        self.assertEqual(1, context['relative_layer'])
        self.assertIn('metric_elevation_required', context['unresolved'])
        self.assertNotIn('height_m', context)
        self.assertEqual({'bridge': 'yes', 'layer': '1'}, context['source_tags'])

    def test_layer_covered_and_missing_tags_do_not_invent_tunnel(self):
        for properties in ({'layer': '-1'}, {'covered': 'yes'}, {}):
            with self.subTest(properties=properties):
                context = self.adapt(properties)['attributes'].get('surface_context')
                self.assertIsInstance(context, dict)
                self.assertEqual('unknown', context['ground_relation'])
                self.assertEqual('unknown', context['tunnel'])
                self.assertNotIn('height_m', context)

    def test_conflicting_and_unsupported_evidence_stays_unresolved(self):
        for properties in ({'bridge': 'yes', 'tunnel': 'yes'},
                           {'bridge': 'invented_type'}, {'layer': 'one'}, {'layer': True}):
            with self.subTest(properties=properties):
                context = self.adapt(properties)['attributes'].get('surface_context')
                self.assertIsInstance(context, dict)
                self.assertIn(context['ground_relation'], ('unknown', 'conflicting'))
                self.assertTrue(context['unresolved'])

    def test_ford_is_not_bridge_and_ele_has_no_qualified_datum(self):
        context = self.adapt({'ford': 'yes', 'ele': '12.3'})['attributes'].get('surface_context')
        self.assertIsInstance(context, dict)
        self.assertEqual('at_ground', context['ground_relation'])
        self.assertEqual('present', context['ford'])
        self.assertIn('vertical_datum_required', context['unresolved'])
        self.assertNotIn('elevation_m', context)

    def test_constructed_fountain_survives_water_preparation(self):
        profile = _profile()
        profile['water_semantics']['class_rules'].append(
            {'water_class': 'water', 'geometry': 'polygon', 'result': 'surface', 'behavior': 'standing'})
        feature = _feature('way/2', {'type': 'Polygon', 'coordinates': [
            [[.1, .1], [.2, .1], [.2, .2], [.1, .2], [.1, .1]]]},
            natural='water', amenity='fountain', barrier='kerb')
        original = deepcopy(feature)
        terrain = {str(i): {'bounds': [i * 1000, 0, (i + 1) * 1000, 1000],
                            'sample_spacing': [1000, 1000], 'core_samples': [[50, 50], [50, 50]]}
                   for i in range(2)}
        scratch = ROOT / 'tmp/world/surface_context_tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            result = compile_features(_bundle([feature]), profile, 'grid_0000000000000000',
                                      {'feature_overrides': []}, ROOT, Path(directory), terrain=terrain)
        admitted = next(f for group in result.by_owner.values() for f in group)
        context = admitted['attributes'].get('surface_context')
        self.assertIsInstance(context, dict, 'Water prepass erased the constructed basin evidence')
        self.assertEqual('constructed', context['water_origin'])
        self.assertEqual('fountain', context['water_kind'])
        self.assertIn('constructed_surface_elevation_required', context['unresolved'])
        self.assertEqual('standing', admitted['attributes']['surface_behavior'])
        self.assertEqual(feature, original)

    def test_natural_water_key_does_not_assert_natural_origin(self):
        context = self.adapt({'natural': 'water'}, 'water')['attributes'].get('surface_context')
        self.assertIsInstance(context, dict)
        self.assertEqual('unknown', context['water_origin'])

    def test_semantic_changes_change_identity_without_affecting_other_features(self):
        old = self.adapt({'highway': 'primary'})
        bridge = self.adapt({'highway': 'primary', 'bridge': 'yes'})
        irrelevant = self.adapt({'highway': 'primary', 'unrelated': 'value'})
        self.assertNotEqual(canonical_hash(old), canonical_hash(bridge))
        self.assertEqual(canonical_hash(old), canonical_hash(irrelevant))

    def test_extreme_layer_stays_observably_unresolved(self):
        for value in ('9' * 5000, '2147483648', '-2147483649'):
            with self.subTest(length=len(value)):
                context = self.adapt({'layer': value})['attributes']['surface_context']
                self.assertIsNone(context['relative_layer'])
                self.assertIn('unsupported_layer', context['unresolved'])

    def test_malformed_source_evidence_refuses_without_mutation(self):
        for value in (float('nan'), float('inf'), ['yes'], {'value': 'yes'}):
            properties = {'bridge': value}
            before = canonical_hash(properties)
            with self.subTest(value=repr(value)), self.assertRaises(CompilerError) as error:
                self.adapt(properties)
            self.assertEqual('surface_context_invalid', error.exception.code)
            self.assertIs(properties['bridge'], value)
            self.assertEqual(before, canonical_hash(properties))

    def test_native_way_nodes_keep_original_projected_sequence(self):
        context = self.adapt({'waterway': 'drain', '@way_nodes': [100, 200]}, 'water')['attributes']['surface_context']
        self.assertEqual([{'provider_node_id': 'node/100', 'xy': [0.0, 0.0]},
                          {'provider_node_id': 'node/200', 'xy': [10.0, 0.0]}], context['source_nodes'])

    def test_invalid_way_node_sequences_refuse_instead_of_inventing_connectivity(self):
        for values in ([100], [100, True], [100, -1], [100, '200'], '100,200'):
            with self.subTest(values=values):
                with self.assertRaises(CompilerError) as error:
                    self.adapt({'@way_nodes': values})
                self.assertEqual('surface_topology_invalid', error.exception.code)

    def test_repeated_nodes_and_large_ids_preserve_source_identity(self):
        geometry = {'type': 'LineString', 'coordinates': [[0, 0], [10, 0], [0, 0]]}
        identifier = 2 ** 53 + 1
        result = surface_context({'@way_nodes': [identifier, 20, identifier]}, 'road', geometry)
        nodes = result['source_nodes']
        self.assertEqual(f'node/{identifier}', nodes[0]['provider_node_id'])
        self.assertEqual(nodes[0], nodes[2])
        distinct = surface_context({'@way_nodes': [10, 20, 30]}, 'road', geometry)['source_nodes']
        self.assertEqual(distinct[0]['xy'], distinct[2]['xy'])
        self.assertNotEqual(distinct[0]['provider_node_id'], distinct[2]['provider_node_id'])
        with self.assertRaises(CompilerError):
            surface_context({'@way_nodes': [10, 10, 30]}, 'road', geometry)

    def test_polygon_ring_does_not_receive_positional_node_ids(self):
        polygon = {'type': 'Polygon', 'coordinates': [[[0, 0], [10, 0], [0, 10], [0, 0]]]}
        self.assertEqual([], surface_context({'@way_nodes': [10, 20, 30, 10]}, 'water', polygon)['source_nodes'])


if __name__ == '__main__':
    unittest.main()
