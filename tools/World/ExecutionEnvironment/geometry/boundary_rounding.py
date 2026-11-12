"""Read-only binary64 encoding certificate for a simple mapped ring subdivision."""
import hashlib
import json
import math
from fractions import Fraction
from pathlib import Path

from ..app.contracts import ExecutionEnvironmentError
from .api import execution_identity as parent_identity


def execution_identity(repo_root, verify_native_tools):
    parent = parent_identity(repo_root, verify_native_tools)
    content = {'parent': parent, 'boundary_rounding_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    return {**parent, 'identity_sha256': hashlib.sha256(json.dumps(content, sort_keys=True, separators=(',', ':')).encode()).hexdigest()}


def _refuse(message):
    raise ExecutionEnvironmentError('boundary_rounding_invalid', message)


def _ring(value, capacity):
    if (not isinstance(value, list) or not 4 <= len(value) <= capacity+1 or value[0] != value[-1] or
            any(not isinstance(p, list) or len(p) != 2 or any(type(v) not in (int, float) for v in p) for p in value)):
        _refuse('Encoding needs bounded finite closed XY rings')
    result = []
    for point in value[:-1]:
        exact = []
        for coordinate in point:
            try:
                encoded = float(coordinate)
                if not math.isfinite(encoded) or Fraction(encoded) != Fraction(coordinate):
                    _refuse('Ring coordinates must be exactly representable finite binary64 values')
            except (OverflowError, ValueError):
                _refuse('Ring coordinate cannot be encoded as finite binary64')
            exact.append(Fraction(encoded))
        result.append(tuple(exact))
    if len(set(result)) != len(result):
        _refuse('Ring has repeated nonclosing vertices')
    return result


def _cross(a, b, c):
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])


def _area2(ring):
    origin = ring[0]
    return sum(_cross(origin, a, b) for a, b in zip(ring, (*ring[1:], ring[0])))


def _simple(ring, tick):
    for i,b in enumerate(ring):
        tick()
        a,c=ring[i-1],ring[(i+1)%len(ring)]
        if _cross(a,b,c)==0 and sum((b[k]-a[k])*(c[k]-b[k]) for k in (0,1))<=0:
            _refuse('Ring has adjacent backtracking or overlapping edges')
    edges = list(zip(ring, (*ring[1:], ring[0])))
    for i, (a, b) in enumerate(edges):
        for j in range(i+1, len(edges)):
            tick()
            if j == i+1 or i == 0 and j == len(edges)-1:
                continue
            c, d = edges[j]
            if any(max(a[k], b[k]) < min(c[k], d[k]) or max(c[k], d[k]) < min(a[k], b[k]) for k in (0, 1)):
                continue
            signs = (_cross(a, b, c), _cross(a, b, d), _cross(c, d, a), _cross(c, d, b))
            on = lambda p, x, y: all(min(x[k], y[k]) <= p[k] <= max(x[k], y[k]) for k in (0, 1))
            if (signs[0]*signs[1] < 0 and signs[2]*signs[3] < 0 or
                    signs[0] == 0 and on(c, a, b) or signs[1] == 0 and on(d, a, b) or
                    signs[2] == 0 and on(a, c, d) or signs[3] == 0 and on(b, c, d)):
                _refuse('Ring has a nonadjacent intersection or contact')


def _cell(coordinate):
    value = float(coordinate)
    before, after = math.nextafter(value, -math.inf), math.nextafter(value, math.inf)
    if not math.isfinite(before) or not math.isfinite(after):
        _refuse('Binary64 rounding cell is unbounded')
    return (Fraction(before)+coordinate)/2, (coordinate+Fraction(after))/2


def _interval(a, b, point):
    low, high, radius = Fraction(0), Fraction(1), Fraction(0)
    for axis in (0, 1):
        lower, upper = _cell(point[axis])
        radius += max(point[axis]-lower, upper-point[axis])
        delta = b[axis]-a[axis]
        if delta == 0:
            if not lower < a[axis] < upper:
                _refuse('Encoded node has no strict source-edge rounding witness')
            continue
        first, second = (lower-a[axis])/delta, (upper-a[axis])/delta
        low, high = max(low, min(first, second)), min(high, max(first, second))
    if not low < high:
        _refuse('Encoded node has no strict source-edge rounding witness; midpoint ties are unqualified')
    return low, high, radius


