# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')
}

Describe 'ProjectWorld producer-local generator fingerprints' {
    BeforeEach {
        $projectRoot = Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N'))
        $shared = 'scripts/ue/world/generated_manifest.ps1'
        $wrapper = 'scripts/ue/world/realize_canonical_world.ps1'
        $canonicalBundle = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldCanonicalBundle.cpp'
        $map = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlayRealization.cpp'
        $runtimePartition = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimePartitionPolicy.cpp'
        $evidenceHost = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldEditorModule.cpp'
        $semanticEvidence = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldSemanticEvidence.cpp'
        $staticAuditHost = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldStaticPartitionAudit.cpp'
        $authoredOverlay = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlay.cpp'
        $presentation = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialBinding.cpp'
        $terrain = 'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.cpp'
        $terrainTransformer = 'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Private/ProjectWorldMeshTerrainTransformer.cpp'
        $water = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldWaterRealization.cpp'
        $waterMeshBuilder = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldWaterMeshBuilder.cpp'
        $roads = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRoadRealization.cpp'
        $surface = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.cpp'
        $vegetation = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationPlacement.cpp'
        $vegetationExclusions = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationExclusions.cpp'
        $buildings = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldBuildingMeshBuilder.cpp'
        $gameplay = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGameplayPlacement.cpp'
        $catalogPaths = @(
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldLayerInventory.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldLayerInventory.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationGeneratorRegistry.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationGeneratorRegistry.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationProfile.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationProfile.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationService.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldRealizationService.h',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_realization_profile.schema.json',
            'scripts/ue/world/realization_layer_operation.ps1'
        )
        foreach ($path in @(
            $shared, $wrapper, $canonicalBundle, $map, $runtimePartition, $evidenceHost, $semanticEvidence, $staticAuditHost, $authoredOverlay, $presentation, $terrain, $water, $roads, $surface,
            $terrainTransformer, $waterMeshBuilder, $vegetation, $vegetationExclusions, $buildings, $gameplay) + $catalogPaths) {
            $full = Join-Path $projectRoot $path
            New-Item -ItemType Directory -Path (Split-Path -Parent $full) -Force | Out-Null
            Set-Content -LiteralPath $full -Value "baseline:$path" -NoNewline
        }
    }

    It 'moves exactly the declared owners of each producer-local source' {
        $producers = @(
            'map:v1', 'presentation:v1', 'project_mesh_terrain:v1',
            'project_water_mesh:v1', 'project_road_mesh:v1', 'project_vegetation_instances:v1',
            'project_building_massing:v1', 'project_building_massing:v2', 'project_gameplay_placement:v1')
        $cases = @(
            @{ Path = $map; Owners = @('map:v1') },
            @{ Path = $runtimePartition; Owners = @('map:v1') },
            @{ Path = $evidenceHost; Owners = @() },
            @{ Path = $semanticEvidence; Owners = @() },
            @{ Path = $staticAuditHost; Owners = @() },
            @{ Path = $authoredOverlay; Owners = @(
                'map:v1', 'project_vegetation_instances:v1', 'project_building_massing:v1',
                'project_building_massing:v2') },
            @{ Path = $presentation; Owners = @('presentation:v1') },
            @{ Path = $terrain; Owners = @('project_mesh_terrain:v1') },
            @{ Path = $terrainTransformer; Owners = @('project_mesh_terrain:v1') },
            @{ Path = $water; Owners = @('project_water_mesh:v1') },
            @{ Path = $waterMeshBuilder; Owners = @('project_water_mesh:v1') },
            @{ Path = $roads; Owners = @('project_road_mesh:v1') },
            @{ Path = $surface; Owners = @(
                'map:v1', 'project_road_mesh:v1', 'project_vegetation_instances:v1',
                'project_building_massing:v1', 'project_building_massing:v2', 'project_gameplay_placement:v1') },
            @{ Path = $vegetation; Owners = @('project_vegetation_instances:v1') },
            @{ Path = $vegetationExclusions; Owners = @('project_vegetation_instances:v1') },
            @{ Path = $buildings; Owners = @('project_building_massing:v1', 'project_building_massing:v2') },
            @{ Path = $gameplay; Owners = @('project_gameplay_placement:v1') })
        foreach ($case in $cases) {
            $before = @{}
            foreach ($producer in $producers) {
                $before[$producer] = Get-ProjectWorldGeneratorFingerprint `
                    -ProjectRoot $projectRoot -ProducerId $producer
            }
            Set-Content -LiteralPath (Join-Path $projectRoot $case.Path) `
                -Value ([System.Guid]::NewGuid().ToString('N')) -NoNewline
            foreach ($producer in $producers) {
                $changed = (Get-ProjectWorldGeneratorFingerprint `
                    -ProjectRoot $projectRoot -ProducerId $producer) -cne $before[$producer]
                $changed | Should -Be ($case.Owners -contains $producer)
            }
        }
    }

    It 'moves every producer when a true shared primitive changes' {
        $before = @{}
        foreach ($producer in @(
            'map:v1', 'presentation:v1', 'project_mesh_terrain:v1', 'project_water_mesh:v1',
            'project_road_mesh:v1', 'project_vegetation_instances:v1',
            'project_building_massing:v1', 'project_building_massing:v2', 'project_gameplay_placement:v1')) {
            $before[$producer] = Get-ProjectWorldGeneratorFingerprint `
                -ProjectRoot $projectRoot -ProducerId $producer
        }

        Set-Content -LiteralPath (Join-Path $projectRoot $shared) -Value 'shared-v2' -NoNewline

        foreach ($producer in $before.Keys) {
            Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot `
                -ProducerId $producer | Should -Not -Be $before[$producer]
        }
    }

    It 'ignores wrapper admission checks but tracks wrapper producer behavior' {
        $path = Join-Path $projectRoot $wrapper
        [System.IO.File]::WriteAllText($path, @'
producer-before
# PROJECTWORLD_PRODUCER_BEGIN preflight_only
admission-v1
# PROJECTWORLD_PRODUCER_END preflight_only
producer-after
'@)
        $producers = @(
            'map:v1', 'presentation:v1', 'project_mesh_terrain:v1',
            'project_water_mesh:v1', 'project_road_mesh:v1', 'project_vegetation_instances:v1',
            'project_building_massing:v1', 'project_building_massing:v2', 'project_gameplay_placement:v1')
        $before = @{}
        foreach ($producer in $producers) {
            $before[$producer] = Get-ProjectWorldGeneratorFingerprint `
                -ProjectRoot $projectRoot -ProducerId $producer
        }
        [System.IO.File]::WriteAllText($path,
            ([System.IO.File]::ReadAllText($path)).Replace('admission-v1', 'admission-v2'))
        foreach ($producer in $producers) {
            Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot `
                -ProducerId $producer | Should -Be $before[$producer]
        }
        [System.IO.File]::WriteAllText($path,
            ([System.IO.File]::ReadAllText($path)).Replace('producer-after', 'producer-changed'))
        foreach ($producer in $producers) {
            Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot `
                -ProducerId $producer | Should -Not -Be $before[$producer]
        }
    }

    It 'rejects unmatched wrapper source-scope markers' {
        $path = Join-Path $projectRoot $wrapper
        [System.IO.File]::WriteAllText($path, @'
producer-before
# PROJECTWORLD_PRODUCER_END preflight_only
producer-after
'@)
        {
            Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot -ProducerId 'map:v1'
        } | Should -Throw '*Malformed or unmatched producer-scoped source region*'
    }

    It 'rejects a malformed marker inside a scoped region' {
        $path = Join-Path $projectRoot $wrapper
        [System.IO.File]::WriteAllText($path, @'
producer-before
# PROJECTWORLD_PRODUCER_BEGIN preflight_only
admission
#PROJECTWORLD_PRODUCER_END preflight_only
producer-hidden
# PROJECTWORLD_PRODUCER_END preflight_only
producer-after
'@)
        {
            Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot -ProducerId 'map:v1'
        } | Should -Throw '*Malformed or unmatched producer-scoped source region*'
    }

    It 'is stable across text line endings but moves for a semantic edit' {
        $path = Join-Path $projectRoot $shared
        [System.IO.File]::WriteAllText($path, "line-one`r`nline-two`r`n")
        $crlf = Get-ProjectWorldGeneratorFingerprint `
            -ProjectRoot $projectRoot -ProducerId 'map:v1'

        [System.IO.File]::WriteAllText($path, "line-one`nline-two`n")
        $lf = Get-ProjectWorldGeneratorFingerprint `
            -ProjectRoot $projectRoot -ProducerId 'map:v1'
        $lf | Should -Be $crlf

        [System.IO.File]::WriteAllText($path, "line-one`nline-three`n")
        Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot `
            -ProducerId 'map:v1' | Should -Not -Be $lf
    }

    It 'moves only Building producers for an explicitly scoped shared parser region' {
        $producerIds = @(
            'map:v1', 'presentation:v1', 'project_mesh_terrain:v1', 'project_water_mesh:v1',
            'project_road_mesh:v1', 'project_vegetation_instances:v1',
            'project_building_massing:v1', 'project_building_massing:v2', 'project_gameplay_placement:v1')
        Set-Content -LiteralPath (Join-Path $projectRoot $canonicalBundle) -NoNewline -Value @'
shared-before
// PROJECTWORLD_PRODUCER_BEGIN project_building_massing
building-v1
// PROJECTWORLD_PRODUCER_END project_building_massing
shared-after
'@
        $before = @{}
        foreach ($producer in $producerIds) {
            $before[$producer] = Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot -ProducerId $producer
        }
        (Get-Content -Raw -LiteralPath (Join-Path $projectRoot $canonicalBundle)).Replace('building-v1', 'building-v2') |
            Set-Content -NoNewline -LiteralPath (Join-Path $projectRoot $canonicalBundle)
        foreach ($producer in $producerIds) {
            $changed = (Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot -ProducerId $producer) -cne $before[$producer]
            $changed | Should -Be $producer.StartsWith('project_building_massing:', [System.StringComparison]::Ordinal)
        }
    }

    It 'moves only the producer that owns byte-affecting catalog and dispatch surfaces' {
        $producers = @(
            'map:v1', 'presentation:v1', 'project_mesh_terrain:v1',
            'project_water_mesh:v1', 'project_road_mesh:v1', 'project_vegetation_instances:v1',
            'project_building_massing:v1', 'project_building_massing:v2', 'project_gameplay_placement:v1')
        $meshTerrainOwnedCatalogPaths = @(
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldLayerInventory.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationGeneratorRegistry.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationService.cpp')
        foreach ($path in $catalogPaths) {
            $before = @{}
            foreach ($producer in $producers) {
                $before[$producer] = Get-ProjectWorldGeneratorFingerprint `
                    -ProjectRoot $projectRoot -ProducerId $producer
            }
            Set-Content -LiteralPath (Join-Path $projectRoot $path) `
                -Value "new-tuple:$path" -NoNewline
            foreach ($producer in $producers) {
                $changed = (Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot `
                    -ProducerId $producer) -cne $before[$producer]
                $expected = $producer -ceq 'project_mesh_terrain:v1' -and
                    $meshTerrainOwnedCatalogPaths -contains $path
                $changed | Should -Be $expected
            }
        }
    }

    It 'moves every producer fingerprint when the engine build identity changes' {
        $previous = $env:PROJECT_WORLD_ENGINE_IDENTITY_OVERRIDE
        try {
            $env:PROJECT_WORLD_ENGINE_IDENTITY_OVERRIDE = 'engine-a'
            $before = Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot -ProducerId 'map:v1'
            $env:PROJECT_WORLD_ENGINE_IDENTITY_OVERRIDE = 'engine-b'
            Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot -ProducerId 'map:v1' |
                Should -Not -Be $before
        }
        finally {
            $env:PROJECT_WORLD_ENGINE_IDENTITY_OVERRIDE = $previous
        }
    }

    It 'derives the producer from manifest ownership and rejects unknown generators' {
        Get-ProjectWorldManifestProducerId -Manifest ([pscustomobject]@{ owning_layer = 'map' }) |
            Should -Be 'map:v1'
        Get-ProjectWorldManifestProducerId -Manifest ([pscustomobject]@{ owning_layer = 'presentation' }) |
            Should -Be 'presentation:v1'
        Get-ProjectWorldManifestProducerId -Manifest ([pscustomobject]@{
            owning_layer = 'roads'
            layer_contract = [pscustomobject]@{
                generator_id = 'project_road_mesh'
                generator_version = 1
            }
        }) | Should -Be 'project_road_mesh:v1'
        Get-ProjectWorldManifestProducerId -Manifest ([pscustomobject]@{
            owning_layer = 'vegetation'
            layer_contract = [pscustomobject]@{
                generator_id = 'project_vegetation_instances'
                generator_version = 1
            }
        }) | Should -Be 'project_vegetation_instances:v1'
        Get-ProjectWorldManifestProducerId -Manifest ([pscustomobject]@{
            owning_layer = 'buildings'
            layer_contract = [pscustomobject]@{
                generator_id = 'project_building_massing'
                generator_version = 1
            }
        }) | Should -Be 'project_building_massing:v1'
        Get-ProjectWorldManifestProducerId -Manifest ([pscustomobject]@{
            owning_layer = 'buildings'
            layer_contract = [pscustomobject]@{
                generator_id = 'project_building_massing'
                generator_version = 2
            }
        }) | Should -Be 'project_building_massing:v2'
        Get-ProjectWorldManifestProducerId -Manifest ([pscustomobject]@{
            owning_layer = 'gameplay'
            layer_contract = [pscustomobject]@{
                generator_id = 'project_gameplay_placement'
                generator_version = 1
            }
        }) | Should -Be 'project_gameplay_placement:v1'
        {
            Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot -ProducerId 'unknown:v1'
        } | Should -Throw '*Unknown ProjectWorld manifest producer*'
    }
}

Describe 'Mesh Terrain adapter compiler fingerprint locality' {
    It 'includes realization shapers and excludes diagnostic-only sources' {
        $buildPath = Join-Path $PSScriptRoot `
            '../../../../Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/ProjectWorldMeshTerrainEditor.Build.cs'
        $source = Get-Content -LiteralPath $buildPath -Raw
        $match = [regex]::Match(
            $source,
            'string\[\] FingerprintRelativePaths\s*=\s*\{(?<paths>.*?)\};',
            [System.Text.RegularExpressions.RegexOptions]::Singleline)
        $match.Success | Should -BeTrue
        $paths = @([regex]::Matches($match.Groups['paths'].Value, '"([^"]+)"') |
            ForEach-Object { $_.Groups[1].Value })
        $paths | Should -Contain `
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.cpp'
        $paths | Should -Contain `
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Private/ProjectWorldMeshTerrainTransformer.cpp'
        $paths | Should -Contain `
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainLayoutReceipt.cpp'
        $paths | Should -Contain `
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/ProjectWorldMeshTerrainEditor.Build.cs'
        $paths | Should -Not -Contain `
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainAuditCommandlet.cpp'
        $paths | Should -Not -Contain `
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainReceiptCommandlet.cpp'
        $source | Should -Not -Match 'Directory\.GetFiles\(ModuleDirectory'

        $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
        function Get-AdapterFingerprint([string]$ChangedPath, [byte[]]$ChangedBytes) {
            $stream = [System.IO.MemoryStream]::new()
            $writer = [System.IO.BinaryWriter]::new($stream, [System.Text.Encoding]::UTF8, $true)
            foreach ($path in ($paths | Sort-Object -CaseSensitive)) {
                $fullPath = Join-Path $repoRoot $path
                $bytes = if ($path -ceq $ChangedPath) { $ChangedBytes } else {
                    [System.IO.File]::ReadAllBytes($fullPath)
                }
                $writer.Write($path)
                $writer.Write([int]$bytes.Length)
                $writer.Write($bytes)
            }
            $writer.Flush()
            $sha = [System.Security.Cryptography.SHA256]::Create()
            $hash = $sha.ComputeHash($stream.ToArray())
            $sha.Dispose()
            $writer.Dispose()
            $stream.Dispose()
            return [BitConverter]::ToString($hash).Replace('-', '').ToLowerInvariant()
        }
        $baseline = Get-AdapterFingerprint '' @()
        $diagnostic = Get-AdapterFingerprint `
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainAuditCommandlet.cpp' `
            ([byte[]](1, 2, 3))
        $producer = Get-AdapterFingerprint `
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.cpp' `
            ([byte[]](1, 2, 3))
        $transformer = Get-AdapterFingerprint `
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Private/ProjectWorldMeshTerrainTransformer.cpp' `
            ([byte[]](1, 2, 3))
        $diagnostic | Should -BeExactly $baseline
        $producer | Should -Not -BeExactly $baseline
        $transformer | Should -Not -BeExactly $baseline
    }
}
