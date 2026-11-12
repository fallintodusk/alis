"""Canonical-owned immutable payload installation and recoverable selection CAS."""
import base64
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, exclusive_file_lock
from ...app import promotion
from ...app.contracts import CompilerError, canonical_hash, file_hash, resolve_owned_path
from .contracts import validate_document
from .identity import request_is_current
from .authority import selected_request, validate_current_surface_selection


SCHEMA = 'https://alis.world/schemas/world-compiler/canonical-selection-transition-v1.json'


def _refuse(message):
    raise CompilerError('canonical_selection_unproven', message)


@contextmanager
def _locked(repo, profile):
    identity = canonical_hash({'profile_path': str(profile.resolve())})
    lock = repo / 'tmp/world/canonical_compilation/promotion/locks' / f'{identity}.lock'
    try:
        with exclusive_file_lock(lock):
            yield
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code, str(error)) from error
    except OSError as error:
        raise CompilerError('canonical_selection_io_failure', 'Selection publication or recovery failed',
                            reason=type(error).__name__) from error


def _pointer(payload):
    try:
        value = json.loads(payload)
    except (ValueError, UnicodeError) as error:
        raise CompilerError('invalid_json', 'Canonical selection bytes are not JSON') from error
    if not isinstance(value, dict):
        _refuse('Canonical selection must be a JSON object')
    return value


def _current_producer(repo, selected, root):
    if not request_is_current(repo, selected_request(root, selected)):
        _refuse('First activation requires current child, source and native producing contracts')


def _base_identity(authority):
    return {k: v for k, v in authority.items() if k not in ('$schema', 'selected_refinement')}


def _fresh_schema(authority, active):
    from .schema_reference import canonical_schema_reference
    if authority.get('$schema') != canonical_schema_reference(active):
        _refuse('Fresh selection requires the exact relative schema reference at its durable target')


def _active_path(repo, profile):
    if not profile.is_relative_to(repo) or not profile.is_file():
        _refuse('Selection profile must be an existing repository-owned input')
    active = promotion.canonical_active_path(repo, profile).resolve()
    if not active.is_relative_to(repo) or active.name != 'active.json':
        _refuse('Selection target differs from the canonical owner')
    return active


def _install(repo, source, target, descriptor):
    if target.exists():
        if target.stat().st_size != descriptor['byte_size'] or file_hash(target) != descriptor['sha256']:
            _refuse('Existing immutable child payload differs; it cannot be replaced')
        return
    scratch = repo / 'tmp/world/canonical_compilation/promotion/selection-staging'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as directory:
        staged = Path(directory) / 'child.zip'
        shutil.copyfile(source, staged)
        if staged.stat().st_size != descriptor['byte_size'] or file_hash(staged) != descriptor['sha256']:
            _refuse('Child payload changed during installation')
        target.parent.mkdir(parents=True, exist_ok=True)
        os.replace(staged, target)


def _prepare(repo, profile, candidate, expected):
    if not isinstance(expected, str) or len(expected) != 64 or any(c not in '0123456789abcdef' for c in expected):
        _refuse('Selection preparation requires the exact expected prior pointer hash')
    profile_before = file_hash(profile)
    active = _active_path(repo, profile)
    old_bytes = active.read_bytes()
    if hashlib.sha256(old_bytes).hexdigest() != expected:
        _refuse('Canonical selection changed before preparation')
    if not candidate.is_relative_to(repo / 'tmp') or candidate == active:
        _refuse('Selection preparation requires a disjoint repository-tmp candidate')
    new_bytes = candidate.read_bytes()
    old = promotion._validate_authority_document(repo, _pointer(old_bytes), active, profile)
    new = promotion._validate_authority_document(repo, _pointer(new_bytes), candidate, profile)
    selected = new.get('selected_refinement')
    if old.get('selected_refinement') or not selected or _base_identity(old) != _base_identity(new):
        _refuse('Only an explicit unchanged-base to selected-feature transition is supported')
    _fresh_schema(new, active)
    _current_producer(repo, selected, candidate.parent)
    validate_current_surface_selection(repo, new, candidate.parent, profile)
    _current_producer(repo, selected, candidate.parent)
    if file_hash(profile) != profile_before or active.read_bytes() != old_bytes or candidate.read_bytes() != new_bytes:
        _refuse('Canonical pointers changed during fresh selection admission')
    descriptor = selected['bundle']
    _install(repo, resolve_owned_path(candidate.parent, descriptor['path']),
             resolve_owned_path(active.parent, descriptor['path'], must_exist=False), descriptor)
    promotion._validate_authority_document(repo, new, active, profile)
    if active.read_bytes() != old_bytes or candidate.read_bytes() != new_bytes:
        _refuse('Canonical pointers changed during transition preparation')
    return _transition(repo, profile, active, old_bytes, new_bytes, old, selected)


def _transition(repo, profile, active, old_bytes, new_bytes, old, selected, *, kind=None):
    transition = {'$schema': SCHEMA, 'schema_version': 1,
        'profile_path': profile.relative_to(repo).as_posix(), 'profile_sha256': file_hash(profile),
        'active_path': active.relative_to(repo).as_posix(), 'old_active_sha256': hashlib.sha256(old_bytes).hexdigest(),
        'new_active_sha256': hashlib.sha256(new_bytes).hexdigest(),
        'old_active_bytes_base64': base64.b64encode(old_bytes).decode('ascii'),
        'new_active_bytes_base64': base64.b64encode(new_bytes).decode('ascii'),
        'base_authority_id': old['authority_id'], 'base_result_sha256': old['compile_result_sha256'],
        'effective_identity': selected['effective_identity'], 'effective_result_sha256': selected['result_sha256'],
        'bundle': {k: selected['bundle'][k] for k in ('path','byte_size','sha256')}}
    if kind is not None:
        transition['transition_kind'] = kind
    validate_document(transition, Path('canonical-selection-transition.json'))
    return transition


