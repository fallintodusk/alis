# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')
}

Describe 'ProjectWorld producer-local generator fingerprints' {
    BeforeEach {
        $projectRoot = Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N'))
        $shared = 'scripts/ue/world/generated_manifest.ps1'
        $map = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlayRealization.cpp'
        $runtimePartition = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimePartitionPolicy.cpp'
        $evidenceHost = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldEditorModule.cpp'
        $semanticEvidence = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldSemanticEvidence.cpp'
        $staticAuditHost = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldStaticPartitionAudit.cpp'
        $authoredOverlay = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlay.cpp'
        $presentation = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialBinding.cpp'
        $terrain = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldLandscapeRealization.cpp'
        $water = 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldWaterRealization.cpp'
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
            $shared, $map, $runtimePartition, $evidenceHost, $semanticEvidence, $staticAuditHost, $authoredOverlay, $presentation, $terrain, $water, $roads, $surface,
            $vegetation, $vegetationExclusions, $buildings, $gameplay) + $catalogPaths) {
            $full = Join-Path $projectRoot $path
            New-Item -ItemType Directory -Path (Split-Path -Parent $full) -Force | Out-Null
            Set-Content -LiteralPath $full -Value "baseline:$path" -NoNewline
        }
    }

    It 'moves exactly the declared owners of each producer-local source' {
        $producers = @(
            'map:v1', 'presentation:v1', 'project_landscape:v1',
            'project_water_mesh:v1', 'project_road_mesh:v1', 'project_vegetation_instances:v1',
            'project_building_massing:v1', 'project_gameplay_placement:v1')
        $cases = @(
            @{ Path = $map; Owners = @('map:v1') },
            @{ Path = $runtimePartition; Owners = @('map:v1') },
            @{ Path = $evidenceHost; Owners = @() },
            @{ Path = $semanticEvidence; Owners = @() },
            @{ Path = $staticAuditHost; Owners = @() },
            @{ Path = $authoredOverlay; Owners = @(
                'map:v1', 'project_vegetation_instances:v1', 'project_building_massing:v1') },
            @{ Path = $presentation; Owners = @('presentation:v1') },
            @{ Path = $terrain; Owners = @('map:v1', 'project_landscape:v1') },
            @{ Path = $water; Owners = @('project_water_mesh:v1') },
            @{ Path = $roads; Owners = @('project_road_mesh:v1') },
            @{ Path = $surface; Owners = @(
                'map:v1', 'project_road_mesh:v1', 'project_vegetation_instances:v1',
                'project_building_massing:v1', 'project_gameplay_placement:v1') },
            @{ Path = $vegetation; Owners = @('project_vegetation_instances:v1') },
            @{ Path = $vegetationExclusions; Owners = @('project_vegetation_instances:v1') },
            @{ Path = $buildings; Owners = @('project_building_massing:v1') },
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
            'map:v1', 'presentation:v1', 'project_landscape:v1', 'project_water_mesh:v1',
            'project_road_mesh:v1', 'project_vegetation_instances:v1',
            'project_building_massing:v1', 'project_gameplay_placement:v1')) {
            $before[$producer] = Get-ProjectWorldGeneratorFingerprint `
                -ProjectRoot $projectRoot -ProducerId $producer
        }

        Set-Content -LiteralPath (Join-Path $projectRoot $shared) -Value 'shared-v2' -NoNewline

        foreach ($producer in $before.Keys) {
            Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot `
                -ProducerId $producer | Should -Not -Be $before[$producer]
        }
    }

    It 'keeps existing producers stable when a new tuple extends catalog and dispatch surfaces' {
        $producers = @(
            'map:v1', 'presentation:v1', 'project_landscape:v1',
            'project_water_mesh:v1', 'project_road_mesh:v1', 'project_vegetation_instances:v1',
            'project_building_massing:v1', 'project_gameplay_placement:v1')
        foreach ($path in $catalogPaths) {
            $before = @{}
            foreach ($producer in $producers) {
                $before[$producer] = Get-ProjectWorldGeneratorFingerprint `
                    -ProjectRoot $projectRoot -ProducerId $producer
            }
            Set-Content -LiteralPath (Join-Path $projectRoot $path) `
                -Value "new-tuple:$path" -NoNewline
            foreach ($producer in $producers) {
                Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot `
                    -ProducerId $producer | Should -Be $before[$producer]
            }
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