def _upper(value):
    try:
        result = float(value)
    except OverflowError:
        _refuse('Encoding envelope exceeds finite reporting range')
    if not math.isfinite(result):
        _refuse('Encoding envelope exceeds finite reporting range')
    if Fraction(result) < value:
        result = math.nextafter(result, math.inf)
    if not math.isfinite(result):
        _refuse('Upward encoding envelope exceeds finite reporting range')
    return result


def certify_ring_rounding(source_ring, encoded_ring, *, maximum_work=1_000_000):
    """Certify an ordered rounded subdivision, never exact source-set equality.

    Each added node must have a strict-interior binary64 rounding-cell witness
    on its original source edge. Witnesses are existential; they do not recover
    a producer's latent intersection parameters. No coordinate is modified.
    """
    if type(maximum_work) is not int or not 1 <= maximum_work <= 1_000_000:
        _refuse('Encoding proof needs a bounded work limit')
    source, encoded = _ring(source_ring, 256), _ring(encoded_ring, 4096)
    work = 0
    def tick():
        nonlocal work
        work += 1
        if work > maximum_work:
            _refuse('Encoding proof exceeds its work capacity')
    source_area, encoded_area = _area2(source), _area2(encoded)
    if source_area == 0 or source_area*encoded_area <= 0:
        _refuse('Source and encoded rings must have the same nondegenerate winding')
    _simple(source, tick)
    _simple(encoded, tick)
    originals = set(source)
    if not originals <= set(encoded):
        _refuse('Encoding omitted an original mapped vertex')
    first = encoded.index(source[0])
    order = list(range(first, len(encoded)))+list(range(first))
    rotated = [encoded[i] for i in order]
    rotated.append(rotated[0])
    order.append(order[0])
    cursor, radius, strip_area, off_edge, witnesses = 0, Fraction(0), Fraction(0), 0, []
    for edge, (a, b) in enumerate(zip(source, (*source[1:], source[0]))):
        if rotated[cursor] != a:
            _refuse('Original mapped vertices have changed cyclic order')
        previous, previous_high = a, Fraction(0)
        cursor += 1
        while rotated[cursor] != b:
            tick()
            point = rotated[cursor]
            if point in originals:
                _refuse('Original mapped vertices have changed cyclic order')
            low, high, cell_radius = _interval(a, b, point)
            if low < previous_high:
                _refuse('Encoded source-edge subdivision parameters are reordered')
            parameter = (low+high)/2
            ideal = [a[i]+parameter*(b[i]-a[i]) for i in (0, 1)]
            if any(Fraction(float(ideal[i])) != point[i] for i in (0, 1)):
                _refuse('Strict rounding witness does not encode the supplied node')
            witnesses.append({'source_edge_index': edge, 'encoded_vertex_index': order[cursor],
                'parameter_lower': str(low), 'parameter_upper': str(high), 'ideal_parameter': str(parameter)})
            off_edge += _cross(a, b, point) != 0
            radius = max(radius, cell_radius)
            strip_area += abs(_cross(a, previous, point))/2
            previous, previous_high = point, high
            cursor += 1
        strip_area += abs(_cross(a, previous, b))/2
    if cursor != len(encoded):
        _refuse('Encoded boundary has unmatched edge subdivisions')
    # The signed source/model cycle difference is a sum of these triangle
    # fans. Their absolute areas bound the symmetric difference of simple rings.
    return {'contract': 'strict_binary64_source_edge_subdivision_v1',
        'source_ring_sha256': hashlib.sha256(json.dumps(source_ring, separators=(',', ':')).encode()).hexdigest(),
        'encoded_ring_sha256': hashlib.sha256(json.dumps(encoded_ring, separators=(',', ':')).encode()).hexdigest(),
        'source_vertices': len(source), 'encoded_vertices': len(encoded), 'rounded_subdivision_vertices': len(witnesses),
        'vertices_off_exact_source_edges': off_edge, 'l1_hausdorff_upper_m': _upper(radius),
        'symmetric_difference_area_upper_m2': _upper(strip_area), 'work_count': work,
        'rounding_witnesses': witnesses}
