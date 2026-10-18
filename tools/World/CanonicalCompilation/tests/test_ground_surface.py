"""Unpublished raster-Ground construction and boundary refusal."""
from copy import deepcopy
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

from ..app.contracts import CompilerError, canonical_hash
from .test_terrain_refinement import fixture as terrain_fixture

ROOT=Path(__file__).resolve().parents[4]


def fixture():
    cells,_,_,selected=terrain_fixture()
    values=[[[.1,.5],[.9,.3]],[[.5,.4],[.3,.2]]]
    for cell,weights in zip(cells.values(),values):
        cell['raster_alignment']={'canonical_crs':'EPSG:32618'}
        cell['surface_semantics']['core_samples']={'ground':weights,'hydro_transition':[[1-z for z in row] for row in weights]}
    grid={'basis':'raster_ground_model','frame':{'horizontal':'EPSG:32618','vertical':'EPSG:3855','unit':'m'},
        'sample_rule':'grid_nodes','origin':[0.,30.],'spacing':[15.,-15.],'size':[5,3],
        'samples':[[12.]*5,[12.,10.,10.,10.,12.],[12.]*5],'nodata':-9999.}
    evidence={'basis':'raster_ground_model','canonical_crs':'EPSG:32618','vertical_datum':'EPSG:3855',
        'grid_content_sha256':canonical_hash(grid),'source_sha256':'a'*64,'processing_sha256':'b'*64}
    return cells,grid,evidence,selected


