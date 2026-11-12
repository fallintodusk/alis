"""Unpublished original-strip continuation into one caller-qualified east grid."""
from copy import deepcopy
import math
from pathlib import Path

from World.ExecutionEnvironment.geometry.api import triangulate_points, validate_triangle_cover, execution_identity as geometry_identity
from World.ExecutionEnvironment.geometry.affine import measure_affine_cover, execution_identity as affine_identity
from World.ExecutionEnvironment.geometry.constrained import triangulate_constraints, execution_identity as constrained_identity
from ..app.contracts import CompilerError, canonical_hash, file_hash
from .app.geometry import _domain, _check_seams
from .app.road_elevation import _height
from .ground import _grid, _layout
from .ground_composite import _semantic

RULE = 'identified_original_strip_east_continuation_v1'
MAXIMUM_NODES = 512
MAXIMUM_FACES = 1024
MAXIMUM_QUADS = 16
NUMERIC_ERROR = 1e-8


def _refuse(message):
    raise CompilerError('ground_continuation_invalid', message)


def implementation_identity():
    owner = Path(__file__).resolve().parent
    paths = [Path(__file__), owner/'ground.py', owner/'ground_composite.py', owner/'app/geometry.py',
        owner/'app/road_elevation.py', owner.parent/'app/contracts.py']
    return canonical_hash({p.relative_to(owner.parent).as_posix(): file_hash(p) for p in paths})


def _execution(repo):
    return {'geometry': geometry_identity(repo, True)['identity_sha256'],
        'constrained': constrained_identity(repo, True)['identity_sha256'],
        'affine': affine_identity(repo, True)['identity_sha256']}


def _finite(value):
    if type(value) not in (int, float):
        return False
    try:
        return math.isfinite(value)
    except OverflowError:
        return False


def _positive_winding(quad):
    for a, b, c in _faces(quad):
        area = ((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))*.5
        if not _finite(area) or area <= 1e-12:
            _refuse('Incoming and emitted faces require positive noncollapsed native winding')


def _selection(value):
    if (not isinstance(value, dict) or not value or any(not isinstance(key, str) or not isinstance(rows, list) or
            not rows or any(not isinstance(row, list) or len(row) != 2 or any(type(v) is not int or v < 0 for v in row)
                for row in rows) for key, rows in value.items())):
        _refuse('Continuation requires explicit original quad coordinates')


def _decode(incoming, crs):
    if (not isinstance(incoming, dict) or set(incoming) != {'version', 'basis', 'canonical_crs', 'vertical_datum', 'surfaces'} or
            type(incoming['version']) is not int or incoming['version'] != 1 or incoming['basis'] != 'identified_model' or
            incoming['canonical_crs'] != crs or incoming['vertical_datum'] != 'EPSG:3855' or
            not isinstance(incoming['surfaces'], dict) or not incoming['surfaces']):
        _refuse('Incoming field requires one explicitly identified model in the canonical frame')
    quads, selection, nodes = {}, {}, {}
    total = 0
    for identifier, surface in incoming['surfaces'].items():
        if (not isinstance(identifier, str) or not isinstance(surface, dict) or set(surface) != {'quads'} or
                not isinstance(surface['quads'], list) or not surface['quads']):
            _refuse('Incoming surface layout is unsupported')
        selection[identifier] = []
        for quad in surface['quads']:
            if not isinstance(quad, dict) or set(quad) != {'coordinate', 'vertices', 'triangles', 'water_triangles'}:
                _refuse('Incoming original quad fields are unsupported')
            coordinate, vertices, faces, wet = (quad[k] for k in ('coordinate', 'vertices', 'triangles', 'water_triangles'))
            _selection({identifier: [coordinate]})
            key = identifier, *coordinate
            if key in quads or len(quads) >= MAXIMUM_QUADS-1:
                _refuse('Incoming strip repeats or exceeds its original quad capacity')
            if (not isinstance(vertices, list) or len(vertices) < 3 or len(vertices) > MAXIMUM_NODES or
                    any(not isinstance(p, list) or len(p) != 5 or any(not _finite(v) for v in p) or
                        any(not 0 <= v <= 1 for v in p[3:]) or abs(sum(p[3:])-1) > NUMERIC_ERROR for p in vertices) or
                    len({tuple(p[:2]) for p in vertices}) != len(vertices)):
                _refuse('Incoming vertices require unique finite XY and normalized original weights')
            if (not isinstance(faces, list) or not faces or any(not isinstance(row, list) or len(row) != 3 or
                    any(type(i) is not int or not 0 <= i < len(vertices) for i in row) or len(set(row)) != 3 for row in faces) or
                    not isinstance(wet, list) or any(type(i) is not int or not 0 <= i < len(faces) for i in wet) or len(set(wet)) != len(wet)):
                _refuse('Incoming face indices or declared wet roles are invalid')
            if {i for row in faces for i in row} != set(range(len(vertices))):
                _refuse('Incoming field contains unused vertices')
            _positive_winding(quad)
            total += len(faces)
            if total >= MAXIMUM_FACES:
                _refuse('Incoming field exceeds the finite complete-model face capacity')
            for p in vertices:
                xy = tuple(p[:2])
                if xy in nodes and nodes[xy] != p:
                    _refuse('Incoming quads disagree on shared XYZ or original weights')
                nodes[xy] = p[:]
            quads[key] = quad
            selection[identifier].append(coordinate[:])
    return quads, selection, nodes, total


