"""Import declared identified fields with complete source bindings; never refit or publish authority."""
from copy import deepcopy
from datetime import date, datetime, timezone
from fractions import Fraction as F
import math
from pathlib import Path
import re

from World.SourceIngestion.api import IngestionError, osm_context_identity, osm_multipolygon_context
from World.SourceIngestion.api import osm_multipolygon_context_identity, osm_way_context
from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.api import validate_triangle_cover
from ..app.contracts import CompilerError, canonical_hash, file_hash
from ..app.adapters import canonical_feature_id
from ..app.features import _representations
from ..app.membership import feature_cell_membership
from ..app.projection import project_features
from ..app.spatial import cell_id, geometry_bounds
from .app.contracts import validate_document
from .app.retained_input import read_retained_input

KIND = 'constructed_surface_import'


def _refuse(message):
    raise CompilerError('constructed_import_invalid', message)


def implementation_identity(qualification):
    root = Path(__file__).resolve().parent
    names = ('constructed_import.py', '../app/adapters.py', '../contracts/constructed-surface.schema.json',
        'contracts/constructed-import-model.schema.json', 'contracts/constructed-import-policy.schema.json')
    if qualification not in ('way_context', 'way_and_multipolygon_context'):
        _refuse('Unknown structural source qualification contract')
    return canonical_hash({'files': {name: file_hash(root/name) for name in names},
        'source': osm_context_identity(), 'multipolygon': osm_multipolygon_context_identity()
        if qualification == 'way_and_multipolygon_context' else None})


def _read_input(repo, descriptor, retained, *, native=False):
    return read_retained_input(repo, descriptor, retained, _refuse, native=native)


def admit(repo, operation, all_features):
    if not isinstance(operation, dict) or set(operation) != {'kind', 'feature_ids', 'inputs'} or operation['kind'] != KIND:
        _refuse('Structural import operation fields are unsupported')
    identifiers = operation['feature_ids']; inputs = operation['inputs']
    if (not isinstance(identifiers, list) or not 1 <= len(identifiers) <= 16 or
            any(not isinstance(v, str) or not v for v in identifiers) or len(set(identifiers)) != len(identifiers) or
            set(identifiers) & all_features.keys()):
        _refuse('Structural creation requires globally absent, distinct declared canonical IDs')
    if not isinstance(inputs, dict) or set(inputs) != {'model', 'policy', 'source_contexts'}:
        _refuse('Structural import requires its declared model, policy and source inputs')
    contexts = inputs['source_contexts']
    if not isinstance(contexts, list) or not 1 <= len(contexts) <= 16:
        _refuse('Structural source context census is empty or exceeds capacity')
    retained, stored_contexts, documents = {}, [], {}
    model, stored_model = _read_input(repo, inputs['model'], retained)
    policy, stored_policy = _read_input(repo, inputs['policy'], retained)
    prefix = 'https://alis.world/schemas/world-compiler/'
    if model.get('$schema') != prefix+'constructed-import-model-v1.json' or policy.get('$schema') != prefix+'constructed-import-policy-v1.json':
        _refuse('Structural model and policy inputs must use their declared role contracts')
    validate_document(model, Path('constructed-model.json')); validate_document(policy, Path('constructed-policy.json'))
    for context in contexts:
        if (not isinstance(context, dict) or set(context) != {'id', 'normalized', 'native'} or
                not isinstance(context['id'], str) or not re.fullmatch('[a-z0-9_]{1,64}', context['id']) or context['id'] in documents):
            _refuse('Structural source context identities are unknown or repeated')
        document, stored = _read_input(repo, context['normalized'], retained)
        native, stored_native = _read_input(repo, context['native'], retained, native=True) if context['native'] is not None else (None, None)
        documents[context['id']] = (document, native)
        stored_contexts.append({'id': context['id'], 'normalized': stored, 'native': stored_native})
    used_contexts = {binding['context_id'] for item in model['models'] for binding in item['source_bindings']}
    if used_contexts != set(documents):
        _refuse('Structural source context closure has missing or unused inputs')
    relation_contexts = {binding['context_id'] for item in model['models'] for binding in item['source_bindings']
        if binding['provider_feature_id'].startswith('relation/')}
    if any((native is not None) != (identifier in relation_contexts) for identifier, (_, native) in documents.items()):
        _refuse('Native relation snapshots must match exactly the consumed multipolygon contexts')
    qualification = ('way_and_multipolygon_context' if any(binding['provider_feature_id'].startswith('relation/')
        for item in model['models'] for binding in item['source_bindings']) else 'way_context')
    admitted = {'kind': KIND, 'feature_ids': sorted(identifiers), 'base_features': {},
        'inputs': {'model': stored_model, 'policy': stored_policy, 'source_contexts': sorted(stored_contexts, key=lambda c: c['id'])},
        'source_qualification': qualification, 'construction_identity': implementation_identity(qualification)}
    return admitted, retained, (model, policy, documents)


