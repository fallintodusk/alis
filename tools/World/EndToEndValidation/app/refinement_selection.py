"""Explicit new-child input admission against an unchanged historical base."""
from pathlib import Path

from World.CanonicalCompilation.api import CanonicalCompilationError, materialize_canonical, validate_canonical_authority
from World.CanonicalCompilation.refinement.api import implementation_identity, validate_current_surface_selection
from World.ExecutionEnvironment.geometry.api import execution_identity
from World.SourceIngestion.api import osm_context_identity
from .contracts import ValidationFailure, file_hash, read_json, validate_against
from .immutable_canonical import consumer_identity, snapshot_selection


SCHEMA = 'https://alis.world/schemas/world-validation/refinement-candidate-selection-v1.json'
MODE = 'refinement_candidate'


def _refuse(message, **details):
    raise ValidationFailure('refinement_candidate_unproven', message, **details)


def _candidate(repo, settings, baseline, path):
    path = path.resolve()
    scratch = (repo / 'tmp').resolve()
    if not path.is_relative_to(scratch) or not path.is_file():
        _refuse('New child admission requires an explicit repository tmp authority candidate')
    before = file_hash(path)
    compiler = repo / settings['compiler_profile_path']
    try:
        authority = validate_canonical_authority(repo, path, compiler)
    except CanonicalCompilationError as error:
        _refuse('Candidate authority authentication failed', reason=error.code)
    original_path = repo / baseline['active_path']
    if file_hash(original_path) != baseline['active_sha256']:
        _refuse('Accepted base selection changed during candidate admission')
    original = read_json(original_path)
    if ({k: v for k, v in authority.items() if k not in ('$schema', 'selected_refinement')} !=
            {k: v for k, v in original.items() if k != '$schema'}):
        _refuse('A refinement candidate cannot replace the accepted base authority or source lineage')
    selected = authority.get('selected_refinement')
    if (not selected or selected['base_result_sha256'] != baseline['compile_result_sha256'] or
            authority['authority_id'] != baseline['authority_id']):
        _refuse('Candidate does not consume the selected immutable compile base')
    if (selected['implementation_sha256'] != implementation_identity() or
            selected['source_context_identity'] != osm_context_identity() or
            selected['execution_identity'] != execution_identity(repo, True)['identity_sha256']):
        _refuse('New refinement admission requires current child, source and native producing contracts')
    try:
        validate_current_surface_selection(repo, authority, path.parent, compiler)
        result_path = materialize_canonical(repo, path, compiler)
    except CanonicalCompilationError as error:
        _refuse('Candidate effective materialization failed', reason=error.code)
    result = read_json(result_path)
    if (file_hash(path) != before or file_hash(result_path) != selected['result_sha256'] or
            result['operation'] != 'refine' or result['base_result_sha256'] != baseline['compile_result_sha256'] or
            result['inputs_hash'] != selected['inputs_hash']):
        _refuse('Candidate changed during input admission or materialized the wrong operation')
    return {
        'active_path': path.relative_to(repo).as_posix(), 'active_sha256': before,
        'base_authority_id': baseline['authority_id'], 'base_result_sha256': baseline['compile_result_sha256'],
        'effective_identity': selected['effective_identity'], 'result_sha256': selected['result_sha256'],
        'inputs_hash': selected['inputs_hash'], 'bundle_sha256': selected['bundle']['sha256'],
        'implementation_sha256': selected['implementation_sha256'],
        'source_context_identity': selected['source_context_identity'], 'execution_identity': selected['execution_identity'],
        'materialized_result_path': result_path.relative_to(repo).as_posix(),
    }


def _check_base_pins(repo, baseline):
    pins = {baseline['validation_profile_path']: baseline['validation_profile_sha256']}
    for record in baseline['profiles'].values():
        pins[record['active_path']] = record['active_sha256']
        pins.update(record['geographic_input_files'])
    for relative, digest in pins.items():
        path = (repo / relative).resolve()
        if not path.is_relative_to(repo.resolve()) or not path.is_file() or file_hash(path) != digest:
            _refuse('Immutable authority or geographic input changed during candidate admission', path=relative)
    for record in baseline['profiles'].values():
        active = repo / record['active_path']
        archive = (active.parent / read_json(active)['bundle']['path']).resolve()
        if not archive.is_relative_to(active.parent.resolve()) or not archive.is_file() or file_hash(archive) != record['bundle_sha256']:
            _refuse('Immutable base bundle changed during candidate admission')
    if consumer_identity() != baseline['current_consumer_implementation_sha256']:
        _refuse('Candidate reader implementation changed during input admission')


def snapshot_candidates(repo: Path, profile: dict, changed_base: str, validation_profile_path: Path,
                        candidates: dict[str, Path]) -> dict:
    """Freeze candidate inputs; World acceptance, promotion and recovery are not implied."""
    baseline = snapshot_selection(repo, profile, changed_base, validation_profile_path)
    if not isinstance(candidates, dict) or not candidates or set(candidates) - set(baseline['profiles']):
        _refuse('Candidate profiles must be a nonempty subset of the selected immutable authorities')
    records = {name: _candidate(repo, profile['profiles'][name], baseline['profiles'][name], path)
               for name, path in sorted(candidates.items())}
    _check_base_pins(repo, baseline)
    for record in records.values():
        active = repo / record['active_path']
        if not active.is_file() or file_hash(active) != record['active_sha256']:
            _refuse('A candidate selection changed during admission')
        descriptor = read_json(active)['selected_refinement']['bundle']
        archive = active.parent / descriptor['path']
        if not archive.is_file() or file_hash(archive) != record['bundle_sha256']:
            _refuse('A candidate bundle changed during admission')
    producing = {'implementation_sha256': implementation_identity(), 'source_context_identity': osm_context_identity(),
        'execution_identity': execution_identity(repo, True)['identity_sha256']}
    if any(record[key] != value for record in records.values() for key, value in producing.items()):
        _refuse('Child, source or native producing contracts changed during candidate admission')
    selection = {'$schema': SCHEMA, 'schema_version': 1, 'mode': MODE, 'base_selection': baseline,
        'candidates': records, 'executed_upstream_operations': {
            'source_acquisition': False, 'base_compilation': False, 'canonical_promotion': False}}
    validate_against(selection, 'refinement-candidate-selection.schema.json')
    return selection


def validate_candidates(repo: Path, profile: dict, selection: dict) -> None:
    validate_against(selection, 'refinement-candidate-selection.schema.json')
    validate_against(selection['base_selection'], 'immutable-canonical-selection.schema.json')
    base = selection['base_selection']
    current = snapshot_candidates(repo, profile, base['changed_base'], repo / base['validation_profile_path'],
        {name: repo / record['active_path'] for name, record in selection['candidates'].items()})
    if current != selection:
        _refuse('Immutable base, new child or current admission contracts changed after selection')
