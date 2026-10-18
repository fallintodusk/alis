"""Pinned native Ground triangulation and polygon-union contract controls."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[5]
sys.path.insert(0,str(ROOT/'tools'))
from World.ExecutionEnvironment.geometry.api import triangulate_points, polygon_union, covered_points
from World.ExecutionEnvironment.api import ExecutionEnvironmentError


def rectangle(w,s,e,n):
    return {'type':'Polygon','coordinates':[[[w,s],[e,s],[e,n],[w,n],[w,s]]]}


class NativeSurfaceTests(unittest.TestCase):
    def test_translation_stable_source_indices_and_input_refusal(self):
        points = [[0.,0.],[3.,0.],[0.,4.],[4.,5.],[1.,1.]]
        faces = triangulate_points(ROOT,points)
        moved = [[x+583000.,y+4510000.] for x,y in points]
        self.assertEqual(faces,triangulate_points(ROOT,moved))
        self.assertEqual({i for face in faces for i in face},set(range(len(points))))
        for bad in (points+[points[0]],[[0,0],[1,0],[2,0]],[[0,0],[1,0],[True,1]]):
            with self.assertRaises(ExecutionEnvironmentError):
                triangulate_points(ROOT,bad)

    def test_shared_edges_union_preserves_hole_and_measures_real_exterior(self):
        pieces = [rectangle(0,0,4,1),rectangle(0,3,4,4),rectangle(0,1,1,3),rectangle(3,1,4,3)]
        result = polygon_union(ROOT,pieces,rectangle(0,0,4,4))
        self.assertEqual(result['area_m2'],12)
        self.assertEqual(result['outside_area_m2'],0)
        self.assertEqual(covered_points(ROOT,[[2,2],[.5,.5]],result['geometry']),[False,True])
        bad = polygon_union(ROOT,[*pieces,rectangle(4,1,5,2)],rectangle(0,0,4,4))
        self.assertEqual(bad['outside_area_m2'],1)

    def test_unknown_point_membership_refuses(self):
        for points in ([],[[float('nan'),0]],[[0,True]]):
            with self.assertRaises(ExecutionEnvironmentError):
                covered_points(ROOT,points,rectangle(0,0,1,1))
