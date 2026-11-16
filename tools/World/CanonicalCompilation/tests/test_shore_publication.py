"""Publisher mechanics are separate from captured bank/source qualification."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError,canonical_hash,file_hash,read_json
from World.CanonicalCompilation.refinement.app.geometry import SCHEMA,_point
from World.CanonicalCompilation.refinement.app.identity import implementation_identity
from World.ExecutionEnvironment.geometry.api import execution_identity
from World.CanonicalCompilation.tests import test_ground_base_publication as ground_tests
from World.CanonicalCompilation.tests import test_road_refinement as roads


class ShorePublicationTests(unittest.TestCase):
    def setUp(self):
        self.ground=ground_tests.GroundBasePublicationTests();self.ground.setUp();self.addCleanup(self.ground.doCleanups)
        self.base,self.root=self.ground.base,self.ground.root
        key=next(iter(self.ground.payloads));cell=self.ground.payloads[key]
        cell['core_samples']=[[v+.1 for v in row] for row in cell['core_samples']]
        vertices=[_point(cell,i,j) for i,j in ((0,0),(1,0),(0,1),(1,1))]
        consumed={k:'2'*64 for k in ('shore_input_sha256','ground_input_sha256','base_water_sha256','water_sha256','domain_sha256')}
        consumed.update(water_feature_id='alis:fixture:way:1',implementation_sha256=implementation_identity(),
            execution_identity=execution_identity(ROOT,True)['identity_sha256'])
        policy={'bank_grade_x':.6,'bank_wet_offset_m':.01,'south_collar_zero_y':0.,'north_collar_zero_y':30.,
            'collar_grade':.08,'maximum_bank_gradient':.65,'maximum_triangles':4096,'maximum_restore_m':3.31}
        self.payload={key:{'$schema':SCHEMA,'schema_version':1,'grid_id':cell['grid_id'],'cell_id':key,
            'construction':'inferred_east_bank_v1','coordinate_rule':'canonical_boundary_intersections_float64',
            'consumed':consumed,'policy':policy,'quads':[{'coordinate':[0,0],'vertices':vertices,'triangles':[[0,2,1],[1,2,3]],'water_triangles':[]}],
            'metrics':{'affected_quads':1,'triangles':2,'maximum_bank_gradient':0.,'maximum_restore_m':0.,'wet_height_upper_m':0.}}}
        path=self.root/'shore-input.json';path.write_text('{"fixture":"publisher_mechanics"}',encoding='ascii')
        descriptor={'path':path.relative_to(ROOT).as_posix(),'sha256':file_hash(path),'byte_size':path.stat().st_size}
        self.operation={'kind':'inferred_shore_transition','feature_ids':['alis:fixture:way:1'],'input':descriptor}
        retained={**descriptor,'path':f'refinement/inputs/{descriptor["sha256"]}.json'}
        self.admitted={**self.operation,'input':retained,'base_features':{'alis:fixture:way:1':'2'*64}}
        self.admit=patch('World.CanonicalCompilation.refinement.app.shore_transition.admit',return_value=(self.admitted,{retained['path']:path.read_bytes()},{}))
        self.admit.start();self.addCleanup(self.admit.stop)

    def prepare(self):return self.ground.prepare([self.operation,self.ground.operation])

    def test_wrapper_uses_staged_ground_and_order_is_explicit(self):
        before=roads.inventory(self.base)
        def shore(repo,operation,document,ground,coverage,cells,features):
            self.assertEqual('qualified_ground_base',ground['kind'])
            self.assertEqual(self.ground.payloads,cells)
            return deepcopy(self.payload)
        with patch('World.CanonicalCompilation.refinement.app.shore_transition.construct',side_effect=shore):
            result,out=self.prepare()
        coverage=read_json(out/'canonical/coverage.json');key=next(iter(self.payload))
        entry=next(e for e in coverage['cells'] if e['cell_id']==key)
        manifest=read_json(out/entry['path']);wrapper=read_json(out/manifest['terrain_artifact']['path'])
        self.assertTrue(manifest['terrain_artifact']['path'].startswith('canonical/refined_terrain/'))
        self.assertEqual(file_hash(out/wrapper['base']['path']),wrapper['base']['sha256'])
        self.assertEqual(self.ground.payloads[key],read_json(out/wrapper['base']['path']))
        request=read_json(out/'refinement/request.json')
        self.assertEqual(['qualified_ground_base','inferred_shore_transition'],[op['kind'] for op in request['operations']])
        self.assertEqual(before,roads.inventory(self.base))
        for other in coverage['cells']:
            if other['cell_id']!=key:
                self.assertTrue(read_json(out/other['path'])['terrain_artifact']['path'].startswith('canonical/terrain/'))

    def test_later_shore_failure_cannot_publish_the_ground_operation(self):
        before=roads.inventory(self.base)
        with patch('World.CanonicalCompilation.refinement.app.shore_transition.construct',side_effect=CompilerError('fixture_failure','shore refuses')):
            with self.assertRaisesRegex(CompilerError,'shore refuses'):self.prepare()
        self.assertFalse((self.root/'candidate').exists());self.assertEqual(before,roads.inventory(self.base))


if __name__=='__main__':unittest.main()
