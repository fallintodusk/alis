"""Pure fixed-pier/floating-landing candidates; qualification and publication stay external."""
from copy import deepcopy
from datetime import date
import math
import re

from World.ExecutionEnvironment.geometry.api import partition_triangles, validate_triangle_cover, execution_identity
from ...app.contracts import CompilerError, canonical_hash
from .identity import implementation_identity
from .road_elevation import _affine, _area, _cross_section_cuts, _finite, _height, _overlaps, _polygon, _terrain
from .road_surface_projection import project_straight_surface


def _refuse(message):
    raise CompilerError('gangway_profile_unsupported', message)


def _hash(value):
    return isinstance(value, str) and len(value) == 64 and all(c in '0123456789abcdef' for c in value)


def _epoch(value):
    if not isinstance(value, dict) or set(value) != {'start', 'end'}:
        return False
    try:
        return date.fromisoformat(value['start']) <= date.fromisoformat(value['end'])
    except (ValueError, TypeError):
        return False


def _metadata(value, canonical_crs):
    return (value.get('canonical_crs') == canonical_crs and value.get('vertical_datum') == 'EPSG:3855'
            and _epoch(value.get('source_epoch')) and _hash(value.get('source_sha256')))


def _footprint(value):
    if (not isinstance(value, dict) or set(value) != {'type', 'coordinates'}
            or value['type'] not in ('Polygon', 'MultiPolygon')):
        return False
    polygons = value['coordinates'] if value['type'] == 'MultiPolygon' else [value['coordinates']]
    if not isinstance(polygons, list) or not polygons:
        return False
    count = 0
    for rings in polygons:
        if not isinstance(rings, list) or not rings:
            return False
        for ring in rings:
            if (not isinstance(ring, list) or len(ring) < 4 or ring[0] != ring[-1] or any(
                    not isinstance(p, list) or len(p) != 2 or any(not _finite(v) for v in p) for p in ring)):
                return False
            count += len(ring)
            if count > 16384:
                return False
    return True


def _support(repo, support, node, canonical_crs):
    if not isinstance(support, dict):
        _refuse('Each endpoint requires an explicit finite structural support')
    fields = {'role', 'provider_node_id', 'basis', 'canonical_crs', 'vertical_datum',
              'source_epoch', 'source_sha256', 'footprint', 'triangles'}
    if support.get('role') == 'floating_landing': fields.add('freeboard_m')
    if (set(support) != fields or support.get('role') not in ('fixed_pier', 'floating_landing')
            or support.get('provider_node_id') != node['provider_node_id']
            or support.get('basis') not in ('observed', 'identified_model') or not _metadata(support, canonical_crs)):
        _refuse('Endpoint role, source node, basis, frame and epoch must be qualified explicitly')
    return _surface(repo, support)


def _surface(repo, surface):
    faces = surface['triangles']
    if (not isinstance(faces, list) or not 1 <= len(faces) <= 4096 or any(
            not isinstance(face, list) or len(face) != 3 or any(
                not isinstance(p, list) or len(p) != 3 or any(not _finite(v) for v in p)
                for p in face) for face in faces)):
        _refuse('Structural support requires bounded finite XYZ triangles')
    if any(_area(face) <= 1e-12 for face in faces):
        _refuse('Structural support contains a collapsed triangle')
    if not _footprint(surface['footprint']):
        _refuse('Structural support needs a bounded finite polygon footprint with explicit holes')
    validate_triangle_cover(repo, surface['footprint'], [[p[:2] for p in face] for face in faces])
    # A complete XY cover alone cannot reject a vertical crack or a discontinuous T junction.
    for point in {tuple(p) for face in faces for p in face}:
        _section_height(faces, point[:2])
    return faces


