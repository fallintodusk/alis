# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Get-ProjectWorldManifestProducerId {
    param([Parameter(Mandatory = $true)][object]$Manifest)

    $owner = [string]$Manifest.owning_layer
    if ($owner -eq 'map') { return 'map:v1' }
    if ($owner -eq 'presentation') { return 'presentation:v1' }
    if (-not ($Manifest.PSObject.Properties.Name -contains 'layer_contract') -or
        $null -eq $Manifest.layer_contract) {
        throw "Layer manifest has no producer contract: $($Manifest.scope_id)"
    }
    $generatorId = [string]$Manifest.layer_contract.generator_id
    $generatorVersion = [int]$Manifest.layer_contract.generator_version
    return "${generatorId}:v${generatorVersion}"
}

function Get-ProjectWorldProducerSourcePaths {
    param([Parameter(Mandatory = $true)][string]$ProducerId)

    # Catalog, validation, and dispatch surfaces admit producers but do not
    # produce an existing owner's bytes. They are intentionally absent here;
    # owner-local byte and manifest behavior belongs in the producer branches.
    $shared = @(
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/ProjectWorldEditor.Build.cs',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldCanonicalBundle.h',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldRealizeCommandlet.h',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldCanonicalBundle.cpp',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldCanonicalUtilities.cpp',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldCoordinateMapping.cpp',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldDataRoots.cpp',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldDataRoots.h',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeometryParsing.cpp',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeometryParsing.h',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldLayerDirtyInput.cpp',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldLayerDirtyInput.h',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPartitionPolicy.cpp',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPartitionPolicy.h',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizeCommandlet.cpp',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldSavePolicy.h',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldSchemaReference.cpp',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldSchemaReference.h',
        'Plugins/World/ProjectWorld/Data/Schemas/project_world_active_manifest_set.schema.json',
        'Plugins/World/ProjectWorld/Data/Schemas/project_world_generated_manifest.schema.json',
        'Plugins/World/ProjectWorld/Data/Schemas/project_world_layer_dirty_input.schema.json',
        'scripts/ue/world/execution_envelope.ps1',
        'scripts/ue/world/generated_content_transaction.ps1',
        'scripts/ue/world/generated_layer_manifest.ps1',
        'scripts/ue/world/generated_manifest.ps1',
        'scripts/ue/world/operator_controls.ps1',
        'scripts/ue/world/realize_canonical_world.ps1',
        'scripts/ue/world/world_data_roots.ps1'
    )
    $producer = switch ($ProducerId) {
        'map:v1' { @(
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAnchorPlacement.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAnchorPlacement.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlay.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlay.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlayRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlayRealization.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldEvidenceCapture.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldEvidenceCapture.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedActorLifecycle.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationRealization.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimeNavigation.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimeNavigation.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimePartitionPolicy.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimePartitionPolicy.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimePartitionRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimePartitionRealization.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimeProfile.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimeProfile.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimeRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimeRealization.h',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_authored_overlay.schema.json',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_runtime_profile.schema.json'
        ) }
        'presentation:v1' { @(
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialBinding.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialBinding.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.h',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_presentation_profile.schema.json'
        ) }
        'project_mesh_terrain:v1' { @(
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldTerrainProducerRegistry.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldTerrainProducerRegistry.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldLayerInventory.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationGeneratorRegistry.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationService.cpp',
            'Plugins/World/ProjectWorldMeshTerrain/ProjectWorldMeshTerrain.uplugin',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/ProjectWorldMeshTerrain.Build.cs',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Public/ProjectWorldMeshTerrainPartition.h',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Public/ProjectWorldMeshTerrainTransformer.h',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Private/ProjectWorldMeshTerrainPartition.cpp',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain/Private/ProjectWorldMeshTerrainTransformer.cpp',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/ProjectWorldMeshTerrainEditor.Build.cs',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Public/ProjectWorldMeshTerrainLayoutReceipt.h',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainLayoutReceipt.cpp',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainBuildPipeline.h',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.h',
            'Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.cpp',
            'Plugins/World/ProjectWorldMeshTerrain/Data/Schemas/mesh-terrain-layout-receipt.schema.json',
            'Plugins/World/ProjectWorldMeshTerrain/Content/Terrain/MPD_ProjectTerrain_Shared_v1.uasset'
        ) }
        'project_water_mesh:v1' { @(
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldWaterContractParsing.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldWaterContractParsing.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldWaterMeshBuilder.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldWaterMeshBuilder.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldWaterRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldWaterRealization.h'
        ) }
        'project_road_mesh:v1' { @(
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRoadRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRoadRealization.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedActorLifecycle.cpp',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_presentation_profile.schema.json'
        ) }
        'project_vegetation_instances:v1' { @(
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlay.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlay.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedActorLifecycle.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationExclusions.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationExclusions.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationPlacement.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationRealization.h',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_authored_overlay.schema.json',
            'Plugins/Resources/ProjectObject/Content/Nature/ExteriorPlant/Tree/AmurCork/SM_Tree_AmurCork_Big.uasset',
            'Plugins/Resources/ProjectObject/Content/Nature/ExteriorPlant/Tree/Hornbeam/SM_Tree_Hornbeam_Medium.uasset'
        ) }
        { $_ -in @('project_building_massing:v1', 'project_building_massing:v2') } { @(
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlay.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAuthoredOverlay.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldBuildingInventory.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldBuildingInventory.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldBuildingMeshBuilder.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldBuildingMeshBuilder.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldBuildingRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldBuildingRealization.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedActorLifecycle.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.h',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_authored_overlay.schema.json',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_presentation_profile.schema.json'
        ) }
        'project_gameplay_placement:v1' { @(
            'Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Services/IObjectSpawnService.h',
            'Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Services/ObjectSpawnServiceImpl.cpp',
            'Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Services/ObjectSpawnServiceImpl.h',
            'Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp',
            'Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Spawning/ObjectSpawnUtility.h',
            'Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Private/Pickup/PickupCapabilityComponent.cpp',
            'Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Pickup/PickupCapabilityComponent.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGameplayPlacement.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGameplayPlacement.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedActorLifecycle.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.h',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_gameplay_placement.schema.json'
        ) }
        default { throw "Unknown ProjectWorld manifest producer: $ProducerId" }
    }
    return @($shared + $producer | Sort-Object -Unique)
}

