"""Authenticate distinct Matrix, selected-target and actual rollout evidence."""
import copy
from pathlib import Path

from World.CanonicalCompilation.refinement.api import validate_selection_transition
from World.CanonicalCompilation.api import validate_canonical_authority
from .completion_archive import EvidenceArchive, digest, require
from .completion_replay import verify_frozen_inputs
from .contracts import file_hash, read_json, validate_against, validation_profile_contract
from .acceptance import gated_leg_records, verify_manifest_provenance


NATIVE_LEGS = ('unreal_first','unreal_second','unreal_road_locality','unreal_incremental','unreal_rejected_apply','unreal_clean_rebuild')


def retain_completion(repo, root, request):
    """Copy only explicitly pinned evidence before original scratch paths disappear."""
    validate_against(request,'completion-evidence.schema.json')
    require('retained_evidence' not in request and 'target_request' not in request,
            'A retained completion record is immutable; read it instead of preparing it again')
    archive = EvidenceArchive(repo,root)
    matrix = archive.add_json(**request['matrix'])
    target = archive.add_json(**request['target'])
    frozen = archive.add_json(**request['execution_inputs'])
    for pin in request['original_sources'].values():
        archive.add(**pin)
    for pin in frozen['base_authority_copies'].values():
        archive.add(**pin)
    archive.retain_pins(request['rollout'])
    if 'schema_repair' in request:
        repair = archive.add_json(**request['schema_repair'])
        parent = Path(request['schema_repair']['path']).parent
        for name,sha in repair['payloads'].items():
            archive.add(str(parent/name),sha)
    audit = _document(archive,request['rollout']['audit'])
    for key in ('prior_active','active'):
        active = _document(archive,request['rollout'][key])
        for scope in active['scopes']:
            archive.add(str(repo/audit['manifest_root']/scope['manifest_path']),scope['manifest_sha256'])
    evidence = matrix['evidence']
    archive.add(evidence['common_checks']['receipt'],evidence['common_checks']['receipt_sha256'])
    for leg in evidence['profiles'].values():
        for key in NATIVE_LEGS:
            if leg.get(key):
                archive.add(leg[key],leg[key+'_sha256'])
    transport = archive.add_json(target['evidence']['transport_path'],target['evidence']['transport_sha256'])
    request_path = str(Path(target['evidence']['transport_path']).parent/'request.json')
    target_request = archive.add_json(request_path,transport['request_sha256'])
    require(target_request==target['evidence']['request'],'Target request bytes differ from accepted target')
    archive.add(target_request['selection_path'],target_request['selection_sha256'])
    archive.retain_pins(transport['receipts'])
    return archive.write({'schema_version':1,**request,'target_request':{
        'path':request_path,'sha256':transport['request_sha256']}})


def _document(archive,pin):
    return archive.read(pin['path'],pin['sha256'])


def _native(document,candidate,layers,*,noop=False):
    require(document.get('status')=='accepted' and not document.get('errors') and document.get('input_sha256')==candidate['result_sha256'] and
            sorted(document.get('execution_layer_ids',[]))==sorted(layers),'Native receipt consumed another selected closure')
    require(bool(document.get('layer_inventories')),'Native receipt omits layer inventories')
    if noop:
        require('updated_actors' in document.get('changes',{}) and
                all(not isinstance(v,bool) and v in (0,'') for v in document['changes'].values()) and
                all(row.get('final_dirty_units')==[] for row in document['layer_inventories']),
                'Cold selected replay has unproved or nonzero writes')


def _candidate_identity(candidate):
    return {key:value for key,value in candidate.items() if key not in ('active_path','materialized_result_path')}


def _input_map(rows):
    require(isinstance(rows,list),'Missing native consumed-input set')
    result = {}
    for row in rows:
        require(isinstance(row,dict) and set(row)=={'unit_id','sha256'} and
                isinstance(row['unit_id'],str) and row['unit_id'] and row['unit_id'] not in result,
                'Consumed-input set has missing, duplicate or unknown fields')
        result[row['unit_id']] = digest(row['sha256'])
    return result


