"""Unpublished common north/south path field across one or two original quads."""
from collections import Counter
from copy import deepcopy
from fractions import Fraction as F
import math
from pathlib import Path

from World.ExecutionEnvironment.geometry.affine import measure_affine_cover, execution_identity as affine_identity
from World.ExecutionEnvironment.geometry.constrained import triangulate_constraints, execution_identity as geometry_identity
from ..app.contracts import CompilerError, canonical_hash, file_hash
from .ground_composite import _semantic

RULE = 'identified_straight_path_floor_source_controls_v1'


def _refuse(message):
    raise CompilerError('identified_path_floor_invalid', message)


def implementation_identity():
    owner = Path(__file__).resolve().parent
    return canonical_hash({name: file_hash(owner/name) for name in
        ('ground_path_floor.py', 'ground_composite.py', 'app/road_elevation.py', '../app/contracts.py')})


def _finite(value):
    try:
        return type(value) in (int, float) and math.isfinite(value)
    except OverflowError:
        return False


def _point(value, dimensions=2):
    return isinstance(value, list) and len(value) == dimensions and all(_finite(v) for v in value)


def _cross(a, b, c):
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])


def _signed_area(poly):
    return sum(a[0]*b[1]-a[1]*b[0] for a, b in zip(poly, (*poly[1:], poly[0])))/2 if len(poly)>2 else F(0)


def _clip(poly, a, b, sign=1):
    if not poly:
        return []
    result, previous = [], poly[-1]
    before = sign*_cross(a, b, previous)
    for point in poly:
        after = sign*_cross(a, b, point)
        if (after >= 0) != (before >= 0):
            t = before/(before-after)
            result.append([previous[k]+t*(point[k]-previous[k]) for k in (0, 1)])
        if after >= 0:
            result.append(point[:2])
        previous, before = point, after
    return result


def _overlap(subject, boundary):
    result = [p[:2] for p in subject]
    sign = 1 if _signed_area(boundary) > 0 else -1
    for a, b in zip(boundary, (*boundary[1:], boundary[0])):
        result = _clip(result, a, b, sign)
    return result


def _height(face, point):
    a, b, c = face
    determinant = _cross(a, b, c)
    u = ((point[0]-a[0])*(c[1]-a[1])-(point[1]-a[1])*(c[0]-a[0]))/determinant
    v = ((b[0]-a[0])*(point[1]-a[1])-(b[1]-a[1])*(point[0]-a[0]))/determinant
    return a[2]+u*(b[2]-a[2])+v*(c[2]-a[2])


def _upper(value):
    try:
        result = 0. if value == 0 else math.nextafter(float(value), math.inf)
    except OverflowError:
        _refuse('Model measurement exceeds finite reporting range')
    if not math.isfinite(result):
        _refuse('Model measurement exceeds finite reporting range')
    return result


def _reference(reference):
    if (not isinstance(reference, dict) or set(reference) != {'cell_id', 'coordinate', 'vertices'} or
        not isinstance(reference['cell_id'], str) or not reference['cell_id'] or
        not isinstance(reference['coordinate'], list) or len(reference['coordinate']) != 2 or
        any(type(i) is not int or i < 0 for i in reference['coordinate'])):
        _refuse('One explicit original cell/quad reference is required')
    corners = reference['vertices']
    if not isinstance(corners, list) or len(corners) != 4 or any(not _point(p, 5) for p in corners):
        _refuse('Original quad requires four finite XYZ/semantic corners')
    a, b, c, d = corners
    if (a[0] != c[0] or b[0] != d[0] or a[1] != b[1] or c[1] != d[1] or
        not 0 < b[0]-a[0] <= 100 or not 0 < a[1]-c[1] <= 100 or
        b[0]-a[0] != a[1]-c[1] or any(any(not 0 <= v <= 1 for v in p[3:]) or
        abs(sum(p[3:])-1) > 1e-8 for p in corners)):
        _refuse('Reference must be one complete bounded square with original convex semantics')
    return corners, [[F(v) for v in p[:2]] for p in (c, d, b, a)]