def _finite(value):
    try:
        return type(value) in (int, float) and math.isfinite(value)
    except OverflowError:
        return False


def _cross(a, b, p):
    return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])


def _height(face, point):
    a, b, c = face; den = _cross(a, b, c)
    u = ((point[0]-a[0])*(c[1]-a[1])-(point[1]-a[1])*(c[0]-a[0]))/den
    v = ((b[0]-a[0])*(point[1]-a[1])-(b[1]-a[1])*(point[0]-a[0]))/den
    return a[2]+u*(b[2]-a[2])+v*(c[2]-a[2])


def _prove_field(repo, item, policy, projected):
    faces = item['triangles']
    if len(faces) > policy['maximum_triangles'] or any(not _finite(v) for face in faces for p in face for v in p):
        _refuse('Structural field exceeds its finite coordinate/triangle contract')
    polygons = item['footprint']['coordinates'] if item['footprint']['type'] == 'MultiPolygon' else [item['footprint']['coordinates']]
    if any(ring[0] != ring[-1] or any(not _finite(v) for p in ring for v in p) for polygon in polygons for ring in polygon):
        _refuse('Structural model footprint is nonfinite or open')
    try:
        validate_triangle_cover(repo, item['footprint'], [[p[:2] for p in face] for face in faces])
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code, str(error), **error.details) from error
    indices = [i for role in item['face_roles'] for i in role['triangles']]
    if sorted(indices) != list(range(len(faces))):
        _refuse('Structural face roles must cover every face exactly once')
    source_ids = {binding['provider_feature_id'] for binding in item['source_bindings']}
    exact = [[list(map(F, p)) for p in face] for face in faces]
    if any(_cross(*face) == 0 for face in exact):
        _refuse('A structural field face is collapsed')
    boxes = [(min(p[0] for p in f), min(p[1] for p in f), max(p[0] for p in f), max(p[1] for p in f)) for f in faces]
    vertices = {tuple(p) for face in faces for p in face}
    error, ordinary, grade, cross = F(0), F(0), F(0), F(0)
    for point in vertices:
        p = list(map(F, point))
        for face, bounds in zip(exact, boxes):
            if not bounds[0] <= point[0] <= bounds[2] or not bounds[1] <= point[1] <= bounds[3]:
                continue
            signs = [_cross(a, b, p) for a, b in zip(face, (*face[1:], face[0]))]
            if all(v >= 0 for v in signs) or all(v <= 0 for v in signs):
                error = max(error, abs(_height(face, p)-p[2]))
    for role in item['face_roles']:
        if not set(role['source_ids']) <= source_ids:
            _refuse('A structural face role has unbound source membership')
        direction = None
        if role['role'] == 'gangway':
            axis = projected.get(role['axis_source_id'], {})
            points = axis.get('coordinates')
            if (role['axis_source_id'] not in role['source_ids'] or axis.get('type') != 'LineString' or
                    not isinstance(points, list) or len(points) != 2):
                _refuse('Gangway role requires its exact bound straight source axis')
            delta = [points[1][k]-points[0][k] for k in (0, 1)]; length = math.hypot(*delta)
            if not _finite(length) or length <= 0:
                _refuse('Gangway source axis is collapsed or nonfinite')
            direction = list(map(F, (d/length for d in delta)))
        for index in role['triangles']:
            a, b, c = exact[index]; den = _cross(a, b, c)
            if den == 0:
                _refuse('A structural field face is collapsed')
            gx = ((b[2]-a[2])*(c[1]-a[1])-(c[2]-a[2])*(b[1]-a[1]))/den
            gy = ((b[0]-a[0])*(c[2]-a[2])-(c[0]-a[0])*(b[2]-a[2]))/den
            if direction is None:
                ordinary = max(ordinary, gx*gx+gy*gy)
            else:
                tx, ty = direction
                grade = max(grade, abs(gx*tx+gy*ty)); cross = max(cross, abs(-gx*ty+gy*tx))
    if (error > F(policy['maximum_numeric_error_m']) or ordinary > F(policy['maximum_ordinary_gradient'])**2 or
            grade > F(policy['maximum_gangway_grade']) or cross > F(policy['maximum_cross_slope'])):
        _refuse('Structural field violates its complete continuity or predeclared role quality')
    return {'faces': len(faces), 'continuity_error_m': float(error), 'maximum_ordinary_gradient': math.sqrt(float(ordinary)),
        'maximum_gangway_grade': float(grade), 'maximum_cross_slope': float(cross)}