def _target(archive,target):
    require(target.get('status')=='accepted' and target['evidence'].get('matrix_mode')=='refinement_selected' and
            target['evidence'].get('scope')=='selected_child_target_not_full_matrix','Not an accepted selected-child target')
    evidence = target['evidence']
    transport = archive.read(evidence['transport_path'],evidence['transport_sha256'])
    require(transport.get('status')=='accepted','Target adapter did not accept the frozen execution')
    request = evidence['request']
    selection = archive.read(request['selection_path'],request['selection_sha256'])
    validate_against(selection,'refinement-candidate-selection.schema.json')
    validate_against(selection['base_selection'],'immutable-canonical-selection.schema.json')
    require(len(selection['candidates'])==1,'Completion supports one explicitly accepted child')
    name,candidate = next(iter(selection['candidates'].items()))
    for key in ('failure_recovery','saved_publication','cold_noop','restoration'):
        require(transport.get(key,{}).get('status')=='accepted','Selected target omits a required saved/recovery phase')
    failure,saved,noop,restored = [transport[k] for k in ('failure_recovery','saved_publication','cold_noop','restoration')]
    for value in (saved['world_sha256'],saved['canonical_sha256'],restored['before_inventory_sha256'],
                  restored['after_inventory_sha256'],restored['before_world_sha256'],restored['after_world_sha256']):
        digest(value)
    require(failure.get('host_exit',0)!=0 and failure.get('phase')=='publishing' and
            failure.get('canonical_state')=='selected','Target failure did not reach paired post-CAS publication')
    require(saved['world_sha256']==noop['world_sha256'] and saved['canonical_sha256']==noop['canonical_sha256']==candidate['active_sha256'],
            'Target cold no-op lost the selected authority pair')
    require(restored['before_inventory_sha256']==restored['after_inventory_sha256'] and
            restored['before_world_sha256']==restored['after_world_sha256']==failure['world_sha256'] and
            restored['canonical_state']=='prior','Target restoration is not exact')
    required = {'failure_native','saved_native','noop_native','failure_journal','failure_staging','saved_audit','saved_active'}
    require(isinstance(transport.get('receipts'),dict) and required<=set(transport['receipts']),
            'Target lacks its raw saved/native/recovery evidence')
    receipts = {k:archive.read(v['path'],v['sha256']) for k,v in transport['receipts'].items()}
    ticket,journal = transport['transition'],receipts['failure_journal']
    require(ticket['old_active_sha256']==selection['base_selection']['profiles'][name]['active_sha256'] and
            ticket['new_active_sha256']==candidate['active_sha256'] and
            ticket['effective_result_sha256']==candidate['result_sha256'],'Target ticket belongs to another transition')
    require(journal['phase']=='publishing' and journal['canonical_selection_transition']==ticket and
            journal['prior_active_set_sha256']==restored['before_world_sha256'] and
            journal['expected_active_set_sha256']==transport['receipts']['failure_staging']['sha256'],
            'Frozen native journal does not prove the selected transaction boundary')
    require(transport['receipts']['saved_active']['sha256']==saved['world_sha256'] and
            receipts['saved_audit']['status']=='accepted' and
            receipts['saved_audit']['active_set']['sha256']==saved['world_sha256'],
            'Saved target authority was not audited as these exact bytes')
    for key in ('failure_native','saved_native','noop_native'):
        _native(receipts[key],candidate,request['execute_layers'],noop=key=='noop_native')
    require(any(row.get('input_selection_sha256')==request['selection_sha256'] and row.get('candidate')==_candidate_identity(candidate)
                for row in receipts['saved_active'].get('canonical_input_compatibility',[])),
            'Saved target does not bind the exact selected child')
    return name,candidate,selection,request,transport,receipts


