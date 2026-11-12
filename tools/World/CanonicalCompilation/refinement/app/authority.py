"""Isolated effective-authority preparation using the existing immutable bundle store."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import tempfile
import zipfile

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, exclusive_file_lock
from World.SourceIngestion.api import validate_document as validate_source
from ...app.contracts import CompilerError, canonical_hash, file_hash, read_json, resolve_owned_path
from .contracts import existing_result, request_input_descriptors, strict_json_document, validate_document, validate_native_snapshot
from .identity import request_is_current
from .surface_publication import prepare_surface_refinement


def _refuse(message):
    raise CompilerError('refinement_authority_invalid', message)


def _validate_payload(payload, path):
    document = strict_json_document(payload, path)
    if str(document.get('$schema', '')).startswith('https://alis.world/schemas/world-source/'):
        validate_source(document, Path(path))
    else:
        validate_document(document, Path(path))
    return document


def _archive_result(bundle, name, digest, count):
    names = bundle.namelist()
    if len(names) != len(set(names)) or any(
            '\\' in p or PurePosixPath(p).is_absolute() or ':' in p or
            any(v in ('', '.', '..') for v in p.split('/')) for p in names):
        _refuse('Effective bundle paths are unsafe or duplicated')
    if name not in names or hashlib.sha256(bundle.read(name)).hexdigest() != digest:
        _refuse('Effective bundle result differs from its authority')
    result = _validate_payload(bundle.read(name), name)
    entries = result['outputs']
    if len(entries) != count or len({e['path'] for e in entries}) != count or set(names) != {name, *(e['path'] for e in entries)}:
        _refuse('Effective bundle inventory differs from its result')
    request_entry = next((e for e in entries if e['path'] == 'refinement/request.json'), None)
    if request_entry is None:
        _refuse('Effective refinement has no declared request')
    request_bytes = bundle.read(request_entry['path'])
    if len(request_bytes) != request_entry['byte_size'] or hashlib.sha256(request_bytes).hexdigest() != request_entry['sha256']:
        _refuse('Effective refinement request differs from its pinned result')
    request = _validate_payload(request_bytes, request_entry['path'])
    inputs = request_input_descriptors(request)
    if not set(inputs) <= {entry['path'] for entry in entries}:
        _refuse('Effective refinement omits a declared retained input')
    for entry in entries:
        payload = bundle.read(entry['path'])
        if (not entry['path'].endswith('.json') or len(payload) != entry['byte_size'] or
                hashlib.sha256(payload).hexdigest() != entry['sha256']):
            _refuse('Effective output is not a pinned canonical document')
        descriptor, role = inputs.get(entry['path'], (None, None))
        if descriptor is not None and any(entry[key] != descriptor[key] for key in ('sha256', 'byte_size')):
            _refuse('Retained refinement input disagrees with its request descriptor')
        if role == 'native':
            validate_native_snapshot(payload, descriptor)
        else:
            document = _validate_payload(payload, entry['path'])
            if role == 'basin' and document.get('$schema') != 'https://alis.world/schemas/world-compiler/identified-basin-input-v1.json':
                _refuse('Retained basin input differs from its declared role')
            if role == 'authored' and document.get('$schema') != 'https://alis.world/schemas/world-compiler/authored-overlay-v2.json':
                _refuse('Retained authored input differs from its declared role')
    return result


def _selection_identity(authority, selected):
    return canonical_hash({'base_authority_id': authority['authority_id'],
        'base_bundle_sha256': authority['bundle']['sha256'],
        'selected_refinement': {k: v for k, v in selected.items() if k != 'effective_identity'}})


def selected_request(root, selected):
    """Read the pinned request for current-producer checks after authority authentication."""
    archive = resolve_owned_path(root, selected['bundle']['path'])
    with zipfile.ZipFile(archive) as bundle:
        payload = bundle.read('refinement_result.json')
        if hashlib.sha256(payload).hexdigest() != selected['result_sha256']:
            _refuse('Current-producer request has another refinement result')
        result = _validate_payload(payload, 'refinement_result.json')
        entry = next((e for e in result['outputs'] if e['path'] == 'refinement/request.json'), None)
        if entry is None:
            _refuse('Current-producer request is not declared by the refinement result')
        payload = bundle.read(entry['path'])
        if len(payload) != entry['byte_size'] or hashlib.sha256(payload).hexdigest() != entry['sha256']:
            _refuse('Current-producer request bytes differ from the declared input')
        request = _validate_payload(payload, entry['path'])
    if (canonical_hash(request) != selected['inputs_hash'] or request['base_result_sha256'] != selected['base_result_sha256'] or
            any(request[key] != selected[key] for key in ('implementation_sha256', 'source_context_identity', 'execution_identity'))):
        _refuse('Current-producer request disagrees with its selected identity')
    return request


def validate_selection_bundle(authority: dict, root: Path) -> None:
    """Authenticate frozen child provenance, without rerunning a historical producer."""
    selected = authority['selected_refinement']
    descriptor = selected['bundle']
    archive = resolve_owned_path(root, descriptor['path'])
    if archive.stat().st_size != descriptor['byte_size'] or file_hash(archive) != descriptor['sha256']:
        _refuse('Selected refinement bundle changed')
    if selected['effective_identity'] != _selection_identity(authority, selected):
        _refuse('Effective identity is not derived from the immutable base and child')
    identity = canonical_hash({k: selected[k] for k in ('base_result_sha256', 'inputs_hash', 'result_sha256')})
    if descriptor['path'] != f'refinements/{identity}.zip':
        _refuse('Selected refinement bundle address differs from its inputs')
    with zipfile.ZipFile(archive) as child, zipfile.ZipFile(root / authority['bundle']['path']) as base:
        result = _archive_result(child, 'refinement_result.json', selected['result_sha256'], selected['output_count'])
        if (result['status'] != 'accepted' or result['operation'] != 'refine' or
                result['profile_id'] != authority['profile_id'] or result['inputs_hash'] != selected['inputs_hash'] or
                result['base_result_sha256'] != authority['compile_result_sha256'] or
                result['base_result_sha256'] != selected['base_result_sha256']):
            _refuse('Selected refinement is not a child of this accepted base')
        request = _validate_payload(child.read('refinement/request.json'), 'refinement/request.json')
        expected = 'https://alis.world/schemas/world-compiler/surface-refinement-request-v1.json'
        if (request['$schema'] != expected or canonical_hash(request) != result['inputs_hash'] or
                request['base_result_sha256'] != result['base_result_sha256'] or
                any(request[k] != selected[k] for k in
                    ('implementation_sha256', 'source_context_identity', 'execution_identity'))):
            _refuse('Selected operation and historical child producer disagree')
        inputs = request_input_descriptors(request)
        contexts = set(inputs)
        for path, (descriptor, _) in inputs.items():
            payload = child.read(path)
            if len(payload) != descriptor['byte_size'] or hashlib.sha256(payload).hexdigest() != descriptor['sha256']:
                _refuse('Selected source context differs from the operation request')
        base_result = json.loads(base.read('compile_result.json'))
        base_paths = {e['path'] for e in base_result['outputs']}
        child_paths = {e['path'] for e in result['outputs']}
        terrain_paths = _declared_terrain(base, child, request)
        if child_paths != base_paths | contexts | set(terrain_paths) | {'refinement/request.json', 'reports/refinement.json'}:
            _refuse('Surface refinement adds or removes unrelated canonical artifacts')
        authored_only = all(op['kind'] == 'authored_dry_terrain' for op in request['operations'])
        for path in base_paths:
            if authored_only and path.startswith('canonical/features/') and child.read(path) != base.read(path):
                _refuse('Authored-only refinement changed an original feature artifact')
            permitted = path == 'canonical/coverage.json' or path.startswith(('canonical/features/', 'canonical/cells/'))
            if not permitted and child.read(path) != base.read(path):
                _refuse('Surface refinement changed an unrelated base artifact')
        report = _validate_payload(child.read('reports/refinement.json'), 'reports/refinement.json')
        if report['inputs_hash'] != result['inputs_hash'] or report['base_result_sha256'] != result['base_result_sha256']:
            _refuse('Surface refinement report belongs to another operation')


def _current_request(repo, request):
    if not request_is_current(repo, request):
        _refuse('New child does not bind current producing contracts')


def _declared_terrain(base, child, request):
    """Permit only the declared selected-cell payloads, preserving every base grid."""
    coverage = json.loads(base.read('canonical/coverage.json'))
    cells = {entry['cell_id']: entry for entry in coverage['cells']}
    child_paths = set(child.namelist())
    declared = {}
    for operation in request['operations']:
        if operation['kind'] == 'authored_dry_terrain':
            from .authored_terrain import CONSTRUCTION, POLICY, cell_quads, domain_identity, selection
            document = _validate_payload(child.read(operation['input']['path']), operation['input']['path'])
            originals = {key: json.loads(base.read(json.loads(base.read(entry['path']))['terrain_artifact']['path']))
                         for key, entry in cells.items()}
            selected, changed = selection(originals, document, coverage['grid'])
            domain_hash = domain_identity(coverage, originals, selected)
            for key, coordinates in selected.items():
                manifest = json.loads(base.read(cells[key]['path']))
                original = manifest['terrain_artifact']
                relative = original['path'].replace('canonical/terrain/', 'canonical/refined_terrain/')
                if relative == original['path'] or relative in declared or relative not in child_paths:
                    _refuse('Authored Terrain ownership is absent, nested or interacting')
                payload = _validate_payload(child.read(relative), relative)
                if (payload.get('construction') != CONSTRUCTION or payload['cell_id'] != key or
                        payload['grid_id'] != manifest['grid_id'] or payload['policy'] != POLICY or
                        payload['base'] != {'path': original['path'], 'sha256': original['content_hash'], 'byte_size': original['byte_size']} or
                        sorted(q['coordinate'] for q in payload['quads']) != sorted(map(list, coordinates)) or
                        payload['quads'] != cell_quads(originals, key, coordinates, changed) or
                        payload['consumed']['authored_input_sha256'] != operation['input']['sha256'] or
                        payload['consumed']['domain_sha256'] != domain_hash or
                        any(payload['consumed'][field] != request[field]
                            for field in ('implementation_sha256', 'execution_identity'))):
                    _refuse('Authored Terrain payload differs from its declared immutable input')
                declared[relative] = key
            continue
        if operation['kind'] != 'identified_basin':
            continue
        document = _validate_payload(child.read(operation['input']['path']), operation['input']['path'])
        identifier = operation['feature_ids'][0]
        if (set(operation['base_features']) != {identifier} or document['original_feature']['feature_id'] != identifier or
                canonical_hash(document['original_feature']) != operation['base_features'][identifier]):
            _refuse('Basin source feature is not bound to the immutable operation base')
        for key, coordinates in document['selected'].items():
            if key not in cells:
                _refuse('Basin declares a Terrain owner outside the immutable base')
            manifest = json.loads(base.read(cells[key]['path']))
            original = manifest['terrain_artifact']
            relative = original['path'].replace('canonical/terrain/', 'canonical/refined_terrain/')
            if relative == original['path'] or relative in declared:
                _refuse('Basin Terrain ownership is nested or interacting')
            if relative not in child_paths:
                _refuse(f'Declared basin Terrain payload is absent: {relative}')
            payload = _validate_payload(child.read(relative), relative)
            if (payload.get('construction') != 'identified_static_basin_pwa_v1' or payload['cell_id'] != key or
                    payload['grid_id'] != manifest['grid_id'] or
                    payload['base'] != {'path': original['path'], 'sha256': original['content_hash'], 'byte_size': original['byte_size']} or
                    sorted(q['coordinate'] for q in payload['quads']) != sorted(coordinates) or
                    payload['consumed']['model_input_sha256'] != operation['input']['sha256'] or
                    payload['consumed']['water_sha256'] != operation['base_features'][identifier] or
                    payload['consumed']['model_sha256'] != canonical_hash(document['model']) or
                    payload['policy'] != document['model']['parameters']):
                _refuse('Basin Terrain payload differs from its declared joint construction')
            declared[relative] = key
    return declared


def _replay_result(repo, base_result_path, result_path, output_root):
    result = read_json(result_path)
    validate_document(result, result_path)
    existing_result(result_path.parent, result['inputs_hash'])
    request = read_json(result_path.parent / 'refinement/request.json')
    validate_document(request, result_path.parent / 'refinement/request.json')
    if result['base_result_sha256'] != file_hash(base_result_path):
        _refuse('New child does not consume this exact accepted base')
    _current_request(repo, request)
    operations = []
    for operation in request['operations']:
        if operation['kind'] in ('identified_basin', 'authored_dry_terrain'):
            descriptor = {**operation['input'], 'path': resolve_owned_path(result_path.parent, operation['input']['path']).relative_to(repo).as_posix()}
            operations.append({'kind': operation['kind'], 'feature_ids': operation['feature_ids'], 'input': descriptor})
        elif operation['kind'] == 'constructed_surface_import':
            from copy import deepcopy
            from .contracts import operation_input_descriptors
            inputs = deepcopy(operation['inputs'])
            for descriptor, _ in operation_input_descriptors({**operation, 'inputs': inputs}):
                descriptor['path'] = resolve_owned_path(result_path.parent, descriptor['path']).relative_to(repo).as_posix()
            operations.append({'kind': operation['kind'], 'feature_ids': operation['feature_ids'], 'inputs': inputs})
        else:
            operations.append({**{k: operation[k] for k in ('kind', 'feature_ids')},
                'context_path': resolve_owned_path(result_path.parent, operation['context']['path']).relative_to(repo).as_posix(),
                'sha256': operation['context']['sha256'], 'byte_size': operation['context']['byte_size']})
    replay, replay_root = prepare_surface_refinement(repo, base_result_path, output_root, operations)
    if result != replay or file_hash(result_path) != file_hash(replay_root / 'refinement_result.json'):
        _refuse('Candidate is not the exact deterministic replay of its qualified operations')
    _current_request(repo, request)
    return replay, replay_root, request


def validate_current_surface_selection(repo: Path, authority: dict, root: Path, profile: Path) -> dict:
    """Fresh admission replays origin; frozen reading and recovery only authenticate retained bytes."""
    from ...app import promotion
    promotion._validate_authority_document(repo, authority, root / 'active.json', profile)
    scratch = repo / 'tmp/world/canonical_compilation/refinement-admission'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temporary:
        temporary = Path(temporary)
        for label, descriptor, name, digest, count in (
                ('base', authority['bundle'], 'compile_result.json', authority['compile_result_sha256'], authority['output_count']),
                ('child', authority['selected_refinement']['bundle'], 'refinement_result.json',
                 authority['selected_refinement']['result_sha256'], authority['selected_refinement']['output_count'])):
            with zipfile.ZipFile(resolve_owned_path(root, descriptor['path'])) as bundle:
                # The parent authenticates base admission artifacts as well as outputs.
                # Only the child has the closed result-only ZIP inventory.
                result = (json.loads(bundle.read(name)) if label == 'base' else
                          _archive_result(bundle, name, digest, count))
                for path in [name, *(e['path'] for e in result['outputs'])]:
                    target = resolve_owned_path(temporary / label, path, must_exist=False)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(bundle.read(path))
        replay, _, _ = _replay_result(repo, temporary / 'base/compile_result.json',
                                     temporary / 'child/refinement_result.json', temporary / 'replay')
        promotion._validate_authority_document(repo, authority, root / 'active.json', profile)
        return {'status': 'accepted', 'inputs_hash': replay['inputs_hash'],
                'result_sha256': authority['selected_refinement']['result_sha256']}


def _prepare_locked(repo, active_path, profile_path, result_path, output_root):
    from ...app import promotion
    active_bytes = active_path.read_bytes()
    authority = promotion.validate_canonical_authority(repo, active_path, profile_path)
    base_authority = {k: v for k, v in authority.items() if k != 'selected_refinement'}
    schema_ref = base_authority.get('$schema')
    schema = Path(__file__).resolve().parents[2] / 'contracts/canonical-authority.schema.json'
    if (not isinstance(schema_ref, str) or '://' in schema_ref or Path(schema_ref).is_absolute() or
            (active_path.parent / schema_ref).resolve() != schema):
        _refuse('New selection requires the accepted authority schema reference at its durable location')
    base_lock = repo / 'tmp/world/canonical_compilation/materialized/locks' / (authority['bundle']['sha256'] + '.lock')
    with exclusive_file_lock(base_lock):
        base_result_path = promotion._materialize_canonical_locked(repo, active_path, base_authority)
    with tempfile.TemporaryDirectory(dir=output_root.parent, prefix='authority-') as temporary:
        temporary = Path(temporary)
        replay, replay_root, request = _replay_result(repo, base_result_path, result_path, temporary / 'replay')
        result = replay
        selected = {'kind': 'surface_relationships_v1', 'base_result_sha256': result['base_result_sha256'],
            'inputs_hash': result['inputs_hash'], 'result_sha256': file_hash(result_path),
            'output_count': len(result['outputs']),
            **{k: request[k] for k in ('implementation_sha256', 'source_context_identity', 'execution_identity')}}
        identity = canonical_hash({k: selected[k] for k in ('base_result_sha256', 'inputs_hash', 'result_sha256')})
        relative = f'refinements/{identity}.zip'
        staging = temporary / 'authority'
        bundle = staging / relative
        bundle.parent.mkdir(parents=True)
        promotion._write_deterministic_bundle(bundle,
            [(e['path'], replay_root / e['path']) for e in replay['outputs']], replay, result_name='refinement_result.json')
        # The base ZIP stays immutable and independently addressable in the same authority.
        target = staging / authority['bundle']['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(active_path.parent / authority['bundle']['path'], target)
        selected['bundle'] = {'format': 'deterministic_zip_v1', 'path': relative,
            'byte_size': bundle.stat().st_size, 'sha256': file_hash(bundle)}
        selected['effective_identity'] = _selection_identity(authority, selected)
        prepared = {**base_authority, 'selected_refinement': selected}
        # This is the durable pointer's publication payload; its relative schema
        # is resolved at the original authority location, not this private stage.
        (staging / 'active.json').write_text(json.dumps(prepared, indent=2, ensure_ascii=True) + '\n',
            encoding='utf-8', newline='\n')
        promotion.validate_canonical_authority(repo, staging / 'active.json', profile_path)
        if active_path.read_bytes() != active_bytes:
            _refuse('Accepted selection changed during candidate preparation')
        if output_root.exists():
            _refuse('Authority candidate destination already exists')
        staging.rename(output_root)
    return prepared, output_root / 'active.json'


def prepare_surface_authority(repo: Path, active_path: Path, profile_path: Path,
                              result_path: Path, output_root: Path) -> tuple[dict, Path]:
    """Prepare a reversible authority candidate; this never activates production."""
    repo, output_root = repo.resolve(), output_root.resolve()
    if output_root == repo / 'tmp' or not output_root.is_relative_to(repo / 'tmp'):
        _refuse('Authority candidate preparation is confined to project tmp')
    for protected in (active_path.parent.resolve(), result_path.parent.resolve()):
        if output_root.is_relative_to(protected) or protected.is_relative_to(output_root):
            _refuse('Authority candidate and protected inputs must be disjoint')
    output_root.parent.mkdir(parents=True, exist_ok=True)
    identity = canonical_hash({'profile_path': str(profile_path.resolve())})
    lock = repo / 'tmp/world/canonical_compilation/promotion/locks' / f'{identity}.lock'
    try:
        with exclusive_file_lock(lock):
            return _prepare_locked(repo, active_path.resolve(), profile_path.resolve(), result_path.resolve(), output_root)
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code, str(error)) from error
