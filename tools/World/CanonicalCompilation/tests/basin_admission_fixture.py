"""Captured source basin over a distinct coarse, closed mechanical reference."""
from copy import deepcopy
import json
from pathlib import Path

from ..app.artifacts import output_descriptor
from ..app.contracts import canonical_hash, file_hash, read_json
from ..app.spatial import cell_bounds, cell_id
from ..refinement.app.contracts import write_json
from .identified_basin_fixture import fixture
from .refinement_fixture import make_base

ROOT = Path(__file__).resolve().parents[4]
PREFIX = 'https://alis.world/schemas/world-compiler/'


def coarse_values():
    values = fixture()
    grid, evidence = values['grid'], values['evidence']
    parent = deepcopy(evidence)
    grid.update(spacing=[30., -30.], size=[2, 3],
        samples=[[grid['samples'][r][c] for c in (0, 10)] for r in (0, 10, 20)])
    grid.update({'$schema': 'https://alis.world/schemas/world-source/ground-grid-v1.json', 'schema_version': 1})
    evidence['grid_content_sha256'] = canonical_hash(grid)
    evidence['processing_sha256'] = canonical_hash({'method': 'captured_corner_node_affine_coarsening_v1',
        'parent_evidence': parent, 'grid_content_sha256': canonical_hash(grid)})
    for offset, cell in enumerate(values['cells'].values()):
        cell['core_samples'] = deepcopy(grid['samples'][offset:offset+2])
    return values


def reauthenticate(base):
    result_path = base/'compile_result.json'
    result = read_json(result_path)
    result['outputs'] = [output_descriptor('compiler_document', p, base)
        for p in sorted(base.rglob('*.json')) if p != result_path]
    write_json(result_path, result)


def add_feature(values, feature):
    base = values['base'].parent
    coverage = read_json(base/'canonical/coverage.json')
    entry = next(e for e in coverage['cells'] if e['cell_id'] == feature['owner_cell_id'])
    manifest_path = base/entry['path']
    manifest = read_json(manifest_path)
    path = base/manifest['feature_artifact']['path']
    shard = read_json(path)
    shard['features'].append(feature)
    write_json(path, shard)
    manifest['owned_feature_ids'].append(feature['feature_id'])
    manifest['feature_artifact'].update(content_hash=file_hash(path), byte_size=path.stat().st_size)
    write_json(manifest_path, manifest)
    entry.update(sha256=file_hash(manifest_path), byte_size=manifest_path.stat().st_size)
    for descriptor in coverage['artifact_descriptors']:
        if descriptor['artifact_id'] == manifest['feature_artifact']['artifact_id']:
            descriptor.update({k: v for k, v in manifest['feature_artifact'].items() if k != 'path'})
    coverage['feature_count'] += 1
    write_json(base/'canonical/coverage.json', coverage)
    reauthenticate(base)


def add_east_cell(values):
    """Add one unselected regular neighbor with the original shared-edge samples."""
    base = values['base'].parent
    coverage = read_json(base/'canonical/coverage.json')
    lower = next(e for e in coverage['cells'] if read_json(base/e['path'])['cell_y'] == 0)
    old = read_json(base/lower['path'])
    identifier = cell_id(coverage['grid_id'], 1, 0)
    manifest = deepcopy(old)
    manifest.update(cell_id=identifier, cell_x=1, cell_y=0,
        bounds=list(cell_bounds(coverage['grid'], 1, 0)),
        owned_feature_ids=[], referenced_feature_ids=[], neighbors={})
    for kind, key in (('terrain', 'terrain_artifact'), ('features', 'feature_artifact')):
        document = read_json(base/old[key]['path'])
        document['cell_id'] = identifier
        if kind == 'terrain':
            document['bounds'] = manifest['bounds']
            document['core_samples'] = [[row[1], row[1]] for row in document['core_samples']]
            document['water_fit']['core_samples'] = deepcopy(document['core_samples'])
            for name, rows in document['surface_semantics']['core_samples'].items():
                document['surface_semantics']['core_samples'][name] = [[row[1], row[1]] for row in rows]
        else:
            document['features'] = []
        relative = f'canonical/{kind}/east.json'
        write_json(base/relative, document)
        descriptor = deepcopy(old[key])
        descriptor.update(artifact_id=f'{kind}:{identifier}', cell_id=identifier, path=relative,
            content_hash=file_hash(base/relative), byte_size=(base/relative).stat().st_size)
        manifest[key] = descriptor
        coverage['artifact_descriptors'].append({k: v for k, v in descriptor.items() if k != 'path'})
    relative = 'canonical/cells/east.json'
    write_json(base/relative, manifest)
    coverage['cells'].append({**lower, 'cell_id': identifier, 'path': relative,
        'sha256': file_hash(base/relative), 'byte_size': (base/relative).stat().st_size})
    write_json(base/'canonical/coverage.json', coverage)
    provenance = read_json(base/'reports/provenance.json')
    provenance['cell_lineage'] = coverage['cells']
    write_json(base/'reports/provenance.json', provenance)
    reauthenticate(base)
    return identifier