def _region(references):
    if not isinstance(references, list) or not 1 <= len(references) <= 2:
        _refuse('The common field requires one or two explicit original quad references')
    for reference in references:
        _reference(reference)
    references = sorted(deepcopy(references), key=lambda r: -r['vertices'][0][1])
    if len({(r['cell_id'], *r['coordinate']) for r in references}) != len(references):
        _refuse('Original quad owner coordinates must be unique')
    if len(references) == 2 and references[0]['vertices'][2:] != references[1]['vertices'][:2]:
        _refuse('Original north/south neighbors must share exact XYZ and semantic corners')
    corners = [*references[0]['vertices'][:2], *references[-1]['vertices'][2:]]
    rectangle = [[F(v) for v in p[:2]] for p in (corners[2], corners[3], corners[1], corners[0])]
    return references, corners, rectangle


def _reader(sample, evidence, canonical_crs):
    keys = {'basis', 'canonical_crs', 'vertical_datum', 'unit', 'source_sha256', 'processing_sha256', 'control_contract_sha256'}
    if (not callable(sample) or not isinstance(evidence, dict) or set(evidence) != keys or
        evidence['basis'] != 'canonical_point_controls' or not isinstance(canonical_crs, str) or
        not canonical_crs.startswith('EPSG:') or evidence['canonical_crs'] != canonical_crs or
        evidence['vertical_datum'] != 'EPSG:3855' or evidence['unit'] != 'm' or any(not isinstance(evidence[k], str) or
        len(evidence[k]) != 64 or any(c not in '0123456789abcdef' for c in evidence[k])
        for k in ('source_sha256', 'processing_sha256', 'control_contract_sha256'))):
        _refuse('Qualified canonical source controls need explicit metre frame and source/processing pins')
    observations = {}
    def query(point):
        key = tuple(point)
        if key not in observations:
            if len(observations) >= 8192:
                _refuse('Canonical source query census exceeds the finite model capacity')
            value = sample(list(point))
            if (not isinstance(value, dict) or set(value) != {'state', 'height_m'} or
                value['state'] not in ('known', 'unknown') or
                (value['state'] == 'known' and not _finite(value['height_m'])) or
                (value['state'] == 'unknown' and value['height_m'] is not None)):
                _refuse('Canonical source query is absent, unreadable or has unsupported state/height')
            observations[key] = deepcopy(value)
        return deepcopy(observations[key])
    return query, observations


def _rounded(value, direction):
    rounded = float(value)
    if (F(rounded)-value)*direction < 0:
        rounded = math.nextafter(rounded, math.inf*direction)
    return math.nextafter(rounded, math.inf*direction)


def _reference_height(spec, point):
    point = list(map(F, point))
    for face in spec['reference']:
        signs = [_cross(a, b, point) for a, b in zip(face, (*face[1:], face[0]))]
        if all(v >= 0 for v in signs) or all(v <= 0 for v in signs):
            return _height(face, point)
    _refuse('A model node leaves its original reference quad')


def _station(spec, point):
    return sum((F(point[i])-spec['axis_origin'][i])*spec['direction'][i] for i in (0, 1))


def _floor_height(spec, point):
    value, levels, heights = _station(spec, point), spec['levels'], spec['level_heights']
    if value <= levels[0]:
        return heights[0]
    if value >= levels[-1]:
        return heights[-1]
    for lo, hi, a, b in zip(levels, levels[1:], heights, heights[1:]):
        if lo <= value <= hi:
            return a+(b-a)*(value-lo)/(hi-lo)
    _refuse('The identified floor has no complete height branch')


def _inside(point, polygon):
    return all(_cross(a, b, list(map(F, point))) >= 0 for a, b in zip(polygon, (*polygon[1:], polygon[0])))


