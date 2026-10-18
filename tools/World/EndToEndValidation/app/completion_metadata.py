"""Qualify one closed schema-only successor without replaying geography."""
import ast
import copy
from pathlib import Path

from World.CanonicalCompilation.refinement.api import validate_schema_reference_handoff
from .completion_archive import require
from .contracts import file_hash

CANONICAL = 'tools/World/CanonicalCompilation/'
EXISTING = frozenset(CANONICAL+p for p in (
    'app/promotion.py','app/cli.py','refinement/api.py','refinement/app/authority.py',
    'refinement/app/activation.py','refinement/contracts/canonical-selection-transition.schema.json'))
TESTS = frozenset(CANONICAL+p for p in ('tests/test_refinement_authority.py',))
ADDED = frozenset((CANONICAL+'refinement/app/schema_reference.py',CANONICAL+'tests/test_schema_reference.py',
    'scripts/ue/world/canonical_schema_reference_operation.ps1','scripts/ue/world/repair_canonical_schema_reference.ps1',
    'scripts/ue/world/test/canonical_schema_reference.Tests.ps1'))


def _replace(source, old, new):
    require(source.count(old)==1,'Schema-only source projection is absent or ambiguous')
    return source.replace(old,new,1)


def _remove_function(source, name):
    tree = ast.parse(source)
    matches = [n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name==name]
    require(len(matches)==1,'Schema-only helper boundary changed')
    node = matches[0]
    expected = {
        '_validate_authority_schema':'def _validate_authority_schema(repo_root: Path, authority: dict[str, Any], active_path: Path, profile_path: Path) -> None: pass',
        '_fresh_schema':'def _fresh_schema(authority, active): pass',
    }
    signature = copy.deepcopy(node)
    signature.body = [ast.Pass()]
    require(name in expected and ast.dump(signature)==ast.dump(ast.parse(expected[name]).body[0]),
            'Schema helper has changed executable definition metadata')
    lines = source.splitlines(True)
    return ''.join(lines[:node.lineno-1]+lines[node.end_lineno:])


def _projection(relative, source):
    if relative.endswith('/app/promotion.py'):
        source = _remove_function(source,'_validate_authority_schema')
        return _replace(source,'    _validate_authority_schema(repo_root, authority, active_path, profile_path)\n',
                        '    validate_document(authority, active_path)\n')
    if relative.endswith('/refinement/app/authority.py'):
        source = _replace(source,'from .contracts import existing_result, validate_document\n',
                          'from .contracts import existing_result, validate_document, write_json\n')
        source = _replace(source,"    schema_ref = base_authority.get('$schema')\n"
            "    schema = Path(__file__).resolve().parents[2] / 'contracts/canonical-authority.schema.json'\n"
            "    if (not isinstance(schema_ref, str) or '://' in schema_ref or Path(schema_ref).is_absolute() or\n"
            "            (active_path.parent / schema_ref).resolve() != schema):\n"
            "        _refuse('New selection requires the accepted authority schema reference at its durable location')\n",'')
        source = _replace(source,"        prepared = {**base_authority, 'selected_refinement': selected}\n"
            "        # This is the durable pointer's publication payload; its relative schema\n"
            "        # is resolved at the original authority location, not this private stage.\n"
            "        (staging / 'active.json').write_text(json.dumps(prepared, indent=2, ensure_ascii=True) + '\\n',\n"
            "            encoding='utf-8', newline='\\n')\n",
            "        prepared = {**base_authority, '$schema': 'https://alis.world/schemas/world-compiler/canonical-authority-v1.json',\n"
            "            'selected_refinement': selected}\n        write_json(staging / 'active.json', prepared)\n")
        return source
    if relative.endswith('/refinement/app/activation.py'):
        source = _remove_function(source,'_fresh_schema')
        source = _replace(source,'    _fresh_schema(new, active)\n','')
        source = _replace(source,"        _fresh_schema(_pointer(decoded['new']), active)\n",'')
        source = _replace(source,'    return _transition(repo, profile, active, old_bytes, new_bytes, old, selected)\n\n\n'
            'def _transition(repo, profile, active, old_bytes, new_bytes, old, selected, *, kind=None):\n','')
        source = _replace(source,"'old_active_sha256': hashlib.sha256(old_bytes).hexdigest()","'old_active_sha256': expected")
        source = _replace(source,"'bundle': {k: selected['bundle'][k] for k in ('path','byte_size','sha256')}}\n"
            "    if kind is not None:\n        transition['transition_kind'] = kind\n",
            "'bundle': {k: descriptor[k] for k in ('path','byte_size','sha256')}}\n")
        source = _replace(source,'def _validated_documents(repo, transition):','def _validated(repo, transition):')
        source = _replace(source,"    schema_only = transition.get('transition_kind') == 'schema_reference_v1'\n"
            "    if schema_only:\n        from .schema_reference import canonical_schema_reference\n"
            "        if (old.get('$schema') != 'https://alis.world/schemas/world-compiler/canonical-authority-v1.json' or\n"
            "                new.get('$schema') != canonical_schema_reference(active) or\n"
            "                {k:v for k,v in old.items() if k != '$schema'} != {k:v for k,v in new.items() if k != '$schema'}):\n"
            "            _refuse('Schema-reference transition changes more than the exact owned schema annotation')\n",'')
        source = _replace(source,"if ((not schema_only and old.get('selected_refinement')) or not selected",
            "if (old.get('selected_refinement') or not selected")
        source = _replace(source,'    return active, decoded, selected\n\n\ndef _validated(repo, transition):\n'
            '    active, decoded, selected = _validated_documents(repo, transition)\n','')
        return _replace(source,"    if not restore and transition.get('transition_kind') != 'schema_reference_v1':\n",'    if not restore:\n')
    if relative.endswith('/app/cli.py'):
        source = _replace(source,'    schema_reference = subparsers.add_parser("prepare-schema-reference-transition")\n'
            '    schema_reference.add_argument("--profile", required=True)\n'
            '    schema_reference.add_argument("--expected-active-sha256", required=True)\n','')
        source = _replace(source,'        if args.operation == "prepare-schema-reference-transition":\n'
            '            from ..refinement.api import prepare_schema_reference_transition\n'
            '            profile = args.profile\n'
            '            result = prepare_schema_reference_transition(repo_root(), _path(args.profile) or Path(),\n'
            '                args.expected_active_sha256)\n'
            '        elif args.operation == "prepare-selection-transition":',
            '        if args.operation == "prepare-selection-transition":')
        return _replace(source,", 'prepare-schema-reference-transition'",'')
    if relative.endswith('/refinement/api.py'):
        source = _replace(source,'from .app.schema_reference import prepare_schema_reference_transition, validate_schema_reference_handoff\n','')
        return _replace(source,', "prepare_schema_reference_transition", "validate_schema_reference_handoff"','')
    raise ValueError('Unknown schema-only source projection')


