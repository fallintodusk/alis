"""Exact read-only affine-field certificates over the binary input coordinates."""
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path

from ..api import execution_identity as parent_identity
from ..app.contracts import ExecutionEnvironmentError


def execution_identity(repo_root, verify_native_tools):
    parent = parent_identity(repo_root, verify_native_tools)
    payload = {'parent': parent, 'affine_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    return {**parent, 'identity_sha256': hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(',', ':')).encode()).hexdigest()}


def _refuse(message):
    raise ExecutionEnvironmentError('geometry_affine_certificate', message)


def _cross(a, b, c):
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])


def _signed_area(points):
    return sum(a[0]*b[1]-a[1]*b[0] for a, b in zip(points, (*points[1:], points[0])))/2 if len(points) > 2 else Fraction(0)


def _area(points):
    return abs(_signed_area(points))


def _overlap(subject, clip):
    result, clip = [p[:2] for p in subject], [p[:2] for p in clip]
    if _signed_area(clip) < 0:
        clip.reverse()
    for a, b in zip(clip, (*clip[1:], clip[0])):
        previous, result = result, []
        if not previous:
            break
        start, start_side = previous[-1], _cross(a, b, previous[-1])
        for end in previous:
            end_side = _cross(a, b, end)
            if (start_side >= 0) != (end_side >= 0):
                ratio = start_side/(start_side-end_side)
                result.append([start[i]+ratio*(end[i]-start[i]) for i in (0, 1)])
            if end_side >= 0:
                result.append(end)
            start, start_side = end, end_side
        compact = []
        for point in result:
            if not compact or point != compact[-1]:
                compact.append(point)
        if len(compact) > 1 and compact[0] == compact[-1]:
            compact.pop()
        result = compact
    return result


def _height(face, point):
    a, b, c = face
    determinant = _cross(a, b, c)
    u = ((point[0]-a[0])*(c[1]-a[1])-(point[1]-a[1])*(c[0]-a[0]))/determinant
    v = ((b[0]-a[0])*(point[1]-a[1])-(b[1]-a[1])*(point[0]-a[0]))/determinant
    return a[2]+u*(b[2]-a[2])+v*(c[2]-a[2])


def _finite(value):
    try:
        return type(value) in (int, float) and math.isfinite(value)
    except OverflowError:
        return False


def _faces(value, capacity):
    if (not isinstance(value, list) or not 1 <= len(value) <= capacity or any(
            not isinstance(face, list) or len(face) != 3 or any(not isinstance(p, list) or len(p) != 3 or
                any(not _finite(v) for v in p) for p in face) for face in value)):
        _refuse('Affine certificate requires bounded nonempty finite XYZ triangles')
    result = []
    for face in value:
        exact = [[Fraction(v) for v in p] for p in face]
        if _cross(*exact) == 0:
            _refuse('Affine certificate input has a degenerate projected triangle')
        bounds = [min(p[0] for p in face), min(p[1] for p in face), max(p[0] for p in face), max(p[1] for p in face)]
        result.append((bounds, exact))
    return result


def measure_affine_cover(reference, queries, *, offset_m=0., maximum_work=1_000_000):
    """Certify each query's reference cover and bound the entire affine height difference.

    Queries are independent triangles; callers own their union/topology and height policy.
    No geometry is generated, repaired, rounded or promoted by this operation.
    """
    if not _finite(offset_m) or type(maximum_work) is not int or not 1 <= maximum_work <= 1_000_000:
        _refuse('Affine certificate needs a finite offset and explicit bounded work')
    reference, queries = _faces(reference, 65536), _faces(queries, 4096)
    offset, maximum = Fraction(offset_m), Fraction(0)
    work = pair_tests = piece_tests = vertex_tests = 0
    def tick():
        nonlocal work
        work += 1
        if work > maximum_work:
            _refuse('Affine certificate exceeds its work capacity')
    for bounds, query in queries:
        covered, pieces = Fraction(0), []
        for other, face in reference:
            tick()
            if bounds[0] > other[2] or other[0] > bounds[2] or bounds[1] > other[3] or other[1] > bounds[3]:
                continue
            pair_tests += 1
            polygon = _overlap(query, face)
            for point in polygon:
                maximum = max(maximum, abs(_height(query, point)-_height(face, point)-offset))
                vertex_tests += 1
            area = _area(polygon)
            if area == 0:
                continue
            piece_bounds = (min(p[0] for p in polygon), min(p[1] for p in polygon),
                            max(p[0] for p in polygon), max(p[1] for p in polygon))
            for previous, previous_bounds in pieces:
                tick()
                piece_tests += 1
                if (piece_bounds[0] >= previous_bounds[2] or previous_bounds[0] >= piece_bounds[2] or
                        piece_bounds[1] >= previous_bounds[3] or previous_bounds[1] >= piece_bounds[3]):
                    continue
                # Positive overlap cannot cancel a missing region in the area sum.
                if _area(_overlap(polygon, previous)) != 0:
                    _refuse('Affine reference has overlapping positive-area coverage')
            pieces.append((polygon, piece_bounds))
            covered += area
        if covered != _area(query):
            _refuse('Affine reference does not completely cover a query triangle')
    try:
        upper = 0. if maximum == 0 else math.nextafter(float(maximum), math.inf)
    except OverflowError:
        _refuse('Affine height difference exceeds the finite reporting range')
    if not math.isfinite(upper):
        _refuse('Affine height difference exceeds the finite reporting range')
    return {'maximum_height_error_upper_m': upper, 'reference_triangles': len(reference),
        'query_triangles': len(queries), 'overlap_pair_tests': pair_tests, 'overlap_vertex_tests': vertex_tests,
        'reference_piece_pair_tests': piece_tests, 'work_count': work}