def _prepare(references, land_edge, axis, width, control_points, query, limits):
    references, corners, rectangle = _region(references)
    if (not isinstance(limits, dict) or set(limits) != {'maximum_grade', 'maximum_cross_slope', 'maximum_numeric_error_m'} or
        any(not _finite(v) for v in limits.values()) or not 0 < limits['maximum_grade'] <= .05 or
        not 0 < limits['maximum_cross_slope'] <= .02 or not 0 < limits['maximum_numeric_error_m'] <= 1e-8):
        _refuse('The bounded floor comparison requires explicit conservative numerical/grade/cross limits')
    if (any(not isinstance(line, list) or len(line) != 2 or any(not _point(p) for p in line) or line[0] == line[1]
            for line in (land_edge, axis)) or not _finite(width) or not 0 < width < float(rectangle[1][0]-rectangle[0][0]) or
        not isinstance(control_points, list) or not 1 <= len(control_points) <= 1024 or any(not _point(p) for p in control_points)):
        _refuse('The model needs a finite straight source axis, Land edge, width and bounded explicit controls')
    w, s = rectangle[0]; e, n = rectangle[2]
    a, b = [[F(v) for v in p] for p in land_edge]
    if not min(a[1], b[1]) <= s < n <= max(a[1], b[1]):
        _refuse('This finite Land edge must span the original south/north boundary')
    x_at = lambda y: a[0]+(b[0]-a[0])*(y-a[1])/(b[1]-a[1])
    if not all(w < x_at(y) < e for y in (s, n)):
        _refuse('The original Land edge must cross the interior of this bounded quad')
    sign = 1 if _cross(a, b, rectangle[0]) > 0 else -1
    cut = [[_rounded(x_at(y), -1), float(y)] for y in (s, n)]
    ca, cb = [list(map(F, p)) for p in cut]
    cut_sign = 1 if _cross(ca, cb, rectangle[0]) > 0 else -1
    delta = [axis[1][i]-axis[0][i] for i in (0, 1)]; length = math.hypot(*delta)
    if not all(_finite(v) for v in delta) or not _finite(length) or length <= 0:
        _refuse('The source axis has nonfinite or collapsed arithmetic')
    direction = [v/length for v in delta]; normal = [-direction[1], direction[0]]
    strip = [[F(p[i]+side*width*.5*normal[i]) for i in (0, 1)]
             for p, side in ((axis[0], 1), (axis[1], 1), (axis[1], -1), (axis[0], -1))]
    nominal = _overlap(strip, rectangle)
    if len(nominal) != 4:
        _refuse('This optional model requires a complete straight strip crossing the quad south/north')
    ends = []
    for y in (s, n):
        xs = sorted(p[0] for p in nominal if p[1] == y)
        if len(xs) != 2:
            _refuse('The complete nominal strip does not meet both original quad edges')
        ends.append([_rounded(xs[0], -1), _rounded(xs[1], 1)])
    floor = [[F(ends[0][0]), s], [F(ends[0][1]), s], [F(ends[1][1]), n], [F(ends[1][0]), n]]
    if (abs(_signed_area(_overlap(nominal, floor))) != abs(_signed_area(nominal)) or
        any(sign*_cross(a, b, p) <= 0 for p in floor)):
        _refuse('The encoded floor loses original width or conflicts with the original Land/reference interface')
    spec = {'rectangle': rectangle, 'corners': corners, 'references': references,
        'reference': [[[F(v) for v in reference['vertices'][i][:3]] for i in face]
        for reference in references for face in ((0, 2, 1), (1, 2, 3))], 'land_a': a, 'land_b': b, 'land_sign': sign, 'cut': cut,
        'cut_a': ca, 'cut_b': cb, 'cut_sign': cut_sign, 'nominal_floor': nominal, 'floor': floor,
        'axis_origin': list(map(F, axis[0])), 'direction': list(map(F, direction))}
    float_station = lambda p: sum((p[i]-axis[0][i])*direction[i] for i in (0, 1))
    low, high = min(float_station(list(map(float, p))) for p in nominal), max(float_station(list(map(float, p))) for p in nominal)
    distances = [low, *[i*3. for i in range(math.floor(low/3)+1, math.ceil(high/3))], high]
    if not 2 <= len(distances) <= 64 or any(x >= y for x, y in zip(distances, distances[1:])):
        _refuse('The identified source-axis control census is excessive or unordered')
    rows, sides = [], []
    for distance in distances:
        point = [axis[0][i]+direction[i]*distance for i in (0, 1)]; source = query(point)
        if source['state'] != 'known':
            _refuse('Every defining source-axis control must be known before floor construction')
        rows.append({'station_m': distance, 'xy': point, **source})
        for offset in (-width*.5, width*.5):
            p = [point[i]+normal[i]*offset for i in (0, 1)]
            sides.append({'station_m': distance, 'offset_m': offset, 'xy': p, **query(p)})
    spec['levels'] = [min(_station(spec, p) for p in floor), *[F(row['station_m']) for row in rows[1:-1]], max(_station(spec, p) for p in floor)]
    spec['level_heights'] = [F(row['height_m']) for row in rows]
    if any(abs(y['height_m']-x['height_m'])/(y['station_m']-x['station_m']) > limits['maximum_grade'] for x, y in zip(rows, rows[1:])):
        _refuse('Source-axis controls are infeasible under the predeclared floor grade')
    lines = []
    for level in spec['levels'][1:-1]:
        intersections = []
        for x, y in zip(rectangle, (*rectangle[1:], rectangle[0])):
            vx, vy = _station(spec, x)-level, _station(spec, y)-level
            if vx == 0:
                intersections.append(x)
            if vx*vy < 0:
                t = vx/(vx-vy); intersections.append([x[k]+t*(y[k]-x[k]) for k in (0, 1)])
        intersections = list(dict.fromkeys(tuple(p) for p in intersections))
        if len(intersections) != 2:
            _refuse('A defining floor crease does not span the complete original quad')
        lines.append([[float(v) for v in p] for p in intersections])
    spec.update(axis_rows=rows, side_rows=sides, floor_lines=lines)
    return spec