def _member_metric_interpretation(record, member_ids):
    props = record['properties']
    metrics = {name for name in ('ele', 'height', 'min_height', 'depth') if name in props}
    if not metrics:
        return None
    value = props.get('height')
    conflicts = ('man_made', 'building', 'building:part', 'highway', 'waterway', 'natural',
        'bridge', 'tunnel', 'covered', 'floating', 'layer')
    if (record['provider_feature_id'] not in member_ids or props.get('@type') != 'way' or
            props.get('barrier') != 'fence' or metrics != {'height'} or
            any(name in props for name in conflicts) or not isinstance(value, str) or
            not re.fullmatch(r'[0-9]+(?:\.[0-9]+)?', value)):
        _refuse(f"Structural source metric tags need separate role qualification: {record['provider_feature_id']}")
    height = float(value)
    if not math.isfinite(height) or height <= 0:
        _refuse(f"A member fence height must be a positive finite bare-meter dimension: {record['provider_feature_id']}")
    return {'provider_feature_id': record['provider_feature_id'], 'key': 'height', 'source_value': value,
        'height_m': height, 'meaning': 'barrier_height_above_support_not_surface_elevation',
        'support_elevation_m': None, 'physical_realization': 'unresolved'}


def _qualify(document, native, identifier):
    try:
        if identifier.startswith('relation/'):
            result = osm_multipolygon_context(document, native, identifier)
            primitive = result['relation']; records = [primitive, *result['members'], *result['nodes']]
            identity = osm_multipolygon_context_identity()
        else:
            primitive, nodes = osm_way_context(document, identifier)
            result = {'way': primitive, 'nodes': list(nodes)}; records = [primitive, *nodes]
            identity = osm_context_identity()
        cutoff = datetime.combine(date.fromisoformat(document['release']), datetime.min.time(), timezone.utc)
        member_ids = {member['provider_feature_id'] for member in result.get('members', [])}
        interpretations = []
        for record in records:
            props = record['properties']; stamp = props['@timestamp']
            primitive_stamp = datetime.fromtimestamp(stamp, timezone.utc) if type(stamp) is int else datetime.fromisoformat(stamp.replace('Z', '+00:00'))
            if primitive_stamp > cutoff:
                _refuse('Structural source primitive postdates its declared release')
            interpretation = _member_metric_interpretation(record, member_ids)
            if interpretation is not None:
                interpretations.append(interpretation)
        if interpretations:
            result = {**result, 'source_metric_interpretations': interpretations}
    except IngestionError as error:
        raise CompilerError(error.code, str(error), **error.details) from error
    except (ValueError, OverflowError, OSError, KeyError, TypeError) as error:
        raise CompilerError('constructed_import_invalid', 'Structural source release is unqualified') from error
    return primitive, result, records, identity