def _faces(quad, field=2):
    return [[[quad['vertices'][i][0], quad['vertices'][i][1], quad['vertices'][i][field]] for i in row] for row in quad['triangles']]


def _polygon(bounds):
    w, s, e, n = bounds
    return {'type': 'Polygon', 'coordinates': [[[w, s], [e, s], [e, n], [w, n], [w, s]]]}


def _semantic_error(quad, corners):
    maximum = 0.
    for field in (3, 4):
        original = [[[corners[i][0], corners[i][1], corners[i][field]] for i in row] for row in ((0, 2, 1), (1, 2, 3))]
        maximum = max(maximum, measure_affine_cover(_faces(quad, field), original)['maximum_height_error_upper_m'])
    if maximum > NUMERIC_ERROR:
        _refuse('Continuation changes the complete original semantic field')
    return maximum


def _pack(coordinate, records, corners, retained):
    quad = {'coordinate': list(coordinate), 'vertices': [], 'triangles': [], 'water_triangles': []}
    lookup = {}
    for face, wet in records:
        row = []
        for xyz in face:
            xy = tuple(xyz[:2])
            if xy not in lookup:
                point = retained[xy][:] if xy in retained else [*xyz, *_semantic(corners, xyz[:2])]
                if point[:3] != xyz:
                    _refuse('Shared node assignment conflicts with the retained incoming field')
                lookup[xy] = len(quad['vertices'])
                quad['vertices'].append(point)
            row.append(lookup[xy])
        if wet:
            quad['water_triangles'].append(len(quad['triangles']))
        quad['triangles'].append(row)
    return quad