def _assign_heights(mesh, spec, query):
    reference_nodes = {tuple(p) for face in mesh if abs(_signed_area(_clip([[F(v) for v in p] for p in face],
        spec['land_a'], spec['land_b'], -spec['land_sign']))) > 0 for p in face}
    floor_nodes = {tuple(p) for face in mesh if _inside([sum(F(p[k]) for p in face)/3 for k in (0, 1)], spec['floor']) for p in face}
    if floor_nodes & reference_nodes:
        _refuse('Protected floor and original exterior reference have conflicting node ownership')
    for face in mesh:
        if abs(_signed_area(_overlap([[F(v) for v in p] for p in face], spec['nominal_floor']))) > 0 and any(tuple(p) not in floor_nodes for p in face):
            _refuse('Canonical-restored bank faces intrude the complete nominal floor')
    rows = []
    for point in sorted({tuple(p) for face in mesh for p in face}):
        source = query(point)
        if point in floor_nodes:
            z, branch = float(_floor_height(spec, point)), 'identified_original_width_path_floor'
        elif point in reference_nodes or spec['cut_sign']*_cross(spec['cut_a'], spec['cut_b'], list(map(F, point))) <= 0:
            z, branch = float(_reference_height(spec, point)), 'retained_reference_outside_or_cut'
        elif source['state'] == 'known':
            z, branch = source['height_m'], 'strict_source_control'
        else:
            z, branch = float(_reference_height(spec, point)), 'identified_reference_at_UNKNOWN_control'
        rows.append({'xy': list(point), 'z': z, 'branch': branch, 'source': source})
    return rows


