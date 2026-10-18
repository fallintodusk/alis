"""Frozen derived products use real files and isolated source archives."""
from copy import deepcopy
import hashlib
import importlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.SourceIngestion.app.contracts import IngestionError
from World.SourceIngestion.app.run_identity import run_contract

MODULE = 'World.SourceIngestion.supplemental.api'


class DerivedGroundTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'tmp/world/derived_ground_tests'
        parent.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        self.capsule = self.repo / 'tmp/input'
        self.capsule.mkdir(parents=True)
        config = self.repo / 'scripts/config/world_source_storage.local.json'
        config.parent.mkdir(parents=True)
        config.write_text(json.dumps({'$schema': 'source-storage', 'schema_version': 1,
            'primary_root': str(self.root / 'archive'), 'replica_root': str(self.root / 'replica')}))
        roles = ('raw_source', 'source_ground', 'processed_ground', 'extraction_recipe', 'transform_recipe',
                 'extraction_receipt', 'processing_receipt', 'qualification', 'runtime', 'transform_grid')
        points = {'type': 'FeatureCollection', 'features': [
            {'type': 'Feature', 'properties': {'class': 2},
             'geometry': {'type': 'Point', 'coordinates': p}} for p in [[0., 0., 2.], [1., 0., 2.], [0., 1., 1.]]]}
        self.product = {'$schema': 'https://alis.world/schemas/world-source/derived-ground-v2.json',
            'schema_version': 2, 'basis': 'observed_las_ground', 'kind': 'qualified_derived_ground', 'provider': 'fixture', 'release': 'fixture-v1',
            'scope': 'supplementary_model_candidate', 'fresh_raw_decode': False,
            'source_frame': {'horizontal': 'EPSG:6347', 'vertical': 'EPSG:5703', 'unit': 'm'},
            'target_frame': {'horizontal': 'EPSG:32618', 'vertical': 'EPSG:3855', 'unit': 'm'},
            'query': {'requested_bounds': [0., 0., 1., 1.], 'effective_bounds': [0., 0., 1., 1.],
                      'source_coordinate_decimal_places': 6,
                      'selection_rule': 'exact_rectangle', 'axis_order': 'east_north', 'classification': 2,
                      'full_resolution': True, 'point_count': 3},
            'accuracy': {'horizontal_accuracy_m': None, 'vertical_accuracy_m': None,
                         'confidence': 'qualified_processing_not_a_local_survey_bound'},
            'license': {'id': 'fixture', 'attribution': 'Synthetic fixture'}, 'artifacts': []}
        for role in roles:
            path = self.capsule / (role + '.json')
            path.write_text(json.dumps(points) if role in ('processed_ground', 'source_ground') else json.dumps({'fixture': role}))
            self.product['artifacts'].append({'id': role, 'role': role, 'path': path.name,
                'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'byte_size': path.stat().st_size})
        self.profile = self.capsule / 'product.json'
        self.output = self.repo / 'tmp/admitted'
        self.save()

    def save(self):
        self.profile.write_text(json.dumps(self.product))

    def api(self):
        self.assertIsNotNone(importlib.util.find_spec(MODULE), 'Missing SourceIngestion-owned derived-product admission')
        return importlib.import_module(MODULE)

    def snapshot(self):
        return {p.relative_to(self.output).as_posix(): p.read_bytes() for p in self.output.rglob('*') if p.is_file()}

    def raster_product(self):
        self.product.update(basis='raster_ground_model', schema_version=2,
            source_frame={'horizontal':'EPSG:6539','vertical':'EPSG:6360','unit':'us_ft'})
        self.product['$schema']='https://alis.world/schemas/world-source/derived-ground-v2.json'
        self.product['query']={'selection_rule':'qualified_raster_grid','axis_order':'east_north',
            'source_surface':'provider_bare_earth_hydroflattened','resampling':'bilinear',
            'sample_rule':'grid_nodes','canonical_bounds':[0.,0.,1.,1.],'grid_size':[2,2],
            'source_nodata':-9999.,'target_nodata':-9999.}
        grid={'$schema':'https://alis.world/schemas/world-source/ground-grid-v1.json','schema_version':1,
            'basis':'raster_ground_model','frame':self.product['target_frame'],'sample_rule':'grid_nodes',
            'origin':[0.,1.],'spacing':[1.,-1.],'size':[2,2],'samples':[[1.,2.],[3.,4.]],'nodata':-9999.}
        metadata={'coordinateSystem':{'wkt':'PROJCRS["fixture",ID["EPSG",6539]]'},
            'stac':{'proj:epsg':6539,'proj:projjson':{'type':'ProjectedCRS','id':{'authority':'EPSG','code':6539}}},
            'size':[2,2],'geoTransform':[0.,1.,0.,2.,0.,-1.],
            'metadata':{'':{'AREA_OR_POINT':'Area'}},
            'bands':[{'band':1,'type':'Float32','unit':'US survey foot','noDataValue':-9999.}]}
        for role,document in (('source_ground',metadata),('processed_ground',grid)):
            path=self.capsule/(role+'.json');path.write_text(json.dumps(document))
            artifact=next(a for a in self.product['artifacts'] if a['role']==role)
            artifact.update(sha256=hashlib.sha256(path.read_bytes()).hexdigest(),byte_size=path.stat().st_size)
        self.save()
        return grid,metadata

    def replace_artifact(self,role,document):
        artifact=next(a for a in self.product['artifacts'] if a['role']==role)
        path=self.capsule/artifact['path'];path.write_text(json.dumps(document))
        artifact.update(sha256=hashlib.sha256(path.read_bytes()).hexdigest(),byte_size=path.stat().st_size)
        self.save()

    def test_raster_model_admits_without_claiming_las_ground(self):
        grid,_=self.raster_product()
        api=self.api()
        receipt=api.admit_ground_product(self.repo,self.profile,self.output)
        digest=hashlib.sha256((self.output/'admission.json').read_bytes()).hexdigest()
        admitted=api.read_ground_product(self.output/'admission.json',digest)
        self.assertEqual(grid,admitted['grid'])
        self.assertNotIn('points',admitted)
        self.assertNotIn('classification',admitted['evidence'])
        self.assertNotIn('point_content_sha256',receipt)
        self.assertEqual('raster_ground_model',admitted['evidence']['basis'])
        self.assertEqual(receipt,api.admit_ground_product(self.repo,self.profile,self.output))

    def test_raster_model_rejects_classification_nodata_frame_and_grid_tampering(self):
        grid,metadata=self.raster_product()
        original=deepcopy(self.product)
        for defect in ('class','nodata','nan','frame','size','spacing','bounds','source_unit','source_crs',
                       'float_size','source_shape','vertical_unknown','missing_crs','nested_horizontal','crs_shape'):
            self.product=deepcopy(original)
            modified=deepcopy(grid);info=deepcopy(metadata)
            if defect=='class': self.product['query']['classification']=2
            elif defect=='nodata': modified['samples'][0][0]=-9999.
            elif defect=='nan': modified['samples'][0][0]=float('nan')
            elif defect=='frame': modified['frame']['vertical']='EPSG:5703'
            elif defect=='size': modified['samples'][0].append(5.)
            elif defect=='spacing': modified['spacing'][0]=0.
            elif defect=='bounds': self.product['query']['canonical_bounds'][2]=2.
            elif defect=='source_unit': info['bands'][0]['unit']='m'
            elif defect=='source_crs': info['stac']['proj:epsg']=32618
            elif defect=='float_size': modified['size'][0]=2.0
            elif defect=='source_shape': info['stac']=[]
            elif defect=='missing_crs': del info['stac']['proj:projjson']
            elif defect=='nested_horizontal':
                info['stac']['proj:projjson']['id']['code']=32618
                info['stac']['proj:projjson']['base_crs']={'id':{'authority':'EPSG','code':6539}}
            elif defect=='crs_shape': info['stac']['proj:projjson']={'type':'CompoundCRS','components':[]}
            else: self.product['source_frame']['vertical']='unknown'
            self.replace_artifact('processed_ground',modified)
            self.replace_artifact('source_ground',info)
            with self.subTest(defect=defect),self.assertRaises(IngestionError):
                self.api().admit_ground_product(self.repo,self.profile,self.output)
            self.assertFalse(self.output.exists())
            self.assertFalse((self.root/'archive').exists())

    def test_raster_model_cannot_enter_classified_contained_water_constructor(self):
        self.raster_product()
        api=self.api();api.admit_ground_product(self.repo,self.profile,self.output)
        digest=hashlib.sha256((self.output/'admission.json').read_bytes()).hexdigest()
        ground=api.read_ground_product(self.output/'admission.json',digest)
        from World.CanonicalCompilation.refinement.api import build_contained_water
        from World.CanonicalCompilation.app.contracts import CompilerError
        water={'feature_class':'water','geometry':{'type':'Polygon','coordinates':[[[0,0],[1,0],[0,1],[0,0]]]},
               'attributes':{'surface_geometry':'polygon','surface_behavior':'standing'}}
        from World.CanonicalCompilation.app.contracts import canonical_hash
        context={'amenity':'fountain','barrier':'kerb','geometry_sha256':canonical_hash(water['geometry'])}
        with self.assertRaises(CompilerError) as error:
            build_contained_water(ROOT,water,[[0.,0.,1.],[1.,0.,1.],[0.,1.,1.]],context,
                ground['evidence'],quantile=.5,height_step=.1,canonical_crs='EPSG:32618')
        self.assertEqual('contained_water_unsupported',error.exception.code)
        self.assertIn('Ground class',str(error.exception))

    def test_raster_vertical_crs_cannot_match_only_a_nested_datum_identifier(self):
        _,metadata=self.raster_product()
        metadata['coordinateSystem']['wkt']='COMPOUNDCRS["fixture",PROJCRS["horizontal",ID["EPSG",6539]],VERTCRS["NAVD88 height (ftUS)",VDATUM["North American Vertical Datum 1988",ID["EPSG",5103]],CS[vertical,1],AXIS["gravity-related height (H)",up],LENGTHUNIT["US survey foot",0.304800609601219],ID["EPSG",6360]]]'
        metadata['stac']['proj:projjson']={'type':'CompoundCRS','components':[
            {'type':'ProjectedCRS','id':{'authority':'EPSG','code':6539}},
            {'type':'VerticalCRS','id':{'authority':'EPSG','code':6360},'datum':{'id':{'authority':'EPSG','code':5103}}}]}
        self.product['source_frame']['vertical']='EPSG:5103'
        self.replace_artifact('source_ground',metadata)
        with self.assertRaises(IngestionError):
            self.api().admit_ground_product(self.repo,self.profile,self.output)
        self.assertFalse(self.output.exists())
        self.assertFalse((self.root/'archive').exists())

        self.product['source_frame']['vertical']='EPSG:6360'
        self.save()
        self.api().admit_ground_product(self.repo,self.profile,self.output)

    def test_raster_malformed_band_unit_is_an_owned_refusal(self):
        _,metadata=self.raster_product()
        metadata['bands'][0]['unit']=[]
        self.replace_artifact('source_ground',metadata)
        with self.assertRaises(IngestionError):
            self.api().admit_ground_product(self.repo,self.profile,self.output)
        self.assertFalse(self.output.exists())
        self.assertFalse((self.root/'archive').exists())

    def test_admission_retention_authenticated_read_and_noop(self):
        api = self.api()
        with patch('World.SourceIngestion.app.acquisition._download', side_effect=AssertionError('Offline product')):
            receipt = api.admit_ground_product(self.repo, self.profile, self.output)
            before = self.snapshot()
            self.assertEqual(receipt, api.admit_ground_product(self.repo, self.profile, self.output))
            self.assertEqual(before, self.snapshot())
            digest = hashlib.sha256((self.output / 'admission.json').read_bytes()).hexdigest()
            product = api.read_ground_product(self.output / 'admission.json', digest)
        self.assertEqual(3, len(product['points']))
        self.assertEqual('EPSG:3855', product['evidence']['vertical_datum'])
        self.assertFalse(receipt['fresh_raw_decode'])
        self.assertFalse(receipt['production_promotion'])
        for archive in ('archive', 'replica'):
            copies = list((self.root / archive / 'sha256').rglob('*'))
            self.assertEqual(len(self.product['artifacts']) + 1, sum(p.is_file() for p in copies))

    def test_changed_payload_cannot_reuse_receipt_or_mutate_accepted_output(self):
        api = self.api()
        api.admit_ground_product(self.repo, self.profile, self.output)
        before = self.snapshot()
        (self.capsule / 'processed_ground.json').write_text('{}')
        with self.assertRaises(IngestionError):
            api.admit_ground_product(self.repo, self.profile, self.output)
        self.assertEqual(before, self.snapshot())
        digest = hashlib.sha256((self.output / 'admission.json').read_bytes()).hexdigest()
        (self.output / 'artifacts/processed_ground/processed_ground.json').write_text('{}')
        with self.assertRaises(IngestionError):
            api.read_ground_product(self.output / 'admission.json', digest)

    def test_unknown_class_frame_nonfinite_missing_evidence_and_path_escape_refuse_before_writes(self):
        api = self.api()
        original = deepcopy(self.product)
        for failure in ('class', 'datum', 'nonfinite', 'missing_evidence', 'escape'):
            self.product = deepcopy(original)
            if failure == 'class':
                self.product['query']['classification'] = 1
            elif failure == 'datum':
                self.product['target_frame']['vertical'] = 'unknown'
            elif failure == 'nonfinite':
                self.product['query']['effective_bounds'][0] = float('nan')
            elif failure == 'missing_evidence':
                self.product['artifacts'] = [a for a in self.product['artifacts'] if a['role'] != 'qualification']
            else:
                self.product['artifacts'][0]['path'] = '../escape.json'
            self.save()
            with self.subTest(failure=failure), self.assertRaises(IngestionError):
                api.admit_ground_product(self.repo, self.profile, self.output)
            self.assertFalse(self.output.exists())
            self.assertFalse((self.root / 'archive').exists())

    def test_wrong_point_class_or_conflicting_xy_is_not_ground(self):
        api = self.api()
        point_path = self.capsule / 'processed_ground.json'
        original = json.loads(point_path.read_text())
        for mutation in ('class', 'xy', 'count'):
            document = deepcopy(original)
            if mutation == 'class':
                document['features'][0]['properties']['class'] = True
            elif mutation == 'xy':
                document['features'][1]['geometry']['coordinates'] = [0., 0., 9.]
            else:
                document['features'].pop()
            point_path.write_text(json.dumps(document))
            descriptor = next(a for a in self.product['artifacts'] if a['role'] == 'processed_ground')
            descriptor.update(sha256=hashlib.sha256(point_path.read_bytes()).hexdigest(), byte_size=point_path.stat().st_size)
            self.save()
            with self.subTest(mutation=mutation), self.assertRaises(IngestionError):
                api.admit_ground_product(self.repo, self.profile, self.output)
            self.assertFalse(self.output.exists())

    def test_replica_failure_does_not_publish_and_changed_claim_cannot_overwrite(self):
        api = self.api()
        (self.root / 'replica').write_text('Unavailable archive')
        with self.assertRaises(OSError):
            api.admit_ground_product(self.repo, self.profile, self.output)
        self.assertFalse(self.output.exists())
        (self.root / 'replica').unlink()
        api.admit_ground_product(self.repo, self.profile, self.output)
        before = self.snapshot()
        self.product['release'] = 'other-release'
        self.save()
        with self.assertRaises(IngestionError):
            api.admit_ground_product(self.repo, self.profile, self.output)
        self.assertEqual(before, self.snapshot())

    def test_optional_child_is_not_a_base_ingestion_input(self):
        self.api()
        from World.SourceIngestion.app.contracts import file_hash as actual_hash
        before = run_contract(ROOT, {'profile_id': 'identity_probe'})
        def changed(path, *args):
            if Path(path).is_relative_to(ROOT / 'tools/World/SourceIngestion/supplemental'):
                return 'f' * 64
            return actual_hash(path, *args)
        with patch('World.SourceIngestion.app.run_identity.file_hash', side_effect=changed):
            after = run_contract(ROOT, {'profile_id': 'identity_probe'})
        self.assertEqual(before, after)

    def test_integer_lattice_selection_preserves_declared_serialization(self):
        api = self.api()
        self.product['query'].update(selection_rule='las_integer_inclusive',
            effective_bounds=[0., 0., 1.0000006, 1.0000006])
        path = self.capsule / 'source_ground.json'
        document = json.loads(path.read_text())
        document['features'][1]['geometry']['coordinates'][0] = 1.000001
        path.write_text(json.dumps(document))
        artifact = next(a for a in self.product['artifacts'] if a['role'] == 'source_ground')
        artifact.update(sha256=hashlib.sha256(path.read_bytes()).hexdigest(), byte_size=path.stat().st_size)
        self.save()
        api.admit_ground_product(self.repo, self.profile, self.output)

    def test_descriptor_changed_after_parse_cannot_publish_old_product_with_new_request(self):
        api = self.api()
        implementation = importlib.import_module('World.SourceIngestion.supplemental.app.ground')
        verify = implementation._verified_product
        changed = False
        def replace_after_parse(product, root):
            nonlocal changed
            result = verify(product, root)
            if root == self.capsule and not changed:
                changed = True
                modified = deepcopy(self.product)
                modified['release'] = 'changed-after-parse'
                self.profile.write_text(json.dumps(modified))
            return result
        with patch.object(implementation, '_verified_product', side_effect=replace_after_parse):
            with self.assertRaises(IngestionError):
                api.admit_ground_product(self.repo, self.profile, self.output)
        self.assertTrue(changed)
        self.assertFalse(self.output.exists())

    def test_explicit_processed_crs_cannot_be_relabelled_by_descriptor(self):
        api = self.api()
        path = self.capsule / 'processed_ground.json'
        document = json.loads(path.read_text())
        document['crs'] = {'type': 'name', 'properties': {'name': 'urn:ogc:def:crs,crs:EPSG::32618,crs:EPSG::3855'}}
        path.write_text(json.dumps(document))
        artifact = next(a for a in self.product['artifacts'] if a['role'] == 'processed_ground')
        artifact.update(sha256=hashlib.sha256(path.read_bytes()).hexdigest(), byte_size=path.stat().st_size)
        self.product['target_frame']['horizontal'] = 'EPSG:4326'
        self.save()
        with self.assertRaises(IngestionError):
            api.admit_ground_product(self.repo, self.profile, self.output)
        self.assertFalse(self.output.exists())

    def test_matching_explicit_compound_crs_is_accepted(self):
        api = self.api()
        path = self.capsule / 'processed_ground.json'
        document = json.loads(path.read_text())
        document['crs'] = {'type': 'name', 'properties': {'name': 'urn:ogc:def:crs,crs:EPSG::32618,crs:EPSG::3855'}}
        path.write_text(json.dumps(document))
        artifact = next(a for a in self.product['artifacts'] if a['role'] == 'processed_ground')
        artifact.update(sha256=hashlib.sha256(path.read_bytes()).hexdigest(), byte_size=path.stat().st_size)
        self.save()
        api.admit_ground_product(self.repo, self.profile, self.output)

    def test_producer_change_during_retention_prevents_promotion(self):
        api = self.api()
        implementation = importlib.import_module('World.SourceIngestion.supplemental.app.ground')
        retain = implementation.retain_sources
        identity = implementation.implementation_identity()
        changed = False
        def interrupted(*args):
            nonlocal changed
            result = retain(*args)
            changed = True
            return result
        with patch.object(implementation, 'retain_sources', side_effect=interrupted), \
                patch.object(implementation, 'implementation_identity', side_effect=lambda: 'f'*64 if changed else identity):
            with self.assertRaises(IngestionError):
                api.admit_ground_product(self.repo, self.profile, self.output)
        self.assertTrue(changed)
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()