def build_ground_continuation(repo_root, cells, grid, evidence, selected, incoming, *, canonical_crs, maximum_triangles):
    """Return one conforming identified strip plus east neighbor; outer traces stay OPEN."""
    _grid(grid, evidence, canonical_crs)
    _selection(selected)
    if (type(maximum_triangles) is not int or not 2 <= maximum_triangles <= 4096 or
            sum(map(len, selected.values())) != 1 or grid['size'][0]*grid['size'][1] > 4096):
        _refuse('Continuation requires one east neighbor and a bounded grid/face capacity')
    quads, selection, retained, seed_faces = _decode(incoming, canonical_crs)
    for identifier in set(selection) | set(selected):
        if identifier not in cells or cells[identifier].get('raster_alignment', {}).get('canonical_crs') != canonical_crs:
            _refuse('Original regular-grid owners must use the same canonical frame')
    seed_domain, east_domain = _domain(cells, selection), _domain(cells, selected)
    east_bounds, east_owner = next(iter(east_domain.items()))
    w, s, e, n = east_bounds
    ordered = sorted(seed_domain)
    if (east_bounds in seed_domain or any(b[1] != s or b[3] != n for b in ordered) or ordered[-1][2] != w or
            any(a[2] != b[0] for a, b in zip(ordered, ordered[1:]))):
        _refuse('Incoming quads must form one contiguous E/W strip immediately west of the neighbor')
    domain = {**seed_domain, **east_domain}
    semantic_error = 0.
    for bounds, (identifier, x, y, corners) in seed_domain.items():
        quad = quads[identifier, x, y]
        if len(quad['triangles']) > maximum_triangles or any(not bounds[0] <= p[0] <= bounds[2] or
                not bounds[1] <= p[1] <= bounds[3] for p in quad['vertices']):
            _refuse('Incoming field exceeds original owner bounds or per-quad capacity')
        validate_triangle_cover(repo_root, _polygon(bounds), [[p[:2] for p in f] for f in _faces(quad)])
        semantic_error = max(semantic_error, _semantic_error(quad, corners))
    _check_seams(incoming['surfaces'], seed_domain)
    trace = sorted((p for p in retained.values() if p[0] == w), key=lambda p: p[1])
    if len(trace) < 2 or trace[0][1] != s or trace[-1][1] != n:
        _refuse('Incoming boundary PWA is incomplete')
    x0, y0, size = _layout(east_owner[3], grid)
    source_nodes = {(grid['origin'][0]+(x0+x)*grid['spacing'][0], grid['origin'][1]+(y0+y)*grid['spacing'][1]):
        grid['samples'][y0+y][x0+x] for y in range(size+1) for x in range(size+1)}
    if len(set(retained) | set(source_nodes)) > MAXIMUM_NODES:
        _refuse('Continuation exceeds its complete-model input node capacity')
    consumed = {'implementation_sha256': implementation_identity(), 'execution': _execution(repo_root),
        'incoming_sha256': canonical_hash(incoming), 'ground_evidence': deepcopy(evidence),
        'canonical_crs': canonical_crs, 'selected_sha256': canonical_hash(selected), 'maximum_triangles': maximum_triangles,
        'domain_sha256': canonical_hash({str(bounds): value for bounds, value in sorted(domain.items())})}

    def boundary(y):
        for point in trace:
            if y == point[1]:
                return point[:]
        for a, b in zip(trace, trace[1:]):
            if a[1] < y < b[1]:
                t = (y-a[1])/(b[1]-a[1])
                return [w, y, *[math.fsum(((1-t)*a[i], t*b[i])) for i in (2, 3, 4)]]
        _refuse('Boundary PWA cannot extrapolate unknown incoming values')

    points = sorted(set(source_nodes) | {tuple(p[:2]) for p in trace})
    reference_nodes = [boundary(xy[1])[:3] if xy[0] == w else [*xy, source_nodes[xy]] for xy in points]
    reference = [[reference_nodes[i] for i in row] for row in triangulate_points(repo_root, [list(p) for p in points])]
    segments = [[a[:2], b[:2]] for face in reference for a, b in zip(face, (*face[1:], face[0]))]
    segments.append([east_owner[3][1][:2], east_owner[3][2][:2]])
    mesh = triangulate_constraints(repo_root, _polygon(east_bounds), segments, [list(p) for p in points],
        min(maximum_triangles, MAXIMUM_FACES-seed_faces))
    east = [[[*xy, boundary(xy[1])[2] if xy[0] == w else source_nodes[tuple(xy)]
        if tuple(xy) in source_nodes else _height(reference, xy)] for xy in face] for face in mesh]
    shared_y = sorted({p[1] for face in east for p in face if p[0] == w})
    retained.update({(w, y): boundary(y) for y in shared_y})
    output = deepcopy(incoming['surfaces'])
    changed, maximum_incoming_error = [], 0.
    for bounds, (identifier, x, y, corners) in seed_domain.items():
        if bounds[2] != w:
            continue
        original = quads[identifier, x, y]
        records, added = [], 0
        for ordinal, face in enumerate(_faces(original)):
            wet = ordinal in original['water_triangles']
            edge = next(((a, b, face[(i+2) % 3]) for i, (a, b) in enumerate(zip(face, (*face[1:], face[0]))) if a[0] == b[0] == w), None)
            if edge is None:
                records.append((face, wet))
                continue
            a, b, opposite = edge
            inserted = sorted((z for z in shared_y if min(a[1], b[1]) < z < max(a[1], b[1])), reverse=b[1] < a[1])
            chain = [a, *[retained[w, z][:3] for z in inserted], b]
            records.extend(([u, v, opposite], wet) for u, v in zip(chain, chain[1:]))
            added += len(inserted)
        if added:
            quad = _pack([x, y], records, corners, retained)
            error = measure_affine_cover(_faces(quad), _faces(original))['maximum_height_error_upper_m']
            if error > NUMERIC_ERROR:
                _refuse('Neighbor noding changes the complete incoming PWA field')
            maximum_incoming_error = max(maximum_incoming_error, error)
            output[identifier]['quads'][output[identifier]['quads'].index(original)] = quad
            changed.append({'cell_id': identifier, 'coordinate': [x, y]})
    identifier, x, y, corners = east_owner
    neighbor = _pack([x, y], [(face, False) for face in east], corners, retained)
    output.setdefault(identifier, {'quads': []})['quads'].append(neighbor)
    if (sum(len(q['triangles']) for v in output.values() for q in v['quads']) > MAXIMUM_FACES or
            len({tuple(p[:2]) for v in output.values() for q in v['quads'] for p in q['vertices']}) > MAXIMUM_NODES):
        _refuse('Noded continuation exceeds its complete-model capacity')
    _check_seams(output, domain)
    reference_error = measure_affine_cover(_faces(neighbor), reference)['maximum_height_error_upper_m']
    if reference_error > NUMERIC_ERROR:
        _refuse('Final neighbor changes the frozen reference field')
    for bounds, (identifier, x, y, corners) in domain.items():
        quad = next(q for q in output[identifier]['quads'] if q['coordinate'] == [x, y])
        _positive_winding(quad)
        if len(quad['triangles']) > maximum_triangles:
            _refuse('Noded continuation exceeds per-quad capacity')
        semantic_error = max(semantic_error, _semantic_error(quad, corners))
    if consumed['implementation_sha256'] != implementation_identity() or consumed['execution'] != _execution(repo_root):
        _refuse('Consumed continuation implementation changed during construction')
    return {'construction': RULE, 'basis': 'identified_model', 'publication': 'unpublished_model_space',
        'boundary_condition': 'other_original_outer_traces_require_neighbor_continuation', 'surfaces': output,
        'consumed': consumed, 'metrics': {'shared_stations': len(shared_y),
            'rewritten_incoming_quads': changed, 'unchanged_incoming_quads': len(quads)-len(changed),
            'maximum_incoming_height_error_upper_m': maximum_incoming_error,
            'maximum_neighbor_reference_error_upper_m': reference_error,
            'maximum_original_semantic_field_error_upper': semantic_error,
            'maximum_boundary_source_node_departure_m': max(abs(boundary(xy[1])[2]-z) for xy, z in source_nodes.items() if xy[0] == w)}}