def _prove_topology(faces, corners, reference):
    directed, edges, heights = Counter(), Counter(), {}
    for face in faces:
        if _cross(*[[F(v) for v in p] for p in face]) <= 0:
            _refuse('Final topology contains a collapsed or reversed face')
        for p in face:
            xy = tuple(p[:2])
            if xy in heights and heights[xy] != p[2]:
                _refuse('Final shared height continuity is conflicting')
            heights[xy] = p[2]
        for a, b in zip(face, (*face[1:], face[0])):
            edge = tuple(a[:2]), tuple(b[:2]); directed[edge] += 1; edges[tuple(sorted(edge))] += 1
    if any(n != 1 for n in directed.values()) or any(n not in (1, 2) for n in edges.values()) or any(
        n == 2 and (directed[(a, b)] != 1 or directed[(b, a)] != 1) for (a, b), n in edges.items()):
        _refuse('Final topology has duplicate, nonmanifold or unpaired shared edges')
    w, s, e, n = map(F, (corners[0][0], corners[2][1], corners[1][0], corners[0][1]))
    boundary = [edge for edge, count in edges.items() if count == 1]
    if any(not any(a[k] == b[k] == value for k, value in ((0, w), (0, e), (1, s), (1, n))) for a, b in boundary):
        _refuse('Final topology has an unclosed interior boundary')
    measure_affine_cover(reference, faces)
    cover = measure_affine_cover(faces, reference)
    return {'vertices': len(heights), 'faces': len(faces), 'boundary_edges': len(boundary),
        'shared_edges': sum(count == 2 for count in edges.values()), 'whole_cover': cover}


def _scalar_clip(poly, spec, level, sign):
    if not poly:
        return []
    result, previous = [], poly[-1]; before = sign*(_station(spec, previous)-level)
    for point in poly:
        after = sign*(_station(spec, point)-level)
        if (after >= 0) != (before >= 0):
            t = before/(before-after); result.append([previous[k]+t*(point[k]-previous[k]) for k in (0, 1)])
        if after >= 0:
            result.append(point)
        previous, before = point, after
    return result


def _prove_floor(faces, spec, limits):
    if 'nominal_floor_rational' in spec:
        spec = {'nominal_floor': [[F(int(v[0]), int(v[1])) for v in p] for p in spec['nominal_floor_rational']],
            'axis_origin': list(map(F, spec['axis_origin'])), 'direction': list(map(F, spec['direction'])),
            'levels': [F(int(v[0]), int(v[1])) for v in spec['levels_rational']],
            'level_heights': list(map(F, spec['level_heights']))}
    area, error, grade, cross = F(0), F(0), F(0), F(0)
    tx, ty = spec['direction']
    for face in faces:
        f = [[F(v) for v in p] for p in face]; piece = _overlap([p[:2] for p in f], spec['nominal_floor'])
        if abs(_signed_area(piece)) == 0:
            continue
        area += abs(_signed_area(piece)); a, b, c = f; den = _cross(a, b, c)
        gx = ((b[2]-a[2])*(c[1]-a[1])-(c[2]-a[2])*(b[1]-a[1]))/den
        gy = ((b[0]-a[0])*(c[2]-a[2])-(c[0]-a[0])*(b[2]-a[2]))/den
        grade, cross = max(grade, abs(gx*tx+gy*ty)), max(cross, abs(-gx*ty+gy*tx))
        for low, high in zip(spec['levels'], spec['levels'][1:]):
            part = _scalar_clip(_scalar_clip(piece, spec, low, 1), spec, high, -1)
            for p in part:
                error = max(error, abs(_height(f, p)-_floor_height(spec, p)))
    if (area != abs(_signed_area(spec['nominal_floor'])) or error > F(limits['maximum_numeric_error_m']) or
        grade > F(limits['maximum_grade']) or cross > F(limits['maximum_cross_slope'])):
        _refuse('Protected floor changes its complete field, width or predeclared slope limits')
    return {'protected_floor_area_m2': float(area), 'protected_floor_whole_field_error_m': _upper(error),
        'protected_floor_whole_field_grade': _upper(grade), 'protected_floor_whole_field_cross_slope': _upper(cross)}


