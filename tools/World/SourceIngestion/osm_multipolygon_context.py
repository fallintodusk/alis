"""Literal complete multipolygon context; no projection, height or structural admission."""
from collections import Counter
from copy import deepcopy
from pathlib import Path

from .app.contracts import canonical_hash, file_hash
from .app.osm_context import _point, _primitive, _records, _refuse, _timestamp, osm_context_identity, osm_way_context


def osm_multipolygon_context_identity():
    return canonical_hash({'context': osm_context_identity(), 'multipolygon': file_hash(Path(__file__))})


def _native_records(document):
    if not isinstance(document, dict) or document.get('version') != '0.6':
        _refuse('Complete OSM relation primitives require their native versioned document')
    elements = document.get('elements')
    if not isinstance(elements, list) or not 1 <= len(elements) <= 65536:
        _refuse('Complete native relation context exceeds its finite primitive capacity or is absent')
    records = {}
    for element in elements:
        if (not isinstance(element, dict) or element.get('type') not in ('node', 'way', 'relation') or
                type(element.get('id')) is not int or element['id'] <= 0):
            _refuse('Native relation context contains an invalid primitive identity')
        key = f"{element['type']}/{element['id']}"
        if key in records:
            _refuse('Native relation context contains duplicate primitive records')
        records[key] = element
    return records


def _match_native(normalized, native):
    if not isinstance(native, dict):
        _refuse('Complete native relation context is missing a consumed primitive')
    props = normalized['properties']
    if (native.get('type') != props['@type'] or type(native.get('version')) is not int or
            native['version'] != props['@version'] or _timestamp(native.get('timestamp')) != _timestamp(props['@timestamp'])):
        _refuse('Native and normalized primitive versions or timestamps conflict')
    tags = native.get('tags', {})
    if not isinstance(tags, dict) or any(not isinstance(k, str) or not isinstance(v, str) for k, v in tags.items()):
        _refuse('Native primitive tags are malformed')
    normalized_tags = {k: v for k, v in props.items() if not k.startswith('@')}
    # The native relation supplies the type tag omitted by normalized area export.
    if props['@type'] == 'relation' and tags.get('type') == 'multipolygon' and 'type' not in normalized_tags:
        normalized_tags['type'] = 'multipolygon'
    if tags != normalized_tags:
        _refuse('Native and normalized primitive tags conflict')


def _geometry_edges(geometry, coordinates):
    if geometry.get('type') not in ('Polygon', 'MultiPolygon'):
        _refuse('A qualified multipolygon needs its complete normalized area geometry')
    polygons = [geometry.get('coordinates')] if geometry['type'] == 'Polygon' else geometry.get('coordinates')
    if not isinstance(polygons, list) or not polygons or len(polygons) > 256:
        _refuse('Normalized multipolygon components are absent or exceed capacity')
    edges = {'outer': Counter(), 'inner': Counter()}
    for polygon in polygons:
        if not isinstance(polygon, list) or not polygon or len(polygon) > 256:
            _refuse('Normalized multipolygon rings are absent or exceed capacity')
        for index, ring in enumerate(polygon):
            if (not isinstance(ring, list) or not 4 <= len(ring) <= 65536 or
                    not all(_point(p) for p in ring) or ring[0] != ring[-1]):
                _refuse('Normalized multipolygon ring is not finite and closed')
            nodes = []
            for point in ring:
                key = tuple(point)
                if key not in coordinates:
                    _refuse('Normalized multipolygon contains a coordinate absent from its original member nodes')
                nodes.append(coordinates[key])
            if len(set(nodes[:-1])) != len(nodes)-1:
                _refuse('Normalized multipolygon contains a repeated ring node')
            edges['outer' if index == 0 else 'inner'].update(tuple(sorted((a, b))) for a, b in zip(nodes, nodes[1:]))
    return edges


def osm_multipolygon_context(document, primitives, provider_feature_id):
    """Qualify literal nonbranching outer/inner rings and every native/normalized counterpart.

    The return retains normalized relation/member/node records and the native
    relation membership that area export omits. Callers pin both input documents
    and this optional identity; geographic validity and policy remain downstream.
    """
    if isinstance(document, dict) and isinstance(document.get('features'), list) and len(document['features']) > 65536:
        _refuse('Normalized multipolygon context exceeds its finite primitive capacity')
    normalized, native = _records(document), _native_records(primitives)
    relation = _primitive(normalized, provider_feature_id, 'relation')
    original = native.get(provider_feature_id)
    _match_native(relation, original)
    members = original.get('members')
    if (original.get('tags', {}).get('type') != 'multipolygon' or not isinstance(members, list) or
            not 1 <= len(members) <= 256):
        _refuse('This finite context requires an explicit multipolygon relation and complete member list')
    ways, nodes, coordinates, used, source_edges = [], {}, {}, set(), 0
    edges = {'outer': Counter(), 'inner': Counter()}
    degrees = {'outer': Counter(), 'inner': Counter()}
    for member in members:
        if (not isinstance(member, dict) or set(member) != {'type', 'ref', 'role'} or member['type'] != 'way' or
                type(member['ref']) is not int or member['ref'] <= 0 or not isinstance(member['role'], str) or
                member['role'] not in edges or member['ref'] in used):
            _refuse('Multipolygon context requires unique way members with explicit outer/inner roles')
        used.add(member['ref'])
        key = f"way/{member['ref']}"
        raw = native.get(key)
        if not isinstance(raw, dict) or not isinstance(raw.get('nodes'), list) or not 2 <= len(raw['nodes']) <= 65536:
            _refuse('Native relation member is missing or exceeds its finite node capacity')
        source_edges += len(raw['nodes'])-1
        if source_edges > 65536:
            _refuse('Complete native relation edges exceed the finite context capacity')
        way, ordered = osm_way_context(document, key)
        _match_native(way, raw)
        if raw.get('nodes') != way['properties']['@way_nodes'] or any(type(i) is not int for i in raw['nodes']):
            _refuse('Native and normalized member node order conflicts')
        ways.append(way)
        for node in ordered:
            key = node['provider_feature_id']; raw_node = native.get(key); _match_native(node, raw_node)
            point = [raw_node.get('lon'), raw_node.get('lat')]
            if not _point(point) or point != node['geometry']['coordinates']:
                _refuse('Native and normalized member node coordinates conflict')
            identifier = node['properties']['@id']; xy = tuple(point)
            if xy in coordinates and coordinates[xy] != identifier:
                _refuse('Coincident distinct member nodes need explicit topology qualification')
            nodes[identifier] = node; coordinates[xy] = identifier
        for a, b in zip(raw['nodes'], raw['nodes'][1:]):
            if a == b:
                _refuse('A multipolygon member has a collapsed source edge')
            edges[member['role']][tuple(sorted((a, b)))] += 1
            degrees[member['role']].update((a, b))
    if not edges['outer'] or any(n != 1 for group in edges.values() for n in group.values()) or any(
            n != 2 for group in degrees.values() for n in group.values()):
        _refuse('Incomplete, duplicated or branching member rings need separate source qualification')
    if edges != _geometry_edges(relation['geometry'], coordinates):
        _refuse('Normalized multipolygon area differs from its complete native outer/inner member edges')
    return {'relation': deepcopy(relation), 'primitive': deepcopy(original),
        'members': deepcopy(ways), 'nodes': [deepcopy(node) for _, node in sorted(nodes.items())]}