def _inputs(repo, road, cells, supports, water, limits, canonical_crs):
    if (not isinstance(road, dict) or not isinstance(cells, dict) or any(
            not isinstance(key, str) or not isinstance(value, dict) for key, value in cells.items())):
        _refuse('Gangway construction requires a source Road and finite Terrain context')
    geometry, attrs = road.get('geometry'), road.get('attributes')
    if not isinstance(geometry, dict) or not isinstance(attrs, dict):
        _refuse('Gangway source geometry and attributes are missing')
    points, width = geometry.get('coordinates'), attrs.get('width_m')
    context = attrs.get('surface_context')
    unresolved = context.get('unresolved') if isinstance(context, dict) else None
    if (road.get('feature_class') != 'road' or not isinstance(road.get('feature_id'), str)
            or not road['feature_id'] or geometry.get('type') != 'LineString'
            or not isinstance(points, list) or len(points) != 2 or any(
                not isinstance(p, list) or len(p) != 2 or any(not _finite(v) for v in p) for p in points)
            or not _finite(width) or width <= 0 or not isinstance(context, dict)
            or context.get('version') != 1 or context.get('bridge') != 'present'
            or context.get('ground_relation') != 'above'
            or context.get('tunnel') not in ('absent', 'unknown')
            or context.get('ford') not in ('absent', 'unknown')
            or not isinstance(unresolved, list) or any(not isinstance(v, str) for v in unresolved)
            or set(unresolved) - {'metric_elevation_required'}):
        _refuse('This model requires one straight source bridge with nonconflicting crossing evidence')
    nodes = context.get('source_nodes')
    if (not isinstance(nodes, list) or len(nodes) != 2 or any(
            not isinstance(node, dict) or set(node) != {'provider_node_id', 'xy'}
            or node['xy'] != point or not isinstance(node['provider_node_id'], str)
            or not re.fullmatch(r'node/[1-9][0-9]{0,18}', node['provider_node_id'])
            or int(node['provider_node_id'][5:]) >= 2**63
            for node, point in zip(nodes, points)) or nodes[0]['provider_node_id'] == nodes[1]['provider_node_id']):
        _refuse('Complete directed endpoint source-node bindings are required')
    if not isinstance(canonical_crs, str) or not canonical_crs:
        _refuse('Gangway support needs an explicit canonical CRS')
    if (not isinstance(water, dict) or set(water) != {'model', 'level_m', 'canonical_crs',
            'vertical_datum', 'source_epoch', 'source_sha256'} or water['model'] != 'static_representative'
            or not _finite(water['level_m']) or not _metadata(water, canonical_crs)):
        _refuse('An identified static Water model needs a finite level, qualified frame, epoch and source pin')
    fields = {'maximum_longitudinal_grade', 'maximum_cross_slope', 'maximum_join_step_m',
              'minimum_terrain_clearance_m', 'minimum_water_clearance_m', 'maximum_triangles'}
    if (not isinstance(limits, dict) or set(limits) != fields or any(
            not _finite(v) or v < 0 for k, v in limits.items() if k != 'maximum_triangles')
            or type(limits['maximum_triangles']) is not int or not 2 <= limits['maximum_triangles'] <= 4096):
        _refuse('Explicit finite quality limits and a bounded triangle budget are required')
    if (not isinstance(supports, list) or len(supports) != 2
            or any(not isinstance(support, dict) or not isinstance(support.get('role'), str) for support in supports)
            or {support.get('role') for support in supports} != {'fixed_pier', 'floating_landing'}):
        _refuse('Exactly one fixed-pier and one floating-landing support are required')
    faces = [_support(repo, support, node, canonical_crs) for support, node in zip(supports, nodes)]
    for support, triangles in zip(supports, faces):
        if support['role'] == 'floating_landing':
            freeboard = support.get('freeboard_m')
            if not _finite(freeboard) or freeboard <= 0:
                _refuse('Floating deck freeboard must be an explicit positive model input')
            level = water['level_m']+freeboard
            if not math.isfinite(level) or any(abs(p[2]-level) > 1e-8 for face in triangles for p in face):
                _refuse('Floating support does not match its consumed static Water and freeboard model')
    length = math.dist(*points)
    if not math.isfinite(length) or length <= 1e-8:
        _refuse('Source gangway is collapsed or overflows')
    tangent = [(points[1][i]-points[0][i])/length for i in (0, 1)]
    normal = [-tangent[1], tangent[0]]
    terrain, consumed = _terrain(cells, points, width)
    return points, width, length, tangent, normal, faces, terrain, consumed


def _section_height(faces, point):
    try:
        value = _height(faces, point)
    except CompilerError as error:
        raise CompilerError('gangway_profile_unsupported', 'Endpoint has missing or conflicting structural support') from error
    if not _finite(value):
        _refuse('Endpoint interpolation is unknown or overflows')
    return value


def _validate_sections(points, normal, width, supports):
    sides = sorted(set.union(*[_cross_section_cuts(faces, point, normal, width)
                                for point, faces in zip(points, supports)]))
    # Boundary cuts and interval interiors prove the whole section, including holes.
    probes = sorted(set(sides + [(a+b)/2 for a, b in zip(sides, sides[1:])]))
    for point, faces in zip(points, supports):
        for side in probes:
            _section_height(faces, [p+side*v for p, v in zip(point, normal)])


