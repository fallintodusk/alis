"""Joint inferred basin admission through the existing surface/Terrain publisher."""
from copy import deepcopy
import math
from pathlib import Path

from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.affine import execution_identity as affine_identity
from World.ExecutionEnvironment.geometry.boundary_rounding import execution_identity as rounding_identity
from World.ExecutionEnvironment.geometry.constrained import execution_identity as geometry_identity
from ..app.contracts import CompilerError, canonical_hash, file_hash, read_json
from ..app.terrain_water_coverage import _intersects
from ..app.spatial import geometry_bounds, geometry_points
from . import identified_basin
from .app.civil_grade import support_bounds
from .app.contracts import validate_document
from .app.geometry import _domain, _external_edges, _check_outer, _check_seams
from .app.retained_input import read_retained_input

KIND = 'identified_basin'
PREFIX = 'https://alis.world/schemas/world-compiler/'


def _refuse(message, **details):
    raise CompilerError('identified_basin_admission_invalid', message, **details)


def implementation_identity(repo):
    owner = Path(__file__).resolve().parent
    source = owner.parents[1]/'SourceIngestion/supplemental/contracts'
    return canonical_hash({'admission': file_hash(Path(__file__)),
        'input_contract': file_hash(owner/'contracts/identified_basin/input.schema.json'),
        'surface_contract': file_hash(owner.parent/'contracts/identified_basin/surface.schema.json'),
        'source_contracts': {name: file_hash(source/name) for name in ('ground-grid.schema.json', 'derived-ground.schema.json')},
        'constructor': identified_basin.implementation_identity(),
        'geometry': geometry_identity(repo, True)['identity_sha256'],
        'rounding': rounding_identity(repo, True)['identity_sha256'],
        'affine': affine_identity(repo, True)['identity_sha256']})


def admit(repo, operation, all_features):
    if not isinstance(operation, dict) or set(operation) != {'kind', 'feature_ids', 'input'} or operation['kind'] != KIND:
        _refuse('Identified basin operation fields are unsupported')
    ids = operation['feature_ids']
    if not isinstance(ids, list) or len(ids) != 1 or not isinstance(ids[0], str) or ids[0] not in all_features:
        _refuse('Identified basin requires one immutable base Water feature')
    retained = {}
    document, descriptor = read_retained_input(repo, operation['input'], retained, _refuse)
    validate_document(document, Path(descriptor['path']))
    if document.get('$schema') != PREFIX+'identified-basin-input-v1.json':
        _refuse('Retained basin input differs from its declared role contract')
    original = all_features[ids[0]]
    if (original['feature_class'] != 'water' or canonical_hash(document['original_feature']) != canonical_hash(original) or
            original['attributes'].get('surface_function', {}).get('function_id') == 'identified_static_lined_basin'):
        _refuse('Basin original feature is missing, changed or already modeled')
    selected = {'kind': KIND, 'feature_ids': ids[:], 'base_features': {ids[0]: canonical_hash(original)},
        'input': descriptor, 'construction_identity': implementation_identity(repo)}
    return selected, retained, document


def _touches(a, b):
    return a[0] <= b[2] and b[0] <= a[2] and a[1] <= b[3] and b[1] <= a[3]


def _civil_bounds(feature, cells):
    if feature['feature_class'] == 'road':
        return support_bounds(feature, cells)
    attributes = feature['attributes']
    geometries = ([v['geometry'] for v in attributes['effective_volumes']]
        if feature['feature_class'] == 'building' and 'effective_volumes' in attributes else [feature['geometry']])
    bounds = []
    for geometry in geometries:
        try:
            if geometry['type'] not in ('Polygon', 'MultiPolygon'):
                _refuse('Basin civil interaction requires physical polygon footprints')
            points = geometry_points(geometry)
            if not points or any(len(p) != 2 or any(type(v) not in (int, float) or not math.isfinite(v) for v in p)
                    for p in points):
                _refuse('Basin civil footprint is incomplete or nonfinite')
            bounds.append(geometry_bounds(geometry))
        except (KeyError, TypeError, ValueError) as error:
            _refuse('Basin civil footprint is malformed', feature_id=feature['feature_id'], reason=str(error))
    if not bounds:
        _refuse('Basin civil footprint is absent', feature_id=feature['feature_id'])
    return bounds


