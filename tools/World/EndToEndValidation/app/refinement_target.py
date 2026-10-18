"""Selected-child acceptance composes existing World and canonical owners."""
from datetime import UTC, datetime
import json
from pathlib import Path
import re

from .acceptance import accepted_run_evidence
from .checks import load_common_checks
from .contracts import ValidationFailure, file_hash, load_profile, profile_path, read_json, validate_against, write_result
from .execution import REPO_ROOT, _powershell, _run
from .refinement_selection import validate_candidates


MODE = 'refinement_selected'


def _require(condition, message):
    if not condition:
        raise ValidationFailure('refinement_target_unproven', message)


def _owned_path(relative, *, scratch=False):
    _require(isinstance(relative, str) and not Path(relative).is_absolute(), 'Target paths must be repository-relative')
    path = (REPO_ROOT / relative).resolve()
    owner = REPO_ROOT / 'tmp' if scratch else REPO_ROOT
    _require(path.is_relative_to(owner.resolve()) and path.is_file(), 'Target input is missing or outside its owner')
    return path


def verify_request(request_path: Path):
    """Re-prove real input and gate receipts before mutation and after restoration."""
    path = request_path.resolve()
    _require(path.is_relative_to((REPO_ROOT / 'tmp').resolve()), 'Target requests belong to repository tmp')
    request_hash = file_hash(path)
    request = read_json(path)
    validate_against(request, 'refinement-target-request.schema.json')
    resolved_profile = profile_path(request['validation_profile_path'])
    profile = load_profile(resolved_profile)
    selection_path = _owned_path(request['selection_path'], scratch=True)
    check_path = _owned_path(request['check_result_path'])
    _require(file_hash(selection_path) == request['selection_sha256'] and
             file_hash(check_path) == request['check_result_sha256'], 'Target selection or Check bytes changed')
    selection = read_json(selection_path)
    _require(selection['base_selection']['validation_profile_path'] == request['validation_profile_path'],
             'Target selection belongs to another validation profile')
    validate_candidates(REPO_ROOT, profile, selection)
    _require(len(selection['candidates']) == 1, 'One selected-child target is accepted per operation')
    name, candidate = next(iter(selection['candidates'].items()))
    common = load_common_checks(check_path)
    baseline = accepted_run_evidence(request['base_run'], profile['profile_id'])
    evidence = baseline['document']['evidence']
    _require(baseline['result_sha256'] == request['base_result_sha256'] and
             evidence.get('matrix_mode') == 'immutable_canonical' and
             evidence.get('immutable_canonical_selection') == selection['base_selection'],
             'Target requires the exact current immutable-base Matrix envelope')
    _require(evidence['common_checks']['receipt_sha256'] == common['receipt_sha256'],
             'Target and base Matrix must share the current Common Check')
    _require(file_hash(path) == request_hash and file_hash(selection_path) == request['selection_sha256'] and
             file_hash(check_path) == request['check_result_sha256'], 'Target inputs changed during admission')
    return {'status': 'accepted', 'request_sha256': request_hash, 'request': request,
            'profile_id': profile['profile_id'], 'candidate_name': name, 'candidate': candidate,
            'settings': profile['profiles'][name], 'base': selection['base_selection']['profiles'][name]}