def load_completion(repo,path,profile_path,profile):
    document = read_json(path)
    validate_against(document,'completion-evidence.schema.json')
    require('retained_evidence' in document and 'target_request' in document, 'Completion evidence has not been retained')
    archive = EvidenceArchive(repo,path.parent,document['retained_evidence'],document['execution_root'])
    matrix,target = _document(archive,document['matrix']),_document(archive,document['target'])
    for result in (matrix,target):
        validate_against(result,'validation-result.schema.json')
        require(result['status']=='accepted' and not result['errors'] and result['profile_id']==profile['profile_id'],
                'Completion receipt is rejected, inconsistent or belongs to another profile')
    name,candidate,selection,request,transport,receipts = _target(archive,target)
    original_request = _document(archive,document['target_request'])
    require(original_request==request and document['target_request']['sha256']==transport['request_sha256'],
            'Accepted target request has no retained byte identity')
    matrix_path = Path(archive.key(document['matrix']['path']))
    target_path = Path(archive.key(document['target']['path']))
    owner = Path('Saved/Validation/WorldPipeline')
    require(matrix_path.parent.parent==owner and matrix_path.name=='result.json' and
            matrix_path.parent.name==request['base_run'] and target_path.parent.parent==owner and
            target_path.name=='result.json' and target_path.parent.name.startswith('refinement-') and
            target['operation_id']=='refinement:'+target_path.parent.name.removeprefix('refinement-'),
            'Completion requires the actual owned Matrix and selected-target result records')
    evidence = matrix['evidence']
    require(evidence.get('matrix_mode')=='immutable_canonical' and
            evidence.get('immutable_canonical_selection')==selection['base_selection'] and
            document['matrix']['sha256']==request['base_result_sha256']==target['evidence']['base_result_sha256'],
            'Selected target did not consume this immutable-base Matrix')
    expected_profile = {'path':profile_path.relative_to(repo).as_posix(),'sha256':file_hash(profile_path)}
    require(evidence.get('validation_profile')==expected_profile and
            evidence.get('profile_input_contract')==validation_profile_contract(profile_path),
            'Frozen Matrix profile or transitive inputs changed')
    check = archive.read(evidence['common_checks']['receipt'],evidence['common_checks']['receipt_sha256'])
    validate_against(check,'validation-result.schema.json')
    require(check['status']=='accepted' and check['profile_id']=='common_checks' and
            request['check_result_sha256']==evidence['common_checks']['receipt_sha256'],
            'Matrix and target lack the same accepted Common Check')
    frozen = _document(archive,document['execution_inputs'])
    require(frozen['check_sha256']==request['check_result_sha256'] and
            frozen['selection_sha256']==request['selection_sha256'], 'Frozen hash tables belong to another admitted execution')
    metadata = None
    if 'schema_repair' in document:
        from .completion_metadata import repair_payloads, validate_current
        metadata = repair_payloads(archive,document['schema_repair'])
        state = validate_current(repo,transport['transition'],metadata)
    else:
        state = validate_selection_transition(repo,transport['transition'])
    replay = verify_frozen_inputs(repo,archive,document['execution_inputs'],check,
                                  selection['base_selection'],document['original_sources'],
                                  metadata[0] if metadata else None)
    require(state['state']=='selected','Completion requires the genuine selected Canonical child')
    for key,record in selection['base_selection']['profiles'].items():
        for relative,sha in record['geographic_input_files'].items():
            require(file_hash(repo/relative)==sha,'A geographic input changed after accepted execution')
        if key!=name:
            require(file_hash(repo/record['active_path'])==record['active_sha256'], 'An unrelated Canonical authority changed')
            authority = validate_canonical_authority(repo,repo/record['active_path'],repo/profile['profiles'][key]['compiler_profile_path'])
            require(authority['compile_result_sha256']==record['compile_result_sha256'] and
                    authority['bundle']['sha256']==record['bundle_sha256'],'Unrelated canonical payload lost its accepted identity')
        leg = evidence['profiles'].get(key,{})
        require(all(leg.get(field) is None for field in
                    ('source_root','source_result','compile_first_root','compile_second_root','incremental_root')),
                'Frozen artifact consumption contains unrecorded source/base production')
        require(all(leg.get('canonical_authority',{}).get(field)==record[field] for field in
                    ('authority_id','active_path','active_sha256','bundle_sha256','compile_result_sha256')),
                'Matrix leg did not consume the selected authenticated base')
    require(set(evidence['profiles'])==set(selection['base_selection']['profiles']), 'Matrix omits a selected city')
    projected = copy.deepcopy(matrix)
    for leg in projected['evidence']['profiles'].values():
        for key in NATIVE_LEGS:
            if leg.get(key):
                leg[key] = str(archive.path(leg[key],leg[key+'_sha256']))
    run = {'run_id':request['base_run'],'profile_id':matrix['profile_id'],
        'result_path':str(archive.path(document['matrix']['path'],document['matrix']['sha256'])),
        'result_sha256':document['matrix']['sha256'],'validation_profile':expected_profile,
        'document':projected}
    return {'document':document,'archive':archive,'run':run,'records':gated_leg_records([run]),
            'name':name,'candidate':candidate,'selection':selection,'request':request,
            'target':transport,'target_receipts':receipts,'replay':replay,'metadata':metadata}


