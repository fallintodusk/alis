"""Whole-domain qualified Ground replacement has one pre-Water sampling authority."""
from copy import deepcopy
import importlib
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError,canonical_hash,validate_document
from World.CanonicalCompilation.tests.ground_base_fixture import fixture


class GroundBaseTests(unittest.TestCase):
    def construct(self,document,coverage,cells,features):
        name='World.CanonicalCompilation.refinement.app.ground_base'
        self.assertIsNotNone(importlib.util.find_spec(name),'Missing pure qualified Ground-base constructor')
        parent=ROOT/'tmp/world/ground_base_tests';parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            return importlib.import_module(name).construct(ROOT,document,coverage,cells,features,Path(temporary))

    def test_captured_fountain_and_retained_flow_knots_refit_before_finalization(self):
        d,c,cells,features=fixture();before=canonical_hash([d,c,cells,features])
        output,changed,proof=self.construct(d,c,cells,features)
        self.assertEqual(set(cells),set(output));self.assertEqual(2,len(changed))
        stage=changed['alis:osm:way:394395237']['attributes']['surface_function']
        self.assertEqual(19.,stage['level_m']);self.assertEqual(1,stage['sample_count'])
        old=features['fixture:flow']['attributes']['surface_function']['knots']
        new=changed['fixture:flow']['attributes']['surface_function']['knots']
        self.assertEqual([p[:2] for p in old],[p[:2] for p in new]);self.assertNotEqual(old,new)
        self.assertTrue(all(a[2]>=b[2] for a,b in zip(new,new[1:])))
        for key,cell in output.items():
            self.assertEqual(cells[key]['bounds'],cell['bounds'])
            self.assertIsNone(cell['vertical_provenance']['source_accuracy_m'])
            self.assertEqual('qualified_ground_grid_nodes',cell['raster_alignment']['authority'])
            self.assertNotIn('resampling_scale',cell['raster_alignment'])
            self.assertNotEqual(cells[key]['water_fit'],cell['water_fit'])
            west,south,east,north=cell['bounds']
            for other in output.values():
                if other['bounds'][0]==east and other['bounds'][1]==south:
                    self.assertEqual([r[-1] for r in cell['core_samples']],[r[0] for r in other['core_samples']])
        self.assertNotIn('fixture:building',changed)
        for key,feature in changed.items():
            original=deepcopy(feature);original['attributes']['surface_function']=features[key]['attributes']['surface_function']
            self.assertEqual(features[key],original)
        for cell in output.values():validate_document(cell,Path('terrain-cell.json'))
        self.assertEqual(before,canonical_hash([d,c,cells,features]))
        again=self.construct(d,c,dict(reversed(list(cells.items()))),dict(reversed(list(features.items()))))
        self.assertEqual((output,changed,proof),again)

    def test_missing_limits_preserves_strict_finalizer_refusal_and_inputs(self):
        d,c,cells,features=fixture();d.pop('water_conditioning_limits');before=canonical_hash([d,c,cells,features])
        with self.assertRaises(CompilerError) as caught:self.construct(d,c,cells,features)
        self.assertEqual('water_conditioning_policy_required',caught.exception.code)
        self.assertEqual(before,canonical_hash([d,c,cells,features]))

    def test_grid_domain_frame_and_unknown_refuse_without_input_mutation(self):
        for defect in ['spacing','origin','nodata','missing_cell','frame','profile','bound_support','authored']:
            d,c,cells,features=fixture()
            if defect=='spacing':d['grid']['spacing'][0]=29.
            elif defect=='origin':d['grid']['origin'][0]+=30.
            elif defect=='nodata':d['grid']['samples'][0][0]=d['grid']['nodata']
            elif defect=='missing_cell':cells.pop(next(iter(cells)))
            elif defect=='frame':d['grid']['frame']['vertical']='EPSG:5703'
            elif defect=='profile':d['compiler_profile']['grid']['halo_samples']=0
            elif defect=='authored':cells[next(iter(cells))]['authored_patch_ids']=['retained_intent']
            else:features['fixture:building']['attributes']['civil_grade_reference']={'terrain_artifacts':{next(iter(cells)):'f'*64}}
            d['evidence']['grid_content_sha256']=canonical_hash(d['grid']);before=canonical_hash([d,c,cells,features])
            with self.subTest(defect=defect),self.assertRaises(CompilerError):self.construct(d,c,cells,features)
            self.assertEqual(before,canonical_hash([d,c,cells,features]))

    def test_limits_and_water_family_failures_leave_source_and_features_unchanged(self):
        for defect in ['cut','accuracy','family','basis','knots','group']:
            d,c,cells,features=fixture()
            if defect=='cut':d['water_conditioning_limits']['maximum_cut_m']=0.
            elif defect=='accuracy':d['water_conditioning_limits']['accepted_accuracy']=['unrelated']
            elif defect=='family':features['fixture:flow']['attributes']['surface_function']['function_id']='unqualified'
            elif defect=='basis':features['fixture:flow']['attributes']['surface_function']['vertical_basis']='observed_fixed_stage'
            elif defect=='knots':features['fixture:flow']['attributes']['surface_function']['knots'][0][0]=float('nan')
            else:
                other=deepcopy(features['fixture:flow']);other['feature_id']='fixture:other_flow'
                other['attributes']['surface_function']['knots'][0][2]+=.1
                features[other['feature_id']]=other
            before=deepcopy([d,c,cells,features])
            with self.subTest(defect=defect),self.assertRaises(CompilerError):self.construct(d,c,cells,features)
            if defect!='knots':self.assertEqual(before,[d,c,cells,features])


if __name__=='__main__':unittest.main()