def _measure(repo, triangles, world_triangles, points, width, length, tangent, normal, supports, terrain, water, limits):
    domain = _polygon([[0., -width/2], [length, -width/2], [length, width/2], [0., width/2]])
    if (not triangles or len(triangles) != len(world_triangles) or len(triangles) > limits['maximum_triangles']
            or any(not _finite(v) for face in triangles for p in face for v in p)):
        _refuse('Gangway mesh is unknown or exceeds its explicit output budget')
    validate_triangle_cover(repo, domain, [[p[:2] for p in face] for face in triangles])
    step = 0.
    for station, point, faces in zip((0., length), points, supports):
        cuts = _cross_section_cuts(triangles, [station, 0.], [0., 1.], width)
        cuts |= _cross_section_cuts(faces, point, normal, width)
        for side in cuts:
            target = _section_height(faces, [p+side*v for p, v in zip(point, normal)])
            step = max(step, abs(_section_height(triangles, [station, side])-target))
    if step > limits['maximum_join_step_m']:
        _refuse('Gangway exceeds its complete endpoint join-step limit')
    grade, cross, clearance, water_clearance = 0., 0., math.inf, math.inf
    for face, world in zip(triangles, world_triangles):
        a, b, c = face
        ux, uy, uz = [b[i]-a[i] for i in range(3)]
        vx, vy, vz = [c[i]-a[i] for i in range(3)]
        determinant = ux*vy-uy*vx
        if abs(determinant) <= 1e-12:
            _refuse('Gangway contains a collapsed parameter face')
        grade = max(grade, abs((uz*vy-vz*uy)/determinant))
        cross = max(cross, abs((ux*vz-vx*uz)/determinant))
        water_clearance = min(water_clearance, *(p[2]-water['level_m'] for p in face))
        area = 0.
        for ground in terrain:
            if not _overlaps(world, ground): continue
            pieces = partition_triangles(repo, [p[:2] for p in world], _polygon([p[:2] for p in ground]), 16)
            for piece in pieces['wet']:
                area += _area(piece)
                clearance = min(clearance, *(_affine(world, xy)[2]-_affine(ground, xy)[2] for xy in piece))
        if abs(area-_area(world)) > max(1e-8, _area(world)*1e-8):
            _refuse('Gangway Terrain clearance has missing or overlapping support')
    measured = {'maximum_longitudinal_grade': grade, 'maximum_cross_slope': cross,
                'maximum_join_step_m': step, 'minimum_terrain_clearance_m': clearance,
                'minimum_water_clearance_m': water_clearance}
    if any(not _finite(value) for value in measured.values()):
        _refuse('Gangway quality is unknown; finite source inputs alone are insufficient')
    for key, value in measured.items():
        if key.startswith('maximum_') and value > limits[key] or key.startswith('minimum_') and value < limits[key]:
            _refuse(f'Gangway violates explicit {key}')
    return measured


def build_gangway_profile(repo_root, road, cells, *, endpoint_supports, assembly_surface,
                          water_model, quality_limits, canonical_crs):
    """Project a complete static assembly; this does not admit a neighborhood or activate authority."""
    points, width, length, tangent, normal, supports, terrain, consumed = _inputs(
        repo_root, road, cells, endpoint_supports, water_model, quality_limits, canonical_crs)
    fields = {'basis', 'canonical_crs', 'vertical_datum', 'source_epoch', 'source_sha256', 'footprint', 'triangles'}
    if (not isinstance(assembly_surface, dict) or set(assembly_surface) != fields
            or assembly_surface.get('basis') not in ('observed', 'identified_model')
            or not _metadata(assembly_surface, canonical_crs)):
        _refuse('Complete assembly requires explicit basis, frame, epoch, source pin and finite surface')
    assembly = _surface(repo_root, assembly_surface)
    _validate_sections(points, normal, width, supports)
    _, triangles, world, mapping = project_straight_surface(repo_root, assembly, points, width, quality_limits['maximum_triangles'])
    quality = _measure(repo_root, triangles, world, points, width, length, tangent, normal, supports,
                       terrain, water_model, quality_limits)
    dependencies = {'feature_sha256': canonical_hash(road),
                    'endpoint_supports_sha256': canonical_hash(endpoint_supports),
                    'assembly_surface_sha256': canonical_hash(assembly_surface),
                    'water_model_sha256': canonical_hash(water_model),
                    'quality_limits_sha256': canonical_hash(quality_limits),
                    'terrain': consumed, 'implementation_sha256': implementation_identity(),
                    'execution_identity': execution_identity(repo_root, True)['identity_sha256']}
    return {'model': 'fixed_floating_gangway_candidate_v1', 'schema_version': 1,
            'endpoint_relation': 'constructed_surface', 'canonical_crs': canonical_crs, 'vertical_datum': 'EPSG:3855',
            'coordinate_rule': 'source_arclength_m_signed_lateral_left_m',
            'height_rule': 'canonical_absolute_m_without_presentation_offset',
            'source_feature_id': road['feature_id'], 'source_points': deepcopy(points),
            'length_m': length, 'width_m': width, 'triangles': triangles, 'world_triangles': world,
            'endpoint_supports': deepcopy(endpoint_supports), 'water_model': deepcopy(water_model),
            'quality_limits': deepcopy(quality_limits), 'quality': quality, 'mapping': mapping, 'consumed': dependencies,
            'support_model_sha256': canonical_hash({'supports': endpoint_supports, 'assembly': assembly_surface,
                                                    'water': water_model})}
