"""One immutable base and one publication for independent feature corrections."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError, file_hash, read_json
from World.CanonicalCompilation.refinement.api import prepare_surface_refinement
from World.CanonicalCompilation.refinement.app.contracts import write_json
from World.CanonicalCompilation.tests import test_road_refinement as roads
from World.CanonicalCompilation.tests.test_water_confluence import fixture as water_fixture


class SurfaceRefinementTests(unittest.TestCase):
    def setUp(self):
        self.fixture=roads.RoadRefinementTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root,self.base=self.fixture.root,self.fixture.base
        self.water,self.context,self.projected=water_fixture()
        self.context['features']+=deepcopy(self.fixture.source['features'])
        owner=self.fixture.road['owner_cell_id']
        for feature in self.water:
            feature.update(owner_cell_id=owner,intersecting_cell_ids=[owner],representations=[],provenance_result='accepted')
            feature['attributes']['ribbon_buffer']={'cap_style':'round','join_style':'round','quadrant_segments':8,'suppression_clearance_m':.02}
        path=self.fixture.feature_path
        shard=read_json(path)
        shard['features']+=deepcopy(self.water)
        write_json(path,shard)
        manifest_path=self.base/'canonical/cells/cell_0.json'
        manifest=read_json(manifest_path)
        manifest['owned_feature_ids']=[f['feature_id'] for f in shard['features']]
        manifest['feature_artifact'].update(content_hash=file_hash(path),byte_size=path.stat().st_size)
        write_json(manifest_path,manifest)
        coverage_path=self.base/'canonical/coverage.json'
        coverage=read_json(coverage_path)
        coverage['feature_count']=3
        coverage['cells'][0].update(sha256=file_hash(manifest_path),byte_size=manifest_path.stat().st_size)
        for descriptor in coverage['artifact_descriptors']:
            if descriptor['artifact_id']==manifest['feature_artifact']['artifact_id']:
                descriptor.update({k:v for k,v in manifest['feature_artifact'].items() if k!='path'})
        write_json(coverage_path,coverage)
        self.fixture.reauthenticate_base()
        self.context_path=self.root/'combined-context.json'
        self.fixture.source=self.context
        self.fixture.context=self.context_path
        self.fixture.save_context()
        self.ops=[self.operation('bridge_elevation',[self.fixture.road['feature_id']]),
                  self.operation('water_confluence',[f['feature_id'] for f in self.water])]
        projection={**self.projected,'way/1':self.fixture.road['geometry']}
        self.projection=patch('World.CanonicalCompilation.refinement.app.surface_publication.project_features',
            side_effect=lambda repo,features,grid,scratch:({f['provider_feature_id']:deepcopy(projection[f['provider_feature_id']]) for f in features},[]))
        self.forward=patch('World.CanonicalCompilation.refinement.app.surface_publication.production_geometry_evidence',
            return_value=(None,None,{f['feature_id']:f['geometry'] for f in self.water},None,None))
        self.projection.start();self.forward.start()
        self.addCleanup(self.projection.stop);self.addCleanup(self.forward.stop)

    def operation(self,kind,identifiers,path=None):
        path=path or self.context_path
        return {'kind':kind,'feature_ids':identifiers,'context_path':path.relative_to(ROOT).as_posix(),
                'sha256':file_hash(path),'byte_size':path.stat().st_size}

    def prepare(self,operations=None,name='candidate'):
        return prepare_surface_refinement(ROOT,self.base/'compile_result.json',self.root/name,operations or self.ops)

    def test_same_shard_operations_survive_together_and_ordered_noop(self):
        before=roads.inventory(self.base)
        result,out=self.prepare()
        self.assertEqual('refine',result['operation'])
        self.assertEqual(before,roads.inventory(self.base))
        features={f['feature_id']:f for f in read_json(out/'canonical/features/cell_0.json')['features']}
        self.assertIn('road_elevation_profile',features[self.fixture.road['feature_id']]['attributes'])
        self.assertEqual('constrained_flowing_knots',features[self.water[0]['feature_id']]['attributes']['surface_function']['function_id'])
        self.assertEqual(self.water[1],features[self.water[1]['feature_id']])
        self.assertEqual(file_hash(self.base/'canonical/terrain/cell_0.json'),file_hash(out/'canonical/terrain/cell_0.json'))
        manifest=read_json(out/'canonical/cells/cell_0.json')
        self.assertEqual(1,len([d for d in manifest['feature_artifact']['dependencies'] if d.startswith('surface-refinement:')]))
        inventory=roads.inventory(out)
        self.assertEqual(result,self.prepare(list(reversed(self.ops)))[0])
        self.assertEqual(inventory,roads.inventory(out))

    def test_interacting_or_unknown_operation_refuses_before_publication(self):
        before=roads.inventory(self.base)
        for kind,ops in (('duplicate',[*self.ops,self.ops[0]]),
                         ('unknown',[{**self.ops[0],'kind':'generic_rule_engine'}]),
                         ('wrong_count',[{**self.ops[1],'feature_ids':[self.water[0]['feature_id']]}])):
            with self.subTest(kind=kind),self.assertRaises(CompilerError):self.prepare(ops)
            self.assertFalse((self.root/'candidate').exists())
        self.assertEqual(before,roads.inventory(self.base))

    def test_second_operation_forward_geometry_failure_does_not_publish_first(self):
        before=roads.inventory(self.base)
        with patch('World.CanonicalCompilation.refinement.app.surface_publication.production_geometry_evidence',
                   return_value=(None,None,{},None,None)):
            with self.assertRaises(CompilerError):self.prepare()
        self.assertFalse((self.root/'candidate').exists())
        self.assertEqual(before,roads.inventory(self.base))

    def test_distinct_qualified_contexts_have_distinct_evidence_paths(self):
        bridge=self.root/'bridge-context.json'
        self.fixture.source={**self.context,'features':[f for f in self.context['features']
            if f['provider_feature_id'] in ('way/1','node/10','node/20')]}
        self.fixture.context=bridge
        self.fixture.save_context()
        ops=[self.operation('bridge_elevation',[self.fixture.road['feature_id']],bridge),self.ops[1]]
        _,out=self.prepare(ops)
        request=read_json(out/'refinement/request.json')
        paths=[op['context']['path'] for op in request['operations']]
        self.assertEqual(2,len(set(paths)))
        for op in request['operations']:
            self.assertEqual(op['context']['sha256'],file_hash(out/op['context']['path']))


if __name__=='__main__':
    unittest.main()
