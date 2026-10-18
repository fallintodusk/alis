"""Preserve declared civil grades without assigning unobserved structure semantics."""
from copy import deepcopy
import math
from pathlib import Path

from ...app.contracts import CompilerError, canonical_hash
from .contracts import validate_document


def _refuse(message):
    raise CompilerError('civil_grade_invalid', message)


def _bounds(points):
    if not points or any(len(p) != 2 or any(type(v) not in (int, float) or not math.isfinite(v)
                                           for v in p) for p in points):
        _refuse('Civil grade has incomplete or nonfinite planform')
    return [min(p[0] for p in points), min(p[1] for p in points),
            max(p[0] for p in points), max(p[1] for p in points)]


def _touches(a, b):
    return a[0] <= b[2] and a[2] >= b[0] and a[1] <= b[3] and a[3] >= b[1]


def support_bounds(feature, cells):
    geometry, attributes = feature['geometry'], feature['attributes']
    if feature['feature_class'] == 'road':
        lines = ([geometry['coordinates']] if geometry['type'] == 'LineString' else
                 geometry['coordinates'] if geometry['type'] == 'MultiLineString' else [])
        width = attributes.get('width_m')
        if type(width) not in (int, float) or not math.isfinite(width) or width <= 0 or not lines:
            _refuse('Road grade needs its admitted width and source lines')
        bounds = []
        for line in lines:
            if len(line) < 2:
                _refuse('Road grade has an incomplete source line')
            for a, b in zip(line, line[1:]):
                box = _bounds([a, b])
                bounds.append([box[0]-width/2, box[1]-width/2, box[2]+width/2, box[3]+width/2])
        return bounds
    if feature['feature_class'] != 'building' or feature['owner_cell_id'] not in cells:
        _refuse('Civil grade requires an available Road or Building owner')
    polygons = ([geometry['coordinates']] if geometry['type'] == 'Polygon' else
                geometry['coordinates'] if geometry['type'] == 'MultiPolygon' else [])
    box = _bounds([p for polygon in polygons for p in polygon[0]])
    owner = cells[feature['owner_cell_id']]['bounds']
    x, y = ((box[0]+box[2])/2, (box[1]+box[3])/2)
    x, y = min(max(x, owner[0]), owner[2]), min(max(y, owner[1]), owner[3])
    return [[x, y, x, y]]


def preserve_civil_grades(features, cells, surfaces, manifests):
    """Whole-quad census includes dry anchors; no Water/civil precedence is inferred."""
    domains = []
    for identifier, surface in surfaces.items():
        terrain = cells[identifier]
        for quad in surface['quads']:
            x, y = quad['coordinate']
            west = terrain['bounds'][0] + x*terrain['sample_spacing'][0]
            north = terrain['bounds'][3] - y*terrain['sample_spacing'][1]
            domains.append((identifier, [west, north-terrain['sample_spacing'][1],
                                         west+terrain['sample_spacing'][0], north]))
    changes = {}
    for feature in features:
        if feature['feature_class'] not in ('road', 'building'):
            continue
        bounds = support_bounds(feature, cells)
        if not any(_touches(a, b) for a in bounds for owner, b in domains
                   if feature['feature_class'] == 'road' or owner == feature['owner_cell_id']):
            continue
        attributes = feature['attributes']
        context = attributes.get('surface_context', {})
        if (context.get('ground_relation') in ('below', 'conflicting') or
                context.get('ford') == 'present' or context.get('tunnel') == 'present' or
                any(value in {'unsupported_bridge', 'unsupported_ford', 'unsupported_tunnel',
                              'crossing_mode_required', 'conflicting_bridge_tunnel'}
                    for value in context.get('unresolved', []))):
            _refuse('Conflicting, ford or tunnel evidence requires its own relationship policy')
        if 'civil_grade_reference' in attributes:
            _refuse('Civil preservation requires the immutable original feature')
        support = ([feature['owner_cell_id']] if feature['feature_class'] == 'building' else
                   [key for key, cell in cells.items() if any(_touches(box, cell['bounds']) for box in bounds)])
        if not support:
            _refuse('Civil grade has no authenticated grid support')
        reference = {'$schema': 'https://alis.world/schemas/world-compiler/civil-grade-reference-v1.json',
            'version': 1, 'model': 'preserve_accepted_grid_grade_not_structure_classification',
            'base_feature_sha256': canonical_hash(feature), 'terrain_artifacts':
                {key: manifests[key]['terrain_artifact']['content_hash'] for key in sorted(support)}}
        for key in support:
            if cells[key].get('$schema') != 'https://alis.world/schemas/world-compiler/terrain-cell-v2.json':
                _refuse('Civil preservation requires one immutable regular grid, without nested refinement')
        profile = attributes.get('road_elevation_profile')
        if profile and profile['support_terrain_artifacts'] != reference['terrain_artifacts']:
            _refuse('Existing Road profile is not qualified by the same immutable support')
        validate_document(reference, Path('civil-grade-reference.json'))
        current = deepcopy(feature)
        current['attributes']['civil_grade_reference'] = reference
        changes[current['feature_id']] = current
    return changes
