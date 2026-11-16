"""Captured conditioned Ground must admit a finite bank without losing coverage."""
from copy import deepcopy
from fractions import Fraction as F
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError, canonical_hash
from World.CanonicalCompilation.refinement.app.geometry import _point
from World.CanonicalCompilation.refinement.app.shore_geometry import build, _encoded_metrics
from World.ExecutionEnvironment.geometry.affine import measure_affine_cover
from World.CanonicalCompilation.tests.affine_oracle import height,cross

def fixture():
    data=json.loads(Path(__file__).with_name('fixtures').joinpath('pier_shore_transition.json').read_text())
    cell=data['cell']; key=cell['cell_id']
    return data,{key:cell},{key:[[x,y] for y in range(7) for x in range(5)]}


def triangles(payloads):
    return [[q['vertices'][i][:3] for i in face] for p in payloads.values() for q in p['quads'] for face in q['triangles']]


class ShoreTransitionTests(unittest.TestCase):
    def test_captured_bank_has_complete_encoded_cover_and_preserves_inputs(self):
        data,cells,selected=fixture();before=canonical_hash([data,cells,selected])
        output,metrics=build(ROOT,cells,data['water'],selected,data['parameters'])
        faces=triangles(output);original=[]
        for cell in cells.values():
            for x,y in selected[cell['cell_id']]:
                a,b,c,d=[_point(cell,i,j)[:3] for i,j in ((x,y),(x+1,y),(x,y+1),(x+1,y+1))]
                original.extend([[a,c,b],[b,c,d]])
        measure_affine_cover(faces,original);measure_affine_cover(original,faces)
        self.assertLess(metrics['maximum_bank_gradient'],.608)
        self.assertEqual(-1.,metrics['wet_height_upper_m'])
        self.assertAlmostEqual(3.3,metrics['maximum_restore_m'])
        self.assertEqual(35,metrics['affected_quads'])
        self.assertEqual(before,canonical_hash([data,cells,selected]))
        # The dry southern landing is restored from its eroded -1 m field.
        xy=[583375.3780447837,4508117.584089732];values=[]
        for face in faces:
            f=[[F(v) for v in p] for p in face];p=list(map(F,xy))
            if all(cross(a,b,p)>=0 for a,b in zip(f,(*f[1:],f[0]))):values.append(float(height(f,p)))
        self.assertTrue(values);self.assertAlmostEqual(.9351963194619318,values[0])
        self.assertEqual((output,metrics),build(ROOT,cells,data['water'],selected,data['parameters']))

    def test_quality_unknown_source_and_missing_neighbor_refuse_without_mutation(self):
        for defect in ('grade','capacity','restore','neighbor','surface_source','raised_final','unknown','overflow'):
            data,cells,selected=fixture();cell=next(iter(cells.values()))
            if defect=='grade':data['parameters']['maximum_bank_gradient']=.1
            elif defect=='capacity':data['parameters']['maximum_triangles']=2
            elif defect=='restore':data['parameters']['maximum_restore_m']=.1
            elif defect=='neighbor':selected[cell['cell_id']].remove([2,2])
            elif defect=='surface_source':cell['raster_alignment']['authority']='surface_raster'
            elif defect=='raised_final':cell['core_samples'][0][0]=20.
            elif defect=='unknown':cell['water_fit']['core_samples'][0][0]=None
            else:data['parameters']['maximum_restore_m']=10**1000
            before=canonical_hash([data,cells,selected])
            with self.subTest(defect=defect),self.assertRaises(CompilerError):
                build(ROOT,cells,data['water'],selected,data['parameters'])
            self.assertEqual(before,canonical_hash([data,cells,selected]))

    def test_corrupt_packed_water_height_is_measured_and_refused(self):
        data,cells,selected=fixture()
        output,_=build(ROOT,cells,data['water'],selected,data['parameters'])
        original=canonical_hash([data,cells,selected])
        quad=next(q for p in output.values() for q in p['quads'] if q['coordinate']==[0,0])
        for height in (0.,-1.+1e-9):
            for vertex in quad['vertices']:vertex[2]=height
            with self.subTest(height=height),self.assertRaisesRegex(CompilerError,'packed field'):
                _encoded_metrics(ROOT,output,data['water'],data['cell']['bounds'],data['parameters'])
        self.assertEqual(original,canonical_hash([data,cells,selected]))


if __name__=='__main__':unittest.main()
