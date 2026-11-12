"""Unpublished inner-to-provider model; its exterior requires source continuation."""
from copy import deepcopy
import math
from pathlib import Path

from World.ExecutionEnvironment.geometry.api import covered_points, polygon_union, validate_triangle_cover
from World.ExecutionEnvironment.geometry.affine import measure_affine_cover, execution_identity as affine_identity
from World.ExecutionEnvironment.geometry.constrained import execution_identity, triangulate_constraints
from ..app.contracts import CompilerError, canonical_hash, file_hash
from .app.geometry import _check_seams, _domain, _external_edges
from .app.road_elevation import _area, _finite, _height
from .ground import _grid, _layout, implementation_identity as ground_identity

RULE = 'identified_inner_to_provider_model_v1'


def _refuse(message):
    raise CompilerError('ground_composite_invalid', message)


def implementation_identity():
    owner = Path(__file__).resolve().parent
    return canonical_hash({'ground': ground_identity(), 'files': {
        name: file_hash(owner/name) for name in ('ground_composite.py', 'app/road_elevation.py')}})


def _provider(grid, xy):
    x = (xy[0]-grid['origin'][0])/grid['spacing'][0]
    y = (xy[1]-grid['origin'][1])/grid['spacing'][1]
    if not (0 <= x <= grid['size'][0]-1 and 0 <= y <= grid['size'][1]-1):
        _refuse('Provider interpolation requires four supported grid nodes; no extrapolation')
    col, row = min(grid['size'][0]-2, math.floor(x)), min(grid['size'][1]-2, math.floor(y))
    u, v = x-col, y-row
    s = grid['samples']
    return ((1-u)*s[row][col]+u*s[row][col+1])*(1-v)+((1-u)*s[row+1][col]+u*s[row+1][col+1])*v


def _nearest(point, segments):
    values = []
    for a, b in segments:
        dx, dy = b[0]-a[0], b[1]-a[1]
        denominator = dx*dx+dy*dy
        if denominator <= 0 or not math.isfinite(denominator):
            _refuse('Model boundary segment is collapsed or overflowing')
        t = max(0., min(1., ((point[0]-a[0])*dx+(point[1]-a[1])*dy)/denominator))
        projection = [a[i]+t*(b[i]-a[i]) for i in range(len(a))]
        values.append((math.dist(point, projection[:2]), tuple(sorted((tuple(a), tuple(b)))), projection))
    distance, _, projection = min(values)
    return distance, projection


def _inner(repo, inner, crs):
    if (not isinstance(inner, dict) or set(inner) != {'version', 'basis', 'canonical_crs', 'vertical_datum',
            'model_sha256', 'footprint', 'triangles'} or inner.get('version') != 1 or
            type(inner['version']) is not int or inner.get('basis') != 'identified_model' or
            inner.get('canonical_crs') != crs or inner.get('vertical_datum') != 'EPSG:3855' or
            not isinstance(inner.get('model_sha256'), str) or len(inner['model_sha256']) != 64 or
            any(c not in '0123456789abcdef' for c in inner['model_sha256'])):
        _refuse('Inner field requires an explicit identified model, qualified frame and model pin')
    faces = inner['triangles']
    if (not isinstance(faces, list) or not 1 <= len(faces) <= 1024 or any(
            not isinstance(f, list) or len(f) != 3 or any(not isinstance(p, list) or len(p) != 3 or
                any(not _finite(v) for v in p) for p in f) for f in faces) or
            any(_area(f) <= 1e-12 for f in faces)):
        _refuse('Inner field requires bounded noncollapsed finite XYZ faces')
    footprint = inner['footprint']
    if not isinstance(footprint, dict) or footprint.get('type') not in ('Polygon', 'MultiPolygon'):
        _refuse('Inner footprint must be a finite closed polygon')
    polygons = footprint.get('coordinates') if footprint['type'] == 'MultiPolygon' else [footprint.get('coordinates')]
    if not isinstance(polygons, list) or not polygons:
        _refuse('Inner footprint has no polygon components')
    rings = []
    for component in polygons:
        if not isinstance(component, list) or not component:
            _refuse('Inner footprint has no rings')
        for ring in component:
            if (not isinstance(ring, list) or len(ring) < 4 or ring[0] != ring[-1] or any(
                    not isinstance(p, list) or len(p) != 2 or any(not _finite(v) for v in p) for p in ring)):
                _refuse('Inner footprint requires finite closed XY rings')
            rings.append(ring)
    validate_triangle_cover(repo, inner['footprint'], [[p[:2] for p in f] for f in faces])
    for point in {tuple(p) for face in faces for p in face}:
        _height(faces, point[:2])
    boundaries = []
    for ring in rings:
        for a, b in zip(ring, ring[1:]):
            boundaries.append(([*a, _height(faces, a)], [*b, _height(faces, b)]))
    return faces, boundaries


