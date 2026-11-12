"""Compose source-qualified feature refinements once from an immutable compile base."""
from copy import deepcopy
from datetime import date, datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import tempfile

from World.SourceIngestion.api import IngestionError, osm_context_identity, osm_provider_namespace, osm_way_context
from World.SourceIngestion.api import validate_document as validate_source
from World.ExecutionEnvironment.geometry.api import execution_identity
from ...app.artifacts import accepted_base, output_descriptor, promote_staging
from ...app.contracts import CompilerError, canonical_hash, file_hash, resolve_owned_path
from ...app.projection import project_features
from ...app.surface_context import surface_context
from ...app.water_geometry import TerrainSampler, production_geometry_evidence
from .contracts import existing_result, request_input_descriptors, validate_document, write_json
from .identity import implementation_identity, request_is_current
from .road_elevation import build_bridge_profile
from .water_confluence import build_confluence_constraint


def _refuse(message):
    raise CompilerError('surface_refinement_invalid', message)


def _read_snapshot(root, entries, relative):
    path = resolve_owned_path(root, relative)
    payload = path.read_bytes()
    entry = entries.get(relative)
    if not entry or len(payload) != entry['byte_size'] or hashlib.sha256(payload).hexdigest() != entry['sha256']:
        _refuse('Base document differs from its authenticated receipt')
    document = json.loads(payload)
    validate_document(document, path)
    return document


def _read_artifact(root, entries, descriptor):
    entry = entries.get(descriptor['path'])
    if (not entry or descriptor['content_hash'] != entry['sha256'] or
            descriptor['byte_size'] != entry['byte_size']):
        _refuse('Base artifact descriptor disagrees with the authenticated payload')
    return _read_snapshot(root, entries, descriptor['path'])


def _source(document, feature):
    references = feature.get('source_refs', [])
    if len(references) != 1:
        _refuse('Feature refinement requires one source way')
    reference = references[0]
    try:
        if osm_provider_namespace(reference.get('provider')) != osm_provider_namespace(document.get('provider')):
            _refuse('Supplementary and accepted source namespaces differ')
        way, nodes = osm_way_context(document, reference.get('provider_feature_id'))
        expected_class = {'road': 'highway', 'water': 'water'}.get(feature.get('feature_class'))
        if expected_class is None or way['provider_class'] != expected_class:
            _refuse('Normalized source class differs from the selected canonical feature domain')
        release = date.fromisoformat(reference['release'])
        cutoff = datetime(release.year, release.month, release.day, tzinfo=timezone.utc)
        for primitive in (way, *nodes):
            stamp = primitive['properties']['@timestamp']
            timestamp = datetime.fromtimestamp(stamp, tz=timezone.utc) if type(stamp) is int else datetime.fromisoformat(stamp.replace('Z', '+00:00'))
            if timestamp > cutoff:
                _refuse('Supplementary primitive version postdates the accepted source release')
    except IngestionError as error:
        raise CompilerError(error.code, str(error), **error.details) from error
    except (KeyError, ValueError, TypeError, OverflowError, OSError) as error:
        raise CompilerError('surface_refinement_invalid', 'Source release or primitive timestamp is unqualified') from error
    return way


def _bridge(repo, feature, source, projected, cells, manifests):
    if (feature['feature_class'] != 'road' or 'road_elevation_profile' in feature['attributes'] or
            source['properties'].get('highway') != feature['attributes'].get('road_class') or
            projected != feature['geometry']):
        _refuse('Bridge source class or complete directed projected geometry differs from the unchanged base Road')
    modeled = deepcopy(feature)
    modeled['attributes']['surface_context'] = surface_context(source['properties'], 'road', projected)
    profile = build_bridge_profile(repo, modeled, cells)
    support = {identifier: manifests[identifier]['terrain_artifact']['content_hash'] for identifier in profile['consumed_terrain']}
    changed = deepcopy(feature)
    changed['attributes']['road_elevation_profile'] = {'version': 1, 'surface': profile,
        'support_terrain_artifacts': support, 'base_feature_sha256': canonical_hash(feature)}
    return {changed['feature_id']: changed}