def _prove_reference(faces, spec, limit):
    area, error = F(0), F(0)
    for face in faces:
        f = [[F(v) for v in p] for p in face]
        outside = _clip([p[:2] for p in f], spec['land_a'], spec['land_b'], -spec['land_sign'])
        area += abs(_signed_area(outside))
        for reference in spec['reference']:
            for p in _overlap(outside, reference):
                error = max(error, abs(_height(f, p)-_height(reference, p)))
    expected = abs(_signed_area(_clip(spec['rectangle'], spec['land_a'], spec['land_b'], -spec['land_sign'])))
    if area != expected or error > F(limit):
        _refuse('The complete original non-Land reference field changed beyond its numerical contract')
    return {'whole_original_nonLand_reference_error_m': _upper(error),
        'exact_reference_equality': 'equal' if error == 0 else 'REFUSED_nonzero_binary64_encoding'}


def _split_original_quads(faces, references, maximum_triangles):
    surfaces, quads, owned = {}, [], 0
    for reference in references:
        a, b, c, d = reference['vertices']
        quad = {'coordinate': deepcopy(reference['coordinate']), 'vertices': [], 'triangles': [], 'water_triangles': []}
        indices = {}
        for face in faces:
            center = [sum(F(p[k]) for p in face)/3 for k in (0, 1)]
            if not (F(a[0]) < center[0] < F(b[0]) and F(c[1]) < center[1] < F(a[1])):
                continue
            if any(not (a[0] <= p[0] <= b[0] and c[1] <= p[1] <= a[1]) for p in face):
                _refuse('A common-field face crosses its original quad owner')
            triangle = []
            for point in face:
                key = tuple(point[:2])
                if key not in indices:
                    indices[key] = len(quad['vertices'])
                    quad['vertices'].append([*point, *_semantic(reference['vertices'], point[:2])])
                triangle.append(indices[key])
            quad['triangles'].append(triangle)
        if not 1 <= len(quad['triangles']) <= maximum_triangles:
            _refuse('An original quad exceeds its declared final triangle capacity or is empty')
        owned += len(quad['triangles']); quads.append(quad)
        surfaces.setdefault(reference['cell_id'], {'quads': []})['quads'].append(quad)
    if owned != len(faces):
        _refuse('Common-field faces lost or duplicated original quad ownership')
    if len(quads) == 2:
        y = references[0]['vertices'][2][1]
        traces = [sorted(p for p in quad['vertices'] if p[1] == y) for quad in quads]
        if len(traces[0]) < 2 or traces[0] != traces[1]:
            _refuse('Original quad split has conflicting shared XYZ or semantic traces')
    return surfaces


