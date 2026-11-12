"""Captured relation-member object heights must not become deck elevations."""
from copy import deepcopy
from pathlib import Path
import tempfile
import unittest

from World.ExecutionEnvironment.geometry.api import triangulate_polygon
from ..app.contracts import CompilerError,read_json,file_hash
from ..app.projection import project_features
from ..refinement.constructed_import import _qualify
from ..refinement.api import prepare_surface_refinement
from .pier26_source_metric_fixture import fixture
from .structural_import_fixture import ROOT,make_fixture,_input


class StructuralSourceMetricTests(unittest.TestCase):
    def test_captured_fence_height_is_retained_as_an_object_dimension(self):
        document,native=fixture(); before=deepcopy((document,native))
        _,qualified,records,_=_qualify(document,native,'relation/20784501')
        self.assertEqual(qualified['source_metric_interpretations'],[{
            'provider_feature_id':'way/1524912807','key':'height','source_value':'1.25',
            'height_m':1.25,'meaning':'barrier_height_above_support_not_surface_elevation',
            'support_elevation_m':None,'physical_realization':'unresolved'}])
        fence=next(r for r in records if r['provider_feature_id']=='way/1524912807')
        self.assertEqual(fence['properties']['height'],'1.25')
        self.assertEqual((document,native),before)

    def test_object_height_changes_source_proof_without_changing_declared_deck(self):
        scratch=ROOT/'tmp/world/structural_source_metric_unit';scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            directory=Path(temporary);values=make_fixture(directory)
            document,native=fixture();primary='relation/20784501'
            relation=next(r for r in document['features'] if r['provider_feature_id']==primary)
            coverage=read_json(values['base'].parent/'canonical/coverage.json')
            projected,missing=project_features(ROOT,[relation],coverage['grid'],directory/'p26-projection')
            self.assertFalse(missing)
            faces=[[[*point,3.55] for point in face] for face in triangulate_polygon(ROOT,projected[primary],128)]
            model=values['model'];model['provenance']['producer_sha256']=file_hash(Path(__file__))
            item=model['models'][0];item.update(primary_source_id=primary,footprint=projected[primary],triangles=faces,
                source_bindings=[{'context_id':'parent','provider_feature_id':primary}],
                face_roles=[{'role':'fixed_pier','source_ids':[primary],'triangles':list(range(len(faces)))}])
            operation=values['operation'];operation['feature_ids']=['alis:osm:relation:20784501']
            operation['inputs']['model']=_input(directory/'model.json',model)
            def prepare(name):
                operation['inputs']['source_contexts']=[{'id':'parent',
                    'normalized':_input(directory/'p26-context.json',document),
                    'native':{**_input(directory/'p26-native.json',native),'format':'osm_json'}}]
                _,output=prepare_surface_refinement(ROOT,values['base'],directory/name,[operation])
                effective=read_json(output/'canonical/coverage.json');features=[]
                for entry in effective['cells']:
                    manifest=read_json(output/entry['path'])
                    features.extend(read_json(output/manifest['feature_artifact']['path'])['features'])
                self.assertEqual(len(features),1)
                return features[0]['attributes']['constructed_surface']
            first=prepare('first')
            fence=next(r for r in document['features'] if r['provider_feature_id']=='way/1524912807')
            member=next(r for r in native['elements'] if r['type']=='way' and r['id']==1524912807)
            fence['properties']['height']=member['tags']['height']='2.0'
            second=prepare('height-only')
            self.assertEqual(first['triangles'],faces);self.assertEqual(second['triangles'],faces)
            self.assertEqual(first['face_roles'],second['face_roles'])
            self.assertEqual(first['policy_sha256'],second['policy_sha256'])
            self.assertNotEqual(first['model_sha256'],second['model_sha256'])
            self.assertNotEqual(first['source_bindings'],second['source_bindings'])
            self.assertEqual(second['source_bindings'][0]['qualified']['source_metric_interpretations'][0]['height_m'],2.0)

    def test_unknown_or_ambiguous_metric_meaning_stays_refused(self):
        document,_=fixture()
        with self.assertRaises(CompilerError):_qualify(document,None,'way/1524912807')
        cases=[('height',value) for value in ('NaN','Infinity','1e400','-1','0','1.25 m',1.25,True)]
        cases += [('ele','1.25'),('min_height','0'),('depth','1')]
        for key,value in cases:
            with self.subTest(key=key,value=value):
                document,native=fixture()
                fence=next(r for r in document['features'] if r['provider_feature_id']=='way/1524912807')
                original=next(r for r in native['elements'] if r['type']=='way' and r['id']==1524912807)
                fence['properties'][key]=value; original['tags'][key]=value
                with self.assertRaises(CompilerError):_qualify(document,native,'relation/20784501')
        for kind in ('unknown_barrier','mixed_structure','primary_height','node_height'):
            with self.subTest(kind=kind):
                document,native=fixture()
                fence=next(r for r in document['features'] if r['provider_feature_id']=='way/1524912807')
                original=next(r for r in native['elements'] if r['type']=='way' and r['id']==1524912807)
                if kind=='unknown_barrier':fence['properties']['barrier']=original['tags']['barrier']='unknown'
                elif kind=='mixed_structure':fence['properties']['man_made']=original['tags']['man_made']='pier'
                else:
                    record=next(r for r in document['features'] if r['provider_feature_id']==('relation/20784501' if kind=='primary_height' else 'node/402428381'))
                    source=next(r for r in native['elements'] if f"{r['type']}/{r['id']}"==record['provider_feature_id'])
                    record['properties']['height']='1.25';source.setdefault('tags',{})['height']='1.25'
                with self.assertRaises(CompilerError):_qualify(document,native,'relation/20784501')


if __name__=='__main__':unittest.main()
