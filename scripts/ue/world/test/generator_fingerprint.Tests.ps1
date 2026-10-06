BeforeAll {
    . (Join-Path $PSScriptRoot '..\..\..\..\scripts\ue\world\generated_manifest.ps1')
    . (Join-Path $PSScriptRoot '..\..\..\..\scripts\ue\world\test\producer_identity_test_helpers.ps1')
    $repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
}

Describe 'ProjectWorld descriptor producer fingerprints' {
    BeforeEach {
        $fixtureRoot = Join-Path $TestDrive ([Guid]::NewGuid().ToString('N'))
        $source = 'Plugins/World/ProjectWorld/Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterRealization.cpp'
        $fixture = New-ProjectWorldProducerIdentityFixture -RepositoryRoot $repositoryRoot `
            -ProjectRoot $fixtureRoot -SourcePath @($source)
        $engine = 'e' * 64
    }

    It 'keeps every fingerprint when producer source, module, test, or script files change' {
        $before = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        foreach ($path in @(
            $source,
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationService.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldWaterEditor/ProjectWorldWaterEditor.Build.cs',
            'scripts/ue/world/realize_canonical_world.ps1',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_realization_profile.schema.json'
        )) {
            Write-ProjectWorldFixtureFile -Root $fixtureRoot -RelativePath $path -Text ([Guid]::NewGuid().ToString('N')) | Out-Null
        }
        $after = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        @(Get-ProjectWorldMovedProducers -Before $before -After $after).Count | Should -Be 0
    }

    It 'moves only the bumped producer on an output revision bump' {
        $before = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        Step-ProjectWorldDescriptorOutputRevision -Path $fixture.Descriptors['project_water_mesh:v1']
        $after = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        @(Get-ProjectWorldMovedProducers -Before $before -After $after) | Should -Be @('project_water_mesh:v1')
    }

    It 'moves every producer on a pipeline revision bump' {
        $before = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        $path = Join-Path $fixtureRoot 'Plugins/World/ProjectWorld/Data/Producers/realization_pipeline.json'
        Edit-ProjectWorldDescriptorText -Path $path -Pattern '"pipeline_revision"\s*:\s*1' `
            -Replacement '"pipeline_revision": 2'
        $after = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        @(Get-ProjectWorldMovedProducers -Before $before -After $after).Count | Should -Be $before.Count
    }

    It 'moves only the declaring producer when a data input changes' {
        $before = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        $path = Join-Path $fixtureRoot 'Plugins/World/ProjectWorldMeshTerrain/Content/Terrain/MPD_ProjectTerrain_Shared_v1.uasset'
        [IO.File]::WriteAllText($path, 'changed binary fixture')
        $after = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        @(Get-ProjectWorldMovedProducers -Before $before -After $after) | Should -Be @('project_mesh_terrain:v1')
    }

    It 'ignores descriptor formatting and key order' {
        $before = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        $path = $fixture.Descriptors['project_water_mesh:v1']
        $document = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $reordered = [ordered]@{
            modules = $document.modules
            data_inputs = $document.data_inputs
            output_revision = $document.output_revision
            generator_version = $document.generator_version
            generator_id = $document.generator_id
            kind = $document.kind
            schema_version = $document.schema_version
            '$schema' = $document.'$schema'
        }
        [IO.File]::WriteAllText($path, ($reordered | ConvertTo-Json -Depth 5).Replace("`n", "`r`n"))
        $after = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        @(Get-ProjectWorldMovedProducers -Before $before -After $after).Count | Should -Be 0
    }

    It 'moves every producer when engine identity changes' {
        $catalog = Get-ProjectWorldProducerCatalog -ProjectRoot $fixtureRoot
        foreach ($producerId in $catalog.Producers.Keys) {
            $a = Get-ProjectWorldGeneratorFingerprint -ProjectRoot $fixtureRoot -ProducerId $producerId `
                -Catalog $catalog -EngineIdentity ('a' * 64)
            $b = Get-ProjectWorldGeneratorFingerprint -ProjectRoot $fixtureRoot -ProducerId $producerId `
                -Catalog $catalog -EngineIdentity ('b' * 64)
            $a | Should -Not -Be $b
        }
    }

    It 'resolves engine identity through project config and fails closed when it is unknown' {
        (Get-ProjectWorldEngineBuildIdentity -ProjectRoot $fixtureRoot) | Should -Match '^[a-f0-9]{64}$'
        Remove-Item -LiteralPath (Join-Path $fixtureRoot 'engine/Engine/Build/Build.version')
        { Get-ProjectWorldEngineBuildIdentity -ProjectRoot $fixtureRoot } | Should -Throw '*Engine identity is unknown*'
    }

    It 'reproduces golden fingerprint vectors' {
        $catalog = Get-ProjectWorldProducerCatalog -ProjectRoot $fixtureRoot
        Get-ProjectWorldGeneratorFingerprint -ProjectRoot $fixtureRoot -ProducerId project_water_mesh:v1 `
            -Catalog $catalog -EngineIdentity $engine |
            Should -BeExactly 'fc7a9a4d5043fa1928c3460a6fb1fab2a88ffa4d18d9ad1d1e880c07e71369f1'
        Step-ProjectWorldDescriptorOutputRevision -Path $fixture.Descriptors['project_water_mesh:v1']
        $catalog = Get-ProjectWorldProducerCatalog -ProjectRoot $fixtureRoot
        Get-ProjectWorldGeneratorFingerprint -ProjectRoot $fixtureRoot -ProducerId project_water_mesh:v1 `
            -Catalog $catalog -EngineIdentity $engine |
            Should -BeExactly '155d4e8bbbc6a1364cfb16854686098f8c95079f715df36efe3b2b3173150b14'
    }

    It 'hashes text data inputs independent of line endings and byte order mark' {
        $path = Join-Path $fixtureRoot 'sample.json'
        [IO.File]::WriteAllBytes($path, ([Text.UTF8Encoding]::new($true)).GetPreamble() +
            [Text.Encoding]::UTF8.GetBytes("{  `"a`": 1 }`r`n"))
        $withBom = Get-ProjectWorldDataInputDigest -Path $path
        [IO.File]::WriteAllText($path, "{  `"a`": 1 }`n", [Text.UTF8Encoding]::new($false))
        Get-ProjectWorldDataInputDigest -Path $path | Should -BeExactly $withBom
    }

    It 'discovers only producer directories and ignores verify fixtures' {
        $before = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        $path = 'Plugins/World/ProjectWorld/Data/TestFixtures/Verify/fake.json'
        Write-ProjectWorldFixtureFile -Root $fixtureRoot -RelativePath $path `
            -Text (Get-Content $fixture.Descriptors['project_water_mesh:v1'] -Raw) | Out-Null
        $after = Get-ProjectWorldFixtureFingerprints -Fixture $fixture
        @(Get-ProjectWorldMovedProducers -Before $before -After $after).Count | Should -Be 0
    }

    It 'rejects malformed descriptors, unknown producers, and missing data' {
        $catalog = Get-ProjectWorldProducerCatalog -ProjectRoot $fixtureRoot
        { Get-ProjectWorldGeneratorFingerprint -ProjectRoot $fixtureRoot -ProducerId unknown:v1 -Catalog $catalog -EngineIdentity $engine } |
            Should -Throw '*Unknown ProjectWorld manifest producer*'
        $path = $fixture.Descriptors['project_water_mesh:v1']
        Edit-ProjectWorldDescriptorText -Path $path -Pattern '"output_revision"\s*:\s*1' `
            -Replacement '"output_revision": 0'
        { Get-ProjectWorldProducerCatalog -ProjectRoot $fixtureRoot } | Should -Throw '*output_revision*'
    }

    It 'fails closed on invalid producer descriptor fields and paths' {
        $path = $fixture.Descriptors['project_water_mesh:v1']
        $original = [IO.File]::ReadAllText($path)
        $cases = @(
            @{ Name = 'invalid JSON'; Pattern = '^\{'; Replacement = '[' },
            @{ Name = 'missing required field'; Pattern = '"output_revision"\s*:\s*1,'; Replacement = '' },
            @{ Name = 'unknown field'; Pattern = '"output_revision"\s*:\s*1,'; Replacement = '"output_revision": 1, "unknown": 1,' },
            @{ Name = 'wrong schema'; Pattern = 'project_world_producer_descriptor.schema.json'; Replacement = 'wrong.schema.json' },
            @{ Name = 'unsupported schema version'; Pattern = '"schema_version"\s*:\s*1'; Replacement = '"schema_version": 2' },
            @{ Name = 'fractional output revision'; Pattern = '"output_revision"\s*:\s*1'; Replacement = '"output_revision": 1.5' },
            @{ Name = 'wrong file stem'; Pattern = '"generator_id"\s*:\s*"project_water_mesh"'; Replacement = '"generator_id": "other"' },
            @{ Name = 'missing data'; Pattern = '"data_inputs"\s*:\s*\[\]'; Replacement = '"data_inputs": ["Plugins/World/ProjectWorld/Data/missing.json"]' },
            @{ Name = 'unsupported data type'; Pattern = '"data_inputs"\s*:\s*\[\]'; Replacement = '"data_inputs": ["Plugins/World/ProjectWorld/Data/test.png"]' },
            @{ Name = 'parent escape'; Pattern = '"data_inputs"\s*:\s*\[\]'; Replacement = '"data_inputs": ["Plugins/../other.json"]' },
            @{ Name = 'absolute data path'; Pattern = '"data_inputs"\s*:\s*\[\]'; Replacement = '"data_inputs": ["C:/other.json"]' },
            @{ Name = 'undeclared module'; Pattern = '"ProjectWorldWaterEditor"'; Replacement = '"UnlistedEditor"' },
            @{ Name = 'duplicate module'; Pattern = '"ProjectWorldWaterEditor"'; Replacement = '"ProjectWorldWaterEditor", "ProjectWorldWaterEditor"' },
            @{ Name = 'pipeline module claimed'; Pattern = '"ProjectWorldWaterEditor"'; Replacement = '"ProjectWorldEditor"' }
        )
        foreach ($case in $cases) {
            $matches = [regex]::Matches($original, $case.Pattern)
            $matches.Count | Should -Be 1 -Because "fixture anchor for $($case.Name) must be exact"
            [IO.File]::WriteAllText($path, ([regex]::Replace($original, $case.Pattern, $case.Replacement)))
            { Get-ProjectWorldProducerCatalog -ProjectRoot $fixtureRoot } |
                Should -Throw -Because "a $($case.Name) descriptor must fail closed"
        }
        [IO.File]::WriteAllText($path, $original)
        $catalog = Get-ProjectWorldProducerCatalog -ProjectRoot $fixtureRoot
        $catalog.Producers.Contains('project_water_mesh:v1') | Should -BeTrue
    }

    It 'derives producer ids from manifest ownership' {
        Get-ProjectWorldManifestProducerId -Manifest ([pscustomobject]@{ owning_layer = 'map' }) |
            Should -BeExactly 'map:v1'
        Get-ProjectWorldManifestProducerId -Manifest ([pscustomobject]@{ owning_layer = 'presentation' }) |
            Should -BeExactly 'presentation:v1'
        Get-ProjectWorldManifestProducerId -Manifest ([pscustomobject]@{
            owning_layer = 'terrain'; layer_contract = [pscustomobject]@{ generator_id = 'project_mesh_terrain'; generator_version = 1 }
        }) | Should -BeExactly 'project_mesh_terrain:v1'
    }

    It 'declares exactly the producers selected by tracked realization profiles' {
        $catalog = Get-ProjectWorldProducerCatalog -ProjectRoot $repositoryRoot
        $used = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        $used.Add('map:v1') | Out-Null
        $used.Add('presentation:v1') | Out-Null
        $profiles = Get-ChildItem -Path (Join-Path $repositoryRoot 'Plugins/*/*/Data/Profiles/Realization/*.json') -File
        $profiles.Count | Should -BeGreaterThan 0
        foreach ($profile in $profiles) {
            $document = Get-Content -LiteralPath $profile.FullName -Raw | ConvertFrom-Json
            foreach ($layer in @($document.layers)) {
                $producerId = "$([string]$layer.generator_id):v$([int]$layer.generator_version)"
                $catalog.Producers.Contains($producerId) | Should -BeTrue -Because "$($profile.Name):$($layer.layer_id) needs a descriptor"
                $used.Add($producerId) | Out-Null
                if ($layer.settings.PSObject.Properties.Name -contains 'shared_definition') {
                    $objectPath = [string]$layer.settings.shared_definition
                    $pluginName = ($objectPath -split '/')[1]
                    $assetParts = ($objectPath -split '/', 3)[2] -split '\.'
                    $pluginRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $catalog.Producers[$producerId].Path))
                    (Split-Path -Leaf $pluginRoot) | Should -BeExactly $pluginName
                    $relative = "$((ConvertTo-ProjectWorldFixtureRelativePath -Root $repositoryRoot -Path $pluginRoot))/Content/$($assetParts[0]).uasset"
                    $catalog.Producers[$producerId].DataInputs | Should -Contain $relative
                }
            }
        }
        @($catalog.Producers.Keys | Sort-Object) | Should -Be @($used | Sort-Object)
        foreach ($producerId in $catalog.Producers.Keys) {
            Get-ProjectWorldGeneratorFingerprint -ProjectRoot $repositoryRoot -ProducerId $producerId `
                -Catalog $catalog -EngineIdentity ('e' * 64) | Should -Match '^[a-f0-9]{64}$'
        }
    }

    It 'keeps compiler source fingerprints out of World production modules' {
        $sources = Get-ChildItem -LiteralPath (Join-Path $repositoryRoot 'Plugins/World') -Recurse -File |
            Where-Object {
                $_.FullName -match '[\\/]Source[\\/]' -and
                $_.FullName -notmatch '[\\/](Tests|Verify)[\\/]' -and
                $_.Extension -in @('.cpp', '.h', '.cs')
            }
        $sources.Count | Should -BeGreaterThan 0
        foreach ($sourceFile in $sources) {
            $text = [IO.File]::ReadAllText($sourceFile.FullName)
            $text | Should -Not -Match 'GetAdapterCompilerFingerprint|COMPILER_SOURCE_SHA256|AdapterCompiler' `
                -Because "$($sourceFile.FullName) cannot own a second producer identity"
        }
    }
}
