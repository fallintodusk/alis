"""A complete captured parent over two synthetic flat cells in the real metric frame."""
from copy import deepcopy
import json
from pathlib import Path

from World.SourceIngestion.tests.osm_pier_relation_fixture import fixture as source_fixture
from World.ExecutionEnvironment.geometry.api import triangulate_polygon
from ..app.artifacts import output_descriptor
from ..app.contracts import canonical_hash, file_hash, read_json
from ..app.projection import project_features
from ..app.spatial import cell_bounds, cell_id
from ..refinement.app.contracts import write_json
from .refinement_fixture import make_base

ROOT = Path(__file__).resolve().parents[4]
PREFIX = 'https://alis.world/schemas/world-compiler/'
SOURCE_ID = 'relation/20784500'
FEATURE_ID = 'alis:osm:relation:20784500'


def _input(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False)+'\n', encoding='ascii', newline='\n')
    return {'path': path.relative_to(ROOT).as_posix(), 'sha256': file_hash(path), 'byte_size': path.stat().st_size}


def make_fixture(directory, *, supporting_member=False):
    base = directory/'base'
    make_base(base)
    coverage = read_json(base/'canonical/coverage.json')
    coverage['grid'].update(canonical_crs='EPSG:32618', vertical_datum='EPSG:3855',
        coordinate_transform='wgs84_to_canonical_crs', origin=[584233.41, 4510392.45],
        sample_spacing=[930., 930.], cell_quads=[1, 1])
    coverage['engine_georeference_origin'] = [585628., 4511322.]
    coverage['feature_count'] = 0
    descriptors, entries = [], []
    for old_entry, coordinates in zip(coverage['cells'], ((-2, -3), (-1, -3))):
        manifest_path = base/old_entry['path']; manifest = read_json(manifest_path)
        identifier = cell_id(coverage['grid_id'], *coordinates)
        bounds = list(cell_bounds(coverage['grid'], *coordinates))
        manifest.update(cell_id=identifier, cell_x=coordinates[0], cell_y=coordinates[1], bounds=bounds,
            owned_feature_ids=[], referenced_feature_ids=[], neighbors={})
        for name in ('terrain_lineage', 'feature_lineage'):
            manifest[name].update(identity_namespace='osm', source_area={'area_fingerprint': 'sha256:'+'0'*64,
                'bbox': [582373.41, 4507602.45, 584233.41, 4508532.45], 'axis_order': 'xy', 'crs': 'EPSG:32618'})
        for kind, attribute in (('terrain', 'terrain_artifact'), ('features', 'feature_artifact')):
            descriptor = manifest[attribute]; path = base/descriptor['path']; value = read_json(path)
            value['cell_id'] = identifier
            if kind == 'terrain':
                value.update(bounds=bounds, sample_spacing=[930., 930.], core_samples=[[0., 0.], [0., 0.]])
                value['vertical_provenance']['vertical_datum'] = 'EPSG:3855'
                value['raster_alignment']['canonical_crs'] = 'EPSG:32618'
                value['water_fit']['core_samples'] = [[0., 0.], [0., 0.]]
            else:
                value['features'] = []
            write_json(path, value)
            descriptor.update(artifact_id=f'{kind}:{identifier}', cell_id=identifier,
                content_hash=file_hash(path), byte_size=path.stat().st_size)
            descriptors.append({k: v for k, v in descriptor.items() if k != 'path'})
        write_json(manifest_path, manifest)
        entries.append({**old_entry, 'cell_id': identifier, 'sha256': file_hash(manifest_path),
            'byte_size': manifest_path.stat().st_size,
            'terrain_lineage': manifest['terrain_lineage'], 'feature_lineage': manifest['feature_lineage']})
    coverage.update(cells=entries, artifact_descriptors=descriptors,
        semantic_hash=canonical_hash({'fixture': 'flat_two_cell_structural_import', 'cells': entries}))
    write_json(base/'canonical/coverage.json', coverage)
    provenance = read_json(base/'reports/provenance.json')
    provenance['cell_lineage'] = entries
    write_json(base/'reports/provenance.json', provenance)
    result_path = base/'compile_result.json'; result = read_json(result_path)
    result['outputs'] = [output_descriptor('compiler_document', p, base) for p in sorted(base.rglob('*.json')) if p != result_path]
    write_json(result_path, result)

    document, primitives = source_fixture()
    relation = next(p for p in document['features'] if p['provider_feature_id'] == SOURCE_ID)
    projected, missing = project_features(ROOT, [relation], coverage['grid'], directory/'projection')
    if missing:
        raise ValueError('The complete captured parent projection is unavailable')
    footprint = projected[SOURCE_ID]
    faces = [[[*p, 3.55] for p in face] for face in triangulate_polygon(ROOT, footprint, 64)]
    policy = {'$schema': PREFIX+'constructed-import-policy-v1.json', 'schema_version': 1,
        'maximum_triangles': 4096, 'maximum_ordinary_gradient': .02,
        'maximum_gangway_grade': .125, 'maximum_cross_slope': .02, 'maximum_numeric_error_m': 1e-8}
    model = {'$schema': PREFIX+'constructed-import-model-v1.json', 'schema_version': 1,
        'basis': 'declared_identified_static_pwa', 'canonical_crs': 'EPSG:32618', 'vertical_datum': 'EPSG:3855',
        'provenance': {'basis': 'explicit_fixture_hypothesis_not_surveyed_or_historical_fit_replay',
            'producer_sha256': file_hash(Path(__file__))},
        'models': [{'primary_source_id': SOURCE_ID, 'role': 'fixed_pier',
            'source_bindings': [{'context_id': 'parent', 'provider_feature_id': SOURCE_ID}],
            'footprint': footprint, 'triangles': faces,
            'face_roles': [{'role': 'fixed_pier', 'source_ids': [SOURCE_ID], 'triangles': list(range(len(faces)))}]}]}
    if supporting_member:
        model['models'][0]['source_bindings'].append({'context_id': 'parent', 'provider_feature_id': 'way/1236753478'})
    inputs = {'model': _input(directory/'model.json', model), 'policy': _input(directory/'policy.json', policy),
        'source_contexts': [{'id': 'parent', 'normalized': _input(directory/'source-context.json', document),
            'native': {**_input(directory/'native-primitives.json', primitives), 'format': 'osm_json'}}]}
    operation = {'kind': 'constructed_surface_import', 'feature_ids': [FEATURE_ID], 'inputs': inputs}
    return {'base': result_path, 'operation': operation, 'footprint': deepcopy(footprint), 'faces': faces,
        'source': document, 'model': model, 'policy': policy}


if __name__ == '__main__':
    import argparse
    from ..refinement.api import prepare_surface_refinement
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root', required=True, type=Path)
    parser.add_argument('--supporting-member', action='store_true')
    args = parser.parse_args()
    target = args.output_root.resolve()
    if not target.is_relative_to(ROOT/'tmp'):
        parser.error('Disposable native fixtures must be created under repository tmp/')
    target.mkdir(parents=True, exist_ok=False)
    fixture = make_fixture(target, supporting_member=args.supporting_member)
    _, child = prepare_surface_refinement(ROOT, fixture['base'], target/'candidate', [fixture['operation']])
    receipt = child/'refinement_result.json'
    print(json.dumps({'receipt': receipt.relative_to(ROOT).as_posix(), 'sha256': file_hash(receipt)}))
