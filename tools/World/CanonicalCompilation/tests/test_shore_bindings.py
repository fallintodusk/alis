"""Retained-input and frozen-scope refusals over the captured bank window."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError,canonical_hash,file_hash
from World.CanonicalCompilation.refinement.app import shore_transition as shore
from World.CanonicalCompilation.tests.test_shore_transition import fixture


def document():
    data,cells,selected=fixture();water=data['water'];key=next(iter(cells))
    value={'$schema':shore.INPUT_SCHEMA,'schema_version':1,'grid_id':cells[key]['grid_id'],
        'ground_input_sha256':'a'*64,'water_feature_id':water['feature_id'],
        'base_water_sha256':canonical_hash(water),'effective_water_sha256':canonical_hash(water),'effective_water':water,
        'selected':selected,'parameters':data['parameters'],
        'provenance':{'basis':'authored_game_world_transition_not_surveyed','description':'Captured bank binding control'}}
    return value,cells,{water['feature_id']:water}


class MemoryArchive:
    def __init__(self,documents):self.documents=deepcopy(documents)
    def read(self,name):return json.dumps(self.documents[name],indent=2).encode('ascii')


class ShoreBindingTests(unittest.TestCase):
    def setUp(self):
        parent=ROOT/'tmp/world/shore_binding_tests';parent.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=parent);self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)

    def admit(self,doc,features):
        path=self.root/'shore.json';path.write_text(json.dumps(doc,indent=2)+'\n',encoding='ascii')
        op={'kind':shore.KIND,'feature_ids':[doc['water_feature_id']],
            'input':{'path':path.relative_to(ROOT).as_posix(),'sha256':file_hash(path),'byte_size':path.stat().st_size}}
        return shore.admit(ROOT,op,features)

    def test_wrong_base_snapshot_and_unknown_fields_refuse_at_admission(self):
        for defect in ('base','snapshot','unknown_field','duplicate_quad'):
            doc,cells,features=document()
            if defect=='base':doc['base_water_sha256']='f'*64
            elif defect=='snapshot':doc['effective_water']['attributes']['surface_function']['level_m']=0.
            elif defect=='unknown_field':doc['implicit_fallback']=True
            else:doc['selected'][next(iter(cells))].append([0,0])
            before=canonical_hash([doc,cells,features])
            with self.subTest(defect=defect),self.assertRaises(CompilerError):self.admit(doc,features)
            self.assertEqual(before,canonical_hash([doc,cells,features]))

    def test_stale_post_ground_water_and_ground_input_refuse_before_geometry(self):
        doc,cells,features=document();op,_,_=self.admit(doc,features)
        coverage={'grid_id':doc['grid_id'],'grid':{'canonical_crs':'EPSG:32618'}}
        for defect in ('water','ground'):
            actual=deepcopy(features);ground={'kind':'qualified_ground_base','input':{'sha256':'a'*64}}
            if defect=='water':actual[doc['water_feature_id']]['attributes']['surface_function']['level_m']=0.
            else:ground['input']['sha256']='b'*64
            before=canonical_hash([doc,cells,actual,ground])
            with self.subTest(defect=defect),self.assertRaisesRegex(CompilerError,'post-Ground Water'):
                shore.construct(ROOT,op,doc,ground,coverage,cells,actual)
            self.assertEqual(before,canonical_hash([doc,cells,actual,ground]))

    def test_frozen_wrapper_binds_staged_base_water_and_exact_quad_census(self):
        doc,cells,features=document();op,_,_=self.admit(doc,features);key=next(iter(cells))
        coverage={'grid_id':doc['grid_id'],'grid':{'canonical_crs':'EPSG:32618'}}
        ground={'kind':'qualified_ground_base','input':{'sha256':'a'*64}}
        outputs=shore.construct(ROOT,op,doc,ground,coverage,cells,features)
        regular='canonical/terrain/cell.json';wrapped='canonical/refined_terrain/cell.json';manifest='canonical/cells/cell.json';shard='canonical/features/cell.json'
        base=MemoryArchive({regular:cells[key],manifest:{'terrain_artifact':{'path':regular},'feature_artifact':{'path':shard}},
            shard:{'features':list(features.values())}})
        child=MemoryArchive({regular:cells[key],wrapped:outputs[key],op['input']['path']:doc,shard:{'features':list(features.values())}})
        raw=child.read(regular)
        import hashlib
        child.documents[wrapped]['base']={'path':regular,'sha256':hashlib.sha256(raw).hexdigest(),'byte_size':len(raw)}
        request={'operations':[ground,op],**{k:outputs[key]['consumed'][k] for k in ('implementation_sha256','execution_identity')}}
        entries={key:{'path':manifest}}
        self.assertEqual({wrapped:key},shore.declared_terrain(base,child,request,op,coverage,entries))
        for defect in ('base','extra_quad','water','ground'):
            bad=MemoryArchive(child.documents)
            if defect=='base':bad.documents[wrapped]['base']['sha256']='f'*64
            elif defect=='extra_quad':bad.documents[wrapped]['quads'].append(deepcopy(bad.documents[wrapped]['quads'][0]))
            elif defect=='water':bad.documents[shard]['features'][0]['attributes']['surface_function']['level_m']=0.
            else:bad.documents[wrapped]['consumed']['ground_input_sha256']='f'*64
            before=canonical_hash(bad.documents)
            with self.subTest(defect=defect),self.assertRaises(CompilerError):
                shore.declared_terrain(base,bad,request,op,coverage,entries)
            self.assertEqual(before,canonical_hash(bad.documents))


if __name__=='__main__':unittest.main()
