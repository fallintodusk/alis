"""Source-station bridge elevation candidate; never owns native Road planform."""
import math
from pathlib import Path

from World.ExecutionEnvironment.geometry.api import partition_triangles, execution_identity, validate_triangle_cover
from ...app.contracts import CompilerError, canonical_hash
from .geometry import _point
from .identity import implementation_identity
from .contracts import validate_document


def _refuse(message):
    raise CompilerError('road_profile_unsupported', message)


def _finite(value):
    return not isinstance(value, bool) and isinstance(value, (int, float)) and math.isfinite(value)


def _area(face):
    a, b, c = face
    return abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])) * .5


def _affine(face, xy):
    a, b, c = face
    determinant = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    if not math.isfinite(determinant) or abs(determinant) <= 1e-12:
        _refuse('Source profile has a collapsed parameter or support triangle')
    u = ((xy[0]-a[0])*(c[1]-a[1])-(xy[1]-a[1])*(c[0]-a[0])) / determinant
    v = ((b[0]-a[0])*(xy[1]-a[1])-(b[1]-a[1])*(xy[0]-a[0])) / determinant
    return [*xy, *[a[i]+u*(b[i]-a[i])+v*(c[i]-a[i]) for i in range(2, len(a))]]


def _polygon(points):
    return {'type': 'Polygon', 'coordinates': [[*points, points[0]]]}


def _overlaps(a, b):
    return not (max(p[0] for p in a) < min(p[0] for p in b) or
                max(p[0] for p in b) < min(p[0] for p in a) or
                max(p[1] for p in a) < min(p[1] for p in b) or
                max(p[1] for p in b) < min(p[1] for p in a))


def _terrain(cells, points, width):
    domains = [[(min(a[0], b[0])-width/2, min(a[1], b[1])-width/2),
                (max(a[0], b[0])+width/2, max(a[1], b[1])+width/2)]
               for a, b in zip(points, points[1:])]
    faces, consumed = [], {}
    for identifier, cell in sorted(cells.items()):
        bounds = cell.get('bounds')
        if not isinstance(bounds, (list, tuple)) or len(bounds) != 4 or not all(_finite(v) for v in bounds):
            _refuse('Source profile Terrain bounds are invalid')
        w, s, e, n = bounds
        if w >= e or s >= n:
            _refuse('Source profile Terrain bounds are collapsed')
        if not any(_overlaps([(w, s), (e, n)], domain) for domain in domains):
            continue
        rows, spacing = cell.get('core_samples'), cell.get('sample_spacing')
        if (not isinstance(rows, list) or len(rows) < 2 or not all(isinstance(row, list) for row in rows) or
                len(rows[0]) < 2 or any(len(row) != len(rows[0]) for row in rows) or
                any(not _finite(v) for row in rows for v in row) or
                not isinstance(spacing, (list, tuple)) or len(spacing) != 2 or
                any(not _finite(v) or v <= 0 for v in spacing)):
            _refuse('Source profile needs finite complete final Terrain')
        if cell.get('schema_version', 2) != 2 or cell.get('quads'):
            _refuse('Wrapped final Terrain requires its explicit triangle query contract')
        for y in range(len(rows)-1):
            for x in range(len(rows[0])-1):
                a, b, c, d = (_point(cell, x, y)[:3], _point(cell, x+1, y)[:3],
                              _point(cell, x, y+1)[:3], _point(cell, x+1, y+1)[:3])
                if any(_overlaps([a, d], domain) for domain in domains):
                    faces.extend(((a, b, c), (b, d, c)))
                    consumed[identifier] = canonical_hash(cell)
    if not faces:
        _refuse('Source profile has no Terrain support')
    return faces, consumed


def _inside(face, xy):
    a, b, c = face
    determinant = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    u = ((xy[0]-a[0])*(c[1]-a[1])-(xy[1]-a[1])*(c[0]-a[0])) / determinant
    v = ((b[0]-a[0])*(xy[1]-a[1])-(b[1]-a[1])*(xy[0]-a[0])) / determinant
    return min(u, v, 1-u-v) >= -1e-9


def _height(faces, point):
    heights = [_affine(face, point)[2] for face in faces if _inside(face, point)]
    if not heights or max(heights)-min(heights) > 1e-8:
        _refuse('Source profile endpoint has missing or conflicting Terrain')
    return heights[0]


def _cross_section_cuts(faces, point, normal, width):
    start = [p-width/2*v for p, v in zip(point, normal)]
    delta = [width*v for v in normal]
    cuts = {-width/2, width/2}
    for face in faces:
        for a, b in zip(face, [*face[1:], face[0]]):
            edge = [b[i]-a[i] for i in range(2)]
            determinant = delta[0]*edge[1]-delta[1]*edge[0]
            if abs(determinant) <= 1e-12:
                continue
            diff = [a[i]-start[i] for i in range(2)]
            t = (diff[0]*edge[1]-diff[1]*edge[0])/determinant
            u = (diff[0]*delta[1]-diff[1]*delta[0])/determinant
            if 0 < t < 1 and 0 <= u <= 1:
                cuts.add(-width/2+t*width)
    return cuts


