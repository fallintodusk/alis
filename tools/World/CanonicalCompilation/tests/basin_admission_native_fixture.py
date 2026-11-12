"""Fresh reduced joint children and resealed negative native-loader controls."""
from copy import deepcopy
import json
from pathlib import Path
import shutil
from unittest.mock import patch

from ..app.artifacts import output_descriptor
from ..app.contracts import canonical_hash, file_hash, read_json
from ..refinement.api import prepare_surface_refinement
from ..refinement import basin_admission
from ..refinement.app import publication
from .basin_admission_fixture import ROOT, make_fixture
from .identified_basin_fixture import fixture as fine_fixture


def raw_write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, allow_nan=False)+'\n', encoding='ascii', newline='\n')


def water_document(child):
    for path in sorted(child.glob('canonical/features/*.json')):
        document = read_json(path)
        if document['features']:
            return path, document, document['features'][0]
    raise AssertionError('Missing mechanical Water owner')


def reseal(child):
    coverage_path = child/'canonical/coverage.json'
    coverage = read_json(coverage_path)
    for entry in coverage['cells']:
        path = child/entry['path']
        manifest = read_json(path)
        for field in ('terrain_artifact', 'feature_artifact'):
            descriptor = manifest[field]
            artifact = child/descriptor['path']
            descriptor.update(content_hash=file_hash(artifact), byte_size=artifact.stat().st_size)
            next(d for d in coverage['artifact_descriptors'] if d['artifact_id'] == descriptor['artifact_id']).update(
                {k: v for k, v in descriptor.items() if k != 'path'})
        raw_write(path, manifest)
        entry.update(sha256=file_hash(path), byte_size=path.stat().st_size)
    raw_write(coverage_path, coverage)
    request = read_json(child/'refinement/request.json')
    result_path = child/'refinement_result.json'
    result = read_json(result_path)
    result['inputs_hash'] = canonical_hash(request)
    report = read_json(child/'reports/refinement.json')
    report['inputs_hash'] = result['inputs_hash']
    raw_write(child/'reports/refinement.json', report)
    result['outputs'] = [output_descriptor('compiler_document', p, child)
        for p in sorted(child.rglob('*.json')) if p != result_path]
    raw_write(result_path, result)


def change_input(child, mutate):
    request_path = child/'refinement/request.json'
    request = read_json(request_path)
    operation = request['operations'][0]
    old = child/operation['input']['path']
    document = read_json(old)
    mutate(document)
    raw_write(old, document)
    digest = file_hash(old)
    descriptor = {'path': f'refinement/inputs/{digest}.json', 'sha256': digest, 'byte_size': old.stat().st_size}
    old.rename(child/descriptor['path'])
    operation['input'] = descriptor
    raw_write(request_path, request)
    path, shard, water = water_document(child)
    water['attributes']['surface_function']['model_input'] = deepcopy(descriptor)
    raw_write(path, shard)
    for path in child.glob('canonical/refined_terrain/*.json'):
        value = read_json(path)
        value['consumed']['model_input_sha256'] = digest
        raw_write(path, value)