class GroundSurfaceTests(unittest.TestCase):
    def operation(self):
        spec=importlib.util.find_spec('World.CanonicalCompilation.refinement.ground')
        self.assertIsNotNone(spec,'The canonical refinement owner lacks pure Ground construction')
        from ..refinement import ground
        return ground

    def test_source_nodes_exact_seams_weights_and_determinism(self):
        operation=self.operation();cells,grid,evidence,selected=fixture();before=deepcopy((cells,grid,evidence,selected))
        result=operation.build_ground_surface(ROOT,cells,grid,evidence,selected,maximum_triangles=32,canonical_crs='EPSG:32618')
        self.assertEqual(before,(cells,grid,evidence,selected))
        self.assertEqual(result,operation.build_ground_surface(ROOT,cells,grid,evidence,selected,maximum_triangles=32,canonical_crs='EPSG:32618'))
        cells['unselected_fixture']=deepcopy(next(iter(cells.values())))
        self.assertEqual(result,operation.build_ground_surface(ROOT,cells,grid,evidence,selected,maximum_triangles=32,canonical_crs='EPSG:32618'))
        cells.pop('unselected_fixture')
        surfaces=result['surfaces'];left,right=[surfaces[key]['quads'][0] for key in cells]
        self.assertEqual(sorted(v for v in left['vertices'] if v[0]==30),sorted(v for v in right['vertices'] if v[0]==30))
        self.assertTrue(any(v[:3]==[30.,15.,10.] for v in left['vertices']))
        self.assertFalse(any(0<v[0]<30 and v[1] in (0,30) for v in left['vertices']), 'Unselected outer edges gain no new vertices')
        for quad in surfaces.values():
            for value in quad['quads'][0]['vertices']:self.assertAlmostEqual(sum(value[3:]),1.)
        for face in left['triangles']:
            vs=[left['vertices'][i] for i in face];u=sum(p[0] for p in vs)/90;v=sum(30-p[1] for p in vs)/90
            expected=.1+.4*u+.8*v if u+v<=1 else .3+.2*(1-v)+.6*(1-u)
            self.assertAlmostEqual(sum(p[3] for p in vs)/3,expected,places=12)
        self.assertNotIn('$schema',result,'An unpublished construction is not a loadable Water refinement')

    def test_open_edge_missing_neighbor_and_no_extrapolation_refuse(self):
        operation=self.operation()
        for case in ('outer_height','neighbor','extent','base_seam'):
            cells,grid,evidence,selected=fixture()
            if case=='outer_height':grid['samples'][1][0]=10.
            elif case=='neighbor':selected.pop(next(reversed(selected)))
            elif case=='extent':grid['origin'][0]=15.
            else:cells[next(reversed(cells))]['core_samples'][0][0]=13.
            evidence['grid_content_sha256']=canonical_hash(grid);before=deepcopy((cells,grid,evidence,selected))
            with self.subTest(case=case),self.assertRaises(CompilerError):
                operation.build_ground_surface(ROOT,cells,grid,evidence,selected,maximum_triangles=32,canonical_crs='EPSG:32618')
            self.assertEqual(before,(cells,grid,evidence,selected))

    def test_wrong_basis_stale_nonfinite_unknown_and_budget_refuse(self):
        operation=self.operation()
        for case in ('basis','stale','nan','nodata','frame','budget','geometry_crs','cell_crs'):
            cells,grid,evidence,selected=fixture();budget=32
            if case=='basis':evidence['basis']='observed_las_ground'
            elif case=='stale':evidence['grid_content_sha256']='c'*64
            elif case in ('nan','nodata'):grid['samples'][1][1]=float('nan') if case=='nan' else -9999.
            elif case=='frame':grid['frame']['vertical']='unknown'
            elif case=='cell_crs':next(iter(cells.values()))['raster_alignment']['canonical_crs']='EPSG:32638'
            elif case=='budget':budget=2
            if case not in ('stale','nan'):evidence['grid_content_sha256']=canonical_hash(grid)
            with self.subTest(case=case),self.assertRaises(CompilerError):
                operation.build_ground_surface(ROOT,cells,grid,evidence,selected,maximum_triangles=budget,
                    canonical_crs='EPSG:32638' if case=='geometry_crs' else 'EPSG:32618')

    def test_own_consumed_implementation_identity_is_not_omitted(self):
        operation=self.operation();before=operation.implementation_identity()
        original=operation.file_hash
        with patch.object(operation,'file_hash',side_effect=lambda p:'0'*64 if Path(p)==Path(operation.__file__) else original(p)):
            self.assertNotEqual(before,operation.implementation_identity())

    def test_projected_source_stations_preserve_convex_affine_semantics(self):
        operation=self.operation()
        from ..refinement.app.geometry import _interpolate
        # Captured canonical quad and source spacing: an exact zero became -1.39e-17.
        points=[[585343.41,4510542.45,6.,1.,0.],
            [585373.41,4510542.45,10.,.880104239406888,.11989576059311202],
            [585343.41,4510512.45,7.6,1.,0.],
            [585373.41,4510512.45,13.6,1.,0.]]
        grid={'origin':points[0][:2],'spacing':[2.,-2.],'size':[16,16],'samples':[[12.]*16 for _ in range(16)]}
        quad=operation._quad(ROOT,points,grid,[],4096)
        self.assertEqual(256,len(quad['vertices']))
        self.assertEqual(450,len(quad['triangles']))
        for point in quad['vertices']:
            self.assertTrue(all(0<=v<=1 for v in point[3:]))
            self.assertAlmostEqual(1.,sum(point[3:]),places=15)
            x,y=round((point[0]-points[0][0])/2),round((points[0][1]-point[1])/2)
            triangle=(points[0],points[2],points[1]) if x+y<=15 else (points[1],points[2],points[3])
            expected=_interpolate(triangle,point[:2])
            for actual,prior in zip(point[3:],expected[3:]):self.assertAlmostEqual(actual,prior,places=15)
        edge=next(p for p in quad['vertices'] if p[:2]==[585349.41,4510512.45])
        self.assertEqual([1.,0.],edge[3:])
        invalid=deepcopy(points);invalid[0][3:]=[1.001,-.001]
        with self.assertRaises(CompilerError):operation._quad(ROOT,invalid,grid,[],4096)
