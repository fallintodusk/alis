"""Declared fine/coarse Ground sampling must retain a complete shared trace."""
from copy import deepcopy
from collections import Counter
import unittest

from ..app.contracts import CompilerError, canonical_hash
from .test_ground_surface import ROOT, fixture
from ..refinement.ground import build_ground_surface


def inputs():
    cells,grid,evidence,selected=fixture()
    grid.update(spacing=[2.,-2.],size=[31,16],samples=[
        [12. if x in (0,30) or y in (0,15) else 10.+((x*7+y*3)%11)/20 for x in range(31)] for y in range(16)])
    evidence['grid_content_sha256']=canonical_hash(grid)
    resolution=[{'cell_id':key,'quad':[0,0],'stride':1 if i==0 else 15} for i,key in enumerate(cells)]
    return cells,grid,evidence,selected,resolution


class GroundResolutionTests(unittest.TestCase):
    def build(self,values):
        cells,grid,evidence,selected,resolution=values
        return build_ground_surface(ROOT,cells,grid,evidence,selected,maximum_triangles=4096,
            canonical_crs='EPSG:32618',resolution=resolution)

    def test_coarse_neighbor_consumes_shared_fine_edge_without_full_refinement(self):
        values=inputs();before=deepcopy(values);result=self.build(values)
        self.assertEqual(values,before)
        self.assertEqual(result,self.build(values))
        left,right=[s['quads'][0] for s in result['surfaces'].values()]
        edge=lambda q:sorted(v for v in q['vertices'] if v[0]==30.)
        self.assertEqual(edge(left),edge(right));self.assertEqual(16,len(edge(left)))
        self.assertEqual(18,len(right['vertices']),'Coarse interior gains only the required shared edge stations')
        self.assertEqual(16,len(right['triangles']))
        self.assertTrue(all(v[0] in (30.,60.) for v in right['vertices']))
        uniform=list(values);uniform[-1]=[{**r,'stride':1} for r in values[-1]]
        fine=self.build(uniform)
        self.assertLess(sum(len(s['quads'][0]['triangles']) for s in result['surfaces'].values()),
            sum(len(s['quads'][0]['triangles']) for s in fine['surfaces'].values()))
        self.assertNotEqual(result['consumed']['resolution_sha256'],fine['consumed']['resolution_sha256'])
        reordered=list(values);reordered[-1]=list(reversed(values[-1]))
        self.assertEqual(result,self.build(reordered),'Record order is not a different sampling policy')

    def test_horizontal_vertical_and_nonnested_station_union_have_closed_oriented_topology(self):
        cells,grid,evidence,selected,_=inputs();key=next(iter(cells));cell=cells[key];cells={key:cell}
        cell.update(bounds=[0,0,60,60],core_samples=[[12.]*3 for _ in range(3)])
        cell['surface_semantics']['core_samples']={'ground':[[1.]*3 for _ in range(3)],'hydro_transition':[[0.]*3 for _ in range(3)]}
        grid.update(origin=[0.,60.],size=[31,31],samples=[
            [12. if x in (0,30) or y in (0,30) else 10.+((x*3+y*7)%13)/30 for x in range(31)] for y in range(31)])
        evidence['grid_content_sha256']=canonical_hash(grid);selected={key:[[0,0],[1,0],[0,1],[1,1]]}
        resolution=[{'cell_id':key,'quad':quad,'stride':stride} for quad,stride in zip(selected[key],(3,5,15,1))]
        result=self.build((cells,grid,evidence,selected,resolution));edges=Counter()
        for quad in result['surfaces'][key]['quads']:
            for face in quad['triangles']:
                points=[tuple(quad['vertices'][i][:3]) for i in face]
                for a,b in zip(points,points[1:]+points[:1]):edges[a,b]+=1
        for (a,b),count in edges.items():
            if any(a[i]==b[i]==v for i in (0,1) for v in (0,60)):
                self.assertEqual(1,count);self.assertEqual(0,edges[b,a])
            else:self.assertEqual(count,edges[b,a],'Every internal oriented edge has the opposite incident use')

    def test_invalid_incomplete_or_excessive_resolution_refuses_without_mutation(self):
        for kind in ('missing','duplicate','unknown','zero','boolean','nondivisor','extra_field'):
            values=inputs();cells,grid,evidence,selected,resolution=values
            if kind=='missing':resolution.pop()
            elif kind=='duplicate':resolution.append(deepcopy(resolution[0]))
            elif kind=='unknown':resolution[0]['cell_id']='absent'
            elif kind=='zero':resolution[0]['stride']=0
            elif kind=='boolean':resolution[0]['stride']=True
            elif kind=='nondivisor':resolution[0]['stride']=4
            else:resolution[1]['extra_field']=1
            before=deepcopy(values)
            with self.subTest(kind=kind),self.assertRaises(CompilerError):self.build(values)
            self.assertEqual(before,values)
        cells,grid,evidence,selected,resolution=inputs()
        with self.assertRaises(CompilerError):
            build_ground_surface(ROOT,cells,grid,evidence,selected,maximum_triangles=2,
                canonical_crs='EPSG:32618',resolution=resolution)

    def test_unknown_source_and_changed_outer_trace_still_refuse(self):
        for kind in ('nodata','outer'):
            values=inputs();cells,grid,evidence,selected,resolution=values
            grid['samples'][0 if kind=='outer' else 7][30 if kind=='outer' else 20]=-9999. if kind=='nodata' else 8.
            evidence['grid_content_sha256']=canonical_hash(grid)
            with self.subTest(kind=kind),self.assertRaises(CompilerError):self.build(values)

    def test_redundant_outer_stations_do_not_consume_final_triangle_budget(self):
        cells,grid,evidence,selected=fixture();key=next(iter(cells));cells={key:cells[key]};selected={key:[[0,0]]}
        grid.update(size=[3,3],samples=[[12.,12.,12.],[12.,10.,12.],[12.,12.,12.]])
        evidence['grid_content_sha256']=canonical_hash(grid)
        options={'maximum_triangles':4,'canonical_crs':'EPSG:32618'}
        uniform=build_ground_surface(ROOT,cells,grid,evidence,selected,**options)
        result=build_ground_surface(ROOT,cells,grid,evidence,selected,
            resolution=[{'cell_id':key,'quad':[0,0],'stride':1}],**options)
        self.assertEqual(uniform['surfaces'],result['surfaces'])
        self.assertEqual(4,len(result['surfaces'][key]['quads'][0]['triangles']))
