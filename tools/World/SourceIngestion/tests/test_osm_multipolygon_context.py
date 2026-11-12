"""Complete captured source relation and literal membership/counterpart refusals."""
from copy import deepcopy
from pathlib import Path
import unittest
from unittest.mock import patch

from World.SourceIngestion import api
from World.SourceIngestion.app.contracts import canonical_hash
from World.SourceIngestion import osm_multipolygon_context as operation
from World.SourceIngestion.app import osm_context, run_identity
from .osm_pier_relation_fixture import fixture

RELATION = 'relation/20784500'


class OsmMultipolygonContextTests(unittest.TestCase):
    def test_actual_complete_pier_preserves_all_primitive_geometry_and_evidence(self):
        document, primitives = fixture(); before = deepcopy((document, primitives))
        result = api.osm_multipolygon_context(document, primitives, RELATION)
        self.assertEqual(before, (document, primitives))
        self.assertEqual((4, 15), (len(result['members']), len(result['nodes'])))
        self.assertEqual(RELATION, result['relation']['provider_feature_id'])
        self.assertEqual('multipolygon', result['primitive']['tags']['type'])
        self.assertNotIn('type', result['relation']['properties'])
        self.assertEqual([m['ref'] for m in result['primitive']['members']], [m['properties']['@id'] for m in result['members']])
        self.assertTrue(all(m['role'] == 'outer' for m in result['primitive']['members']))
        self.assertEqual('no', result['relation']['properties']['floating'])
        self.assertEqual('pier', result['relation']['properties']['man_made'])
        result['nodes'][0]['geometry']['coordinates'][0] += 1.
        self.assertEqual(before, (document, primitives))
        # The existing way-only interface cannot substitute for relation qualification.
        with self.assertRaises(api.IngestionError): api.osm_way_context(document, RELATION)

    def test_missing_duplicate_and_incomplete_member_context_refuses_without_mutation(self):
        for defect in ('member', 'node', 'duplicate_native', 'duplicate_normalized', 'missing_closer', 'nested', 'role', 'duplicate_member'):
            document, primitives = fixture()
            relation = next(p for p in primitives['elements'] if p['type'] == 'relation')
            if defect == 'member': primitives['elements'] = [p for p in primitives['elements'] if p['id'] != relation['members'][0]['ref']]
            elif defect == 'node': document['features'] = [p for p in document['features'] if p['provider_feature_id'] != 'node/194612202']
            elif defect == 'duplicate_native': primitives['elements'].append(deepcopy(primitives['elements'][0]))
            elif defect == 'duplicate_normalized': document['features'].append(deepcopy(document['features'][0]))
            elif defect == 'missing_closer': relation['members'].pop()
            elif defect == 'nested': relation['members'][0]['type'] = 'relation'
            elif defect == 'role': relation['members'][0]['role'] = ''
            else: relation['members'].append(deepcopy(relation['members'][0]))
            before = deepcopy((document, primitives))
            with self.subTest(defect=defect), self.assertRaises(api.IngestionError):
                api.osm_multipolygon_context(document, primitives, RELATION)
            self.assertEqual(before, (document, primitives))

    def test_native_normalized_identity_epoch_tags_order_and_coordinates_must_agree(self):
        for kind in ('relation', 'way', 'node'):
            for defect in ('version', 'timestamp', 'tags', 'coordinates_or_order'):
                document, primitives = fixture()
                native = next(p for p in primitives['elements'] if p['type'] == kind)
                if defect == 'version': native['version'] += 1
                elif defect == 'timestamp': native['timestamp'] = '2010-01-01T00:00:00Z'
                elif defect == 'tags': native.setdefault('tags', {})['invented'] = 'yes'
                elif kind == 'node': native['lon'] += .0000001
                elif kind == 'way': native['nodes'].reverse()
                else: native['members'][0]['role'] = 'inner'
                with self.subTest(kind=kind, defect=defect), self.assertRaises(api.IngestionError):
                    api.osm_multipolygon_context(document, primitives, RELATION)

    def test_geometry_only_full_parent_cannot_hide_omitted_or_invented_edges(self):
        for defect in ('omitted', 'hole', 'coordinate', 'split'):
            document, primitives = fixture()
            geometry = next(p['geometry'] for p in document['features'] if p['provider_feature_id'] == RELATION)
            ring = geometry['coordinates'][0][0]
            if defect == 'omitted': ring.pop(2)
            elif defect == 'hole': geometry['coordinates'][0].append(deepcopy(ring))
            elif defect == 'coordinate': ring[2][0] += .0000001
            else: geometry['coordinates'].append(deepcopy(geometry['coordinates'][0]))
            with self.subTest(defect=defect), self.assertRaises(api.IngestionError):
                api.osm_multipolygon_context(document, primitives, RELATION)

    def test_ring_start_winding_way_direction_and_member_order_do_not_change_qualification(self):
        document, primitives = fixture()
        relation = next(p for p in primitives['elements'] if p['type'] == 'relation')
        relation['members'].reverse()
        way = next(p for p in primitives['elements'] if p['type'] == 'way')
        way['nodes'].reverse()
        normalized = next(p for p in document['features'] if p['provider_feature_id'] == f"way/{way['id']}")
        normalized['properties']['@way_nodes'].reverse(); normalized['geometry']['coordinates'].reverse()
        ring = next(p for p in document['features'] if p['provider_feature_id'] == RELATION)['geometry']['coordinates'][0][0]
        changed = list(reversed(ring[:-1])); changed = changed[3:]+changed[:3]; ring[:] = [*changed, changed[0]]
        result = api.osm_multipolygon_context(document, primitives, RELATION)
        self.assertEqual((4, 15), (len(result['members']), len(result['nodes'])))

    def test_consumed_source_change_is_visible_and_unknown_metric_tags_are_retained(self):
        document, primitives = fixture(); original = api.osm_multipolygon_context(document, primitives, RELATION)
        native = next(p for p in primitives['elements'] if p['type'] == 'relation')
        normalized = next(p for p in document['features'] if p['provider_feature_id'] == RELATION)
        native['tags']['height'] = 'unknown datum'; normalized['properties']['height'] = 'unknown datum'
        changed = api.osm_multipolygon_context(document, primitives, RELATION)
        self.assertEqual('unknown datum', changed['primitive']['tags']['height'])
        self.assertNotEqual(canonical_hash(original), canonical_hash(changed))
        self.assertEqual(original['relation']['geometry'], changed['relation']['geometry'])
        self.assertEqual(64, len(api.osm_multipolygon_context_identity()))

    def test_synthetic_closed_inner_ring_and_disjoint_outer_component_preserve_member_roles(self):
        for role in ('inner', 'outer'):
            document, primitives = fixture()
            ring = [[-74.015, 40.72030], [-74.0149, 40.72030], [-74.0149, 40.72032], [-74.015, 40.72032]]
            if role == 'outer': ring = [[x+.02, y] for x, y in ring]
            ring.append(ring[0][:]); references = [90001, 90002, 90003, 90004, 90001]
            native_node = next(p for p in primitives['elements'] if p['type'] == 'node')
            normalized_node = next(p for p in document['features'] if p['provider_feature_id'].startswith('node/'))
            for identifier, point in zip(references[:-1], ring[:-1]):
                raw = deepcopy(native_node); raw.update(id=identifier, lon=point[0], lat=point[1]); raw.pop('tags', None)
                normalized = deepcopy(normalized_node); normalized.update(provider_feature_id=f'node/{identifier}', geometry={'type': 'Point', 'coordinates': point})
                normalized['properties'] = {'@type': 'node', '@id': identifier, '@version': raw['version'], '@timestamp': raw['timestamp']}
                primitives['elements'].append(raw); document['features'].append(normalized)
            raw_way = {'type': 'way', 'id': 91001, 'version': 1, 'timestamp': native_node['timestamp'], 'nodes': references}
            normalized_way = {'provider_feature_id': 'way/91001', 'provider_class': 'provider_feature',
                'geometry': {'type': 'LineString', 'coordinates': deepcopy(ring)}, 'confidence': None,
                'precision': deepcopy(normalized_node['precision']), 'properties': {'@type': 'way', '@id': 91001,
                '@version': 1, '@timestamp': raw_way['timestamp'], '@way_nodes': references[:]}}
            primitives['elements'].append(raw_way); document['features'].append(normalized_way)
            relation = next(p for p in primitives['elements'] if p['type'] == 'relation')
            relation['members'].append({'type': 'way', 'ref': 91001, 'role': role})
            geometry = next(p['geometry'] for p in document['features'] if p['provider_feature_id'] == RELATION)
            if role == 'inner': geometry['coordinates'][0].append(ring)
            else: geometry['coordinates'].append([ring])
            with self.subTest(role=role):
                result = api.osm_multipolygon_context(document, primitives, RELATION)
                self.assertEqual((5, 19), (len(result['members']), len(result['nodes'])))
                relation['members'][-1]['role'] = 'outer' if role == 'inner' else 'inner'
                with self.assertRaises(api.IngestionError): api.osm_multipolygon_context(document, primitives, RELATION)

    def test_malformed_unknown_and_excessive_context_refuses_narrowly(self):
        for defect in ('role_type', 'unknown_node_coordinate', 'nonfinite_node', 'capacity', 'version', 'unknown_id'):
            document, primitives = fixture(); identifier = RELATION
            relation = next(p for p in primitives['elements'] if p['type'] == 'relation')
            if defect == 'role_type': relation['members'][0]['role'] = []
            elif defect == 'unknown_node_coordinate': primitives['elements'][0].pop('lon')
            elif defect == 'nonfinite_node': primitives['elements'][0]['lon'] = 10**500
            elif defect == 'capacity': primitives['elements'] = [primitives['elements'][0]]*65537
            elif defect == 'version': relation['version'] = True
            else: identifier = 'relation/unknown'
            with self.subTest(defect=defect), self.assertRaises(api.IngestionError):
                api.osm_multipolygon_context(document, primitives, identifier)

    def test_optional_qualifier_identity_does_not_spread_to_way_or_raw_source_runs(self):
        before_way = api.osm_context_identity()
        before_relation = api.osm_multipolygon_context_identity()
        root = Path(__file__).resolve().parents[4]
        before_run = run_identity.run_contract(root, {'profile_id': 'fixture'})
        original = operation.file_hash
        def changed(path):
            return 'f'*64 if path.resolve() == Path(operation.__file__).resolve() else original(path)
        with patch.object(operation, 'file_hash', side_effect=changed), patch.object(osm_context, 'file_hash', side_effect=changed), \
                patch.object(run_identity, 'file_hash', side_effect=changed):
            self.assertEqual(before_way, api.osm_context_identity())
            self.assertEqual(before_run, run_identity.run_contract(root, {'profile_id': 'fixture'}))
            self.assertNotEqual(before_relation, api.osm_multipolygon_context_identity())


if __name__ == '__main__':
    unittest.main()
