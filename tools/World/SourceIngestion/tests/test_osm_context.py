from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.SourceIngestion import api


def context():
    def feature(kind, identifier, geometry, properties):
        return {'provider_feature_id': f'{kind}/{identifier}', 'provider_class': 'water' if kind == 'way' else 'other',
                'geometry': geometry, 'properties': {'@type': kind, '@id': identifier, '@version': 1,
                    '@timestamp': 1700000000, **properties},
                'precision': {'source_crs': 'EPSG:4326', 'coordinate_decimals': 7}, 'confidence': None}
    return {'$schema': 'https://alis.world/schemas/world-source/provider-feature-v1.json',
            'schema_version': 1, 'provider': 'OpenStreetMap', 'release': '2026-10-17',
            'snapshot_id': 'fixture', 'shard_id': '0000', 'license': {'id': 'ODbL-1.0'},
            'area': {'area_id': 'fixture', 'area_fingerprint': 'sha256:'+'0'*64},
            'features': [feature('way', 10, {'type': 'LineString', 'coordinates': [[1.,2.],[1.1,2.1]]},
                                 {'@way_nodes': [1,2], 'waterway': 'drain'}),
                         feature('node', 1, {'type': 'Point', 'coordinates': [1.,2.]}, {}),
                         feature('node', 2, {'type': 'Point', 'coordinates': [1.1,2.1]}, {'barrier': 'sluice_gate'})]}


class OsmContextTests(unittest.TestCase):
    def test_complete_ordered_nodes_preserve_control_evidence_without_interpreting_it(self):
        document = context()
        before = deepcopy(document)
        way, nodes = api.osm_way_context(document, 'way/10')
        self.assertEqual([1,2], [node['properties']['@id'] for node in nodes])
        self.assertEqual('sluice_gate', nodes[-1]['properties']['barrier'])
        self.assertEqual('drain', way['properties']['waterway'])
        self.assertEqual(before, document)

    def test_missing_conflicting_or_unqualified_native_node_evidence_refuses(self):
        for defect in ('missing', 'duplicate', 'coordinate', 'identity', 'bool_id', 'string_nodes',
                       'version', 'timestamp', 'precision', 'provider'):
            document = context()
            way, node = document['features'][0], document['features'][-1]
            if defect == 'missing': document['features'].pop()
            elif defect == 'duplicate': document['features'].append(deepcopy(node))
            elif defect == 'coordinate': node['geometry']['coordinates'][0] += .0000001
            elif defect == 'identity': node['properties']['@id'] = 3
            elif defect == 'bool_id': node['properties']['@id'] = True
            elif defect == 'string_nodes': way['properties']['@way_nodes'][0] = '1'
            elif defect == 'version': node['properties']['@version'] = 0
            elif defect == 'timestamp': node['properties']['@timestamp'] = '2024-01-01'
            elif defect == 'precision': node['precision']['coordinate_decimals'] = 6
            elif defect == 'provider': document['provider'] = 'unknown'
            before = deepcopy(document)
            with self.subTest(defect=defect), self.assertRaises(api.IngestionError):
                api.osm_way_context(document, 'way/10')
            self.assertEqual(before, document)


if __name__ == '__main__':
    unittest.main()