def construct(repo, operation, document, coverage, cells, all_features):
    original = all_features[operation['feature_ids'][0]]
    crs = coverage['grid']['canonical_crs']
    if document['model']['canonical_crs'] != crs:
        _refuse('Basin input and immutable grid use different canonical frames')
    if document['grid']['size'][0]*document['grid']['size'][1] > 4096:
        _refuse('Basin reference grid exceeds its finite node capacity')
    if any(key in cells and cells[key].get('$schema') != PREFIX+'terrain-cell-v2.json' for key in document['selected']):
        _refuse('Basin construction requires immutable regular-grid support')
    domain = _domain(cells, document['selected'])
    incident = sorted(feature['feature_id'] for feature in all_features.values()
        if feature['feature_class'] in ('road', 'building', 'constructed_surface') and any(_touches(box, bound)
            for box in _civil_bounds(feature, cells) for bound in domain))
    if incident:
        _refuse('Selected basin has unresolved civil support interactions', feature_ids=incident)
    other = sorted(feature['feature_id'] for feature in all_features.values()
        if feature['feature_class'] == 'water' and feature['feature_id'] != original['feature_id'] and
        (feature['attributes'].get('surface_group_id') == original['attributes'].get('surface_group_id') or
            any(_intersects(feature, bound) for bound in domain)))
    if other or len(original['attributes'].get('surface_group_members', [])) != 1:
        _refuse('Selected basin requires a complete independent Water group', feature_ids=other)
    try:
        result = identified_basin.build_identified_basin(repo, cells=cells, grid=document['grid'], evidence=document['evidence'],
            selected=document['selected'], water=original, model=document['model'], canonical_crs=crs,
            maximum_triangles=document['maximum_triangles'])
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code, str(error), **error.details) from error
    exterior = _external_edges(domain)
    _check_seams(result['surfaces'], domain)
    for surface in result['surfaces'].values():
        for quad in surface['quads']:
            for vertex in quad['vertices']:
                _check_outer(vertex, exterior)
    wet_cells = sorted(key for key, surface in result['surfaces'].items() if any(q['water_triangles'] for q in surface['quads']))
    if wet_cells != sorted(original['intersecting_cell_ids']) or original['owner_cell_id'] not in wet_cells:
        _refuse('Modeled basin changes original Water cell membership')
    metrics = {name: result['metrics'][name] for name in ('maximum_gradient', 'modeled_spill_margin_m', 'modeled_wet_volume_m3')}
    metrics['maximum_outside_reference_error_upper_m'] = result['metrics']['whole_outside_reference_field']['maximum_height_error_upper_m']
    consumed = {**result['consumed'], 'model_input_sha256': operation['input']['sha256']}
    payloads = {key: {'$schema': PREFIX+'terrain-refinement-v1.json', 'schema_version': 1,
        'grid_id': cells[key]['grid_id'], 'cell_id': key, 'coordinate_rule': 'canonical_boundary_intersections_float64',
        'construction': identified_basin.RULE, 'consumed': deepcopy(consumed),
        'policy': deepcopy(document['model']['parameters']), 'metrics': deepcopy(metrics), 'quads': deepcopy(value['quads'])}
        for key, value in result['surfaces'].items()}
    feature = deepcopy(original)
    feature['geometry'] = deepcopy(result['modeled_water']['geometry'])
    feature['attributes']['surface_function'] = {'function_id': 'identified_static_lined_basin', 'function_version': 1,
        'vertical_basis': 'identified_static_basin_pwa', 'accuracy': 'identified_inferred_static_model_not_surveyed',
        'canonical_crs': crs, 'vertical_datum': 'EPSG:3855', 'level_m': result['modeled_water']['level_m'],
        'spill_level_m': result['modeled_water']['spill_level_m'], 'model_input': deepcopy(operation['input']),
        'source_feature_sha256': canonical_hash(original), 'model_sha256': result['consumed']['model_sha256'],
        'boundary_encoding_sha256': canonical_hash(result['boundary_encoding_certificates']['wet'])}
    proof = {'model_input_sha256': operation['input']['sha256'], 'source_feature_sha256': canonical_hash(original),
        'model_sha256': result['consumed']['model_sha256'], 'metrics': metrics,
        'boundary_encoding': result['boundary_encoding_certificates']['wet']}
    return feature, payloads, proof


def bind_staged_support(staging, coverage, feature, cells):
    support = {}
    for key in sorted(cells):
        entry = next(e for e in coverage['cells'] if e['cell_id'] == key)
        descriptor = read_json(staging/entry['path'])['terrain_artifact']
        support[key] = {'path': descriptor['path'], 'sha256': descriptor['content_hash'], 'byte_size': descriptor['byte_size']}
    feature['attributes']['surface_function']['terrain_support'] = support
