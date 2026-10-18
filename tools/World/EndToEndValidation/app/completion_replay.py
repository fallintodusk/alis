"""Prove old producing execution inputs are unchanged across final-reader work."""
import ast

from .completion_archive import require
from .contracts import canonical_hash, file_hash
from .contract_inputs import common_contract_files


PREFIX = 'tools/World/EndToEndValidation/'
ACCEPT = PREFIX+'app/acceptance.py'
CLI = PREFIX+'app/cli.py'
NEW_READER_FILES = frozenset(PREFIX+p for p in (
    'app/completion.py', 'app/completion_archive.py', 'app/completion_replay.py', 'app/completion_metadata.py',
    'contracts/completion-evidence.schema.json', 'tests/test_completion.py', 'tests/test_completion_metadata.py'))
READER_SCHEMA = PREFIX+'contracts/acceptance-chain.schema.json'


def _tree(source):
    return ast.parse(source)


def _accept_projection(source, current):
    tree = _tree(source)
    functions = [node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='accept']
    require(len(functions)==1, 'Final acceptance function lost its original module boundary')
    function = functions[0]
    if current:
        args = function.args
        if args.kwonlyargs:
            expected = _tree('def accept(*, completion_evidence: str | None = None): pass').body[0].args
            require(ast.dump(args.kwonlyargs[-1]) == ast.dump(expected.kwonlyargs[0]) and
                    ast.dump(args.kw_defaults[-1]) == ast.dump(expected.kw_defaults[0]),
                    'Completion keyword has unexpected import-time definition metadata')
            args.kwonlyargs.pop()
            args.kw_defaults.pop()
    # Default expressions, decorators, annotations and original parameters remain compared.
    function.body = [ast.Pass()]
    return ast.dump(tree,include_attributes=False)


def _cli_projection(source, current):
    tree = _tree(source)
    if current:
        expected = _tree("accept.add_argument('--completion-evidence')").body[0]
        parser = next((n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='_parser'),None)
        require(parser is not None,'CLI parser is absent')
        matches = [n for n in parser.body if ast.dump(n)==ast.dump(expected)]
        require(len(matches)==1,'CLI completion option is not the exact final-reader hook')
        parser.body.remove(matches[0])
        calls = [node for node in ast.walk(tree) if isinstance(node,ast.Call) and
                 isinstance(node.func,ast.Name) and node.func.id=='accept']
        require(len(calls)==1,'CLI acceptance dispatch is ambiguous')
        keywords = calls[0].keywords
        expected_keyword = ast.keyword(arg='completion_evidence',value=ast.Attribute(
            value=ast.Name(id='args',ctx=ast.Load()),attr='completion_evidence',ctx=ast.Load()))
        require(len(keywords)==1 and ast.dump(keywords[0])==ast.dump(expected_keyword),
                'CLI completion forwarding changed the executed dispatch')
        keywords.clear()
    return ast.dump(tree,include_attributes=False)


def verify_file_maps(repo, frozen, current, old_sources, metadata=None):
    """Closed final-consumer seam; all remaining files require exact tested bytes."""
    allowed = NEW_READER_FILES | {ACCEPT,CLI,READER_SCHEMA}
    if metadata is not None:
        from .completion_metadata import verify_metadata_sources
        allowed |= verify_metadata_sources(repo,frozen,current,old_sources,metadata)
    changed = sorted(k for k in set(frozen)|set(current) if frozen.get(k)!=current.get(k))
    require(not (set(changed)-allowed), 'Producing/runtime/host/data input changed; new affected proof required')
    for relative in NEW_READER_FILES:
        require(relative not in frozen, 'A final-reader addition cannot replace tested producing input')
    for relative, projection in ((ACCEPT,_accept_projection),(CLI,_cli_projection)):
        if relative in changed:
            require(relative in frozen and relative in current, 'The original entrypoint cannot be removed')
            original = old_sources[relative]
            require(file_hash(original)==frozen[relative], 'Original entrypoint source is not bound to tested bytes')
            require(projection(original.read_text(encoding='utf-8-sig'),False)==
                    projection((repo/relative).read_text(encoding='utf-8-sig'),True),
                    'An executed entrypoint changed outside the final acceptance reader')
    require(all((repo/key).is_file() for key in changed if key in frozen), 'A tested reader input was removed')
    return changed


def verify_frozen_inputs(repo, archive, pin, check, base_selection, source_pins, metadata=None):
    frozen = archive.read(pin['path'],pin['sha256'])
    common = frozen['common_input_files']
    consumer = frozen['consumer_input_files']
    require(canonical_hash(common)==frozen['common_contract_sha256']==
            check['evidence']['common_contract_sha256'], 'Frozen Common preimage does not match the accepted execution')
    require(canonical_hash(consumer)==frozen['consumer_implementation_sha256']==
            base_selection['current_consumer_implementation_sha256'], 'Frozen consumer preimage differs from Matrix admission')
    old_sources = {name:archive.path(value['path'],value['sha256']) for name,value in source_pins.items()}
    current = {p.relative_to(repo).as_posix():file_hash(p) for p in common_contract_files(repo)}
    changes = verify_file_maps(repo,common,current,old_sources,metadata)
    world = repo/'tools/World'
    files = []
    for owner in ('CanonicalCompilation','SourceIngestion','ExecutionEnvironment','EndToEndValidation'):
        directory = world/owner
        files.extend(directory.glob('api.py'))
        files.extend(directory.glob('app/*.py'))
        files.extend(directory.glob('contracts/*.json'))
    old_consumer = {'tools/World/'+p:h for p,h in consumer.items()}
    new_consumer = {p.relative_to(repo).as_posix():file_hash(p) for p in files}
    verify_file_maps(repo,old_consumer,new_consumer,old_sources,metadata)
    return {'classification':'historical producing execution, current final-reader proof required',
            'historical_common_contract_sha256':frozen['common_contract_sha256'],
            'historical_consumer_implementation_sha256':frozen['consumer_implementation_sha256'],
            'current_final_reader_changes':changes,
            'current_final_reader_implementation_sha256':canonical_hash({key:current[key] for key in
                sorted(NEW_READER_FILES|{ACCEPT,CLI,READER_SCHEMA}) if key in current})}