def _confluence(repo, features, all_features, document, projected, cells, grid, scratch):
    contracts = [f['attributes'].get('ribbon_buffer') for f in features]
    if (not contracts[0] or contracts[0] != contracts[1] or contracts[0].get('cap_style') != 'round' or
            contracts[0].get('join_style') != 'round'):
        _refuse('Confluence needs the unchanged admitted native ribbon preparation contract')
    contract = contracts[0]
    polygons = {f['feature_id']: f['geometry'] for f in all_features.values()
                if f['feature_class'] == 'water' and f['attributes'].get('surface_geometry') == 'polygon'}
    lines = {f['feature_id']: projected[f['source_refs'][0]['provider_feature_id']] for f in features}
    widths = {f['feature_id']: f['attributes']['width_m'] for f in features}
    forwarded = production_geometry_evidence(repo, scratch, polygons, lines, widths, TerrainSampler.from_cells(cells),
        grid['canonical_crs'], grid['coordinate_quantization'], contract['quadrant_segments'], contract['suppression_clearance_m'])
    if any(forwarded[2].get(f['feature_id']) != f['geometry'] for f in features):
        _refuse('Complete source axes and admitted polygon suppression do not reproduce the base Water geometry')
    return build_confluence_constraint(features, document, projected)


def prepare_surface_refinement(repo_root: Path, base_result_path: Path, output_root: Path, operations: list) -> tuple[dict, Path]:
    """Prepare one tmp-only result; authority activation belongs to canonical promotion."""
    repo_root, base_result_path, output_root = repo_root.resolve(), base_result_path.resolve(), output_root.resolve()
    scratch = repo_root / 'tmp'
    if output_root == scratch or not output_root.is_relative_to(scratch):
        _refuse('Surface candidates are confined to project tmp')
    base_bytes = base_result_path.read_bytes()
    base_hash = hashlib.sha256(base_bytes).hexdigest()
    base_root, base = accepted_base(base_result_path)
    if base_result_path.read_bytes() != base_bytes or canonical_hash(base) != canonical_hash(json.loads(base_bytes)):
        _refuse('Base receipt changed during authentication')
    if output_root == base_root or output_root.is_relative_to(base_root) or base_root.is_relative_to(output_root):
        _refuse('Base and candidate directories must be disjoint')
    entries = {e['path']: e for e in base['outputs']}
    coverage = _read_snapshot(base_root, entries, 'canonical/coverage.json')
    manifests = {e['cell_id']: _read_snapshot(base_root, entries, e['path']) for e in coverage['cells']}
    shards = {key: _read_artifact(base_root, entries, m['feature_artifact']) for key, m in manifests.items()}
    all_features = {}
    for owner, shard in shards.items():
        for feature in shard['features']:
            if feature['feature_id'] in all_features or feature['owner_cell_id'] != owner:
                _refuse('Base feature identity is duplicated or has inconsistent ownership')
            all_features[feature['feature_id']] = feature
    admitted, contexts, used, structural_documents, basin_documents = [], {}, set(), {}, {}
    authored_documents = {}
    if not isinstance(operations, list) or not operations:
        _refuse('At least one closed surface operation is required')
    context_digests = {op['sha256'] for op in operations if isinstance(op, dict) and isinstance(op.get('sha256'), str)}
    for operation in operations:
        if isinstance(operation, dict) and operation.get('kind') == 'authored_dry_terrain':
            from . import authored_terrain
            selected, retained, document = authored_terrain.admit(repo_root, operation)
            contexts.update(retained); admitted.append(selected)
            authored_documents[canonical_hash(selected)] = document
            continue
        if isinstance(operation, dict) and operation.get('kind') == 'identified_basin':
            from .. import basin_admission
            selected, retained, document = basin_admission.admit(repo_root, operation, all_features)
            if used.intersection(selected['feature_ids']):
                _refuse('Surface operations have interacting feature ownership')
            used.update(selected['feature_ids']); contexts.update(retained); admitted.append(selected)
            basin_documents[canonical_hash(selected)] = document
            continue
        if isinstance(operation, dict) and operation.get('kind') == 'constructed_surface_import':
            from .. import constructed_import
            selected, retained, documents = constructed_import.admit(repo_root, operation, all_features)
            if used.intersection(selected['feature_ids']):
                _refuse('Surface operations have interacting feature ownership')
            used.update(selected['feature_ids']); contexts.update(retained); admitted.append(selected)
            structural_documents[canonical_hash(selected)] = documents
            continue
        if not isinstance(operation, dict) or set(operation) != {'kind', 'feature_ids', 'context_path', 'sha256', 'byte_size'}:
            _refuse('Surface operation fields are unsupported')
        ids = operation['feature_ids']
        if (operation['kind'] not in ('bridge_elevation', 'water_confluence') or not isinstance(ids, list) or
                any(not isinstance(key, str) or key not in all_features for key in ids) or
                len(ids) != (1 if operation['kind'] == 'bridge_elevation' else 2) or len(set(ids)) != len(ids) or used.intersection(ids)):
            _refuse('Surface operations have unknown, duplicated or interacting feature ownership')
        used.update(ids)
        path = resolve_owned_path(repo_root, operation['context_path'])
        payload = path.read_bytes()
        digest = hashlib.sha256(payload).hexdigest()
        if type(operation['byte_size']) is not int or len(payload) != operation['byte_size'] or digest != operation['sha256']:
            _refuse('Supplementary context differs from its explicit byte pin')
        document = json.loads(payload)
        try:
            validate_source(document, path)
            for identifier in ids:
                _source(document, all_features[identifier])
        except IngestionError as error:
            raise CompilerError(error.code, str(error), **error.details) from error
        relative = 'refinement/context.json' if len(context_digests) == 1 else f'refinement/contexts/{digest}.json'
        contexts[relative] = payload
        admitted.append({'kind': operation['kind'], 'feature_ids': sorted(ids),
            'base_features': {key: canonical_hash(all_features[key]) for key in sorted(ids)},
            'context': {'path': relative, 'sha256': digest, 'byte_size': len(payload)}})
    admitted.sort(key=lambda op: (op['kind'], op['feature_ids'], canonical_hash(op)))
    request = {'$schema': 'https://alis.world/schemas/world-compiler/surface-refinement-request-v1.json',
        'schema_version': 1, 'base_result_sha256': base_hash, 'operations': admitted,
        'context_authority': 'supplementary_matching_context_not_historical_replay',
        'source_context_identity': osm_context_identity(), 'implementation_sha256': implementation_identity(),
        'execution_identity': execution_identity(repo_root, True)['identity_sha256']}
    validate_document(request, Path('surface-request.json'))
    input_descriptors = request_input_descriptors(request)
    request_hash = canonical_hash(request)
    if output_root.exists():
        previous = existing_result(output_root, request_hash)
        if previous is None:
            _refuse('Existing candidate is not owned by this operation')
        return previous, output_root
    output_root.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output_root.parent, prefix='surface-refinement-') as temporary:
        temporary = Path(temporary)
        cells = {key: _read_artifact(base_root, entries, m['terrain_artifact']) for key, m in manifests.items()}
        changes, structural_proofs, terrain_payloads, basin_proofs, basin_cells = {}, {}, {}, {}, {}
        authored_cells = set()
        for index, operation in enumerate(admitted):
            if operation['kind'] == 'authored_dry_terrain':
                from . import authored_terrain
                payloads = authored_terrain.construct(repo_root, operation,
                    authored_documents[canonical_hash(operation)], coverage, cells, {**all_features, **changes})
                if set(terrain_payloads).intersection(payloads):
                    _refuse('Authored operations require disjoint original Terrain cell ownership')
                authored_cells.update(payloads); terrain_payloads.update(payloads)
                continue
            if operation['kind'] == 'identified_basin':
                from .. import basin_admission
                feature, payloads, proof = basin_admission.construct(repo_root, operation,
                    basin_documents[canonical_hash(operation)], coverage, cells, {**all_features, **changes})
                if set(terrain_payloads).intersection(payloads):
                    _refuse('Joint basin operations require disjoint original Terrain cell ownership')
                changes[feature['feature_id']] = feature
                terrain_payloads.update(payloads); basin_proofs[feature['feature_id']] = proof
                basin_cells[feature['feature_id']] = sorted(payloads)
                continue
            if operation['kind'] == 'constructed_surface_import':
                from .. import constructed_import
                created, proofs = constructed_import.construct(repo_root, operation,
                    structural_documents[canonical_hash(operation)], coverage, manifests, temporary/f'structural-{index}')
                changes.update(created); structural_proofs.update(proofs)
                continue
            document = json.loads(contexts[operation['context']['path']])
            features = [all_features[key] for key in operation['feature_ids']]
            sources = [_source(document, feature) for feature in features]
            if operation['kind'] == 'bridge_elevation':
                _, nodes = osm_way_context(document, sources[0]['provider_feature_id'])
                if any(key in primitive['properties'] for primitive in (sources[0], *nodes) for key in ('ele', 'height', 'min_height')):
                    _refuse('Explicit bridge elevations need qualified vertical interpretation before terrain-derived construction')
            projected, missing = project_features(repo_root, sources, coverage['grid'], temporary / f'projection-{index}')
            if missing:
                _refuse('Supplementary source projection is incomplete')
            if operation['kind'] == 'bridge_elevation':
                changed = _bridge(repo_root, features[0], sources[0], projected.get(sources[0]['provider_feature_id']), cells, manifests)
            else:
                changed = _confluence(repo_root, features, all_features, document, projected, cells, coverage['grid'], temporary / f'water-{index}')
            changes.update(changed)
        if authored_cells:
            from .authored_terrain import validate_consumers
            validate_consumers(authored_cells, {**all_features, **changes})
        staging = temporary / 'candidate'
        staging.mkdir()
        for entry in base['outputs']:
            if entry['path'].startswith('refinement/'):
                _refuse('Compile base occupies the refinement evidence namespace')
            destination = resolve_owned_path(staging, entry['path'], must_exist=False)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(resolve_owned_path(base_root, entry['path']), destination)
            if destination.stat().st_size != entry['byte_size'] or file_hash(destination) != entry['sha256']:
                _refuse('Base artifact changed during candidate preparation')
        if terrain_payloads:
            from .publication import stage_terrain_refinements
            from ..basin_admission import bind_staged_support
            stage_terrain_refinements(repo_root, staging, coverage, cells, terrain_payloads)
            for identifier, support in basin_cells.items():
                bind_staged_support(staging, coverage, changes[identifier], support)
        created = {identifier: feature for identifier, feature in changes.items() if identifier not in all_features}
        changed_cells = sorted({feature['owner_cell_id'] for feature in changes.values()} |
            {cell for feature in created.values() for cell in feature['intersecting_cell_ids']} | set(terrain_payloads))
        for owner in changed_cells:
            entry = next(e for e in coverage['cells'] if e['cell_id'] == owner)
            manifest, shard = json.loads((staging/entry['path']).read_bytes()), deepcopy(shards[owner])
            owns_changes = any(f['owner_cell_id'] == owner for f in changes.values())
            new_owned = [created[key] for key in sorted(created) if created[key]['owner_cell_id'] == owner]
            if owns_changes:
                shard['features'] = [changes.get(f['feature_id'], f) for f in shard['features']]
                shard['features'].extend(new_owned)
                path = staging / manifest['feature_artifact']['path']
                write_json(path, shard)
                if new_owned:
                    manifest['owned_feature_ids'] = sorted(f['feature_id'] for f in shard['features'])
                manifest['feature_artifact'].update(content_hash=file_hash(path), byte_size=path.stat().st_size,
                    dependencies=[*manifest['feature_artifact']['dependencies'], 'surface-refinement:'+request_hash])
            new_references = {key for key, f in created.items() if owner in f['intersecting_cell_ids'] and f['owner_cell_id'] != owner}
            if new_references:
                manifest['referenced_feature_ids'] = sorted(set(manifest['referenced_feature_ids']) | new_references)
            entry = next(e for e in coverage['cells'] if e['cell_id'] == owner)
            path = staging / entry['path']
            write_json(path, manifest)
            entry.update(sha256=file_hash(path), byte_size=path.stat().st_size)
            for descriptor in coverage['artifact_descriptors']:
                if descriptor['artifact_id'] == manifest['feature_artifact']['artifact_id']:
                    descriptor.update({key: value for key, value in manifest['feature_artifact'].items() if key != 'path'})
        coverage['semantic_hash'] = canonical_hash({'base': coverage['semantic_hash'], 'surface_refinement': request_hash})
        coverage['feature_count'] += len(created)
        write_json(staging / 'canonical/coverage.json', coverage)
        for relative, payload in contexts.items():
            path = staging / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
        write_json(staging / 'refinement/request.json', request)
        report = {'$schema': 'https://alis.world/schemas/world-compiler/refinement-report-v1.json',
            'schema_version': 1, 'base_result_sha256': base_hash, 'inputs_hash': request_hash,
            'changed_cells': changed_cells, 'base_lineage_retained': True, 'production_adoption': False}
        if created:
            report['effective_feature_additions'] = {key: {'owner_cell_id': f['owner_cell_id'],
                'intersecting_cell_ids': f['intersecting_cell_ids'], 'model_sha256': f['attributes']['constructed_surface']['model_sha256'],
                'proof': structural_proofs[key]} for key, f in sorted(created.items())}
        if basin_proofs:
            report['identified_basins'] = basin_proofs
        write_json(staging / 'reports/refinement.json', report)
        result = {**base, '$schema': 'https://alis.world/schemas/world-compiler/refinement-result-v1.json',
            'base_result_sha256': base_hash, 'operation': 'refine', 'operation_id': 'refine:'+request_hash[:16],
            'inputs_hash': request_hash, 'outputs': [output_descriptor('source_evidence' if
                input_descriptors.get(p.relative_to(staging).as_posix(), (None, None))[1] == 'native' else 'compiler_document', p, staging)
                for p in sorted(staging.rglob('*.json'))]}
        write_json(staging / 'refinement_result.json', result)
        if base_result_path.read_bytes() != base_bytes:
            _refuse('Base receipt changed before candidate publication')
        if not request_is_current(repo_root, request):
            _refuse('Consumed surface implementation changed before candidate publication')
        promote_staging(staging, output_root)
    return result, output_root
