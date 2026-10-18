"""Source-qualified surface meaning; metric geometry remains a separate decision."""

import math
import re

from .contracts import CompilerError


BRIDGES = {'yes', 'aqueduct', 'boardwalk', 'cantilever', 'covered', 'low_water_crossing',
           'movable', 'trestle', 'viaduct'}
TUNNELS = {'yes', 'culvert', 'building_passage', 'avalanche_protector'}
SOURCE_KEYS = ('bridge', 'tunnel', 'layer', 'ford', 'covered', 'water', 'waterway',
               'amenity', 'barrier', 'ele', 'embankment', 'man_made', 'landuse', 'leisure')


def _presence(tags: dict, key: str, admitted: set[str], unresolved: list[str]) -> str:
    if key not in tags:
        return 'unknown'
    value = tags[key]
    if value == 'no':
        return 'absent'
    if isinstance(value, str) and value in admitted:
        return 'present'
    unresolved.append(f'unsupported_{key}')
    return 'unknown'


def _source_nodes(properties: dict, geometry: dict | None) -> list[dict]:
    if '@way_nodes' not in properties or not geometry or geometry.get('type') != 'LineString':
        return []
    references = properties['@way_nodes']
    points = geometry['coordinates']
    if not isinstance(references, list) or len(references) != len(points) or any(
        isinstance(ref, bool) or not isinstance(ref, int) or not 0 < ref < 2 ** 63 for ref in references
    ) or any(len(p) != 2 or any(isinstance(v, bool) or not isinstance(v, (int, float))
                                or not math.isfinite(v) for v in p) for p in points):
        raise CompilerError('surface_topology_invalid', 'Way nodes must match the original projected LineString')
    positions = {}
    result = []
    for reference, point in zip(references, points, strict=True):
        key = f'node/{reference}'
        if key in positions and positions[key] != point:
            raise CompilerError('surface_topology_invalid', 'One source node has conflicting positions')
        positions[key] = point
        result.append({'provider_node_id': key, 'xy': list(point)})
    return result


def surface_context(properties: dict, feature_class: str, source_geometry: dict | None = None) -> dict:
    """Keep missing/conflicting evidence explicit, without deriving altitude from tags."""
    tags = {key: properties[key] for key in SOURCE_KEYS if key in properties}
    if any(not isinstance(value, (str, int, float, bool))
           or isinstance(value, float) and not math.isfinite(value) for value in tags.values()):
        raise CompilerError('surface_context_invalid', 'Surface evidence must contain finite scalar tags')
    unresolved = []
    bridge = _presence(tags, 'bridge', BRIDGES, unresolved)
    tunnel = _presence(tags, 'tunnel', TUNNELS, unresolved)
    ford = _presence(tags, 'ford', {'yes', 'stepping_stones'}, unresolved)
    covered = _presence(tags, 'covered', {'yes', 'arcade', 'colonnade'}, unresolved)
    relation = 'unknown'
    if bridge == tunnel == 'present':
        relation = 'conflicting'
        unresolved.append('conflicting_bridge_tunnel')
    elif bridge == ford == 'present':
        unresolved.append('crossing_mode_required')
    elif bridge == 'present':
        relation = 'above'
    elif tunnel == 'present' and tags['tunnel'] in {'yes', 'culvert'}:
        relation = 'below'
    elif ford == 'present':
        relation = 'at_ground'
    if relation in {'above', 'below'}:
        unresolved.append('metric_elevation_required')
    layer = None
    if 'layer' in tags:
        value = tags['layer']
        if isinstance(value, str) and re.fullmatch(r'[+-]?[0-9]{1,10}', value):
            layer = int(value)
        if layer is None or not -(2 ** 31) <= layer < 2 ** 31:
            layer = None
            unresolved.append('unsupported_layer')
    origin, kind = 'unknown', 'unspecified'
    if feature_class == 'water':
        if tags.get('amenity') == 'fountain':
            origin, kind = 'constructed', 'fountain'
        elif tags.get('leisure') == 'swimming_pool':
            origin, kind = 'constructed', 'pool'
        elif tags.get('water') in {'basin', 'reservoir'}:
            origin, kind = 'constructed', tags['water']
        elif tags.get('landuse') in {'basin', 'reservoir'}:
            origin, kind = 'constructed', tags['landuse']
        if origin == 'constructed':
            unresolved.append('constructed_surface_elevation_required')
    if 'ele' in tags:
        unresolved.append('vertical_datum_required')
    return {'version': 1, 'ground_relation': relation, 'bridge': bridge, 'tunnel': tunnel,
            'ford': ford, 'covered': covered, 'relative_layer': layer,
            'water_origin': origin, 'water_kind': kind,
            'source_tags': tags, 'source_nodes': _source_nodes(properties, source_geometry),
            'unresolved': sorted(set(unresolved))}
