from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT/'tools'))
from World.CanonicalCompilation.refinement import api
from World.CanonicalCompilation.app.contracts import CompilerError, canonical_hash


def fixture():
    projected = {'way/10': {'type':'LineString','coordinates':[[0,5],[5,0]]},
                 'way/20': {'type':'LineString','coordinates':[[0,10],[5,0],[10,-10]]}}
    locations = {1:[1.,2.],2:[1.1,2.1],3:[1.,2.2],4:[1.2,2.]}
    def record(kind, identifier, geometry, properties):
        return {'provider_feature_id':f'{kind}/{identifier}','provider_class':'water' if kind=='way' else 'provider_feature',
                'geometry':geometry,'properties':{'@type':kind,'@id':identifier,'@version':1,'@timestamp':1700000000,**properties},
                'precision':{'source_crs':'EPSG:4326','coordinate_decimals':7},'confidence':None}
    records = [record('node', key, {'type':'Point','coordinates':xy}, {}) for key,xy in locations.items()]
    for key,nodes in ((10,[1,2]),(20,[3,2,4])):
        records.append(record('way',key,{'type':'LineString','coordinates':[locations[n] for n in nodes]},
                              {'@way_nodes':nodes,'waterway':'drain'}))
    context = {'$schema':'https://alis.world/schemas/world-source/provider-feature-v1.json','schema_version':1,
               'provider':'OpenStreetMap','release':'2026-10-17','snapshot_id':'fixture','shard_id':'0000',
               'license':{'id':'ODbL-1.0'},'area':{'area_id':'fixture','area_fingerprint':'sha256:'+'0'*64},'features':records}
    features = []
    for key,z in ((10,[54,53.7]),(20,[53.8,53.5,53.2])):
        provider = f'way/{key}'
        features.append({'feature_id':f'alis:osm:way:{key}','feature_class':'water','geometry':deepcopy(projected[provider]),
            'attributes':{'water_class':'drain','surface_group_members':[provider],
                'surface_geometry':'ribbon','surface_behavior':'flowing','surface_role':'flow_axis','width_m':1.,
                'surface_function':{'function_id':'rolling_quantile_l1_isotonic','function_version':1,
                    'vertical_basis':'canonical_terrain_robust_fit','accuracy':'derived_not_surveyed','sample_count':len(z),
                    'knots':[[*xy,height] for xy,height in zip(projected[provider]['coordinates'],z)],
                    'fit_diagnostics':{'max_correction_m':.8}}},
            'source_refs':[{'provider':'OpenStreetMap via Geofabrik','provider_feature_id':provider,
                            'release':'2026-08-02','snapshot_id':'accepted'}]})
    return features,context,projected


class WaterConfluenceTests(unittest.TestCase):
    def test_shared_source_node_constrains_only_tributary_terminal(self):
        features,context,projected = fixture()
        before = deepcopy((features,context,projected))
        changed = api.build_confluence_constraint(features,context,projected)
        self.assertEqual({'alis:osm:way:10'}, set(changed))
        result = changed['alis:osm:way:10']
        old = features[0]['attributes']['surface_function']
        function = result['attributes']['surface_function']
        self.assertEqual('constrained_flowing_knots',function['function_id'])
        self.assertEqual('derived_not_surveyed',function['accuracy'])
        self.assertEqual(old,function['baseline_function'])
        self.assertEqual(old['knots'][:-1],function['knots'][:-1])
        self.assertEqual([5,0,53.5],function['knots'][-1])
        self.assertNotIn('fit_diagnostics',function)
        self.assertEqual('node/2',function['constraint']['provider_node_id'])
        self.assertAlmostEqual(.2,function['constraint']['lowering_m'])
        self.assertEqual(canonical_hash(features[1]['attributes']['surface_function']),function['constraint']['receiver_function_sha256'])
        self.assertEqual(before,(features,context,projected))
        self.assertEqual(changed,api.build_confluence_constraint(list(reversed(features)),context,projected))

    def test_source_control_or_unknown_topology_refuses_without_mutation(self):
        for defect in ('weir','gate','sluice','unknown_waterway','provider_class','ele','layer','underground','source_order','missing_node','future_node','no_shared_node','source_xy','receiver_missing_knot','low_confidence','rising','raising'):
            features,context,projected = fixture()
            node = next(f for f in context['features'] if f['provider_feature_id']=='node/2')
            way = next(f for f in context['features'] if f['provider_feature_id']=='way/10')
            if defect=='weir': node['properties']['waterway']='weir'
            elif defect=='gate': node['properties']['barrier']='sluice_gate'
            elif defect=='sluice': node['properties']['waterway']='sluice_gate'
            elif defect=='unknown_waterway': node['properties']['waterway']='unsupported_control'
            elif defect=='provider_class': way['provider_class']='highway'
            elif defect=='ele': node['properties']['ele']='53.5'
            elif defect=='layer': way['properties']['layer']='-1'
            elif defect=='underground': way['properties']['location']='underground'
            elif defect=='source_order':
                for identifier,xy in ((5,[1.02,2.02]),(6,[1.04,2.04])):
                    extra=deepcopy(node)
                    extra['provider_feature_id']=f'node/{identifier}'
                    extra['properties']['@id']=identifier
                    extra['geometry']['coordinates']=xy
                    context['features'].append(extra)
                way['properties']['@way_nodes']=[1,5,6,2]
                way['geometry']['coordinates']=[[1.,2.],[1.02,2.02],[1.04,2.04],[1.1,2.1]]
                projected['way/10']['coordinates']=[[0,5],[2,3],[3,2],[5,0]]
                features[0]['attributes']['surface_function']['knots']=[[0,5,54],[3,2,53.9],[2,3,53.8],[5,0,53.7]]
            elif defect=='missing_node': context['features'].remove(node)
            elif defect=='future_node': node['properties']['@timestamp']=1900000000
            elif defect=='no_shared_node': way['properties']['@way_nodes'][-1]=4;way['geometry']['coordinates'][-1]=[1.2,2.]
            elif defect=='source_xy': projected['way/10']['coordinates'][-1]=[5.01,0]
            elif defect=='receiver_missing_knot': features[1]['attributes']['surface_function']['knots'][1][:2]=[5.01,0]
            elif defect=='low_confidence': features[0]['attributes']['surface_function']['accuracy']='low_confidence_point_fallback'
            elif defect=='rising': features[0]['attributes']['surface_function']['knots'][0][2]=53.6
            elif defect=='raising': features[1]['attributes']['surface_function']['knots'][1][2]=53.75
            before = deepcopy((features,context,projected))
            with self.subTest(defect=defect),self.assertRaises(CompilerError):
                api.build_confluence_constraint(features,context,projected)
            self.assertEqual(before,(features,context,projected))

    def test_equal_terminal_is_an_actual_no_change(self):
        features,context,projected = fixture()
        features[0]['attributes']['surface_function']['knots'][-1][2]=53.5
        self.assertEqual({},api.build_confluence_constraint(features,context,projected))


if __name__=='__main__':
    unittest.main()