def _prove_roles(item, selected):
    primitives = {name: primitive for name, primitive, _, _ in selected}
    results = {name: result for name, _, result, _ in selected}
    allowed = {'fixed_pier': {'fixed_pier'}, 'floating_assembly': {'floating_deck', 'gangway'},
        'inferred_apron': {'inferred_apron'}, 'inferred_gate_apron': {'inferred_gate_apron'}}
    if item['role'] in allowed and any(role['role'] not in allowed[item['role']] for role in item['face_roles']):
        _refuse('Structural model and exhaustive face roles contradict each other')
    primary = primitives[item['primary_source_id']]['properties']
    if (item['role'] in ('fixed_pier', 'inferred_apron') and primary.get('floating') != 'no' or
            item['role'] == 'floating_assembly' and primary.get('floating') != 'yes'):
        _refuse('Structural model role lacks its explicit fixed/floating source context')
    for role in item['face_roles']:
        if any(name not in primitives for name in role['source_ids']):
            _refuse('A structural face role has unknown original source membership')
        props = [primitives[name]['properties'] for name in role['source_ids']]
        if role['role'] == 'fixed_pier' and not any(p.get('man_made') == 'pier' and p.get('floating') == 'no' for p in props):
            _refuse('Fixed-pier face role lacks explicit source context')
        if role['role'] == 'floating_deck' and not any(p.get('man_made') == 'pier' and p.get('floating') == 'yes' for p in props):
            _refuse('Floating-deck face role lacks explicit source context')
        if role['role'] == 'gangway' and primitives.get(role['axis_source_id'], {}).get('properties', {}).get('bridge') != 'yes':
            _refuse('Gangway source axis needs its qualified bridge relationship')
        if role['role'] == 'inferred_gate_apron' and not any(
                primitives[name]['geometry'].get('type') == 'LineString' and
                len(primitives[name]['geometry'].get('coordinates', [])) == 2 and
                any(node['properties'].get('barrier') == 'gate' for node in results[name].get('nodes', []))
                for name in role['source_ids']):
            _refuse('Inferred gate-apron role lacks its complete mapped gate/stub context')


