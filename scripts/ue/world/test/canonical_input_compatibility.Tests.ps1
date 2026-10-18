BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')
    . (Join-Path $PSScriptRoot '..\realization_layer_operation.ps1')
    . (Join-Path $PSScriptRoot '..\layer_execution_selection.ps1')
    $script:RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
    $script:FixtureParent = Join-Path $script:RepositoryRoot 'tmp/world/canonical_compatibility_tests'

    . (Join-Path $PSScriptRoot 'canonical_compatibility_test_helpers.ps1')
}

Describe 'Persisted native input compatibility metadata' {
    BeforeEach {
        $projectRoot=Join-Path $script:FixtureParent ([guid]::NewGuid().ToString('N'))
        $manifestRoot=Join-Path $projectRoot 'Plugins/World/ProjectWorldTestData/Data/Manifests'
        $schemaRoot=Join-Path $projectRoot 'Plugins/World/ProjectWorld/Data/Schemas'
        New-Item -ItemType Directory -Path $manifestRoot,$schemaRoot -Force | Out-Null
        foreach ($name in @('project_world_active_manifest_set.schema.json','project_world_generated_manifest.schema.json')) {
            Copy-Item -LiteralPath (Join-Path $script:RepositoryRoot ('Plugins/World/ProjectWorld/Data/Schemas/'+$name)) -Destination $schemaRoot
        }
        $fixture=New-CompatibilityFixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
    }
    AfterEach {
        $target=[IO.Path]::GetFullPath($projectRoot)
        if (-not $target.StartsWith(([IO.Path]::GetFullPath($script:FixtureParent)+'\'),[StringComparison]::OrdinalIgnoreCase)) {throw 'Fixture cleanup escapes scratch.'}
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    It 'publishes truthful mixed lineage and cold reads it without changing carried manifests' {
        $carried=@{}; foreach ($entry in $fixture.Active.Record.scopes | Where-Object scope_id -ne layer_fixture_roads) {
            $carried[$entry.manifest_path]=Get-ProjectWorldFileSha256 (Join-Path $manifestRoot $entry.manifest_path)
        }
        $accepted=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $accepted.Manifests.layer_fixture_roads.input_identity.compile_result_sha256 | Should -Be ('7'*64)
        foreach ($scope in @('map_fixture','layer_fixture_terrain','layer_fixture_vegetation','layer_fixture_buildings','layer_fixture_water')) {
            $accepted.Manifests[$scope].input_identity.compile_result_sha256 | Should -Be ('a'*64)
        }
        @($accepted.Record.canonical_input_compatibility).Count | Should -Be 1
        foreach ($path in $carried.Keys) { (Get-ProjectWorldFileSha256 (Join-Path $manifestRoot $path)) | Should -Be $carried[$path] }
        $noop=Publish-ProjectWorldActiveSet -ProjectRoot $projectRoot -ManifestRoot $manifestRoot -TransactionId ('2'*32) `
            -OperationId noop -CandidateManifests @() -PriorActiveSet $accepted
        $noop.Sha256 | Should -Be $accepted.Sha256
        $repeated=Publish-ProjectWorldActiveSet -ProjectRoot $projectRoot -ManifestRoot $manifestRoot -TransactionId ('2'*32) `
            -OperationId noop -CandidateManifests @() -PriorActiveSet $accepted `
            -CanonicalInputCompatibility $accepted.Record.canonical_input_compatibility[0]
        $repeated.Sha256 | Should -Be $accepted.Sha256
    }
    It 'refuses forged inputs or changed-unit declarations before writing manifests or activation' {
        $before=Get-ProjectWorldFileSha256 (Join-Path $manifestRoot 'active_set.json')
        $fixture.Proof.layers.layer_fixture_roads.after_canonical_inputs_sha256='0'*64
        { Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot } | Should -Throw '*consumed*'
        (Get-ProjectWorldFileSha256 (Join-Path $manifestRoot 'active_set.json')) | Should -Be $before
        Test-Path (Join-Path $manifestRoot 'scopes/layer_fixture_roads.2.json') | Should -BeFalse
        $fixture.Proof.layers.layer_fixture_roads.after_canonical_inputs_sha256 = Get-ProjectWorldInputSetDigest $fixture.Next.layer_contract.canonical_inputs
        $fixture.Proof.layers.layer_fixture_roads.changes.canonical_inputs.changed_units = @('forged')
        { Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot } | Should -Throw '*changed-unit*'
        $fixture.Proof.layers.layer_fixture_roads.changes.canonical_inputs.changed_units = @('cell')
        $fixture.Proof.candidate['unqualified'] = $true
        { Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot } | Should -Throw '*unknown field*'
        (Get-ProjectWorldFileSha256 (Join-Path $manifestRoot 'active_set.json')) | Should -Be $before
    }
    It 'refuses corrupted historical owner evidence on cold read' {
        $accepted=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        Add-Content -LiteralPath (Join-Path $manifestRoot 'scopes/layer_fixture_roads.1.json') -Value ' '
        { Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot } | Should -Throw '*prior owner evidence*'
        (Get-ProjectWorldFileSha256 (Join-Path $manifestRoot 'active_set.json')) | Should -Be $accepted.Sha256
    }
    It 'refuses unselected producer changes even when the carried proof claims the new fingerprint' {
        $before=Get-ProjectWorldFileSha256 (Join-Path $manifestRoot 'active_set.json')
        $other=New-ProjectWorldFingerprintMigrationCandidate -PriorManifest $fixture.Active.Manifests.layer_fixture_buildings `
            -Generation 2 -OperationId child -GeneratorFingerprint ('0'*64)
        $fixture.Proof.layers.layer_fixture_buildings.accepted.current_generator_fingerprint='0'*64
        { Publish-ProjectWorldActiveSet -ProjectRoot $projectRoot -ManifestRoot $manifestRoot -TransactionId ('4'*32) `
            -OperationId child -CandidateManifests @($fixture.Next,$other) -PriorActiveSet $fixture.Active `
            -CanonicalInputCompatibility $fixture.Proof } | Should -Throw '*Unselected producer*'
        (Get-ProjectWorldFileSha256 (Join-Path $manifestRoot 'active_set.json')) | Should -Be $before
        Test-Path (Join-Path $manifestRoot 'scopes/layer_fixture_buildings.2.json') | Should -BeFalse
        Test-Path (Join-Path $manifestRoot 'scopes/layer_fixture_roads.2.json') | Should -BeFalse
    }
    It 'refuses pretending changed effective units retain the old base identity' {
        $fixture.Next.input_identity.compile_result_sha256='a'*64
        { Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot } | Should -Throw '*carried lineage*'
    }
    It 'carries another map binding and retires only the removed map' {
        $first=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $other=New-CompatibilityFixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot -Prefix other -PriorActiveSet $first
        $both=Save-CompatibilityFixture -Fixture $other -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        @($both.Record.canonical_input_compatibility).Count | Should -Be 2
        $retired=@('map_other')+@($other.Definitions.Keys)
        Publish-ProjectWorldActiveSet -ProjectRoot $projectRoot -ManifestRoot $manifestRoot -TransactionId ('3'*32) `
            -OperationId delete -CandidateManifests @() -RetiredScopeIds $retired -PriorActiveSet $both | Out-Null
        $remaining=Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
        @($remaining.Record.canonical_input_compatibility).Count | Should -Be 1
        $remaining.Record.canonical_input_compatibility[0].map_scope_id | Should -Be map_fixture
    }
    It 'validates the persisted closed schema and rejects extra nested fields' {
        Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot | Out-Null
        $probe=@'
import copy,json,pathlib,sys,jsonschema
schema=json.loads(pathlib.Path(sys.argv[1]).read_text())
document=json.loads(pathlib.Path(sys.argv[2]).read_text())
validator=jsonschema.Draft202012Validator(schema)
validator.check_schema(schema);validator.validate(document)
bad=copy.deepcopy(document);bad['canonical_input_compatibility'][0]['candidate']['unqualified']=True
assert list(validator.iter_errors(bad)), 'closed nested contract failed to refuse'
'@
        & python -c $probe (Join-Path $schemaRoot 'project_world_active_manifest_set.schema.json') (Join-Path $manifestRoot 'active_set.json')
        $LASTEXITCODE | Should -Be 0
    }
}