def build_identified_path_floor(repo_root, references, land_edge, axis, width_m, control_points, sample, evidence,
        limits, *, canonical_crs, maximum_triangles):
    """Construct the west-Land/north-south-strip case; admission and exterior continuation stay separate."""
    if type(maximum_triangles) is not int or not 2 <= maximum_triangles <= 4096:
        _refuse('An explicit bounded final-quad triangle capacity is required')
    before = {'implementation': implementation_identity(), 'geometry': geometry_identity(repo_root, True),
        'affine': affine_identity(repo_root, True)}
    query, queried = _reader(sample, evidence, canonical_crs)
    spec = _prepare(references, land_edge, axis, width_m, control_points, query, limits)
    region = {'type': 'Polygon', 'coordinates': [[*[list(map(float, p)) for p in spec['rectangle']], list(map(float, spec['rectangle'][0]))]]}
    ring = [[float(v) for v in p] for p in spec['floor']]
    segments = [spec['cut'], *[[reference['vertices'][1][:2], reference['vertices'][2][:2]] for reference in spec['references']],
        *[[reference['vertices'][2][:2], reference['vertices'][3][:2]] for reference in spec['references'][:-1]], *spec['floor_lines'],
        *[[a, b] for a, b in zip(ring, (*ring[1:], ring[0]))]]
    mesh = triangulate_constraints(repo_root, region, segments, control_points, maximum_triangles*len(spec['references']))
    rows = _assign_heights(mesh, spec, query)
    heights = {tuple(row['xy']): row['z'] for row in rows}
    faces = [[[ *point, heights[tuple(point)]] for point in face] for face in mesh]
    reference_faces = [[[float(v) for v in p] for p in face] for face in spec['reference']]
    metrics = _prove_topology(faces, spec['corners'], reference_faces)
    metrics.update(_prove_floor(faces, spec, limits), **_prove_reference(faces, spec, limits['maximum_numeric_error_m']))
    surfaces = _split_original_quads(faces, spec['references'], maximum_triangles)
    sides = []
    exact_faces = [([[F(v) for v in p] for p in face],
        [min(p[0] for p in face), min(p[1] for p in face), max(p[0] for p in face), max(p[1] for p in face)]) for face in faces]
    for row in spec['side_rows']:
        point = list(map(F, row['xy'])); matches = []
        for f, bounds in exact_faces:
            if not bounds[0] <= row['xy'][0] <= bounds[2] or not bounds[1] <= row['xy'][1] <= bounds[3]:
                continue
            if _inside(point, [p[:2] for p in f]):
                matches.append(_height(f, point))
        if matches and any(value != matches[0] for value in matches):
            _refuse('A retained side observation has conflicting final field heights')
        actual = float(matches[0]) if matches else None
        sides.append({**deepcopy(row), 'model_state': 'inside_domain' if matches else 'outside_domain',
            'model_height_m': actual, 'signed_departure_m': None if actual is None or row['state'] != 'known' else actual-row['height_m']})
    departures = [abs(row['signed_departure_m']) for row in sides if row['signed_departure_m'] is not None]
    metrics.update(retained_side_inside_count=sum(row['model_state'] == 'inside_domain' for row in sides),
        retained_side_outside_count=sum(row['model_state'] == 'outside_domain' for row in sides),
        maximum_retained_inside_side_departure_m=max(departures) if departures else None)
    w, s = spec['rectangle'][0]; e, n = spec['rectangle'][2]
    exterior = sorted([*row['xy'], row['z']] for row in rows if row['xy'][0] in (w, e) or row['xy'][1] in (s, n))
    current = {'implementation': implementation_identity(), 'geometry': geometry_identity(repo_root, True),
        'affine': affine_identity(repo_root, True)}
    if current != before:
        _refuse('Consumed implementation or native geometry changed during model construction')
    records = [[*point, value] for point, value in sorted(queried.items())]
    return {'construction': RULE, 'basis': 'identified_model', 'publication': 'unpublished_model_space',
        'boundary_condition': 'identified_source_control_trace_requires_neighbor_continuation',
        'surfaces': surfaces, 'whole_ground_triangles': faces, 'nodes': rows,
        'axis_observations': deepcopy(spec['axis_rows']), 'side_observations': sides,
        'floor_specification': {'nominal_floor_rational': [[[str(v.numerator), str(v.denominator)] for v in p] for p in spec['nominal_floor']],
            'axis_origin': [float(v) for v in spec['axis_origin']], 'direction': [float(v) for v in spec['direction']],
            'levels_rational': [[str(v.numerator), str(v.denominator)] for v in spec['levels']],
            'level_heights': [float(v) for v in spec['level_heights']]},
        'metrics': metrics, 'consumed': {'implementation_sha256': before['implementation'],
            'geometry_execution_identity': before['geometry']['identity_sha256'], 'affine_verifier_identity': before['affine']['identity_sha256'],
            'source_evidence': deepcopy(evidence), 'source_queries_sha256': canonical_hash(records), 'source_query_count': len(records),
            'boundary_trace_sha256': canonical_hash(exterior), 'input_sha256': canonical_hash({'references': spec['references'], 'land_edge': land_edge,
                'axis': axis, 'width_m': width_m, 'control_points': control_points, 'limits': limits}), 'maximum_triangles': maximum_triangles}}
