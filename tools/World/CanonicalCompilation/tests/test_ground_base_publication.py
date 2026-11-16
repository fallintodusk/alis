"""Shared publisher ownership; source admission and Ground geometry have separate proofs."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError,file_hash,read_json
from World.CanonicalCompilation.refinement.api import prepare_surface_refinement
from World.CanonicalCompilation.refinement.app import ground_admission
from World.CanonicalCompilation.tests import test_road_refinement as roads


class GroundBasePublicationTests(unittest.TestCase):
    def setUp(self):
        self.fixture=roads.RoadRefinementTests();self.fixture.setUp();self.addCleanup(self.fixture.doCleanups)
        self.base,self.root=self.fixture.base,self.fixture.root
        self.input=self.root/'input.json';self.input.write_text('{}\n',encoding='ascii')
        self.descriptor={'path':self.input.relative_to(ROOT).as_posix(),'sha256':file_hash(self.input),'byte_size':self.input.stat().st_size}
        self.operation={'kind':ground_admission.KIND,'feature_ids':[],'input':self.descriptor}
        self.admitted={**self.operation,'input':{**self.descriptor,'path':f'refinement/inputs/{file_hash(self.input)}.json'},
            'base_features':{},'construction_identity':ground_admission.construction_identity()}
        self.payloads={}
        for entry in read_json(self.base/'canonical/coverage.json')['cells']:
            manifest=read_json(self.base/entry['path']);cell=read_json(self.base/manifest['terrain_artifact']['path'])
            self.payloads[cell['cell_id']]=deepcopy(cell)
            self.payloads[cell['cell_id']]['vertical_provenance']['source_ref']='qualified_ground:mechanical_publisher_control'
        self.document={'source_admission':{'sha256':'1'*64},'construction':{'evidence':{'grid_content_sha256':'2'*64}}}
        self.admit=patch.object(ground_admission,'admit',return_value=(self.admitted,
            {self.admitted['input']['path']:self.input.read_bytes()},self.document))
        self.construct=patch('World.CanonicalCompilation.refinement.app.ground_base.construct',
            return_value=(self.payloads,{}, {'construction':'mechanical_publisher_control'}))
        self.revalidate=patch.object(ground_admission,'validate_inputs')
        self.revalidate.start();self.addCleanup(self.revalidate.stop)
        self.admit.start();self.construct.start();self.addCleanup(self.admit.stop);self.addCleanup(self.construct.stop)

    def prepare(self,operations=None):
        return prepare_surface_refinement(ROOT,self.base/'compile_result.json',self.root/'candidate',operations or [self.operation])

    def test_regular_grid_replacement_uses_shared_publisher_and_deterministic_noop(self):
        before=roads.inventory(self.base)
        result,out=self.prepare();self.assertEqual('refine',result['operation'])
        self.assertEqual(before,roads.inventory(self.base))
        for entry in read_json(out/'canonical/coverage.json')['cells']:
            manifest=read_json(out/entry['path']);descriptor=manifest['terrain_artifact']
            self.assertTrue(descriptor['path'].startswith('canonical/terrain/'))
            self.assertEqual(file_hash(out/descriptor['path']),descriptor['content_hash'])
            self.assertEqual(self.payloads[entry['cell_id']],read_json(out/descriptor['path']))
            self.assertEqual((self.base/manifest['feature_artifact']['path']).read_bytes(),(out/manifest['feature_artifact']['path']).read_bytes())
        snapshot=roads.inventory(out);self.assertEqual(result,self.prepare()[0]);self.assertEqual(snapshot,roads.inventory(out))

    def test_ground_is_exclusive_and_later_construction_failure_cannot_publish(self):
        before=roads.inventory(self.base)
        with self.assertRaises(CompilerError):self.prepare([self.operation,self.operation])
        self.assertFalse((self.root/'candidate').exists())
        self.construct.stop()
        with patch('World.CanonicalCompilation.refinement.app.ground_base.construct',side_effect=CompilerError('fixture_failure','bounded refusal')):
            with self.assertRaises(CompilerError):self.prepare()
        self.assertFalse((self.root/'candidate').exists());self.assertEqual(before,roads.inventory(self.base))

    def test_source_changed_before_publication_cannot_leave_a_candidate(self):
        before=roads.inventory(self.base)
        self.revalidate.stop()
        with patch.object(ground_admission,'validate_inputs',side_effect=CompilerError('source_changed','bounded refusal')):
            with self.assertRaises(CompilerError):self.prepare()
        self.assertFalse((self.root/'candidate').exists());self.assertEqual(before,roads.inventory(self.base))


if __name__=='__main__':unittest.main()