def add_east_building(values):
    """Synthetic crossing footprint with its centroid in an unselected cell."""
    identifier = add_east_cell(values)
    base = values['base'].parent
    coverage = read_json(base/'canonical/coverage.json')
    lower = next(e for e in coverage['cells'] if read_json(base/e['path'])['cell_y'] == 0
        and read_json(base/e['path'])['cell_x'] == 0)
    old = read_json(base/lower['path'])
    feature = {'feature_id': 'alis:osm:way:9002', 'feature_class': 'building',
        'geometry': {'type': 'Polygon', 'coordinates': [[[586195., 4512908.], [586240., 4512908.],
            [586240., 4512911.], [586195., 4512911.], [586195., 4512908.]]]},
        'attributes': {'height_m': 10., 'min_height_m': 0.},
        'source_refs': [{'provider': 'OpenStreetMap', 'provider_feature_id': 'way/9002', 'release': '2026-08-31'}],
        'owner_cell_id': identifier, 'intersecting_cell_ids': sorted([old['cell_id'], identifier]),
        'representations': [], 'provenance_result': 'accepted'}
    add_feature(values, feature)
    old['referenced_feature_ids'].append(feature['feature_id'])
    write_json(base/lower['path'], old)
    lower.update(sha256=file_hash(base/lower['path']), byte_size=(base/lower['path']).stat().st_size)
    # add_feature refreshed the coverage file; retain its feature/artifact census.
    coverage = read_json(base/'canonical/coverage.json')
    next(e for e in coverage['cells'] if e['cell_id'] == old['cell_id']).update(lower)
    write_json(base/'canonical/coverage.json', coverage)
    provenance = read_json(base/'reports/provenance.json')
    provenance['cell_lineage'] = coverage['cells']
    write_json(base/'reports/provenance.json', provenance)
    reauthenticate(base)
    return feature


