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
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldSemanticEvidence.cpp',
        'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldSemanticEvidence.h',
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
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialRealization.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.h',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_presentation_profile.schema.json'
        ) }
        'project_landscape:v1' { @(
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldTerrainVerification.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldLandscapeRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldLandscapeRealization.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialRealization.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.h',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldTerrainVerification.cpp',
            'Plugins/World/ProjectWorld/Data/Schemas/project_world_presentation_profile.schema.json'
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
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialRealization.h',
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
        'project_building_massing:v1' { @(
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
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialRealization.cpp',
            'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialRealization.h',
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

function Get-ProjectWorldGeneratorFingerprint {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$ProducerId
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("project_world_producer_fingerprint_v2`0$ProducerId")
    foreach ($relative in Get-ProjectWorldProducerSourcePaths -ProducerId $ProducerId) {
        $full = Join-Path $ProjectRoot $relative.Replace('/', [System.IO.Path]::DirectorySeparatorChar)
        $digest = if (Test-Path -LiteralPath $full -PathType Leaf) {
            (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        else { 'missing' }
        $lines.Add("$relative`0$digest")
    }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}
