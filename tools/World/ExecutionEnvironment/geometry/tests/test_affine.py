"""Whole-field affine certificates reject missing creases and balanced overlap/hole errors."""
from copy import deepcopy
from fractions import Fraction
from pathlib import Path
import unittest
from unittest.mock import patch

from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from .. import affine
from ..api import execution_identity
from .affine_native_fixture import fixture as native_fixture

ROOT = Path(__file__).resolve().parents[5]


def fixture():
    a, b, c, d = [0., 0., 0.], [10., 0., 1.], [10., 10., 0.], [0., 10., 0.]
    return [[a, b, c], [a, c, d]], [[a, b, d], [b, c, d]]


class AffineCertificateTests(unittest.TestCase):
    def test_disjoint_piece_boxes_skip_clipping_without_skipping_work_accounting(self):
        reference = []
        for x in range(32):
            a, b, c, d = [float(x), 0., 0.], [float(x+1), 0., 0.], [float(x+1), 1., 0.], [float(x), 1., 0.]
            reference.extend([[a, b, c], [a, c, d]])
        queries = [[[0., 0., 0.], [32., 0., 0.], [32., 1., 0.]],
                   [[0., 0., 0.], [32., 1., 0.], [0., 1., 0.]]]
        with patch.object(affine, '_overlap', wraps=affine._overlap) as overlap:
            measured = affine.measure_affine_cover(reference, queries)
        self.assertEqual(0., measured['maximum_height_error_upper_m'])
        self.assertGreater(measured['reference_piece_pair_tests'], 1000)
        self.assertEqual(measured['work_count'], len(reference)*len(queries)+measured['reference_piece_pair_tests'])
        self.assertLess(overlap.call_count, 400, 'Provably disjoint piece pairs need no polygon clipping')

    def test_equal_and_flipped_crease_with_identical_corner_heights(self):
        reference, changed = fixture(); before = deepcopy((reference, changed))
        equal = affine.measure_affine_cover(reference, reference)
        self.assertEqual(0., equal['maximum_height_error_upper_m'])
        measured = affine.measure_affine_cover(reference, changed)
        self.assertGreaterEqual(measured['maximum_height_error_upper_m'], .5)
        self.assertLess(measured['maximum_height_error_upper_m'], .500000000000001)
        self.assertEqual(before, (reference, changed))
        self.assertEqual(measured, affine.measure_affine_cover(reference, changed))
        lifted = [[[p[0], p[1], p[2]+.25] for p in face] for face in reference]
        self.assertEqual(0., affine.measure_affine_cover(reference, lifted, offset_m=.25)['maximum_height_error_upper_m'])
        reversed_faces = [list(reversed(face)) for face in reference]
        self.assertEqual(0., affine.measure_affine_cover(reversed_faces, reference)['maximum_height_error_upper_m'])

    def test_balanced_overlap_and_hole_cannot_pass_the_area_sum(self):
        a, b, c, m = [0., 0., 0.], [4., 0., 0.], [0., 4., 0.], [2., 2., 0.]
        with self.assertRaisesRegex(ExecutionEnvironmentError, 'overlapping'):
            affine.measure_affine_cover([[a, b, m], [a, b, m]], [[a, b, c]])
        reference, _ = fixture()
        with self.assertRaisesRegex(ExecutionEnvironmentError, 'completely cover'):
            affine.measure_affine_cover(reference[:1], reference)

    def test_reporting_rounds_up_instead_of_hiding_a_threshold_violation(self):
        reference = [[[0., 0., 0.], [3., 0., 1.], [0., 3., 0.]]]
        queries = [[[0., 0., 0.], [1., 0., 0.], [0., 1., 0.]]]
        exact = Fraction(1, 3)
        self.assertLess(Fraction(float(exact)), exact)
        result = affine.measure_affine_cover(reference, queries)
        self.assertGreaterEqual(Fraction(result['maximum_height_error_upper_m']), exact)
        self.assertGreater(result['maximum_height_error_upper_m'], float(exact))

    def test_captured_native_float_triangle_crossing_a_real_composite_crease(self):
        reference, queries, essential = native_fixture()
        result = affine.measure_affine_cover(reference, queries, offset_m=.15)
        self.assertLess(result['maximum_height_error_upper_m'], 1e-6)
        self.assertGreater(result['overlap_vertex_tests'], 3)
        reference.pop(essential)
        with self.assertRaisesRegex(ExecutionEnvironmentError, 'completely cover'):
            affine.measure_affine_cover(reference, queries, offset_m=.15)

    def test_nonfinite_empty_degenerate_and_exhausted_work_refuse(self):
        for case in ('empty', 'nonfinite', 'boolean', 'degenerate', 'offset', 'work'):
            reference, _ = fixture(); queries = deepcopy(reference); options = {}
            if case == 'empty': reference = []
            elif case == 'nonfinite': reference[0][0][2] = float('nan')
            elif case == 'boolean': reference[0][0][0] = True
            elif case == 'degenerate': queries[0][2] = deepcopy(queries[0][0])
            elif case == 'offset': options['offset_m'] = float('inf')
            else: options['maximum_work'] = 1
            before = repr((reference, queries))
            with self.subTest(case=case), self.assertRaises(ExecutionEnvironmentError):
                affine.measure_affine_cover(reference, queries, **options)
            self.assertEqual(before, repr((reference, queries)))

    def test_optional_identity_is_consumed_without_changing_legacy_geometry(self):
        parent, own = execution_identity(ROOT, True), affine.execution_identity(ROOT, True)
        original = Path.read_bytes
        with patch.object(Path, 'read_bytes', lambda p: original(p)+b'\n' if p.resolve() == Path(affine.__file__).resolve() else original(p)):
            self.assertEqual(parent, execution_identity(ROOT, True))
            self.assertNotEqual(own['identity_sha256'], affine.execution_identity(ROOT, True)['identity_sha256'])


if __name__ == '__main__': unittest.main()
