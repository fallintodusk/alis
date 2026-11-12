"""Unpublished joint static basin/PWA model; never an observed Water function."""
from collections import Counter, defaultdict
from copy import deepcopy
from fractions import Fraction
import heapq
import math
from pathlib import Path

from World.ExecutionEnvironment.geometry.api import covered_points
from World.ExecutionEnvironment.geometry.affine import measure_affine_cover, execution_identity as affine_identity
from World.ExecutionEnvironment.geometry.boundary_rounding import certify_ring_rounding, execution_identity as rounding_identity
from World.ExecutionEnvironment.geometry.constrained import triangulate_constraints, execution_identity as geometry_identity
from ..app.contracts import CompilerError, canonical_hash, file_hash
from .app.geometry import _domain, _external_edges, _check_seams
from .app.road_elevation import _area, _finite
from .ground import _grid, _layout, implementation_identity as ground_identity
from .ground_composite import _semantic

RULE = 'identified_static_basin_pwa_v1'
REPRESENTATION = 'strict_binary64_source_edge_subdivision_v1'


def _refuse(message):
    raise CompilerError('identified_basin_invalid', message)


def implementation_identity():
    owner = Path(__file__).resolve().parent
    return canonical_hash({'ground': ground_identity(), 'files': {
        name: file_hash(owner/name) for name in ('identified_basin.py', 'ground_composite.py', 'app/road_elevation.py')}})


def _cross(a, b, c):
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])


def _convex(ring):
    # Ring encoding is shared; strict convexity belongs to this basin model.
    certify_ring_rounding(ring, ring)
    exact = [tuple(Fraction(v) for v in point) for point in ring[:-1]]
    turns = [_cross(exact[i-1], point, exact[(i+1) % len(exact)]) for i, point in enumerate(exact)]
    if not (all(turn > 0 for turn in turns) or all(turn < 0 for turn in turns)):
        _refuse('Identified basin contours must be strictly convex')
    if turns[0] < 0:
        exact.reverse()
    return exact


def _nested(inner, outer):
    return all(_cross(a, b, point) > 0 for a, b in zip(outer, (*outer[1:], outer[0])) for point in inner)


def _reference(grid, xy):
    u, v = [(xy[i]-grid['origin'][i])/grid['spacing'][i] for i in (0, 1)]
    nx, ny = grid['size']
    if not 0 <= u <= nx-1 or not 0 <= v <= ny-1:
        _refuse('Reference field does not cover the complete selected model')
    x, y = min(nx-2, math.floor(u)), min(ny-2, math.floor(v))
    a, b = grid['samples'][y][x:x+2]
    c, d = grid['samples'][y+1][x:x+2]
    fx, fy = u-x, v-y
    return a+fx*(b-a)+fy*(c-a) if fx+fy <= 1 else d+(1-fx)*(c-d)+(1-fy)*(b-d)


def _distance(xy, ring):
    distances = []
    for a, b in zip(ring, ring[1:]):
        dx, dy = b[0]-a[0], b[1]-a[1]
        t = max(0., min(1., ((xy[0]-a[0])*dx+(xy[1]-a[1])*dy)/(dx*dx+dy*dy)))
        distances.append(math.hypot(xy[0]-a[0]-t*dx, xy[1]-a[1]-t*dy))
    return min(distances)


def _boundary(mesh, indices):
    directed, counts = Counter(), Counter()
    for index in indices:
        face = mesh[index]
        if len(face) != 3 or _cross(*face) <= 0:
            _refuse('Model has a collapsed or reversed face')
        for a, b in zip(face, (*face[1:], face[0])):
            edge = tuple(a[:2]), tuple(b[:2])
            directed[edge] += 1
            counts[tuple(sorted(edge))] += 1
    if any(n not in (1, 2) for n in counts.values()) or any(n != 1 for n in directed.values()):
        _refuse('Model has duplicate/nonmanifold or equally oriented incident faces')
    edges = [edge for edge in directed if counts[tuple(sorted(edge))] == 1]
    links = dict(edges)
    if not edges or len(links) != len(edges) or len(set(links.values())) != len(edges):
        _refuse('Model boundary has missing or point-only connectivity')
    start, current, ring = min(links), min(links), []
    while not ring or current != start:
        if current in ring or current not in links:
            _refuse('Model boundary has an open or branching loop')
        ring.append(current)
        current = links[current]
    if len(ring) != len(links):
        _refuse('Model has holes or disconnected components')
    return [list(point) for point in (*ring, start)]


