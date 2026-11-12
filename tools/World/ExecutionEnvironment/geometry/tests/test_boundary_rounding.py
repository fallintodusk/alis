import math
import sys
import unittest
from copy import deepcopy
from fractions import Fraction

from World.ExecutionEnvironment.app.contracts import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.boundary_rounding import certify_ring_rounding, _upper
from .boundary_rounding_fixture import SOURCE_RING, ENCODED_RING


class BoundaryRoundingTests(unittest.TestCase):
    def test_upward_bound_conversion_overflow_refuses(self):
        largest = Fraction(sys.float_info.max)
        gap = largest-Fraction(math.nextafter(sys.float_info.max, 0.))
        with self.assertRaises(ExecutionEnvironmentError):
            _upper(largest+gap/4)

    def test_captured_conversion_is_bounded_but_not_exact_source_geometry(self):
        result = certify_ring_rounding(SOURCE_RING, ENCODED_RING)
        self.assertEqual(result['source_vertices'], 19)
        self.assertEqual(result['encoded_vertices'], 45)
        self.assertEqual(result['rounded_subdivision_vertices'], 26)
        self.assertGreater(result['vertices_off_exact_source_edges'], 0)
        self.assertLess(result['l1_hausdorff_upper_m'], 6e-10)
        # Independent exact clipping of the captured sets has this nonzero area.
        self.assertGreaterEqual(result['symmetric_difference_area_upper_m2'], 1.697276199e-9)

    def test_perpendicular_move_cannot_enlarge_its_own_rounding_budget(self):
        result = certify_ring_rounding(SOURCE_RING, ENCODED_RING)
        witness = result['rounding_witnesses'][0]
        edge, index = witness['source_edge_index'], witness['encoded_vertex_index']
        a, b = SOURCE_RING[edge:edge+2]
        axis = 1 if abs(b[0]-a[0]) >= abs(b[1]-a[1]) else 0
        changed = deepcopy(ENCODED_RING)
        changed[index][axis] += 1e-8
        with self.assertRaises(ExecutionEnvironmentError):
            certify_ring_rounding(SOURCE_RING, changed)

    def test_reordered_subdivision_and_lost_original_vertex_refuse(self):
        originals = {tuple(point) for point in SOURCE_RING[:-1]}
        extra = [i for i, point in enumerate(ENCODED_RING[:-1]) if tuple(point) not in originals]
        pair = next((a, b) for a, b in zip(extra, extra[1:]) if b == a+1)
        changed = deepcopy(ENCODED_RING)
        changed[pair[0]], changed[pair[1]] = changed[pair[1]], changed[pair[0]]
        with self.assertRaises(ExecutionEnvironmentError):
            certify_ring_rounding(SOURCE_RING, changed)
        changed = [p for p in ENCODED_RING[:-1] if p != SOURCE_RING[1]]
        changed.append(changed[0])
        with self.assertRaises(ExecutionEnvironmentError):
            certify_ring_rounding(SOURCE_RING, changed)

    def test_midpoint_only_cell_intersection_remains_unqualified(self):
        source = [[0., 0.], [2., 2.], [0., 2.], [0., 0.]]
        encoded = [source[0], [1., math.nextafter(1., math.inf)], *source[1:]]
        with self.assertRaises(ExecutionEnvironmentError):
            certify_ring_rounding(source, encoded)

    def test_unchanged_ring_and_cyclic_start(self):
        result = certify_ring_rounding(SOURCE_RING, SOURCE_RING)
        self.assertEqual(result['rounded_subdivision_vertices'], 0)
        self.assertEqual(result['l1_hausdorff_upper_m'], 0.)
        self.assertEqual(result['symmetric_difference_area_upper_m2'], 0.)
        moved_start = ENCODED_RING[7:-1]+ENCODED_RING[:8]
        result = certify_ring_rounding(SOURCE_RING, moved_start)
        self.assertEqual(result['encoded_vertices'], 45)

    def test_axis_aligned_edge_requires_the_exact_constant_coordinate(self):
        source = [[10., 10.], [12., 10.], [12., 12.], [10., 12.], [10., 10.]]
        encoded = [source[0], [11., 10.], *source[1:]]
        result = certify_ring_rounding(source, encoded)
        self.assertEqual(result['rounded_subdivision_vertices'], 1)
        self.assertEqual(result['vertices_off_exact_source_edges'], 0)
        self.assertEqual(result['symmetric_difference_area_upper_m2'], 0.)
        encoded[1][1] = math.nextafter(10., math.inf)
        with self.assertRaises(ExecutionEnvironmentError):
            certify_ring_rounding(source, encoded)

    def test_unknown_nonfinite_topology_and_work_refuse(self):
        reversed_ring = list(reversed(ENCODED_RING))
        nonfinite = deepcopy(ENCODED_RING)
        nonfinite[1][0] = math.nan
        for changed in (reversed_ring, nonfinite, ENCODED_RING[:-1]):
            with self.subTest(changed=changed[:2]), self.assertRaises(ExecutionEnvironmentError):
                certify_ring_rounding(SOURCE_RING, changed)
        with self.assertRaises(ExecutionEnvironmentError):
            certify_ring_rounding(SOURCE_RING, ENCODED_RING, maximum_work=1)

    def test_concave_ring_retains_the_independent_nonzero_rounding_bound(self):
        original=certify_ring_rounding(SOURCE_RING,ENCODED_RING)
        def cross(a,b,p):
            a,b,p=[list(map(Fraction,row)) for row in (a,b,p)]
            return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
        witness=next(row for row in original['rounding_witnesses'] if
            cross(SOURCE_RING[row['source_edge_index']],SOURCE_RING[row['source_edge_index']+1],ENCODED_RING[row['encoded_vertex_index']])!=0)
        edge=witness['source_edge_index'];point=ENCODED_RING[witness['encoded_vertex_index']]
        indent=(edge+5)%(len(SOURCE_RING)-1)
        center=[sum(p[k] for p in SOURCE_RING[:-1])/(len(SOURCE_RING)-1) for k in (0,1)]
        source=[];encoded=[]
        for index,p in enumerate(SOURCE_RING[:-1]):
            source.append(p);encoded.append(p)
            if index==edge:encoded.append(point)
            if index==indent:source.append(center);encoded.append(center)
        source.append(source[0]);encoded.append(encoded[0])
        result=certify_ring_rounding(source,encoded)
        independently_measured_area=abs(cross(SOURCE_RING[edge],SOURCE_RING[edge+1],point))/2
        self.assertGreater(independently_measured_area,0)
        self.assertEqual(result['vertices_off_exact_source_edges'],1)
        self.assertGreaterEqual(Fraction(result['symmetric_difference_area_upper_m2']),independently_measured_area)

    def test_simple_collinear_original_vertices_survive_but_contacts_refuse(self):
        source=[[0.,0.],[1.,0.],[2.,0.],[2.,2.],[1.,1.],[0.,2.],[0.,0.]]
        encoded=[source[0],[.5,0.],*source[1:]]
        result=certify_ring_rounding(source,encoded)
        self.assertEqual(result['source_vertices'],6)
        self.assertEqual(result['symmetric_difference_area_upper_m2'],0.)
        for invalid in (
            [[0.,0.],[2.,0.],[1.,0.],[2.,2.],[0.,2.],[0.,0.]],
            [[0.,0.],[2.,0.],[2.,2.],[1.,0.],[0.,2.],[0.,0.]]):
            with self.assertRaises(ExecutionEnvironmentError):certify_ring_rounding(invalid,invalid)