def build_bridge_profile(repo_root, road, cells):
    """Build an isolated model. Exact native clearance and join acceptance remain mandatory."""
    context = road.get('attributes', {}).get('surface_context', {})
    width = road.get('attributes', {}).get('width_m')
    if (road.get('feature_class') != 'road' or road.get('geometry', {}).get('type') != 'LineString' or
            context.get('version') != 1 or context.get('ground_relation') != 'above' or context.get('bridge') != 'present' or
            context.get('tunnel') not in {'absent', 'unknown'} or context.get('ford') not in {'absent', 'unknown'} or
            any(value in {'unsupported_bridge', 'unsupported_tunnel', 'unsupported_ford',
                          'crossing_mode_required', 'conflicting_bridge_tunnel'} for value in context.get('unresolved', [])) or
            not _finite(width) or width <= 0):
        _refuse('Bridge profile requires explicit nonconflicting source evidence and one finite source way')
    points = road['geometry']['coordinates']
    if (not isinstance(points, list) or len(points) < 2 or any(
            not isinstance(p, (list, tuple)) or len(p) != 2 or any(not _finite(v) for v in p) for p in points)):
        _refuse('Bridge source geometry is invalid')
    stations, directions = [0.], []
    for a, b in zip(points, points[1:]):
        length = math.dist(a, b)
        if length <= 1e-8:
            _refuse('Bridge source has ambiguous repeated stations')
        stations.append(stations[-1]+length)
        if not math.isfinite(stations[-1]):
            _refuse('Bridge source station overflows')
        directions.append([(q-p)/length for p, q in zip(a, b)])
    normals = []
    for i in range(len(points)):
        adjacent = directions[max(0, i-1):min(len(directions), i+1)]
        tangent = [sum(p[k] for p in adjacent) for k in range(2)]
        length = math.hypot(*tangent)
        if length <= 1e-8:
            _refuse('Bridge source has an ambiguous reversing section')
        normals.append([-tangent[1]/length, tangent[0]/length])
    terrain, consumed = _terrain(cells, points, width)
    sides = sorted(_cross_section_cuts(terrain, points[0], normals[0], width) |
                   _cross_section_cuts(terrain, points[-1], normals[-1], width))
    ends = [[_height(terrain, [p+side*v for p, v in zip(point, normal)]) for side in sides]
            for point, normal in ((points[0], normals[0]), (points[-1], normals[-1]))]
    rows = [[[p+side*v for p, v in zip(point, normal)] +
             [ends[0][j]*(1-station/stations[-1])+ends[1][j]*station/stations[-1], station, side]
             for j, side in enumerate(sides)] for point, normal, station in zip(points, normals, stations)]
    baseline = []
    for a, b in zip(rows, rows[1:]):
        for i in range(len(sides)-1):
            baseline.extend(([a[i+1], a[i], b[i+1]], [a[i], b[i], b[i+1]]))
    controls = {0.: 0., stations[-1]: 0.}
    for face in baseline:
        area = 0.
        for ground in terrain:
            if not _overlaps(face, ground):
                continue
            parts = partition_triangles(repo_root, [p[:2] for p in face], _polygon([p[:2] for p in ground]), 16)
            for triangle in parts['wet']:
                area += _area(triangle)
                for xy in triangle:
                    p = _affine(face, xy)
                    station = max(0., min(stations[-1], p[3]))
                    deficit = max(0., _affine(ground, xy)[2]-p[2])
                    if station < 1e-8 or stations[-1]-station < 1e-8:
                        if deficit > 1e-8:
                            _refuse('Bridge support conflicts with its unchanged abutment section')
                        continue
                    controls[station] = max(controls.get(station, 0.), deficit)
        if abs(area-_area(face)) > max(1e-8, _area(face)*1e-8):
            _refuse('Bridge source support has missing or overlapping Terrain')
    hull = []
    for point in sorted(controls.items()):
        while len(hull) > 1:
            a, b = hull[-2:]
            if (b[0]-a[0])*(point[1]-a[1])-(b[1]-a[1])*(point[0]-a[0]) < 0:
                break
            hull.pop()
        hull.append(point)
    triangles = []
    for face in baseline:
        parameter = [[p[3], p[4], p[2]] for p in face]
        for left, right in zip(hull, hull[1:]):
            if right[0] <= min(p[0] for p in parameter) or left[0] >= max(p[0] for p in parameter):
                continue
            domain = _polygon([[left[0], -width/2], [right[0], -width/2], [right[0], width/2], [left[0], width/2]])
            parts = partition_triangles(repo_root, [p[:2] for p in parameter], domain, 16)
            for face_uv in parts['wet']:
                triangles.append([[*uv, _affine(parameter, uv)[2] + left[1] +
                                   (uv[0]-left[0])/(right[0]-left[0])*(right[1]-left[1])] for uv in face_uv])
    if any(not math.isfinite(value) for face in triangles for point in face for value in point):
        _refuse('Bridge profile contains an unknown or overflowing elevation')
    result = {'$schema': 'https://alis.world/schemas/world-compiler/road-elevation-v1.json', 'schema_version': 1,
            'interpolation': 'affine_uv_triangles_v1',
            'coordinate_rule': 'source_arclength_m_signed_lateral_left_m',
            'height_rule': 'canonical_absolute_m_without_presentation_offset',
            'model': 'source_abutment_support_candidate_not_surveyed',
            'source_feature_id': road['feature_id'], 'source_points': points,
            'length_m': stations[-1], 'width_m': width, 'triangles': triangles,
            'consumed_feature_sha256': canonical_hash(road), 'consumed_terrain': consumed,
            'implementation_sha256': implementation_identity(),
            'execution_identity': execution_identity(repo_root, True)['identity_sha256']}
    validate_document(result, Path('road-elevation.json'))
    domain = _polygon([[0., -width/2], [stations[-1], -width/2], [stations[-1], width/2], [0., width/2]])
    validate_triangle_cover(repo_root, domain, [[p[:2] for p in face] for face in triangles])
    return result