def _containment(faces, wet_indices, level, outer_ring, minimum_margin):
    heights, adjacency = {}, defaultdict(set)
    for face in faces:
        for point in face:
            xy = tuple(point[:2])
            if not _finite(point[2]) or xy in heights and heights[xy] != point[2]:
                _refuse('Model has unknown or conflicting shared heights')
            heights[xy] = point[2]
        for a, b in zip(face, (*face[1:], face[0])):
            adjacency[tuple(a[:2])].add(tuple(b[:2]))
            adjacency[tuple(b[:2])].add(tuple(a[:2]))
    wet_nodes = {tuple(p[:2]) for i in wet_indices for p in faces[i]}
    if not wet_nodes or any(heights[p] > level for p in wet_nodes) or any(
            all(p[2] == level for p in faces[i]) for i in wet_indices):
        _refuse('Every wet face needs nonnegative node depths and positive interior depth')
    seed = min(wet_nodes, key=lambda p: (heights[p], p))
    outer = set(map(tuple, outer_ring[:-1]))
    costs, queue, spill = {seed: heights[seed]}, [(heights[seed], seed)], None
    while queue:
        value, node = heapq.heappop(queue)
        if value != costs[node]:
            continue
        if node in outer:
            spill = value
            break
        for other in adjacency[node]:
            proposed = max(value, heights[other])
            if proposed < costs.get(other, math.inf):
                costs[other] = proposed
                heapq.heappush(queue, (proposed, other))
    if spill is None or not spill > level or spill-level < minimum_margin:
        _refuse('Modeled wet component reaches its spill or lacks the explicit retention margin')
    connected, pending = {seed}, [seed]
    while pending:
        for other in adjacency[pending.pop()]:
            if heights[other] <= level and other not in connected:
                connected.add(other)
                pending.append(other)
    if connected != wet_nodes:
        _refuse('Modeled wet component is truncated, expanded or disconnected')
    return spill


def _gradient(face):
    a, b, c = face
    determinant = _cross(a, b, c)
    return math.hypot(((b[2]-a[2])*(c[1]-a[1])-(c[2]-a[2])*(b[1]-a[1]))/determinant,
                      ((b[0]-a[0])*(c[2]-a[2])-(c[0]-a[0])*(b[2]-a[2]))/determinant)


def _prove_exterior(reference, faces, influence):
    proof = measure_affine_cover(reference, [face for i, face in enumerate(faces) if i not in influence])
    if proof['maximum_height_error_upper_m'] > 1e-8:
        _refuse('Complete outside-influence field differs from the identified provider reference')
    return proof