def _semantic(points, xy):
    a, b, c, d = points
    for first, second in ((a, b), (a, c), (b, d), (c, d)):
        axis = 0 if first[1] == second[1] else 1
        if xy[1-axis] == first[1-axis]:
            start, end = sorted((first, second), key=lambda p: p[axis])
            t = (xy[axis]-start[axis])/(end[axis]-start[axis])
            if not 0 <= t <= 1:
                _refuse('Boundary station leaves its original edge')
            return [math.fsum(((1-t)*start[i], t*end[i])) for i in (3, 4)]
    u, v = (xy[0]-a[0])/(b[0]-a[0]), (a[1]-xy[1])/(a[1]-c[1])
    if not 0 <= u <= 1 or not 0 <= v <= 1:
        _refuse('Arrangement vertex leaves its original quad')
    half, weights = ((a, c, b), (1-(u+v), v, u)) if u+v <= 1 else ((b, c, d), (1-v, 1-u, u+v-1))
    result = [math.fsum(weight*p[i] for weight, p in zip(weights, half)) for i in (3, 4)]
    if any(not 0 <= value <= 1 for value in result) or abs(sum(result)-1) > 1e-8:
        _refuse('Original affine semantic field is invalid')
    return result


def _prove_inner_field(repo, inner_faces, outputs):
    reference = [[quad['vertices'][i][:3] for i in indices] for cell in outputs.values()
                 for quad in cell['quads'] for indices in quad['triangles']]
    result = measure_affine_cover(reference, inner_faces)
    if result['maximum_height_error_upper_m'] > 1e-8:
        _refuse('Final mesh changes the protected inner field between its input vertices')
    return {'maximum_inner_field_error_m': result['maximum_height_error_upper_m'],
        'inner_overlap_comparisons': result['overlap_pair_tests'],
        'inner_reference_piece_pair_tests': result['reference_piece_pair_tests']}


