"""The contact diagnostic derives its direction from the actual upper boundary."""
import importlib.util
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
SPEC=importlib.util.spec_from_file_location('prepare_region_shore_gameplay',ROOT/'prepare_region_shore_gameplay.py')
MODULE=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(MODULE)


class ShoreContactPreparationTests(unittest.TestCase):
    def test_both_contacts_cross_ground_to_inside_without_claiming_a_mapped_connector(self):
        a,b,c,d=[0.,0.,0.],[10.,0.,0.],[0.,2.,0.],[10.,2.,0.]
        result=MODULE.contacts([[a,b,c],[b,d,c]],distance=2.)
        self.assertEqual(2,len(result))
        for contact in result:
            outside,seam,inside=contact['points']
            self.assertTrue(outside[0]<0 or outside[0]>10)
            self.assertTrue(0<inside[0]<10)
            self.assertIn(seam[0],(0.,10.));self.assertEqual([1.,1.,1.],[p[1] for p in (outside,seam,inside)])
        for width in (1.,3.):
            with self.subTest(width=width),self.assertRaises(ValueError):MODULE.contacts([[a,b,c],[b,d,c]],width=width)


if __name__=='__main__':unittest.main()