function Get-ProjectWorldProducerSourceDigest {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ProducerId
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return 'missing'
    }

    $extension = [System.IO.Path]::GetExtension($Path).ToLowerInvariant()
    if ($extension -in @('.uasset', '.umap', '.zip')) {
        return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }

    $text = [System.IO.File]::ReadAllText($Path)
    $text = $text.Replace("`r`n", "`n").Replace("`r", "`n")
    if ($text -notmatch 'PROJECTWORLD_PRODUCER_(BEGIN|END)') {
        $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($text)
        $sha = [System.Security.Cryptography.SHA256]::Create()
        try {
            return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
        }
        finally { $sha.Dispose() }
    }

    $pattern = '(?ms)^[ \t]*(?://|#) PROJECTWORLD_PRODUCER_BEGIN (?<owner>[A-Za-z0-9_.-]+)\r?\n(?<body>.*?)^[ \t]*(?://|#) PROJECTWORLD_PRODUCER_END \k<owner>\r?\n'
    # Every marker token must delimit a well-formed region. A stray token inside
    # a region body would otherwise extend that region over producing code.
    $markers = [regex]::Matches($text, 'PROJECTWORLD_PRODUCER_(?:BEGIN|END)')
    $regions = [regex]::Matches($text, $pattern)
    if ($markers.Count -ne 2 * $regions.Count) {
        throw "Malformed or unmatched producer-scoped source region: $Path"
    }
    $scoped = [System.Text.RegularExpressions.Regex]::Replace(
        $text,
        $pattern,
        [System.Text.RegularExpressions.MatchEvaluator]{
            param($match)
            $owner = $match.Groups['owner'].Value
            if ($ProducerId.StartsWith("${owner}:", [System.StringComparison]::Ordinal)) {
                return $match.Groups['body'].Value
            }
            return ''
        })

    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($scoped)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}

function Get-ProjectWorldEngineBuildIdentity {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    if (-not [string]::IsNullOrWhiteSpace($env:PROJECT_WORLD_ENGINE_IDENTITY_OVERRIDE)) {
        return "override:$($env:PROJECT_WORLD_ENGINE_IDENTITY_OVERRIDE)"
    }
    $configRoot = Join-Path $ProjectRoot 'scripts\config'
    $engineRoot = ''
    foreach ($configName in @('ue_path.local.conf', 'ue_path.conf')) {
        $configPath = Join-Path $configRoot $configName
        if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) { continue }
        $match = Select-String -LiteralPath $configPath -Pattern '^UE_PATH=(?<value>.+)$' | Select-Object -First 1
        if ($null -ne $match) {
            $engineRoot = $match.Matches[0].Groups['value'].Value
            break
        }
    }
    if ([string]::IsNullOrWhiteSpace($engineRoot)) {
        return 'unavailable'
    }
    $buildVersion = Join-Path $engineRoot 'Engine\Build\Build.version'
    if (-not (Test-Path -LiteralPath $buildVersion -PathType Leaf)) {
        return 'missing'
    }
    return (Get-FileHash -LiteralPath $buildVersion -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-ProjectWorldGeneratorFingerprint {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$ProducerId
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("project_world_producer_fingerprint_v3`0$ProducerId")
    $lines.Add("engine_build_identity`0$(Get-ProjectWorldEngineBuildIdentity -ProjectRoot $ProjectRoot)")
    foreach ($relative in Get-ProjectWorldProducerSourcePaths -ProducerId $ProducerId) {
        $full = Join-Path $ProjectRoot $relative.Replace('/', [System.IO.Path]::DirectorySeparatorChar)
        $digest = Get-ProjectWorldProducerSourceDigest -Path $full -ProducerId $ProducerId
        $lines.Add("$relative`0$digest")
    }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}
