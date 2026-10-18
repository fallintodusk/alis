"""Civil consumers in a whole refined quad, including a dry Building anchor."""
from pathlib import Path
import sys

from .refinement_fixture import make_base, PREFIX
from ..app.artifacts import output_descriptor
from ..app.contracts import file_hash, read_json
from ..refinement.app.contracts import write_json
from ..refinement.api import prepare_terrain_refinement


def make_civil_base(root):
    cells, water, policy, selected = make_base(root, cross_cell=False)
    owner = next(iter(cells))
    terrain = cells[owner]
    terrain['core_samples'] = [[12, 12], [12, 14]]
    write_json(root / 'canonical/terrain/cell_0.json', terrain)
    road = dict(feature_id='alis:fixture:way:2', feature_class='road', owner_cell_id=owner,
        geometry={'type': 'LineString', 'coordinates': [[2, 15], [28, 15]]},
        attributes={'road_class': 'primary', 'width_m': 2}, intersecting_cell_ids=[owner],
        source_refs=[{'source_id': 'fixture'}], provenance_result='accepted',
        representations=[{'cell_id': owner, 'representation': 'road_fragment',
                          'geometry': {'type': 'MultiLineString', 'coordinates': [[[2, 15], [28, 15]]]}}])
    geometry = {'type': 'Polygon', 'coordinates': [[[2, 2], [6, 2], [6, 6], [2, 6], [2, 2]]]}
    building = dict(feature_id='alis:fixture:way:3', feature_class='building', owner_cell_id=owner,
        geometry=geometry, intersecting_cell_ids=[owner], source_refs=[{'source_id': 'fixture'}],
        representations=[], provenance_result='accepted',
        attributes={'height_m': 8, 'building_massing_version': 2, 'logical_building_id': 'way/3',
            'volume_selection': 'outline_no_parts', 'effective_volumes': [dict(volume_id='way/3/outline',
                source_feature_id='way/3', geometry=geometry, min_height_m=0, height_m=8,
                height_basis='fixture', min_height_basis='fixture')]})
    shard_path = root / 'canonical/features/cell_0.json'
    shard = read_json(shard_path)
    shard['features'] += [road, building]
    write_json(shard_path, shard)
    manifest_path = root / 'canonical/cells/cell_0.json'
    manifest = read_json(manifest_path)
    manifest['owned_feature_ids'] += [road['feature_id'], building['feature_id']]
    for descriptor in (manifest['terrain_artifact'], manifest['feature_artifact']):
        path = root / descriptor['path']
        descriptor.update(content_hash=file_hash(path), byte_size=path.stat().st_size)
    write_json(manifest_path, manifest)
    coverage_path = root / 'canonical/coverage.json'
    coverage = read_json(coverage_path)
    coverage['cells'][0].update(sha256=file_hash(manifest_path), byte_size=manifest_path.stat().st_size)
    coverage['feature_count'] += 2
    for descriptor in coverage['artifact_descriptors']:
        for updated in (manifest['terrain_artifact'], manifest['feature_artifact']):
            if descriptor['artifact_id'] == updated['artifact_id']:
                descriptor.update({k: v for k, v in updated.items() if k != 'path'})
    write_json(coverage_path, coverage)
    result_path = root / 'compile_result.json'
    result = read_json(result_path)
    result['outputs'] = [output_descriptor('compiler_document', p, root)
                         for p in sorted(root.rglob('*.json')) if p != result_path]
    write_json(result_path, result)
    policy['maximum_cut_m'] = 4
    return cells, water, policy, selected


def make_candidate(root):
    cells, water, policy, selected = make_civil_base(root / 'base')
    prepare_terrain_refinement(Path(__file__).resolve().parents[4], root / 'base/compile_result.json',
                              root / 'candidate', water, policy, selected)
    return cells, water, policy, selected


if __name__ == '__main__':
    make_candidate(Path(sys.argv[1]))