def validate_transport(request_path: Path, transport_path: Path):
    """The adapter must prove the saved pair, cold no-op and exact restored state."""
    report = read_json(transport_path)
    _require(report.get('status') == 'accepted' and report.get('request_sha256') == file_hash(request_path),
             'Selected target adapter did not accept this exact request')
    for name in ('failure_recovery', 'saved_publication', 'cold_noop', 'restoration'):
        _require(report.get(name, {}).get('status') == 'accepted', f'Target lacks accepted {name}')
    failure = report['failure_recovery']
    _require(failure.get('host_exit', 0) != 0 and failure.get('phase') == 'publishing' and
             failure.get('canonical_state') == 'selected', 'Failure control did not reach the post-CAS boundary')
    saved, noop, restored = report['saved_publication'], report['cold_noop'], report['restoration']
    for value in (saved.get('world_sha256'), saved.get('canonical_sha256'),
                  restored.get('before_inventory_sha256'), restored.get('before_world_sha256')):
        _require(isinstance(value, str) and re.fullmatch('[a-f0-9]{64}', value), 'Target contains an unmeasured identity')
    _require(saved.get('world_sha256') == noop.get('world_sha256') and
             saved.get('canonical_sha256') == noop.get('canonical_sha256'), 'Cold replay changed accepted pointers')
    _require(restored.get('before_inventory_sha256') == restored.get('after_inventory_sha256') and
             restored.get('before_world_sha256') == restored.get('after_world_sha256') and
             restored.get('canonical_state') == 'prior', 'Target did not restore the exact original authorities and files')
    request = read_json(request_path)
    selection = read_json(_owned_path(request['selection_path'], scratch=True))
    candidate = next(iter(selection['candidates'].values()))
    _require(saved.get('canonical_sha256') == candidate['active_sha256'] and
             saved.get('world_sha256') != restored.get('before_world_sha256'), 'Target did not publish the selected pair')
    _require(failure.get('world_sha256') == restored.get('before_world_sha256'), 'Failure control lost the prior World authority')
    documents = {}
    for key, pin in report.get('receipts', {}).items():
        receipt = _owned_path(pin.get('path', ''))
        _require(file_hash(receipt) == pin.get('sha256'), 'Target receipt changed after the adapter completed')
        documents[key] = read_json(receipt)
    journal = documents.get('failure_journal', {})
    ticket = report.get('transition', {})
    _require(journal.get('phase') == 'publishing' and journal.get('canonical_selection_transition') == ticket and
             journal.get('prior_active_set_sha256') == restored['before_world_sha256'] and
             journal.get('expected_active_set_sha256') == report['receipts'].get('failure_staging', {}).get('sha256'),
             'Actual failure journal does not establish the paired publication boundary')
    _require(ticket.get('old_active_sha256') == selection['base_selection']['profiles'][next(iter(selection['candidates']))]['active_sha256'] and
             ticket.get('new_active_sha256') == candidate['active_sha256'] and
             ticket.get('effective_result_sha256') == candidate['result_sha256'], 'Target transition belongs to another child')
    _require(report['receipts'].get('saved_active', {}).get('sha256') == saved['world_sha256'], 'Saved World bytes are unpinned')
    bindings = documents.get('saved_active', {}).get('canonical_input_compatibility', [])
    _require(any(row.get('candidate', {}).get('result_sha256') == candidate['result_sha256'] and
                 row.get('input_selection_sha256') == request['selection_sha256'] for row in bindings),
             'Saved World has no exact selected-child binding')
    for key in ('failure_native', 'saved_native', 'noop_native', 'saved_audit'):
        document = documents.get(key, {})
        _require(document.get('status') == 'accepted', 'Target native execution or cold audit was rejected')
        if key != 'saved_audit':
            _require(document.get('input_sha256') == candidate['result_sha256'] and
                     sorted(document.get('execution_layer_ids', [])) == sorted(request['execute_layers']),
                     'Target consumed the wrong child or execution closure')
        if key == 'noop_native':
            _require(bool(document.get('layer_inventories')) and 'updated_actors' in document.get('changes', {}) and
                     all(isinstance(row.get('final_dirty_units'), list) and not row['final_dirty_units'] for row in document['layer_inventories']) and
                     all(value in (0, '') for value in document.get('changes', {}).values()), 'Cold selected replay was not a no-op')
    return report


def run_target(args):
    stamp = datetime.now(UTC).strftime('%Y%m%dT%H%M%SZ')
    root = REPO_ROOT / 'tmp/world/end_to_end_validation' / f'refinement-{stamp}'
    root.mkdir(parents=True, exist_ok=False)
    evidence_root = REPO_ROOT / 'Saved/Validation/WorldPipeline' / f'refinement-{stamp}'
    evidence_root.mkdir(parents=True, exist_ok=False)
    result = {'schema_version': 1, '$schema': 'https://alis.world/schemas/world-validation/validation-result-v1.json',
        'operation_id': f'refinement:{stamp}', 'operation': 'world_pipeline_validation', 'status': 'failed',
        'profile_id': 'unknown', 'checks': [], 'metrics': {}, 'evidence': {}, 'errors': []}
    try:
        resolved = profile_path(args.profile)
        profile = load_profile(resolved)
        baseline = accepted_run_evidence(args.base_run, profile['profile_id'])
        selection_path = Path(args.selection).resolve()
        check_path = Path(args.check_result).resolve()
        request = {'schema_version': 1, 'validation_profile_path': resolved.relative_to(REPO_ROOT).as_posix(),
            'selection_path': selection_path.relative_to(REPO_ROOT).as_posix(), 'selection_sha256': file_hash(selection_path),
            'check_result_path': check_path.relative_to(REPO_ROOT).as_posix(), 'check_result_sha256': file_hash(check_path),
            'base_run': args.base_run, 'base_result_sha256': baseline['result_sha256'], 'execute_layers': sorted(args.execute_layer)}
        request_path = root / 'request.json'
        request_path.write_text(json.dumps(request, indent=2) + '\n', encoding='utf-8')
        verified = verify_request(request_path)
        result['profile_id'] = verified['profile_id']
        _run('selected_refinement_target', [_powershell(), '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
            '-File', str(REPO_ROOT / 'scripts/ue/world/accept_canonical_refinement.ps1'),
            '-RequestPath', str(request_path), '-EvidencePath', str(root / 'transport.json')], evidence_root / 'logs')
        transport = validate_transport(request_path, root / 'transport.json')
        verify_request(request_path)
        result.update(status='accepted', checks=[{'check': 'selected_child_saved_cold_recovery', 'status': 'passed'}],
            evidence={'matrix_mode': MODE, 'request': request, 'transport_path': str(root / 'transport.json'),
                'transport_sha256': file_hash(root / 'transport.json'), 'scope': 'selected_child_target_not_full_matrix',
                'base_result_sha256': request['base_result_sha256'], 'receipts': transport['receipts']})
        code = 0
    except Exception as error:
        result['status'] = 'rejected'
        result['errors'] = [{'code': getattr(error, 'code', 'refinement_target_failed'), 'message': str(error)}]
        code = 3
    write_result(evidence_root / 'result.json', result)
    return code, {'status': result['status'], 'result': str(evidence_root / 'result.json')}
