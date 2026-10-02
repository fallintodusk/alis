# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

BeforeAll {
    $script:ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
    $script:SchemaPath = Join-Path $script:ProjectRoot `
        'Plugins\World\ProjectWorld\Data\Schemas\project_world_realization_profile.schema.json'
    $script:ProfilePaths = @(
        'Plugins\World\ProjectWorldData\Data\Profiles\Realization\kazan_territory_v1.realization.json',
        'Plugins\World\ProjectWorldData\Data\Profiles\Realization\kazan_territory_public_v1.realization.json',
        'Plugins\World\ProjectWorldData\Data\Profiles\Realization\manhattan_showcase_v1.realization.json',
        'Plugins\World\ProjectWorldData\Data\Profiles\Realization\manhattan_showcase_public_v1.realization.json',
        'Plugins\World\ProjectWorldTestData\Data\Profiles\Realization\synthetic_landscape_water_twin.realization.json'
    ) | ForEach-Object { Join-Path $script:ProjectRoot $_ }
    $script:Validator = @'
import copy
import json
import pathlib
import sys

import jsonschema

schema = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding='utf-8'))
profiles = [json.loads(pathlib.Path(path).read_text(encoding='utf-8')) for path in sys.argv[2:]]
validator_type = jsonschema.validators.validator_for(schema)
validator_type.check_schema(schema)
validator = validator_type(schema)
for profile in profiles:
    validator.validate(profile)

mesh = profiles[0]
legacy = copy.deepcopy(mesh)
legacy['layers'][0].update({
    'generator_id': 'project_landscape',
    'canonical_selectors': ['terrain', 'water'],
    'spatial_ownership': 'logical_landscape_with_cell_proxies',
    'runtime_mapping': 'world_partition_owner',
    'settings': {'components_per_proxy': 1},
})
if not list(validator.iter_errors(legacy)):
    raise AssertionError('the removed Landscape terrain tuple was accepted')

parallel_identity = copy.deepcopy(mesh)
parallel_identity['landscape'] = {
    'logical_landscape_id': 'invalid_parallel_identity',
    'components_per_proxy': 1,
}
if not list(validator.iter_errors(parallel_identity)):
    raise AssertionError('Mesh Terrain accepted a parallel Landscape identity')
'@
}

Describe 'ProjectWorld realization profile schema' {
    It 'accepts the one Mesh Terrain tuple and rejects the removed Landscape tuple' {
        $arguments = @('-c', $script:Validator, $script:SchemaPath) + $script:ProfilePaths
        & python @arguments
        $LASTEXITCODE | Should -Be 0
    }
}