def verify_completion_world(repo,completion,audit):
    if completion.get('metadata'):
        from .completion_metadata import validate_current
        state = validate_current(repo,completion['target']['transition'],completion['metadata'])
    else:
        state = validate_selection_transition(repo,completion['target']['transition'])
    require(state['state']=='selected',
            'Canonical selection changed inside the acceptance window')
    archive,document = completion['archive'],completion['document']
    prior = _document(archive,document['rollout']['prior_active'])
    saved = _document(archive,document['rollout']['active'])
    native = _document(archive,document['rollout']['native'])
    rollout_audit = _document(archive,document['rollout']['audit'])
    world = repo/audit['active_set']['path']
    current_world_sha = document['rollout']['active']['sha256']
    current_canonical_sha = completion['candidate']['active_sha256']
    if completion.get('metadata'):
        proof,values = completion['metadata']
        require(proof['prior_world_sha256']==current_world_sha and values['prior-world.json']==saved and
                proof['prior_canonical_sha256']==current_canonical_sha and proof['world_path']==audit['active_set']['path'],
                'Metadata successor does not extend this actual selected rollout')
        current_world_sha = proof['current_world_sha256']
        current_canonical_sha = proof['current_canonical_sha256']
    require(audit['status']=='accepted' and rollout_audit['status']=='accepted' and
            rollout_audit['active_set']['sha256']==document['rollout']['active']['sha256'] and
            file_hash(world)==audit['active_set']['sha256']==current_world_sha,
            'Current World differs from the actual audited selected rollout')
    candidate,request = completion['candidate'],completion['request']
    _native(native,candidate,request['execute_layers'])
    bindings = [row for row in saved.get('canonical_input_compatibility',[])
                if row.get('candidate')==_candidate_identity(candidate) and row.get('input_selection_sha256')==request['selection_sha256']]
    require(len(bindings)==1,'Actual World does not bind the exact target child and consumed selection')
    binding = bindings[0]
    require(binding['prior_active_set_sha256']==document['rollout']['prior_active']['sha256'],
            'Actual rollout lost its original accepted World')
    before = {r['scope_id']:r for r in prior['scopes']}
    after = {r['scope_id']:r for r in saved['scopes']}
    require(set(before)==set(after)=={r['scope_id'] for r in audit['scopes']}, 'Rollout changed the declared scope inventory')
    changed = {key for key in after if before[key]!=after[key]}
    require(changed and changed<=set(binding['selected_scope_ids']), 'Rollout changed an unselected scope')
    target_native = completion['target_receipts']['saved_native']
    require(digest(native.get('semantic_fingerprint'))==target_native.get('semantic_fingerprint'),
            'Actual rollout does not reproduce the target native semantic output')
    for field in ('map_package','world_data_plugin','presentation_profile_sha256','runtime_profile_sha256',
                  'authored_overlay_set_sha256','realization_profile_sha256'):
        require(native.get(field)==target_native.get(field) and field in native,
                'Actual rollout differs from the exact target consuming contract')
    actual_layers = {row['scope_id']:row for row in native['layer_inventories']}
    target_layers = {row['scope_id']:row for row in target_native['layer_inventories']}
    require(set(actual_layers)==set(target_layers)==set(binding['layers']), 'Rollout omits a proven consumer layer')
    for key,row in actual_layers.items():
        accepted_target = target_layers[key]
        for field in ('layer_id','normalized_layer_contract_sha256','generator_id','generator_version',
                      'generator_fingerprint','artifact_root','final_dirty_units'):
            require(row.get(field)==accepted_target.get(field) and field in row,
                    'Actual native consumed units or producer differ from the tested child')
        for field in ('canonical_inputs','dependency_inputs'):
            require(_input_map(row.get(field))==_input_map(accepted_target.get(field)),
                    'Actual native consumed units differ from the tested child')
    view = copy.deepcopy(audit['scopes'])
    for scope in view:
        key = scope['scope_id']
        require(scope['manifest_sha256']==after[key]['manifest_sha256'], 'Current scope is not the saved rollout manifest')
        old_manifest = archive.read((Path(audit['manifest_root'])/before[key]['manifest_path']).as_posix(),before[key]['manifest_sha256'])
        new_manifest = archive.read((Path(audit['manifest_root'])/after[key]['manifest_path']).as_posix(),after[key]['manifest_sha256'])
        require(new_manifest['input_identity']==scope['input_identity'], 'Saved scope input identity changed')
        if key in actual_layers:
            for field in ('canonical_inputs','dependency_inputs'):
                require(_input_map(new_manifest['layer_contract'][field])==_input_map(actual_layers[key][field]),
                        'Saved scope consumed units differ from the actual native rollout')
        if key not in changed:
            require(old_manifest==new_manifest, 'Carried scope lost its exact prior manifest')
            continue
        row = binding['layers'].get(key)
        require(row is not None and row['accepted']['accepted_manifest_sha256']==before[key]['manifest_sha256'] and
                row['accepted']['accepted_generator_fingerprint']==scope['generator_fingerprint'],
                'Changed scope lost its accepted prior or producing identity')
        actual = scope['input_identity']
        require(actual['compile_result_sha256']==candidate['result_sha256'], 'Changed scope consumed an unrelated child')
        # Only after child validation, reuse the existing checks for the other identity fields.
        base = completion['records'].get(actual['map_package'])
        require(base is not None,'Changed scope belongs to an ungated city')
        actual['compile_result_sha256'] = base['compile_result_sha256']
    problems = verify_manifest_provenance(view,completion['records'])
    require(not problems,'Mixed scope identity fields do not match their accepted base/child execution')
    return {'changed_scope_ids':sorted(changed),'carried_scope_ids':sorted(set(after)-changed),
            'active_set_sha256':file_hash(world),'canonical_sha256':current_canonical_sha}
