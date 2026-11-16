"""Authenticate a qualified full-domain Ground replacement; never select production geography."""
from pathlib import Path

from World.SourceIngestion.supplemental.api import (
    implementation_identity as source_identity, read_ground_product,
)
from World.SourceIngestion.api import IngestionError
from ...app.contracts import CompilerError, canonical_hash, file_hash, read_json, resolve_owned_path
from ...app.territory_admission import validate_profile_ownership
from .contracts import validate_document
from .retained_input import read_retained_input

KIND = 'qualified_ground_base'
SCHEMA = 'https://alis.world/schemas/world-compiler/ground-base-input-v1.json'


def _refuse(message):
    raise CompilerError('ground_base_admission_invalid', message)


def construction_identity():
    return canonical_hash({'source_admission': source_identity(),
        'constructor': file_hash(Path(__file__).with_name('ground_base.py')),
        'admission': file_hash(Path(__file__))})


def admit(repo, operation, base_root, entries, coverage):
    if (not isinstance(operation, dict) or set(operation) != {'kind', 'feature_ids', 'input'} or
            operation['kind'] != KIND or operation['feature_ids'] != []):
        _refuse('Qualified Ground base is one exclusive whole-domain operation')
    retained = {}
    document, descriptor = read_retained_input(repo, operation['input'], retained, _refuse)
    validate_document(document, Path(descriptor['path']))
    if document.get('$schema') != SCHEMA:
        _refuse('Ground base input differs from its declared role')
    validate_inputs(repo, document, base_root, entries, coverage)
    admitted = {'kind': KIND, 'feature_ids': [], 'base_features': {}, 'input': descriptor,
        'construction_identity': construction_identity()}
    return admitted, retained, document


def validate_inputs(repo, document, base_root, entries, coverage):
    """Recheck selected immutable source/profile inputs before candidate publication."""
    construction = document['construction']
    profile_path = resolve_owned_path(repo, document['compiler_profile_path'])
    profile = read_json(profile_path)
    validate_profile_ownership(repo, profile_path, profile)
    validate_document(profile, profile_path)
    contract_path = 'run_contract.json'
    entry = entries.get(contract_path)
    path = resolve_owned_path(base_root, contract_path)
    if (not entry or path.stat().st_size != entry['byte_size'] or file_hash(path) != entry['sha256'] or
            read_json(path).get('profile_sha256') != file_hash(profile_path) or
            construction['compiler_profile'] != profile or profile['profile_id'] != coverage['profile_id']):
        _refuse('Ground base needs the unchanged compiler profile authenticated by the immutable base')
    selected = document['source_admission']
    source_path = resolve_owned_path(repo, selected['path'])
    if source_path.stat().st_size != selected['byte_size']:
        _refuse('Ground admission differs from its selected byte pin')
    try:
        source = read_ground_product(source_path, selected['sha256'])
    except IngestionError as error:
        raise CompilerError(error.code, str(error), **error.details) from error
    if source['product']['basis'] != 'raster_ground_model':
        _refuse('Whole-domain Ground replacement requires a qualified raster model')
    metadata = {'source_crs': source['product']['source_frame']['horizontal'],
        'source_accuracy_m': source['product']['accuracy']['vertical_accuracy_m'],
        'confidence': source['product']['accuracy']['confidence']}
    if (construction['grid'] != source['grid'] or construction['evidence'] != source['evidence'] or
            construction['source_metadata'] != metadata):
        _refuse('Ground construction differs from the public source reader content or provenance')


def lineage(parent, request_hash, operation, document, base_hash):
    return {'kind': KIND, 'base_result_sha256': base_hash, 'parent_lineage_sha256': canonical_hash(parent),
        'refinement_inputs_sha256': request_hash, 'source_admission_sha256': document['source_admission']['sha256'],
        'ground_grid_sha256': document['construction']['evidence']['grid_content_sha256'],
        'construction_identity': operation['construction_identity']}


def dependencies(request_hash, document, base_hash):
    return ['base-compile:'+base_hash, 'ground-base:'+request_hash,
        'source-admission:'+document['source_admission']['sha256']]


def stage(staging, coverage, terrain, request_hash, operation, document, base_hash):
    """Reuse regular Terrain artifacts and manifest ownership in the shared publisher."""
    from .contracts import write_json
    for key, cell in sorted(terrain.items()):
        entry = next(e for e in coverage['cells'] if e['cell_id'] == key)
        manifest_path = staging/entry['path']
        manifest = read_json(manifest_path)
        descriptor = manifest['terrain_artifact']
        if not descriptor['path'].startswith('canonical/terrain/'):
            _refuse('Ground base requires original regular-grid artifact ownership')
        target = staging/descriptor['path']
        write_json(target, cell)
        descriptor.update(content_hash=file_hash(target), byte_size=target.stat().st_size,
            dependencies=dependencies(request_hash, document, base_hash) +
                ['raster-component:'+c['component_id']+':'+c['sample_semantic_sha256']
                    for c in cell['raster_component_dependencies']])
        manifest['terrain_component_dependencies'] = cell['raster_component_dependencies']
        manifest['terrain_lineage'] = lineage(manifest['terrain_lineage'], request_hash, operation, document, base_hash)
        write_json(manifest_path, manifest)
        entry.update(sha256=file_hash(manifest_path), byte_size=manifest_path.stat().st_size,
            terrain_lineage=manifest['terrain_lineage'])
        for artifact in coverage['artifact_descriptors']:
            if artifact['artifact_id'] == descriptor['artifact_id']:
                artifact.update({k:v for k,v in descriptor.items() if k != 'path'})