def export(destination):
    destination = destination.resolve()
    if destination.exists() or not destination.is_relative_to(ROOT/'tmp'):
        raise ValueError('Native joint controls require fresh project scratch')
    destination.mkdir(parents=True)
    values = make_fixture(destination)
    _, positive = prepare_surface_refinement(ROOT, values['base'], destination/'positive', [values['operation']])
    controls = []
    for name, code in (
            ('water_string_level', 'water-contract'), ('water_fraction_version', 'water-contract'),
            ('water_unknown_field', 'water-contract'), ('water_frame', 'water-contract'),
            ('input_wrong_schema', 'refinement-retained-input'), ('input_frame', 'refinement-retained-input'),
            ('input_level', 'identified-basin-input'), ('input_selected', 'identified-basin-input'),
            ('source_refs', 'identified-basin-source'), ('support_alias', 'identified-basin-input'),
            ('support_hash', 'identified-basin-input'), ('support_missing', 'identified-basin-input'),
            ('terrain_policy', 'terrain-refinement'), ('terrain_evidence', 'identified-basin-input')):
        child = destination/name
        shutil.copytree(positive, child)
        path, shard, water = water_document(child)
        function = water['attributes']['surface_function']
        if name == 'water_string_level': function['level_m'] = str(function['level_m'])
        elif name == 'water_fraction_version': function['function_version'] = 1.5
        elif name == 'water_unknown_field': function['observed'] = True
        elif name == 'water_frame': function['canonical_crs'] = 'EPSG:+32618'
        elif name == 'source_refs': water['source_refs'][0]['provider_feature_id'] = 'way/9003'
        elif name.startswith('support_'):
            key = sorted(function['terrain_support'])[0]
            support = function['terrain_support'][key]
            if name == 'support_alias':
                alias = 'canonical/refined_terrain/alias.json'
                shutil.copyfile(child/support['path'], child/alias)
                support['path'] = alias
            elif name == 'support_hash': support['sha256'] = '0'*64
            else: del function['terrain_support'][key]
        if name in ('water_string_level', 'water_fraction_version', 'water_unknown_field', 'water_frame',
                'source_refs', 'support_alias', 'support_hash', 'support_missing'):
            raw_write(path, shard)
        if name.startswith('input_'):
            def mutate(document):
                if name == 'input_wrong_schema': document['$schema'] = 'https://alis.world/schemas/world-compiler/constructed-import-model-v1.json'
                elif name == 'input_frame': document['grid']['frame']['horizontal'] = 'EPSG:32639'
                elif name == 'input_level': document['model']['parameters']['level_m'] -= .1
                else: document['selected'][sorted(document['selected'])[0]] = [[0, 1]]
            change_input(child, mutate)
        if name.startswith('terrain_'):
            key = sorted(function['terrain_support'])[0]
            support = function['terrain_support'][key]
            terrain = child/support['path']
            document = read_json(terrain)
            if name == 'terrain_policy': document['policy']['depth_m'] = str(document['policy']['depth_m'])
            else: document['consumed']['ground_evidence']['processing_sha256'] = '0'*64
            raw_write(terrain, document)
            support.update(sha256=file_hash(terrain), byte_size=terrain.stat().st_size)
            raw_write(path, shard)
        reseal(child)
        receipt = child/'refinement_result.json'
        controls.append({'name': name, 'receipt': receipt.relative_to(ROOT).as_posix(),
            'sha256': file_hash(receipt), 'error_code': code})

    # Deliberately bypass only the two Python outer-trace guards in this test
    # control. The native loader must independently reject the actual open trace.
    fine = fine_fixture()
    document = deepcopy(values['input'])
    document['grid'], document['evidence'] = fine['grid'], fine['evidence']
    document['grid'].update({'$schema': 'https://alis.world/schemas/world-source/ground-grid-v1.json', 'schema_version': 1})
    document['evidence']['grid_content_sha256'] = canonical_hash(document['grid'])
    input_path = destination/'fine-open-input.json'
    raw_write(input_path, document)
    operation = {**values['operation'], 'input': {'path': input_path.relative_to(ROOT).as_posix(),
        'sha256': file_hash(input_path), 'byte_size': input_path.stat().st_size}}
    with patch.object(basin_admission, '_check_outer'), patch.object(publication, '_check_outer'):
        _, child = prepare_surface_refinement(ROOT, values['base'], destination/'open_outer', [operation])
    receipt = child/'refinement_result.json'
    controls.append({'name': 'open_outer', 'receipt': receipt.relative_to(ROOT).as_posix(),
        'sha256': file_hash(receipt), 'error_code': 'terrain-refinement'})
    receipt = positive/'refinement_result.json'
    manifest = {'positive': {'receipt': receipt.relative_to(ROOT).as_posix(), 'sha256': file_hash(receipt)}, 'controls': controls}
    path = destination/'controls.json'
    raw_write(path, manifest)
    return {'manifest': path.relative_to(ROOT).as_posix(), 'sha256': file_hash(path), 'controls': len(controls)}


if __name__ == '__main__':
    import sys
    print(json.dumps(export(Path(sys.argv[1]))))