def make_fixture(directory):
    values = coarse_values()
    base = directory/'base'
    make_base(base)
    coverage = read_json(base/'canonical/coverage.json')
    grid = coverage['grid']
    grid.update(canonical_crs='EPSG:32618', vertical_datum='EPSG:3855',
        coordinate_transform='wgs84_to_canonical_crs', origin=[586183.41, 4512882.45],
        sample_spacing=[30., 30.], cell_quads=[1, 1])
    coverage['engine_georeference_origin'] = [585628., 4511322.]
    ids = [cell_id(coverage['grid_id'], 0, y) for y in (1, 0)]
    water = deepcopy(values['water'])
    water.update(owner_cell_id=ids[1], intersecting_cell_ids=sorted(ids), representations=[])
    descriptors, entries, cells = [], [], {}
    for offset, (old_entry, identifier) in enumerate(zip(coverage['cells'], ids, strict=True)):
        manifest_path = base/old_entry['path']
        manifest = read_json(manifest_path)
        bounds = list(cell_bounds(grid, 0, 1-offset))
        owns = offset == 1
        manifest.update(cell_id=identifier, cell_x=0, cell_y=1-offset, bounds=bounds,
            owned_feature_ids=[water['feature_id']] if owns else [],
            referenced_feature_ids=[] if owns else [water['feature_id']], neighbors={})
        for name in ('terrain_lineage', 'feature_lineage'):
            manifest[name].update(identity_namespace='osm', source_area={'area_fingerprint': 'sha256:'+'0'*64,
                'bbox': [586183.41, 4512882.45, 586213.41, 4512942.45], 'axis_order': 'xy', 'crs': 'EPSG:32618'})
        for kind, attribute in (('terrain', 'terrain_artifact'), ('features', 'feature_artifact')):
            descriptor = manifest[attribute]
            path = base/descriptor['path']
            value = read_json(path)
            value['cell_id'] = identifier
            if kind == 'terrain':
                value.update(bounds=bounds, sample_spacing=[30., 30.],
                    core_samples=deepcopy(values['grid']['samples'][offset:offset+2]))
                value['raster_alignment']['canonical_crs'] = 'EPSG:32618'
                value['vertical_provenance']['vertical_datum'] = 'EPSG:3855'
                value['water_fit']['core_samples'] = deepcopy(value['core_samples'])
                cells[identifier] = value
            else:
                value['features'] = [water] if owns else []
            write_json(path, value)
            descriptor.update(artifact_id=f'{kind}:{identifier}', cell_id=identifier,
                content_hash=file_hash(path), byte_size=path.stat().st_size)
            descriptors.append({k: v for k, v in descriptor.items() if k != 'path'})
        write_json(manifest_path, manifest)
        entries.append({**old_entry, 'cell_id': identifier, 'sha256': file_hash(manifest_path),
            'byte_size': manifest_path.stat().st_size,
            'terrain_lineage': manifest['terrain_lineage'], 'feature_lineage': manifest['feature_lineage']})
    coverage.update(cells=entries, artifact_descriptors=descriptors,
        semantic_hash=canonical_hash({'fixture': 'coarse_captured_basin_node_pwa', 'cells': entries}))
    write_json(base/'canonical/coverage.json', coverage)
    provenance = read_json(base/'reports/provenance.json')
    provenance['cell_lineage'] = entries
    write_json(base/'reports/provenance.json', provenance)
    reauthenticate(base)
    document = {'$schema': PREFIX+'identified-basin-input-v1.json', 'schema_version': 1,
        'reference_basis': 'identified_retained_grid_node_pwa', 'original_feature': water,
        'grid': values['grid'], 'evidence': values['evidence'],
        'selected': {identifier: [[0, 0]] for identifier in ids}, 'model': values['model'],
        'maximum_triangles': 4096}
    path = directory/'basin-input.json'
    path.write_text(json.dumps(document, indent=2, allow_nan=False)+'\n', encoding='ascii', newline='\n')
    operation = {'kind': 'identified_basin', 'feature_ids': [water['feature_id']],
        'input': {'path': path.relative_to(ROOT).as_posix(), 'sha256': file_hash(path), 'byte_size': path.stat().st_size}}
    return {'base': base/'compile_result.json', 'operation': operation, 'input_path': path,
        'input': document, 'cells': cells, 'water': water}


if __name__ == '__main__':
    import sys
    from ..refinement.api import prepare_surface_refinement
    destination = Path(sys.argv[1]).resolve()
    if destination.exists() or not destination.is_relative_to(ROOT/'tmp'):
        raise ValueError('Basin fixture generation requires fresh project scratch')
    destination.mkdir(parents=True)
    values = make_fixture(destination)
    result, child = prepare_surface_refinement(ROOT, values['base'], destination/'candidate', [values['operation']])
    receipt = child/'refinement_result.json'
    print(json.dumps({'receipt': receipt.relative_to(ROOT).as_posix(), 'sha256': file_hash(receipt),
        'outputs': len(result['outputs'])}))