def build_ground_composite(repo_root, cells, grid, evidence, selected, inner, *, canonical_crs, maximum_triangles):
    """Return MODEL-SPACE final quads, never a loadable Terrain/admission document."""
    _grid(grid, evidence, canonical_crs)
    if (grid['size'][0]*grid['size'][1] > 4096 or type(maximum_triangles) is not int or
            not 2 <= maximum_triangles <= 4096 or not isinstance(selected, dict) or not selected or
            any(not isinstance(k, str) or not isinstance(v, (list, tuple)) or not v or
                any(not isinstance(q, (list, tuple)) or len(q) != 2 or any(type(i) is not int for i in q) for q in v)
                for k, v in selected.items())):
        _refuse('Composite requires a small complete grid, explicit quads and final-quad capacity')
    domain = _domain(cells, selected)
    if len(domain) > 64 or any(cells[k].get('raster_alignment', {}).get('canonical_crs') != canonical_crs for k in selected):
        _refuse('Composite requires a bounded selected domain in the provider frame')
    edges = _external_edges(domain)
    for _, _, _, quad in domain.values():
        _layout(quad, grid)
        if any(any(not _finite(v) or not 0 <= v <= 1 for v in p[3:]) or abs(sum(p[3:])-1) > 1e-8 for p in quad):
            _refuse('Original semantic weights are invalid')
    faces, inner_segments = _inner(repo_root, inner, canonical_crs)
    inner_nodes = sorted({tuple(p) for face in faces for p in face})
    if not inner_segments or any(_nearest(p[:2], edges)[0] == 0 for p in inner_nodes):
        _refuse('Protected inner field must be strictly interior to the provider-boundary model')
    quads = [[list(p) for p in item[3]] for item in domain.values()]
    polygons = [{'type': 'Polygon', 'coordinates': [[a[:2], c[:2], d[:2], b[:2], a[:2]]]} for a, b, c, d in quads]
    # The union is a topology operation; it grants no physical Ground support.
    west, south = min(k[0] for k in domain), min(k[1] for k in domain)
    east, north = max(k[2] for k in domain), max(k[3] for k in domain)
    envelope = {'type': 'Polygon', 'coordinates': [[[west, south], [east, south], [east, north], [west, north], [west, south]]]}
    region = polygon_union(repo_root, polygons, envelope)['geometry']
    segments = [[a[:2], b[:2]] for face in faces for a, b in zip(face, (*face[1:], face[0]))]
    stations = set()
    for quad in quads:
        a, b, c, d = quad
        segments.extend([[u[:2], v[:2]] for u, v in ((a, b), (b, d), (d, c), (c, a), (b, c))])
        x0, y0, n = _layout(quad, grid)
        stations.update((grid['origin'][0]+(x0+x)*grid['spacing'][0], grid['origin'][1]+(y0+y)*grid['spacing'][1])
                        for x in range(n+1) for y in range(n+1))
    mesh = triangulate_constraints(repo_root, region, segments, [list(p) for p in sorted(stations)], min(65536, len(domain)*maximum_triangles))
    nodes = sorted({tuple(p) for face in mesh for p in face})
    if not {p[:2] for p in inner_nodes} <= set(nodes):
        _refuse('Arrangement omitted protected inner vertices')
    membership = covered_points(repo_root, [list(p) for p in nodes], inner['footprint'])
    heights = {}
    for point, inside in zip(nodes, membership, strict=True):
        if inside:
            height = _height(faces, point)
        else:
            d_inner, anchor = _nearest(point, inner_segments)
            d_outer, _ = _nearest(point, edges)
            if d_inner+d_outer <= 0:
                _refuse('Inner and exterior boundary conditions conflict')
            height = _provider(grid, point)+d_outer/(d_inner+d_outer)*(_height(faces, anchor[:2])-_provider(grid, anchor[:2]))
        if not _finite(height):
            _refuse('Composite height is unknown or overflowing')
        heights[point] = height
    outputs, indices = {}, {}
    for bounds, (key, x, y, _) in domain.items():
        outputs.setdefault(key, {'quads': []})['quads'].append({'coordinate': [x, y], 'vertices': [], 'triangles': [], 'water_triangles': []})
        indices[bounds] = {}
    indexed = {bounds: next(q for q in outputs[key]['quads'] if q['coordinate'] == [x, y]) for bounds, (key, x, y, _) in domain.items()}
    for face in mesh:
        center = [math.fsum(p[i] for p in face)/3 for i in (0, 1)]
        parents = [bounds for bounds in domain if bounds[0] < center[0] < bounds[2] and bounds[1] < center[1] < bounds[3]]
        if len(parents) != 1:
            _refuse('Arrangement face has no unique original quad')
        bounds = parents[0]
        quad, lookup = indexed[bounds], indices[bounds]
        triangle = []
        for xy in face:
            key = tuple(xy)
            if key not in lookup:
                lookup[key] = len(quad['vertices'])
                quad['vertices'].append([*xy, heights[key], *_semantic(domain[bounds][3], xy)])
            triangle.append(lookup[key])
        quad['triangles'].append(triangle)
        if len(quad['triangles']) > maximum_triangles:
            _refuse('Composite exceeds per-quad triangle capacity')
    for bounds, quad in indexed.items():
        a, b, c, d = domain[bounds][3]
        validate_triangle_cover(repo_root, {'type': 'Polygon', 'coordinates': [[a[:2], c[:2], d[:2], b[:2], a[:2]]]},
                                [[quad['vertices'][i][:2] for i in t] for t in quad['triangles']])
    _check_seams(outputs, domain)
    metrics = _prove_inner_field(repo_root, faces, outputs)
    exterior = sorted([*point, height] for point, height in heights.items() if _nearest(point, edges)[0] == 0)
    if any(abs(p[2]-_provider(grid, p[:2])) > 1e-8 for p in exterior):
        _refuse('Provider boundary trace changed')
    return {'construction': RULE, 'basis': 'identified_model', 'publication': 'unpublished_model_space',
        'boundary_condition': 'provider_node_trace_requires_neighbor_continuation', 'surfaces': outputs,
        'metrics': metrics,
        'consumed': {'implementation_sha256': implementation_identity(), 'geometry_execution_identity': execution_identity(repo_root, True)['identity_sha256'],
            'affine_verification_identity': affine_identity(repo_root, True)['identity_sha256'],
            'ground_evidence': deepcopy(evidence), 'inner_sha256': canonical_hash(inner), 'canonical_crs': canonical_crs,
            'domain_sha256': canonical_hash({str(k): v for k, v in sorted(domain.items())}),
            'boundary_trace_sha256': canonical_hash(exterior), 'maximum_triangles': maximum_triangles}}