def construct(repo, admitted, documents, coverage, manifests, scratch):
    model, policy, contexts = documents; grid = coverage['grid']
    if (model['canonical_crs'] != grid['canonical_crs'] or model['vertical_datum'] != grid['vertical_datum'] or
            any(not _finite(v) for k, v in policy.items() if k not in ('$schema', 'schema_version'))):
        _refuse('Structural model frame or numeric policy is unqualified')
    namespaces = {m['feature_lineage']['identity_namespace'] for m in manifests.values()}
    if len(namespaces) != 1:
        _refuse('Structural creation needs one authenticated canonical identity namespace')
    namespace = next(iter(namespaces)); qualified, primitives, pins = {}, {}, {}
    stored_contexts = {c['id']: c for c in admitted['inputs']['source_contexts']}
    for item in model['models']:
        seen = set()
        for binding in item['source_bindings']:
            key = binding['context_id'], binding['provider_feature_id']
            if key in seen or key[0] not in contexts:
                _refuse('Structural source association is repeated or missing')
            seen.add(key)
            if key in qualified:
                continue
            document, native = contexts[key[0]]
            primitive, result, records, identity = _qualify(document, native, key[1])
            for record in records:
                name = record['provider_feature_id']
                if name in primitives and primitives[name] != record:
                    _refuse('Consumed structural contexts contradict the same original primitive')
                primitives[name] = record
            qualified[key] = (primitive, result, identity)
            pins[key] = stored_contexts[key[0]]
    projected, missing = project_features(repo, list(primitives.values()), grid, scratch/'source-projection')
    if missing:
        _refuse('Structural source projection is incomplete')
    changes, proofs = {}, {}
    available = {(m['cell_x'], m['cell_y']): identifier for identifier, m in manifests.items()}
    for item in model['models']:
        identifier = canonical_feature_id(namespace, item['primary_source_id'])
        if identifier in changes or identifier not in admitted['feature_ids']:
            _refuse('Structural model primary source differs from its exact declared creation IDs')
        sources, references, selected = [], [], []
        for binding in item['source_bindings']:
            key = binding['context_id'], binding['provider_feature_id']; primitive, result, identity = qualified[key]
            document = contexts[key[0]][0]; stored = pins[key]
            sources.append({'context_id': key[0], 'provider_feature_id': key[1], 'qualification_identity': identity,
                'qualified': deepcopy(result), 'projected_geometry': deepcopy(projected[key[1]]),
                'normalized_input': deepcopy(stored['normalized']),
                'native_input': None if stored['native'] is None else {k: v for k, v in stored['native'].items() if k != 'format'}})
            references.append({'provider': document['provider'], 'snapshot_id': document['snapshot_id'],
                'release': document['release'], 'provider_feature_id': key[1]})
            selected.append((key[1], primitive, result, identity))
        primary = [p for name, p, _, _ in selected if name == item['primary_source_id']]
        if len(primary) != 1 or primary[0]['properties'].get('man_made') != 'pier':
            _refuse('Structural primary source needs one explicit original pier context')
        _prove_roles(item, selected)
        proof = _prove_field(repo, item, policy, projected)
        bounds = geometry_bounds(item['footprint']); width = grid['sample_spacing'][0]*grid['cell_quads'][0]; height = grid['sample_spacing'][1]*grid['cell_quads'][1]
        xmin, xmax = math.floor((bounds[0]-grid['origin'][0])/width), math.floor((bounds[2]-grid['origin'][0])/width)
        ymin, ymax = math.floor((bounds[1]-grid['origin'][1])/height), math.floor((bounds[3]-grid['origin'][1])/height)
        if (xmax-xmin+3)*(ymax-ymin+3) > 4096:
            _refuse('Structural model membership exceeds its finite neighborhood capacity')
        candidates = [(x, y) for x in range(xmin-1, xmax+2) for y in range(ymin-1, ymax+2)]
        members = feature_cell_membership(repo, {identifier: item['footprint']}, grid, candidates, scratch/identifier.replace(':', '_'))[identifier]
        if not members or any(tuple(c) not in available for c in members):
            _refuse('Complete structural model requires missing canonical cell context')
        member_ids = [cell_id(coverage['grid_id'], *c) for c in members]; owner = member_ids[0]
        semantic = {'basis': 'identified_model', 'role': item['role'], 'frame': [model['canonical_crs'], model['vertical_datum']],
            'footprint': item['footprint'], 'triangles': item['triangles'], 'face_roles': item['face_roles'],
            'sources': [{'source_id': name, 'qualified': result, 'qualification_identity': identity} for name, _, result, identity in selected],
            'policy_sha256': admitted['inputs']['policy']['sha256']}
        surface = {'schema_version': 1, 'source_feature_id': identifier, 'model_sha256': canonical_hash(semantic),
            'canonical_crs': model['canonical_crs'], 'vertical_datum': model['vertical_datum'], 'basis': 'identified_model',
            'role': item['role'], 'triangles': deepcopy(item['triangles']), 'face_roles': deepcopy(item['face_roles']),
            'source_bindings': sources, 'policy_sha256': admitted['inputs']['policy']['sha256'], 'model_provenance': deepcopy(model['provenance'])}
        feature = {'feature_id': identifier, 'feature_class': 'constructed_surface', 'geometry': deepcopy(item['footprint']),
            'attributes': {'constructed_surface': surface}, 'source_refs': references, 'owner_cell_id': owner,
            'intersecting_cell_ids': member_ids, 'representations': [], 'provenance_result': 'accepted'}
        feature['representations'] = _representations(feature, members, coverage['grid_id'], grid, owner)
        changes[identifier] = feature; proofs[identifier] = proof
    if set(changes) != set(admitted['feature_ids']) or implementation_identity(admitted['source_qualification']) != admitted['construction_identity']:
        _refuse('Structural creation set or consumed construction implementation changed')
    return changes, proofs
