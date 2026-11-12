"""Pure observed constructed-surface candidates; no source admission or publication."""
from copy import deepcopy
import math
from pathlib import Path

from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.api import covered_points, execution_identity, polygon_union, triangulate_points
from ..app.contracts import CompilerError, canonical_hash, file_hash
from .app.road_elevation import _finite, _polygon
from .app.road_surface_projection import project_straight_surface


def _refuse(message):
    raise CompilerError('constructed_surface_unsupported', message)


def implementation_identity():
    owner = Path(__file__).resolve().parent
    files = [owner/'constructed_surface.py', owner/'app/road_elevation.py',
             owner/'app/road_surface_projection.py', owner.parent/'app/contracts.py']
    return canonical_hash({p.relative_to(owner.parent).as_posix(): file_hash(p) for p in files})


def _hash(value):
    return isinstance(value, str) and len(value) == 64 and all(c in '0123456789abcdef' for c in value)


def build_constructed_road_surface(repo_root, road, points, parent, context, evidence, *,
                                   canonical_crs, selection, maximum_triangles=4096):
    """Model only the requested straight ribbon; qualification and incident joins remain external."""
    if any(not isinstance(v, dict) for v in (road, parent, context, evidence, selection)):
        _refuse('Construction requires explicit feature, parent, evidence and spatial-selection objects')
    geometry, attributes = road.get('geometry'), road.get('attributes')
    if not isinstance(geometry, dict) or not isinstance(attributes, dict):
        _refuse('Construction requires explicit Road geometry and attributes')
    coordinates, width = geometry.get('coordinates'), attributes.get('width_m')
    if (road.get('feature_class') != 'road' or geometry.get('type') != 'LineString' or
            not isinstance(road.get('feature_id'), str) or not road['feature_id'] or
            not isinstance(coordinates, list) or len(coordinates) != 2 or
            any(not isinstance(p, list) or len(p) != 2 or any(not _finite(v) for v in p) for p in coordinates) or
            not _finite(width) or width <= 0):
        _refuse('This bounded construction requires one finite straight source Road and its admitted width')
    relation = attributes.get('surface_context', {})
    if (not isinstance(relation, dict) or relation.get('version', 1) != 1 or
            relation.get('ground_relation', 'unknown') not in ('unknown', 'at_ground') or
            any(relation.get(key, 'unknown') not in ('unknown', 'absent') for key in ('bridge', 'tunnel', 'ford')) or
            relation.get('unresolved', [])):
        _refuse('Parent containment cannot override explicit or unresolved crossing evidence')
    polygons = parent.get('coordinates') if parent.get('type') == 'MultiPolygon' else [parent.get('coordinates')]
    if not isinstance(polygons, list) or len(polygons) != 1:
        _refuse('Disconnected constructed parents need explicit component association')
    rings = polygons[0]
    if (parent.get('type') not in ('Polygon', 'MultiPolygon') or not isinstance(rings, list) or not rings or
            any(not isinstance(ring, list) or len(ring) < 4 or ring[0] != ring[-1] or
                any(not isinstance(p, list) or len(p) != 2 or any(not _finite(v) for v in p) for p in ring) for ring in rings) or
            context.get('man_made') != 'pier' or context.get('floating') != 'no' or
            context.get('geometry_sha256') != canonical_hash(parent) or not _hash(context.get('source_sha256')) or
            any(key in context for key in ('ele', 'height', 'min_height', 'depth'))):
        _refuse('A complete fixed-pier footprint and qualified nonmetric structural context are required')
    bounds = selection.get('bounds')
    if (set(selection) != {'rule', 'bounds'} or selection['rule'] != 'explicit_query_window' or
            not isinstance(bounds, list) or len(bounds) != 4 or any(not _finite(v) for v in bounds) or
            bounds[0] >= bounds[2] or bounds[1] >= bounds[3] or
            type(maximum_triangles) is not int or not 2 <= maximum_triangles <= 4096):
        _refuse('Construction needs an explicit finite query window and bounded output budget')
    if (not isinstance(points, list) or not 3 <= len(points) <= 65536 or
            any(not isinstance(p, list) or len(p) != 3 or any(not _finite(v) for v in p) or
                not bounds[0] <= p[0] <= bounds[2] or not bounds[1] <= p[1] <= bounds[3] for p in points) or
            type(evidence.get('classification')) is not int or evidence['classification'] != 2 or
            not isinstance(canonical_crs, str) or not canonical_crs or evidence.get('canonical_crs') != canonical_crs or
            evidence.get('vertical_datum') != 'EPSG:3855' or
            evidence.get('point_content_sha256') != canonical_hash(points) or
            any(not _hash(evidence.get(key)) for key in ('source_sha256', 'processing_sha256'))):
        _refuse('Finite class-2 observations, content pins and their qualified canonical frame are required')
    a, b = coordinates
    length = math.dist(a, b)
    if not math.isfinite(length) or length <= 1e-8:
        _refuse('Constructed source Road is collapsed')
    tangent = [(b[i]-a[i])/length for i in (0, 1)]
    normal = [-tangent[1], tangent[0]]
    corners = [[p[i]+side*width/2*normal[i] for i in (0, 1)]
               for p, side in ((a, 1), (a, -1), (b, -1), (b, 1))]
    if any(not math.isfinite(v) for p in corners for v in p):
        _refuse('Constructed Road width overflows its projected domain')
    footprint = _polygon(corners)
    stage = 'parent containment'
    try:
        if polygon_union(repo_root, [footprint], parent)['outside_area_m2'] > 1e-9:
            _refuse('Requested Road width leaves its qualified structural parent')
        # The query halo supplies observations, not permission to borrow another surface role.
        membership = covered_points(repo_root, [p[:2] for p in points], parent)
        unique = {}
        for p, inside in zip(points, membership):
            if not inside:
                continue
            xy = tuple(p[:2])
            if xy in unique and unique[xy] != p[2]:
                _refuse('Constructed observations conflict at one XY; no averaging policy is implied')
            unique[xy] = p[2]
        support = [[*xy, z] for xy, z in sorted(unique.items())]
        if len(support) < 3:
            _refuse('Structural parent contains insufficient associated height observations')
        stage = 'source triangulation'
        faces = triangulate_points(repo_root, [p[:2] for p in support])
        footprint, uv, world, mapping = project_straight_surface(repo_root,
            ([support[i] for i in indices] for indices in faces), coordinates, width, maximum_triangles)
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code, str(error), surface_stage=stage, **error.details) from error
    return {'model': 'observed_constructed_tin_candidate_not_surveyed', 'endpoint_relation': 'constructed_surface',
        'coordinate_rule': 'source_arclength_m_signed_lateral_left_m',
        'height_rule': 'canonical_absolute_m_without_presentation_offset', 'length_m': length, 'width_m': width,
        'footprint': footprint, 'world_triangles': world, 'uv_triangles': uv, 'mapping': mapping,
        'minimum_height_m': min(p[2] for face in world for p in face),
        'maximum_height_m': max(p[2] for face in world for p in face),
        'consumed': {'feature_sha256': canonical_hash(road), 'parent_sha256': canonical_hash(parent),
            'context_sha256': canonical_hash(context), 'evidence_sha256': canonical_hash(evidence),
            'selection_sha256': canonical_hash(selection), 'associated_points_sha256': canonical_hash(support),
            'implementation_sha256': implementation_identity(),
            'execution_identity': execution_identity(repo_root, True)['identity_sha256']},
        'evidence': deepcopy(evidence), 'selection': deepcopy(selection)}
