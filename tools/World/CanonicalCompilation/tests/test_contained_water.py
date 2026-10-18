"""Closed source-shaped standing model; fixtures own their Ground observations."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError, canonical_hash
from World.CanonicalCompilation.refinement.app.contained_water import build_contained_water as construct


def build_contained_water(*args, **kwargs):
    return construct(*args,canonical_crs='EPSG:32618',**kwargs)


def fixture():
    points = [[float(x),float(y),0.0 if (x,y)==(2,2) else 2.0 if 1<=x<=3 and 1<=y<=3 else 1.0]
              for x in range(5) for y in range(5)]
    water = {'feature_id':'fixture:basin','feature_class':'water',
        'geometry':{'type':'Polygon','coordinates':[[[0,0],[4,0],[4,4],[0,4],[0,0]]]},
        'attributes':{'surface_geometry':'polygon','surface_behavior':'standing','surface_role':'area',
            'surface_function':{'function_id':'standing_polygon_quantile','function_version':1,'level_m':8}}}
    context = {'geometry_sha256':canonical_hash(water['geometry']), 'amenity':'fountain','barrier':'kerb'}
    evidence = {'point_content_sha256':canonical_hash(points),'classification':2,
        'canonical_crs':'EPSG:32618','vertical_datum':'EPSG:3855',
        'source_sha256':'a'*64,'processing_sha256':'b'*64}
    return water,points,context,evidence


class ContainedWaterTests(unittest.TestCase):
    def test_closed_component_exact_level_and_determinism(self):
        water,points,context,evidence = fixture()
        original = deepcopy((water,points,context,evidence))
        result = build_contained_water(ROOT,water,points,context,evidence,quantile=.5,height_step=.1)
        function = result['feature']['attributes']['surface_function']
        self.assertEqual(function['function_id'],'contained_ground_quantile')
        self.assertLess(function['level_m'],function['spill_level_m'])
        self.assertGreater(result['wet_area_m2'],0)
        self.assertEqual(result['outside_mapped_area_m2'],0)
        self.assertEqual(result,build_contained_water(ROOT,water,points,context,evidence,quantile=.5,height_step=.1))
        self.assertEqual((water,points,context,evidence),original)

    def test_open_equality_and_quantization_crossing_refuse(self):
        water,points,context,evidence = fixture()
        # Upper quantiles select the2m rim: equality must refuse.
        for quantile in (.75,1.):
            with self.assertRaises(CompilerError):
                build_contained_water(ROOT,water,points,context,evidence,quantile=quantile,height_step=.1)

    def test_wrong_class_stale_points_duplicate_height_and_missing_datum_refuse(self):
        water,points,context,evidence = fixture()
        for key,value in [('classification',1),('point_content_sha256','c'*64),('vertical_datum',None),('canonical_crs','EPSG:32638')]:
            changed={**evidence,key:value}
            with self.assertRaises(CompilerError):
                build_contained_water(ROOT,water,points,context,changed,quantile=0,height_step=.1)
        points.append([2.,2.,3.])
        evidence['point_content_sha256']=canonical_hash(points)
        with self.assertRaises(CompilerError):
            build_contained_water(ROOT,water,points,context,evidence,quantile=0,height_step=.1)

    def test_wet_component_outside_mapped_domain_is_not_silently_clipped(self):
        water,points,context,evidence = fixture()
        water['geometry']['coordinates']=[[[0,0],[2.2,0],[2.2,4],[0,4],[0,0]]]
        context['geometry_sha256']=canonical_hash(water['geometry'])
        with self.assertRaises(CompilerError) as caught:
            build_contained_water(ROOT,water,points,context,evidence,quantile=.5,height_step=.1)
        self.assertGreater(caught.exception.details['outside_mapped_area_m2'],0)

    def test_rounding_must_not_raise_a_subspill_sample_above_spill(self):
        water,points,context,evidence = fixture()
        for p in points:
            p[2]=1.06 if p[:2]==[2.,2.] else 1.09 if 1<=p[0]<=3 and 1<=p[1]<=3 else 1.
        evidence['point_content_sha256']=canonical_hash(points)
        water['geometry']['coordinates']=[[[1.9,1.9],[2.1,1.9],[2.1,2.1],[1.9,2.1],[1.9,1.9]]]
        context['geometry_sha256']=canonical_hash(water['geometry'])
        with self.assertRaises(CompilerError) as caught:
            build_contained_water(ROOT,water,points,context,evidence,quantile=.5,height_step=.1)
        self.assertEqual(caught.exception.details['level_m'],1.1)
        self.assertEqual(caught.exception.details['spill_level_m'],1.09)

    def test_canonical_single_multipolygon_and_projected_coordinates(self):
        water,points,context,evidence = fixture()
        for point in points:
            point[0]+=583000.; point[1]+=4510000.
        water['geometry']={'type':'MultiPolygon','coordinates':[
            [[[x+583000.,y+4510000.] for x,y in water['geometry']['coordinates'][0]]]]}
        context['geometry_sha256']=canonical_hash(water['geometry'])
        evidence['point_content_sha256']=canonical_hash(points)
        result=build_contained_water(ROOT,water,points,context,evidence,quantile=.5,height_step=.1)
        self.assertGreater(result['wet_area_m2'],0)
        self.assertEqual(result['outside_mapped_area_m2'],0)

    def test_observed_component_is_explicit_and_retains_mapped_provenance(self):
        water,points,context,evidence = fixture()
        water['geometry']['coordinates']=[[[0,0],[2.2,0],[2.2,4],[0,4],[0,0]]]
        original=deepcopy(water)
        context['geometry_sha256']=canonical_hash(water['geometry'])
        with self.assertRaises(CompilerError):
            build_contained_water(ROOT,water,points,context,evidence,quantile=.5,height_step=.1)
        result=build_contained_water(ROOT,water,points,context,evidence,quantile=.5,height_step=.1,
            footprint_model='observed_ground_component')
        function=result['feature']['attributes']['surface_function']
        self.assertGreater(result['outside_mapped_area_m2'],0)
        self.assertEqual(function['footprint_model'],'observed_ground_component')
        self.assertEqual(function['mapped_geometry_sha256'],canonical_hash(water['geometry']))
        self.assertNotEqual(result['feature']['geometry'],water['geometry'])
        self.assertEqual(water,original)

    def test_observed_mode_cannot_bypass_spill_or_unknown_policy(self):
        water,points,context,evidence=fixture()
        for mode,quantile in [('observed_ground_component',1.),('ignore_source',.5)]:
            with self.assertRaises(CompilerError):
                build_contained_water(ROOT,water,points,context,evidence,quantile=quantile,height_step=.1,footprint_model=mode)
