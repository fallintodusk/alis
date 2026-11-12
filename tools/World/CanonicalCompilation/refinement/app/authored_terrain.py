"""Closed dry-only authored deltas over an immutable accepted final grid."""
from copy import deepcopy
import math
from pathlib import Path

from World.ExecutionEnvironment.geometry.api import execution_identity
from ...app.contracts import CompilerError, canonical_hash
from ...app.spatial import quantize
from ...app.terrain_water_coverage import _intersects
from .contracts import validate_document
from .geometry import SCHEMA, _domain, _point
from .identity import implementation_identity
from .retained_input import read_retained_input

KIND = 'authored_dry_terrain'
CONSTRUCTION = 'authored_dry_terrain_v1'
INPUT_SCHEMA = 'https://alis.world/schemas/world-compiler/authored-overlay-v2.json'
POLICY = {'height_rule': 'quantized_sum_on_immutable_final_grid_v1'}


def _refuse(message, **details):
    raise CompilerError('authored_terrain_invalid', message, **details)


def validate_input(document):
    if document.get('$schema') != INPUT_SCHEMA:
        _refuse('Authored Terrain requires its existing closed overlay contract')
    validate_document(document, Path('authored-overlay.json'))
    if document['feature_overrides'] or not document['terrain_patches'] or not document['provenance']:
        _refuse('Authored dry Terrain requires height patches and provenance only')
    provenance = [row['provenance_id'] for row in document['provenance']]
    patches = [row['patch_id'] for row in document['terrain_patches']]
    if len(set(provenance)) != len(provenance) or len(set(patches)) != len(patches):
        _refuse('Authored Terrain identities must be unique')
    if any('surface_weights' in row or row['provenance_ref'] not in provenance for row in document['terrain_patches']):
        _refuse('Authored Terrain cannot replace semantic weights or use unresolved provenance')


def admit(repo, operation):
    if (not isinstance(operation, dict) or set(operation) != {'kind', 'feature_ids', 'input'}
            or operation['kind'] != KIND or operation['feature_ids'] != []):
        _refuse('Authored Terrain operation fields are unsupported')
    retained = {}
    document, descriptor = read_retained_input(repo, operation['input'], retained, _refuse)
    validate_input(document)
    return {'kind': KIND, 'feature_ids': [], 'base_features': {}, 'input': descriptor}, retained, document


def selection(cells, document, grid):
    validate_input(document)
    selected, changed = {}, {}
    for key, cell in sorted(cells.items()):
        west, south, east, north = cell['bounds']
        patches = [p for p in document['terrain_patches'] if
                   west-p['radius_m'] <= p['center'][0] <= east+p['radius_m'] and
                   south-p['radius_m'] <= p['center'][1] <= north+p['radius_m']]
        if not patches:
            continue
        if cell.get('grid_id') != document['grid_id'] or cell.get('$schema') != SCHEMA.replace('terrain-refinement-v1', 'terrain-cell-v2'):
            _refuse('Authored Terrain needs unchanged regular grids in its declared frame')
        rows = cell['core_samples']
        quads = set()
        for y, row in enumerate(rows):
            for x, old in enumerate(row):
                point = _point(cell, x, y)
                delta = sum(p['delta_m'] for p in patches if
                            math.hypot(point[0]-p['center'][0], point[1]-p['center'][1]) <= p['radius_m'])
                if delta == 0:
                    continue
                if not math.isfinite(old+delta):
                    _refuse('Authored Terrain produces a nonfinite height')
                final = quantize(old+delta, grid['height_quantization'])
                if final != old:
                    changed[key, x, y] = final
                    quads.update((i, j) for i in (x-1, x) for j in (y-1, y)
                                 if 0 <= i < len(row)-1 and 0 <= j < len(rows)-1)
        if quads:
            selected[key] = sorted(quads)
    if not selected:
        _refuse('Authored intent changes no grid node; retain the accepted base')
    return selected, changed


def validate_consumers(selected, features):
    for feature in features.values():
        attrs = feature.get('attributes', {})
        references = set(attrs.get('civil_grade_reference', {}).get('terrain_artifacts', {}))
        references.update(attrs.get('road_elevation_profile', {}).get('support_terrain_artifacts', {}))
        if references.intersection(selected):
            _refuse('Authored Terrain conflicts with retained Terrain-bound civil support', feature_id=feature['feature_id'])
        if ('constructed_surface' in attrs or attrs.get('surface_function', {}).get('function_id') == 'identified_static_lined_basin'):
            if set(feature['intersecting_cell_ids']).intersection(selected):
                _refuse('Authored Terrain conflicts with retained constructed support', feature_id=feature['feature_id'])


def domain_identity(coverage, cells, selected):
    domain = _domain(cells, selected)
    return canonical_hash({'frame': coverage['grid'], 'grid_id': coverage['grid_id'],
        'quads': [{'cell_id': key, 'coordinate': [x, y], 'original_corners': list(points)}
                  for _, (key, x, y, points) in sorted(domain.items())]})


def cell_quads(cells, key, coordinates, changed):
    quads = []
    for x, y in coordinates:
        vertices = [_point(cells[key], i, j) for i, j in ((x, y), (x+1, y), (x, y+1), (x+1, y+1))]
        for vertex, (i, j) in zip(vertices, ((x, y), (x+1, y), (x, y+1), (x+1, y+1))):
            vertex[2] = changed.get((key, i, j), vertex[2])
        quads.append({'coordinate': [x, y], 'vertices': vertices,
                      'triangles': [[0, 2, 1], [1, 2, 3]], 'water_triangles': []})
    return quads


def construct(repo, operation, document, coverage, cells, features):
    if document['grid_id'] != coverage['grid_id']:
        _refuse('Authored Terrain targets another immutable grid')
    selected, changed = selection(cells, document, coverage['grid'])
    domain = _domain(cells, selected)
    for feature in features.values():
        if feature['feature_class'] == 'water' and any(_intersects(feature, bounds) for bounds in domain):
            _refuse('Complete authored Terrain support intersects Water', feature_id=feature['feature_id'])
    validate_consumers(selected, features)
    domain_hash = domain_identity(coverage, cells, selected)
    consumed = {'authored_input_sha256': operation['input']['sha256'], 'domain_sha256': domain_hash,
                'implementation_sha256': implementation_identity(),
                'execution_identity': execution_identity(repo, True)['identity_sha256']}
    output = {}
    for key, coordinates in sorted(selected.items()):
        quads = cell_quads(cells, key, coordinates, changed)
        deltas = [value-cells[key]['core_samples'][y][x] for (owner, x, y), value in changed.items() if owner == key]
        output[key] = {'$schema': SCHEMA, 'schema_version': 1, 'grid_id': coverage['grid_id'], 'cell_id': key,
                       'construction': CONSTRUCTION, 'coordinate_rule': 'canonical_boundary_intersections_float64',
                       'consumed': deepcopy(consumed), 'policy': dict(POLICY), 'quads': quads,
                       'metrics': {'changed_nodes': len(deltas), 'affected_quads': len(quads),
                                   'maximum_cut_m': max(0., -min(deltas)), 'maximum_fill_m': max(0., max(deltas))}}
    return output