def verify_metadata_sources(repo, frozen, current, old_sources, proof):
    require(proof.get('status')=='accepted' and proof.get('kind')=='canonical_schema_reference_v1' and
            proof.get('restored_fixture') is False and proof.get('cold_noop') is True,
            'Schema normalization lacks completed persistent save/no-op proof')
    allowed = EXISTING|ADDED|TESTS
    for relative in allowed:
        require((repo/relative).is_file() and proof.get('source_files',{}).get(relative)==file_hash(repo/relative),
                'Current schema repair operation differs from its actual saved execution')
    for relative in EXISTING & set(frozen):
        require(relative in current and relative in old_sources and file_hash(old_sources[relative])==frozen[relative],
                'Schema source preimage is not the original tested byte identity')
        if relative.endswith('.json'):
            import json
            old = json.loads(old_sources[relative].read_bytes())
            now = json.loads((repo/relative).read_bytes())
            require(now['properties'].pop('transition_kind',None)=={'const':'schema_reference_v1'} and now==old,
                    'Selection contract changed beyond the closed annotation operation')
        else:
            original = ast.parse(old_sources[relative].read_text(encoding='utf-8-sig'))
            projected = ast.parse(_projection(relative,(repo/relative).read_text(encoding='utf-8-sig')))
            require(ast.dump(original)==ast.dump(projected),
                    'A tested Canonical input changed beyond the explicit metadata delta')
    require(not (ADDED & set(frozen)), 'Schema-only new operation replaced a tested producing input')
    return allowed


def repair_payloads(archive, pin):
    proof = archive.read(pin['path'],pin['sha256'])
    require(proof.get('status')=='accepted' and proof.get('kind')=='canonical_schema_reference_v1' and
            proof.get('restored_fixture') is False and proof.get('cold_noop') is True and
            proof.get('world_audit_status')=='accepted', 'Metadata successor is not completed accepted state')
    parent = Path(pin['path']).parent
    values = {name:archive.read(str(parent/name),sha) for name,sha in proof['payloads'].items()}
    require(set(values)=={'prior-world.json','current-world.json','prior-canonical.json','current-canonical.json',
                         'transition.json','world-audit.json'},'Metadata repair retained an incomplete closure')
    ticket = values['transition.json']
    require(proof['payloads']['prior-canonical.json']==ticket['old_active_sha256']==proof['prior_canonical_sha256'] and
            proof['payloads']['current-canonical.json']==ticket['new_active_sha256']==proof['current_canonical_sha256'] and
            proof['payloads']['prior-world.json']==proof['prior_world_sha256'] and
            proof['payloads']['current-world.json']==proof['current_world_sha256'] and
            proof['canonical_path']==ticket['active_path'],'Metadata repair lost an exact pointer byte link')
    expected = copy.deepcopy(values['prior-world.json'])
    bindings = [row for row in expected.get('canonical_input_compatibility',[])
                if row['candidate']['active_sha256']==ticket['old_active_sha256']]
    require(len(bindings)==1,'Metadata repair does not match one original accepted binding')
    bindings[0]['candidate']['active_sha256'] = ticket['new_active_sha256']
    expected.update(transaction_id=proof['operation_id'],accepted_operation_id=proof['operation_id'],
                    prior_active_set_sha256=proof['prior_world_sha256'])
    require(expected==values['current-world.json'] and sorted(proof['changed_paths'])==
            sorted([proof['world_path'],proof['canonical_path']]), 'Metadata repair changed another World field or file')
    require(values['world-audit.json']['status']=='accepted' and
            values['world-audit.json']['active_set']['sha256']==proof['current_world_sha256'],
            'Metadata successor lacks its exact durable authority audit')
    return proof,values


def validate_current(repo, original, metadata):
    proof,values = metadata
    state = validate_schema_reference_handoff(repo,original,values['transition.json'])
    require(state['active_sha256']==proof['current_canonical_sha256'], 'Current Canonical differs from its schema successor')
    return state
