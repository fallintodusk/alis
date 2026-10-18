from pathlib import Path
from copy import deepcopy
import json
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.SourceIngestion.app.osm_selection import select_complete_geometries
from World.SourceIngestion.app.adapters import _convert_osmium_geojson
from World.SourceIngestion.app.contracts import IngestionError, file_hash


class NativeOsmTopologyTests(unittest.TestCase):
    def test_closed_water_retains_nodes_across_native_line_area_pair(self):
        scratch = ROOT / 'tmp/world/source_ingestion/topology_tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            root = Path(directory)
            source = root / 'fountain.osm'
            source.write_text('''<osm version="0.6">
              <node id="1" lon="0" lat="0"/><node id="2" lon="0.1" lat="0"/>
              <node id="3" lon="0" lat="0.1"/>
              <way id="100"><nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="1"/>
                <tag k="natural" v="water"/><tag k="amenity" v="fountain"/></way></osm>''', encoding='ascii')
            before = file_hash(source)
            _, path, _ = select_complete_geometries(ROOT, source, ['w/natural=water'], [-.1, -.1, .2, .2], root / 'selected')
            result = _convert_osmium_geojson(path, {'snapshot_id': 'fixture'},
                {'provider': 'fixture', 'release': '1', 'license': {'id': 'CC0-1.0'}}, {})
            self.assertEqual(before, file_hash(source))
            self.assertEqual(1, len(result['features']))
            feature = result['features'][0]
            self.assertEqual('MultiPolygon', feature['geometry']['type'])
            self.assertEqual([1, 2, 3, 1], feature['properties']['@way_nodes'])
            raw = json.loads(path.read_text())
            reversed_raw = deepcopy(raw)
            reversed_raw['features'].reverse()
            path.write_text(json.dumps(reversed_raw), encoding='ascii')
            self.assertEqual(result, _convert_osmium_geojson(path, {'snapshot_id': 'fixture'},
                {'provider': 'fixture', 'release': '1', 'license': {'id': 'CC0-1.0'}}, {}))
            for defect in ('tag', 'nodes', 'area_only'):
                bad = deepcopy(raw)
                area = next(f for f in bad['features'] if f['geometry']['type'] == 'MultiPolygon')
                if defect == 'area_only':
                    line = next(f for f in bad['features'] if f['geometry']['type'] == 'LineString')
                    area['properties']['@way_nodes'] = line['properties'].pop('@way_nodes')
                else:
                    area['properties'].update({'amenity': 'different'} if defect == 'tag' else {'@way_nodes': [3, 2, 1, 3]})
                path.write_text(json.dumps(bad), encoding='ascii')
                before_bad = file_hash(path)
                with self.subTest(defect=defect):
                    with self.assertRaises(IngestionError) as error:
                        _convert_osmium_geojson(path, {'snapshot_id': 'fixture'},
                            {'provider': 'fixture', 'release': '1', 'license': {'id': 'CC0-1.0'}}, {})
                    self.assertEqual('duplicate_feature_conflict', error.exception.code)
                    self.assertEqual(before_bad, file_hash(path))

    def test_selected_export_and_ogr_preserve_ordered_native_way_nodes(self):
        fixture = ROOT / 'Plugins/World/ProjectWorldTestData/Data/Fixtures/Provider/synthetic_multipolygon/crossing_building.osm'
        scratch = ROOT / 'tmp/world/source_ingestion/topology_tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            _, path, _ = select_complete_geometries(ROOT, fixture, ['w/highway'], [-.1, -.1, .1, .1], Path(directory))
            result = _convert_osmium_geojson(path, {'snapshot_id': 'fixture'},
                {'provider': 'fixture', 'release': '1', 'license': {'id': 'CC0-1.0'}}, {})
            roads = {item['provider_feature_id']: item for item in result['features'] if item['provider_class'] == 'highway'}
            self.assertEqual([10, 11, 12], roads['way/200']['properties'].get('@way_nodes'),
                             'Native way-node identity was dropped before normalized source admission')
            self.assertEqual([13, 14], roads['way/201']['properties'].get('@way_nodes'))
            self.assertTrue(all(isinstance(value, int) for value in roads['way/200']['properties']['@way_nodes']))


if __name__ == '__main__':
    unittest.main()
