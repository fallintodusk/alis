"""Authenticated disposable Road refinement from a separately pinned context."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT/'tools'))
from World.CanonicalCompilation.app.artifacts import output_descriptor
from World.CanonicalCompilation.app.contracts import CompilerError, file_hash, read_json
from World.CanonicalCompilation.refinement.app.contracts import write_json
from World.CanonicalCompilation.refinement.api import prepare_bridge_refinement
from World.CanonicalCompilation.tests.refinement_fixture import make_base


def inventory(root):
    return {p.relative_to(root).as_posix(): file_hash(p) for p in root.rglob('*') if p.is_file()}


class RoadRefinementTests(unittest.TestCase):
    def setUp(self):
        scratch=ROOT/'tmp/world/road_refinement_unit'
        scratch.mkdir(parents=True, exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.base=self.root/'base'
        make_base(self.base, cross_cell=False)
        self.road={'feature_id':'alis:fixture:way:1','feature_class':'road',
                   'geometry':{'type':'LineString','coordinates':[[5.,15.],[25.,15.]]},
                   'attributes':{'road_class':'primary','width_m':4.},
                   'source_refs':[{'provider':'OpenStreetMap','provider_feature_id':'way/1','release':'2026-08-02'}],
                   'owner_cell_id':'grid_'+'0'*16+':x0:y0', 'intersecting_cell_ids':[],
                   'representations':[], 'provenance_result':'accepted'}
        coverage=read_json(self.base/'canonical/coverage.json')
        self.road['owner_cell_id']=coverage['cells'][0]['cell_id']
        self.road['intersecting_cell_ids']=[self.road['owner_cell_id']]
        self.feature_path=self.base/'canonical/features/cell_0.json'
        features=read_json(self.feature_path)
        features['features']=[self.road]
        write_json(self.feature_path,features)
        manifest_path=self.base/'canonical/cells/cell_0.json'
        manifest=read_json(manifest_path)
        manifest['feature_artifact'].update(content_hash=file_hash(self.feature_path),byte_size=self.feature_path.stat().st_size)
        write_json(manifest_path,manifest)
        coverage['cells'][0].update(sha256=file_hash(manifest_path),byte_size=manifest_path.stat().st_size)
        for entry in coverage['artifact_descriptors']:
            if entry['artifact_id']==manifest['feature_artifact']['artifact_id']:
                entry.update({k:v for k,v in manifest['feature_artifact'].items() if k!='path'})
        coverage['grid']['fixture_scale_m']=1.
        write_json(self.base/'canonical/coverage.json',coverage)
        self.reauthenticate_base()
        self.context=self.root/'context.json'
        self.source={'$schema':'https://alis.world/schemas/world-source/provider-feature-v1.json','schema_version':1,
                     'snapshot_id':'supplementary-current-2026-10-17','provider':'OpenStreetMap','release':'2026-10-17',
                     'area':{'area_id':'fixture','area_fingerprint':'sha256:'+'0'*64},'shard_id':'0000',
                     'license':{'id':'ODbL-1.0','attribution':'OpenStreetMap contributors'},
                     'features':[{'provider_feature_id':'way/1','provider_class':'highway',
                         'geometry':deepcopy(self.road['geometry']),
                         'properties':{'@id':1,'@type':'way','@version':2,'@timestamp':'2024-01-01T00:00:00Z',
                                       '@way_nodes':[10,20],'bridge':'yes','highway':'primary','layer':'1'},
                         'precision':{'source_crs':'EPSG:4326','coordinate_decimals':7},'confidence':None}]}
        for identifier,xy in zip((10,20),self.road['geometry']['coordinates']):
            self.source['features'].append({'provider_feature_id':f'node/{identifier}','provider_class':'provider_feature',
                'geometry':{'type':'Point','coordinates':deepcopy(xy)},
                'properties':{'@id':identifier,'@type':'node','@version':1,'@timestamp':'2024-01-01T00:00:00Z'},
                'precision':{'source_crs':'EPSG:4326','coordinate_decimals':7},'confidence':None})
        self.save_context()

    def save_context(self):
        self.context.write_text(json.dumps(self.source,indent=2)+'\n',encoding='ascii')

    def reauthenticate_base(self):
        result_path=self.base/'compile_result.json'
        result=read_json(result_path)
        result['outputs']=[output_descriptor('compiler_document',p,self.base) for p in sorted(self.base.rglob('*.json'))
                           if p!=result_path]
        write_json(result_path,result)

    def prepare(self,name='candidate',digest=None):
        return prepare_bridge_refinement(ROOT,self.base/'compile_result.json',self.root/name,
            self.road['feature_id'],self.context,digest or file_hash(self.context),self.context.stat().st_size)

    def test_publication_retains_base_terrain_and_historical_lineage_and_noops(self):
        before=inventory(self.base)
        result,output=self.prepare()
        self.assertEqual('refine',result['operation'])
        self.assertEqual(before,inventory(self.base))
        original=read_json(self.base/'canonical/cells/cell_0.json')
        current=read_json(output/'canonical/cells/cell_0.json')
        self.assertEqual(original['terrain_artifact'],current['terrain_artifact'])
        self.assertEqual(original['feature_lineage'],current['feature_lineage'])
        feature=read_json(output/'canonical/features/cell_0.json')['features'][0]
        self.assertEqual(self.road['source_refs'],feature['source_refs'])
        self.assertEqual(self.road['geometry'],feature['geometry'])
        self.assertEqual(file_hash(self.context),file_hash(output/'refinement/context.json'))
        self.assertIn('road_elevation_profile',feature['attributes'])
        for path,digest in before.items():
            if path.startswith('canonical/terrain/'):
                self.assertEqual(digest,file_hash(output/path))
        saved=inventory(output)
        self.assertEqual(result,self.prepare()[0])
        self.assertEqual(saved,inventory(output))

    def test_stale_pin_geometry_later_version_and_missing_topology_refuse_without_output(self):
        before=inventory(self.base)
        with self.assertRaises(CompilerError):
            self.prepare(digest='0'*64)
        for kind in ('reversed','later','nodes','identity','crossing'):
            source=deepcopy(self.source)
            feature=source['features'][0]
            if kind=='reversed': feature['geometry']['coordinates'].reverse()
            elif kind=='later': feature['properties']['@timestamp']='2026-08-03T00:00:00Z'
            elif kind=='nodes': feature['properties'].pop('@way_nodes')
            elif kind=='identity': feature['properties']['@id']=2
            else: feature['properties']['tunnel']='maybe'
            self.context.write_text(json.dumps(source),encoding='ascii')
            with self.subTest(kind=kind),self.assertRaises(CompilerError): self.prepare()
            self.assertFalse((self.root/'candidate').exists())
        self.assertEqual(before,inventory(self.base))

    def test_promotion_failure_preserves_existing_and_protected_inputs(self):
        before=inventory(self.base)
        with patch('World.CanonicalCompilation.refinement.app.surface_publication.promote_staging',
                   side_effect=CompilerError('injected','fixture')):
            with self.assertRaises(CompilerError): self.prepare()
        self.assertEqual(before,inventory(self.base))
        self.assertFalse((self.root/'candidate').exists())
        (self.root/'candidate').mkdir()
        marker=self.root/'candidate/operator.txt'
        marker.write_text('retain',encoding='ascii')
        with self.assertRaises(CompilerError): self.prepare()
        self.assertEqual({'operator.txt':file_hash(marker)},inventory(self.root/'candidate'))

    def test_unqualified_provider_cannot_reuse_osm_way_identity(self):
        self.source['provider']='DifferentProvider'
        self.save_context()
        with self.assertRaises(CompilerError): self.prepare()
        self.assertFalse((self.root/'candidate').exists())

    def test_distribution_channels_share_namespace_without_relabeling_evidence(self):
        manifest_path=self.base/'canonical/cells/cell_0.json'
        features=read_json(self.feature_path)
        features['features'][0]['source_refs'][0]['provider']='OpenStreetMap via Geofabrik'
        write_json(self.feature_path,features)
        manifest=read_json(manifest_path)
        manifest['feature_artifact'].update(content_hash=file_hash(self.feature_path),byte_size=self.feature_path.stat().st_size)
        write_json(manifest_path,manifest)
        coverage=read_json(self.base/'canonical/coverage.json')
        coverage['cells'][0].update(sha256=file_hash(manifest_path),byte_size=manifest_path.stat().st_size)
        for entry in coverage['artifact_descriptors']:
            if entry['artifact_id']==manifest['feature_artifact']['artifact_id']:
                entry.update({k:v for k,v in manifest['feature_artifact'].items() if k!='path'})
        write_json(self.base/'canonical/coverage.json',coverage)
        self.reauthenticate_base()
        _,output=self.prepare()
        current=read_json(output/'canonical/features/cell_0.json')['features'][0]
        self.assertEqual('OpenStreetMap via Geofabrik',current['source_refs'][0]['provider'])
        self.assertEqual('OpenStreetMap',read_json(output/'refinement/context.json')['provider'])

    def test_consumed_source_contract_change_invalidates_noop(self):
        self.prepare()
        before=inventory(self.root/'candidate')
        with patch('World.CanonicalCompilation.refinement.app.surface_publication.osm_context_identity',return_value='f'*64):
            with self.assertRaises(CompilerError): self.prepare()
        self.assertEqual(before,inventory(self.root/'candidate'))

    def test_native_osmium_unix_timestamp_is_qualified_without_coercion(self):
        self.source['features'][0]['properties']['@timestamp']=1704067200
        self.save_context()
        self.prepare()
        for bad in (True,1704067200.0,'1704067200',None,10**100):
            self.source['features'][0]['properties']['@timestamp']=bad
            self.save_context()
            with self.subTest(value=bad),self.assertRaises(CompilerError): self.prepare('bad')
            self.assertFalse((self.root/'bad').exists())

    def test_changed_base_receipt_between_authentication_and_publication_refuses(self):
        from World.CanonicalCompilation.refinement.app import surface_publication
        original=surface_publication.accepted_base
        def race(path):
            root,result=original(path)
            changed=deepcopy(result)
            changed['profile_id']='changed_after_authentication'
            write_json(path,changed)
            return root,result
        with patch.object(surface_publication,'accepted_base',side_effect=race):
            with self.assertRaises(CompilerError): self.prepare()
        self.assertFalse((self.root/'candidate').exists())

    def test_complete_source_node_context_is_required_before_publication(self):
        self.source['features'].pop()
        self.save_context()
        before=inventory(self.base)
        with self.assertRaises(CompilerError): self.prepare()
        self.assertEqual(before,inventory(self.base))
        self.assertFalse((self.root/'candidate').exists())

    def test_explicit_source_elevation_is_not_silently_ignored(self):
        self.source['features'][1]['properties']['ele']='54.3'
        self.save_context()
        with self.assertRaises(CompilerError): self.prepare()
        self.assertFalse((self.root/'candidate').exists())

    def test_base_artifact_descriptor_must_match_authenticated_payload(self):
        path=self.base/'canonical/cells/cell_0.json'
        manifest=read_json(path)
        manifest['terrain_artifact']['content_hash']='a'*64
        write_json(path,manifest)
        coverage_path=self.base/'canonical/coverage.json'
        coverage=read_json(coverage_path)
        coverage['cells'][0].update(sha256=file_hash(path),byte_size=path.stat().st_size)
        write_json(coverage_path,coverage)
        self.reauthenticate_base()
        before=inventory(self.base)
        with self.assertRaises(CompilerError): self.prepare()
        self.assertEqual(before,inventory(self.base))
        self.assertFalse((self.root/'candidate').exists())

    def test_misclassified_provider_way_refuses_without_publication(self):
        self.source['features'][0]['provider_class']='water'
        self.save_context()
        before=inventory(self.base)
        with self.assertRaises(CompilerError): self.prepare()
        self.assertEqual(before,inventory(self.base))
        self.assertFalse((self.root/'candidate').exists())


if __name__=='__main__':
    unittest.main()