def _specification(water, model, canonical_crs):
    attrs = water.get('attributes', {})
    geometry = water.get('geometry', {})
    polygons = geometry.get('coordinates') if geometry.get('type') == 'MultiPolygon' else [geometry.get('coordinates')]
    if (water.get('feature_class') != 'water' or attrs.get('surface_geometry') != 'polygon' or
            attrs.get('surface_behavior') != 'standing' or geometry.get('type') not in ('Polygon', 'MultiPolygon') or
            not isinstance(polygons, list) or len(polygons) != 1 or not isinstance(polygons[0], list) or len(polygons[0]) != 1):
        _refuse('Identified model requires one complete standing polygon without holes')
    if (not isinstance(model, dict) or model.get('version') != 1 or type(model.get('version')) is not int or
            model.get('basis') != 'identified_static_basin' or model.get('canonical_crs') != canonical_crs or
            model.get('vertical_datum') != 'EPSG:3855' or model.get('representation') != REPRESENTATION or
            model.get('source_geometry_sha256') != canonical_hash(geometry) or model.get('role') != 'inferred_constructed_basin'):
        _refuse('Model basis, frame, mapped geometry or explicit representation is missing or stale')
    role = model.get('role_evidence')
    if (not isinstance(role, dict) or role.get('basis') != 'primary_project_association_not_survey' or
            not isinstance(role.get('references'), list) or not role['references'] or
            any(not isinstance(value, str) or not value.startswith('https://') for value in role['references'])):
        _refuse('Identified role needs separate primary-project association evidence, not invented source tags')
    support = model.get('support')
    if support != {'mode': 'inferred_solid_plaza', 'known_canonical_under_routes': [],
            'unmapped_under_routes': 'unknown', 'basis': 'explicit_model_assumption_not_observed_ground'}:
        _refuse('Only an explicit solid-plaza assumption with no known conflicting canonical route is supported')
    parameters = model.get('parameters')
    names = {'level_m', 'depth_m', 'rim_freeboard_m', 'inner_bank_width_m', 'rim_outset_m',
             'transition_extent_m', 'maximum_gradient', 'minimum_spill_margin_m'}
    if (not isinstance(parameters, dict) or set(parameters) != names or any(not _finite(v) for v in parameters.values()) or
            any(parameters[k] <= 0 for k in names-{'level_m'}) or
            parameters['transition_extent_m'] <= parameters['rim_outset_m']):
        _refuse('Model dimensions, stage and quality screens must be explicit, finite and physically ordered')
    contours = model.get('contours')
    if not isinstance(contours, dict) or set(contours) != {'core', 'rim', 'influence'}:
        _refuse('Explicit finite core, rim and influence contours are required')
    rings = {'wet': polygons[0][0], **contours}
    exact = {key: _convex(ring) for key, ring in rings.items()}
    if any(not _nested(exact[a], exact[b]) for a, b in (('core', 'wet'), ('wet', 'rim'), ('rim', 'influence'))):
        _refuse('Complete convex model contours must be strictly nested without contacts or crossings')
    # Winding normalization preserves the source edge set; actual mesh loops
    # follow positive face incidence, including clockwise supplied model contours.
    rings = {name: ring if _cross(*[tuple(Fraction(v) for v in p) for p in ring[:3]]) > 0 else list(reversed(ring))
             for name, ring in rings.items()}
    return rings, parameters