def prepare_selection_transition(repo: Path, profile: Path, candidate: Path, expected_active_sha256: str) -> dict:
    """Install immutable child bytes and return recovery metadata; leave selection unchanged."""
    repo, profile, candidate = repo.resolve(), profile.resolve(), candidate.resolve()
    with _locked(repo, profile):
        return _prepare(repo, profile, candidate, expected_active_sha256)


def _validated_documents(repo, transition):
    validate_document(transition, Path('canonical-selection-transition.json'))
    profile = resolve_owned_path(repo, transition['profile_path'])
    active = _active_path(repo, profile)
    if active != resolve_owned_path(repo, transition['active_path']) or file_hash(profile) != transition['profile_sha256']:
        _refuse('Recovery metadata differs from its derived canonical target/profile')
    decoded = {}
    authorities = {}
    for label in ('old','new'):
        try:
            payload = base64.b64decode(transition[f'{label}_active_bytes_base64'], validate=True)
            authority = _pointer(payload)
        except (ValueError, UnicodeError) as error:
            raise CompilerError('canonical_selection_unproven', 'Recovery pointer bytes are invalid') from error
        if hashlib.sha256(payload).hexdigest() != transition[f'{label}_active_sha256']:
            _refuse('Recovery pointer bytes differ from their pinned identity')
        decoded[label] = payload
        authorities[label] = promotion._validate_authority_document(repo, authority, active, profile)
    old, new = authorities['old'], authorities['new']
    selected = new.get('selected_refinement')
    schema_only = transition.get('transition_kind') == 'schema_reference_v1'
    if schema_only:
        from .schema_reference import canonical_schema_reference
        if (old.get('$schema') != 'https://alis.world/schemas/world-compiler/canonical-authority-v1.json' or
                new.get('$schema') != canonical_schema_reference(active) or
                {k:v for k,v in old.items() if k != '$schema'} != {k:v for k,v in new.items() if k != '$schema'}):
            _refuse('Schema-reference transition changes more than the exact owned schema annotation')
    if ((not schema_only and old.get('selected_refinement')) or not selected or _base_identity(old) != _base_identity(new) or
            transition['base_authority_id'] != old['authority_id'] or
            transition['base_result_sha256'] != old['compile_result_sha256'] or
            transition['effective_identity'] != selected['effective_identity'] or
            transition['effective_result_sha256'] != selected['result_sha256'] or
            transition['bundle'] != {k: selected['bundle'][k] for k in ('path','byte_size','sha256')}):
        _refuse('Recovery metadata lost original base or selected-child identity')
    return active, decoded, selected


def _validated(repo, transition):
    active, decoded, selected = _validated_documents(repo, transition)
    current = file_hash(active)
    state = 'prior' if current == transition['old_active_sha256'] else 'selected' if current == transition['new_active_sha256'] else None
    if state is None:
        _refuse('Canonical pointer matches neither the prior nor selected state; evidence must be retained')
    return active, decoded, selected, state


def validate_selection_transition(repo: Path, transition: dict) -> dict:
    """Authenticate both frozen pointer states and report the actual current state."""
    repo = repo.resolve()
    validate_document(transition, Path('canonical-selection-transition.json'))
    profile = resolve_owned_path(repo, transition['profile_path'])
    with _locked(repo, profile):
        _, _, _, state = _validated(repo, transition)
        return {'status': 'accepted', 'state': state, 'active_sha256': transition['old_active_sha256' if state == 'prior' else 'new_active_sha256']}


def _publish(repo, transition, restore):
    active, decoded, selected, state = _validated(repo, transition)
    desired = 'prior' if restore else 'selected'
    if state == desired:
        return {'status': 'accepted', 'state': state, 'changed': False}
    if not restore and transition.get('transition_kind') != 'schema_reference_v1':
        _fresh_schema(_pointer(decoded['new']), active)
        _current_producer(repo, selected, active.parent)
        validate_current_surface_selection(repo, _pointer(decoded['new']), active.parent,
                                          resolve_owned_path(repo, transition['profile_path']))
        _current_producer(repo, selected, active.parent)
        if file_hash(resolve_owned_path(repo, transition['profile_path'])) != transition['profile_sha256']:
            _refuse('Canonical profile changed during fresh selection admission')
    before = transition['new_active_sha256' if restore else 'old_active_sha256']
    payload = decoded['old' if restore else 'new']
    scratch = repo / 'tmp/world/canonical_compilation/promotion/selection-staging'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as directory:
        staged = Path(directory) / 'active.json'
        staged.write_bytes(payload)
        if file_hash(active) != before:
            _refuse('Canonical pointer changed before compare-and-swap publication')
        os.replace(staged, active)
    if active.read_bytes() != payload:
        _refuse('Published selection differs; retain coordinated recovery evidence')
    return {'status': 'accepted', 'state': desired, 'changed': True}


def publish_selection_transition(repo: Path, transition: dict, *, restore: bool = False) -> dict:
    """CAS one canonical selection; the coordinating World journal owns paired completion."""
    repo = repo.resolve()
    validate_document(transition, Path('canonical-selection-transition.json'))
    profile = resolve_owned_path(repo, transition['profile_path'])
    with _locked(repo, profile):
        return _publish(repo, transition, restore)
