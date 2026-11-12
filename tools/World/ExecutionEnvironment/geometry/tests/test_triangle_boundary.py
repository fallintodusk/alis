"""Exact stored topology, legitimate holes and independent expected-topology controls."""
from copy import deepcopy
from pathlib import Path
from unittest.mock import patch
import unittest

from World.ExecutionEnvironment.app.contracts import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry import triangle_boundary as boundary
from World.ExecutionEnvironment.geometry.api import execution_identity,validate_triangle_cover
from .triangle_boundary_fixture import triangles

ROOT=Path(__file__).resolve().parents[5]


def grid():
    faces=[]
    for y in range(3):
        for x in range(3):
            a,b,c,d=[x*2.,y*2.],[x*2.+2,y*2.],[x*2.+2,y*2.+2],[x*2.,y*2.+2]
            faces.extend([[a,b,c],[a,c,d]])
    return faces


class TriangleBoundaryTests(unittest.TestCase):
    def test_captured_exact_t_junction_nodes_without_moving_any_vertex(self):
        field=triangles();before=deepcopy(field);result=boundary.recover_triangle_boundary(field)
        self.assertEqual((result['components'],result['holes'],result['boundary_edges'],result['internal_edges']),(1,0,5,1))
        self.assertEqual(result['noded_edge_vertices'],1)
        self.assertEqual(field,before)
        self.assertEqual({tuple(p) for face in field for p in face},
            {tuple(p) for ring in result['geometry']['coordinates'] for p in ring})
        validate_triangle_cover(ROOT,result['geometry'],field)

    def test_order_and_input_winding_do_not_change_normalized_geometry(self):
        field=triangles();original=boundary.recover_triangle_boundary(field)
        changed=[[face[1],face[0],face[2]] for face in reversed(field)]
        result=boundary.recover_triangle_boundary(changed)
        self.assertEqual(original['geometry'],result['geometry'])
        self.assertEqual(original['area_rational'],result['area_rational'])

    def test_holes_and_disconnected_components_are_reported_without_repair(self):
        field=grid();del field[8:10]
        field.append([[10.,0.],[11.,0.],[10.,1.]])
        result=boundary.recover_triangle_boundary(field)
        self.assertEqual((result['components'],result['holes']),(2,1))
        self.assertEqual(result['area_rational'],'65/2')
        self.assertEqual(result['geometry']['type'],'MultiPolygon')
        validate_triangle_cover(ROOT,result['geometry'],field)

    def test_removed_interior_face_is_detected_by_independent_expected_topology(self):
        field=grid();expected=boundary.recover_triangle_boundary(field)
        del field[8]
        missing=boundary.recover_triangle_boundary(field)
        self.assertEqual((expected['components'],expected['holes']),(1,0))
        self.assertEqual((missing['components'],missing['holes']),(1,1))
        self.assertNotEqual(expected['area_rational'],missing['area_rational'])

    def test_positive_overlap_duplicates_and_nonmanifold_point_contact_refuse(self):
        first=[[0.,0.],[2.,0.],[0.,2.]]
        for second in (first,[[1.,0.],[3.,0.],[1.,2.]],[[2.,0.],[3.,0.],[2.,-1.]]):
            field=[first,second];before=deepcopy(field)
            with self.subTest(second=second),self.assertRaises(ExecutionEnvironmentError):
                boundary.recover_triangle_boundary(field)
            self.assertEqual(field,before)

    def test_nonfinite_collapsed_unrepresentable_or_excessive_work_refuses(self):
        invalid=[[],[[[0.,0.],[1.,0.],[2.,0.]]],triangles()*2049]
        for value in (float('nan'),float('inf'),True,1<<9999):
            field=triangles();field[0][0][0]=value;invalid.append(field)
        for field in invalid:
            with self.assertRaises(ExecutionEnvironmentError):boundary.recover_triangle_boundary(field)
        with self.assertRaises(ExecutionEnvironmentError):boundary.recover_triangle_boundary(triangles(),maximum_work=1)

    def test_unused_optional_helper_does_not_invalidate_parent_identity(self):
        parent=execution_identity(ROOT,True);own=boundary.execution_identity(ROOT,True)
        read_bytes=Path.read_bytes
        def changed(path):
            value=read_bytes(path)
            return value+b'\n' if path.resolve()==Path(boundary.__file__).resolve() else value
        with patch.object(Path,'read_bytes',changed):
            self.assertEqual(parent,execution_identity(ROOT,True))
            self.assertNotEqual(own['identity_sha256'],boundary.execution_identity(ROOT,True)['identity_sha256'])


if __name__=='__main__':unittest.main()