def build_identified_basin(repo_root, cells, grid, evidence, selected, water, model, *, canonical_crs, maximum_triangles):
    """Return an explicitly inferred MODEL-SPACE joint field, never admission or publication."""
    _grid(grid, evidence, canonical_crs)
    rings, parameters = _specification(water, model, canonical_crs)
    if (grid['size'][0]*grid['size'][1] > 4096 or type(maximum_triangles) is not int or
            not 2 <= maximum_triangles <= 4096 or not isinstance(selected, dict) or not selected or
            any(not isinstance(k, str) or not isinstance(v, list) or not v or
                any(not isinstance(q, list) or len(q) != 2 or any(type(i) is not int for i in q) for q in v)
                for k, v in selected.items())):
        _refuse('A bounded complete source grid, explicit selected quads and final-quad capacity are required')
    domain = _domain(cells, selected)
    if len(domain) > 16 or any(cells[k].get('raster_alignment', {}).get('canonical_crs') != canonical_crs for k in selected):
        _refuse('Model requires at most16 complete original quads in the reference frame')
    _external_edges(domain)
    west, south = min(b[0] for b in domain), min(b[1] for b in domain)
    east, north = max(b[2] for b in domain), max(b[3] for b in domain)
    bounds = west, south, east, north
    boxes = list(domain)
    if (sum((Fraction(b[2])-Fraction(b[0]))*(Fraction(b[3])-Fraction(b[1])) for b in boxes) !=
            (Fraction(east)-Fraction(west))*(Fraction(north)-Fraction(south)) or any(
                max(a[0], b[0]) < min(a[2], b[2]) and max(a[1], b[1]) < min(a[3], b[3])
                for i, a in enumerate(boxes) for b in boxes[i+1:])):
        _refuse('Selected original quads must cover one rectangle without overlap or holes')
    if any(not west < p[0] < east or not south < p[1] < north for p in rings['influence'][:-1]):
        _refuse('Complete modeled influence must be strictly interior to the selected domain')
    segments, stations, reference = [], set(), []
    for _, _, _, quad in domain.values():
        x0, y0, count = _layout(quad, grid)
        if any(any(not 0 <= v <= 1 for v in p[3:]) or abs(sum(p[3:])-1) > 1e-8 for p in quad):
            _refuse('Original affine semantic weights are invalid')
        for y in range(y0, y0+count):
            for x in range(x0, x0+count):
                a, b, c, d = [[grid['origin'][0]+xx*grid['spacing'][0], grid['origin'][1]+yy*grid['spacing'][1]]
                    for xx, yy in ((x, y), (x+1, y), (x, y+1), (x+1, y+1))]
                for face in ((a, c, b), (b, c, d)):
                    reference.append([[*p, _reference(grid, p)] for p in face])
                    segments.extend([[u, v] for u, v in zip(face, (*face[1:], face[0]))])
                    stations.update(tuple(p) for p in face)
    for ring in rings.values():
        segments.extend([[a, b] for a, b in zip(ring, ring[1:])])
    unique = sorted({tuple(sorted((tuple(a), tuple(b)))) for a, b in segments})
    region = {'type': 'Polygon', 'coordinates': [[[west, south], [east, south], [east, north], [west, north], [west, south]]]}
    mesh = triangulate_constraints(repo_root, region, [[list(a), list(b)] for a, b in unique],
        [list(p) for p in sorted(stations)], min(65536, len(domain)*maximum_triangles))
    centers = [[math.fsum(p[i] for p in face)/3 for i in (0, 1)] for face in mesh]
    indices = {name: {i for i, value in enumerate(covered_points(repo_root, centers,
        {'type': 'Polygon', 'coordinates': [ring]})) if value} for name, ring in rings.items()}
    if not indices['core'] <= indices['wet'] <= indices['rim'] <= indices['influence']:
        _refuse('Native arrangement does not preserve the complete nested model regions')
    loops = {name: _boundary(mesh, selected_faces) for name, selected_faces in indices.items()}
    certificates = {name: certify_ring_rounding(rings[name], loop) for name, loop in loops.items()}
    outer = _boundary(mesh, range(len(mesh)))
    if any(not (p[0] in (west, east) or p[1] in (south, north)) for p in outer[:-1]):
        _refuse('Final field has an interior boundary')
    boundary_nodes = {name: set(map(tuple, ring[:-1])) for name, ring in loops.items()}
    region_nodes = {name: {tuple(p) for i in ids for p in mesh[i]} for name, ids in indices.items()}
    level, depth, freeboard = (parameters[k] for k in ('level_m', 'depth_m', 'rim_freeboard_m'))
    width, rim, extent = (parameters[k] for k in ('inner_bank_width_m', 'rim_outset_m', 'transition_extent_m'))
    heights = {}
    for xy in sorted({tuple(p) for face in mesh for p in face}):
        distance = _distance(xy, rings['wet'])
        if xy in boundary_nodes['wet']:
            height = level
        elif xy in region_nodes['core']:
            height = level-depth
        elif xy in region_nodes['wet']:
            height = level-depth*min(1., distance/width)
        elif xy in boundary_nodes['rim']:
            height = level+freeboard
        elif xy in boundary_nodes['influence'] or xy not in region_nodes['influence']:
            height = _reference(grid, xy)
        elif distance <= rim:
            height = level+freeboard*distance/rim
        else:
            t = (distance-rim)/(extent-rim)
            if not 0 <= t <= 1:
                _refuse('Explicit contours do not support the declared finite transition dimensions')
            height = (1-t)*(level+freeboard)+t*_reference(grid, xy)
        if not _finite(height):
            _refuse('Model height is unknown or overflowing')
        heights[xy] = height
    faces = [[[*p, heights[tuple(p)]] for p in face] for face in mesh]
    spill = _containment(faces, indices['wet'], level, outer, parameters['minimum_spill_margin_m'])
    maximum_gradient = max(_gradient(face) for face in faces)
    if not _finite(maximum_gradient) or maximum_gradient > parameters['maximum_gradient']:
        _refuse('Complete modeled PWA exceeds its declared gradient screen')
    exterior_proof = _prove_exterior(reference, faces, indices['influence'])
    surfaces, assigned = {}, Counter()
    for box, (key, x, y, corners) in domain.items():
        vertices, triangles, wet_faces, lookup = [], [], [], {}
        for i, face in enumerate(faces):
            if not box[0] < centers[i][0] < box[2] or not box[1] < centers[i][1] < box[3]:
                continue
            assigned[i] += 1
            triangle = []
            for p in face:
                xy = tuple(p[:2])
                if xy not in lookup:
                    lookup[xy] = len(vertices)
                    vertices.append([*p, *_semantic(corners, p[:2])])
                triangle.append(lookup[xy])
            if i in indices['wet']:
                wet_faces.append(len(triangles))
            triangles.append(triangle)
        if not triangles or len(triangles) > maximum_triangles:
            _refuse('Complete original-quad output is empty or exceeds its finite face capacity')
        surfaces.setdefault(key, {'quads': []})['quads'].append({'coordinate': [x, y], 'vertices': vertices,
            'triangles': triangles, 'water_triangles': wet_faces})
    if len(assigned) != len(faces) or any(n != 1 for n in assigned.values()):
        _refuse('Every actual face needs exactly one complete original-quad owner')
    _check_seams(surfaces, domain)
    return {'construction': RULE, 'basis': 'identified_model', 'publication': 'unpublished_model_space',
        'boundary_condition': 'provider_node_trace_requires_neighbor_continuation', 'surfaces': surfaces,
        'whole_ground_triangles': faces, 'provider_reference_triangles': reference,
        'region_face_indices': {name: sorted(value) for name, value in indices.items()}, 'modeled_contour_rings': loops,
        'modeled_water': {'function_id': 'identified_static_lined_basin', 'function_version': 1,
            'accuracy': 'identified_inferred_static_model_not_surveyed', 'level_m': level, 'spill_level_m': spill,
            'geometry': {'type': 'Polygon', 'coordinates': [loops['wet']]},
            'original_mapped_geometry': deepcopy(water['geometry']),
            'original_mapped_vertices_preserved': certificates['wet']['source_vertices'],
            'modeled_boundary_vertices': certificates['wet']['encoded_vertices']},
        'metrics': {'maximum_gradient': maximum_gradient, 'modeled_spill_margin_m': spill-level,
            'whole_outside_reference_field': exterior_proof,
            'modeled_wet_volume_m3': sum(_area(faces[i])*(level-math.fsum(p[2] for p in faces[i])/3) for i in indices['wet'])},
        'boundary_encoding_certificates': certificates,
        'consumed': {'implementation_sha256': implementation_identity(),
            'geometry_execution_identity': geometry_identity(repo_root, True)['identity_sha256'],
            'rounding_verifier_identity': rounding_identity(repo_root, True)['identity_sha256'],
            'affine_verifier_identity': affine_identity(repo_root, True)['identity_sha256'],
            'water_sha256': canonical_hash(water), 'model_sha256': canonical_hash(model),
            'ground_evidence': deepcopy(evidence), 'canonical_crs': canonical_crs,
            'domain_sha256': canonical_hash({str(k): v for k, v in sorted(domain.items())}),
            'boundary_trace_sha256': canonical_hash([[*p, heights[tuple(p)]] for p in outer[:-1]]),
            'maximum_triangles': maximum_triangles}}
