"""Fast captured-case controls; source admission and native acceptance stay separate."""
from copy import deepcopy
from fractions import Fraction as F
from pathlib import Path
import sys
import unittest

REPO = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO/'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError
from World.CanonicalCompilation.refinement.constructed_import import _qualify, _prove_roles, _prove_field
from World.CanonicalCompilation.tests.pier25_connection_fixture import fixture, shore_contact_xy, original_shore_strip
from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.affine import measure_affine_cover, _overlap, _area, _height
from World.SourceIngestion.tests.osm_pier_relation_fixture import fixture as source_fixture


def contact_error(ground, edge):
    a, b = [list(map(F, p)) for p in edge]
    intervals = []
    error = F(0)
    def cross(p, q, r):
        return (q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0])
    for face in ground:
        face = [list(map(F, p)) for p in face]
        sign = 1 if cross(*face) > 0 else -1
        lower, upper = F(0), F(1)
        for p, q in zip(face, [*face[1:], face[0]]):
            start, end = sign*cross(p, q, a), sign*cross(p, q, b)
            if start < 0 and end < 0:
                lower, upper = F(1), F(0)
                break
            if (start < 0) != (end < 0):
                t = start/(start-end)
                if start < 0: lower = max(lower, t)
                else: upper = min(upper, t)
        if lower <= upper:
            intervals.append((lower, upper))
            for t in (lower, upper):
                point = [a[i]+t*(b[i]-a[i]) for i in range(3)]
                error = max(error, abs(point[2]-_height(face, point)))
    covered = F(0)
    for lower, upper in sorted(intervals):
        if lower > covered: raise AssertionError('Shore contact lacks complete Ground support')
        covered = max(covered, upper)
    if covered != 1: raise AssertionError('Shore contact is not completely covered')
    return float(error)


class Pier25ConnectionTests(unittest.TestCase):
    def test_captured_inferred_deck_meets_existing_role_grade_and_ground_contacts(self):
        item, policy, ground = fixture()
        document, native = source_fixture()
        primitive, qualified, _, identity = _qualify(document, native, 'relation/20784500')
        _prove_roles(item, [('relation/20784500', primitive, qualified, identity)])
        proof = _prove_field(REPO, item, policy, {})
        self.assertEqual(proof['faces'], 72)
        self.assertEqual(proof['continuity_error_m'], 0.)
        self.assertLessEqual(proof['maximum_ordinary_gradient'], .02)
        gap = 1000-measure_affine_cover(ground, item['triangles'], offset_m=1000)['maximum_height_error_upper_m']
        self.assertGreaterEqual(gap, -1e-8)
        self.assertLessEqual(abs(gap), 1e-8, 'The model must meet actual Ground rather than float over both endpoints')
        vertices = {tuple(p[:2]): p for face in item['triangles'] for p in face}
        for xy in shore_contact_xy():
            edge = [vertices[tuple(p)] for p in xy]
            self.assertLessEqual(contact_error(ground, edge), 1e-8)
            raised = [[*p[:2], p[2]+.05] for p in edge]
            self.assertGreater(contact_error(ground, raised), .04)
        original = original_shore_strip()[0]
        entry = shore_contact_xy()[0]
        retained = [[original[0][:2], original[1][:2], entry[1]],
                    [entry[1], original[1][:2], entry[0]]]
        flat = [[[*p, 0.] for p in face] for face in retained]
        dry_minimum = 1000-measure_affine_cover(ground, flat, offset_m=-1000)['maximum_height_error_upper_m']
        self.assertGreater(dry_minimum, .25, 'The entrance inset must retain dry physical support on the original path')
        self.assertEqual(primitive['properties']['floating'], 'no')

    def test_uniform_sixty_centimetre_floor_intersects_captured_shore(self):
        _, _, ground = fixture()
        flat = [[[*point[:2], .60] for point in face] for face in original_shore_strip()]
        gap = 1000-measure_affine_cover(ground, flat, offset_m=1000)['maximum_height_error_upper_m']
        self.assertLess(gap, -.009)

    def test_missing_ground_and_broken_joint_refuse(self):
        item, policy, ground = fixture()
        first = [[F(v) for v in p] for p in item['triangles'][0]]
        missing = next(i for i, face in enumerate(ground)
                       if _area(_overlap([[F(v) for v in p] for p in face], first)) > 0)
        with self.assertRaises(ExecutionEnvironmentError) as refused:
            measure_affine_cover(ground[:missing]+ground[missing+1:], item['triangles'])
        self.assertEqual(refused.exception.code, 'geometry_affine_certificate')
        broken = deepcopy(item)
        broken['triangles'][0][0][2] += 1.
        with self.assertRaises(CompilerError) as refused:
            _prove_field(REPO, broken, policy, {})
        self.assertEqual(refused.exception.code, 'constructed_import_invalid')


if __name__ == '__main__':
    unittest.main()
