# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function New-ProjectWorldLayerInputIdentity {
    param([Parameter(Mandatory = $true)][System.Collections.IDictionary]$OperationIdentity)

    $identity = [ordered]@{}
    foreach ($entry in $OperationIdentity.GetEnumerator()) {
        $identity[[string]$entry.Key] = $entry.Value
    }
    # Runtime partition policy is serialized only by the map owner. Generated
    # layer producers must never claim it as an input or republish for it.
    $identity.runtime_profile_sha256 = 'none'
    return $identity
}

function Assert-ProjectWorldExecutableLayerTuple {
    param([Parameter(Mandatory = $true)][object]$Layer)

    $selectors = @($Layer.canonical_selectors)
    $common = [string]$Layer.layer_kind -ceq 'generated_geography' -and
        [int]$Layer.generator_version -eq 1 -and
        [string]$Layer.dirty_granularity -ceq 'canonical_cell'
    $meshTerrainVariants = if ($Layer.settings.PSObject.Properties.Name -contains 'render_variants') {
        @($Layer.settings.render_variants)
    } else {
        @()
    }
    $meshTerrain = $common -and
        [string]$Layer.generator_id -ceq 'project_mesh_terrain' -and
        $selectors.Count -eq 1 -and [string]$selectors[0] -ceq 'terrain' -and
        [string]$Layer.spatial_ownership -ceq 'compiled_sections_from_canonical_cells' -and
        [string]$Layer.runtime_mapping -ceq 'world_partition_spatial' -and
        [int]$Layer.dependency_halo_cells -eq 0 -and
        [string]$Layer.settings.shared_definition -ceq '/ProjectWorldMeshTerrain/Terrain/MPD_ProjectTerrain_Shared_v1.MPD_ProjectTerrain_Shared_v1' -and
        [string]$Layer.settings.surface_contract_id -ceq 'terrain_surface_semantics' -and
        [int]$Layer.settings.surface_contract_version -eq 1 -and
        [int]$Layer.settings.section_max_complexity -eq 2048 -and
        [double]$Layer.settings.channel_texel_size_cm -eq 3000.0 -and
        [int]$Layer.settings.channel_texture_max_dimension -eq 4096 -and
        [string]$Layer.settings.collision -ceq 'complex_as_simple' -and
        $meshTerrainVariants.Count -eq 2 -and
        [string]$meshTerrainVariants[0] -ceq 'nanite' -and
        [string]$meshTerrainVariants[1] -ceq 'fallback' -and
        @($Layer.settings.PSObject.Properties).Count -eq 8
    $water = $common -and
        [string]$Layer.generator_id -ceq 'project_water_mesh' -and
        $selectors.Count -eq 1 -and [string]$selectors[0] -ceq 'water' -and
        [string]$Layer.spatial_ownership -ceq 'cell_local' -and
        [string]$Layer.runtime_mapping -ceq 'world_partition_spatial' -and
        [string]$Layer.settings.material_shading_model -ceq 'solid_opaque' -and
        [double]$Layer.settings.surface_offset_m -eq 0.25 -and
        [bool]$Layer.settings.nanite -eq $false -and
        @($Layer.settings.PSObject.Properties).Count -eq 3
    $roadClasses = if ($Layer.settings.PSObject.Properties.Name -contains 'selected_classes') {
        @($Layer.settings.selected_classes)
    } else {
        @()
    }
    $roads = $common -and
        [string]$Layer.generator_id -ceq 'project_road_mesh' -and
        $selectors.Count -eq 1 -and [string]$selectors[0] -ceq 'roads' -and
        [string]$Layer.spatial_ownership -ceq 'cell_local' -and
        [string]$Layer.runtime_mapping -ceq 'world_partition_spatial' -and
        [int]$Layer.dependency_halo_cells -eq 1 -and
        $roadClasses.Count -gt 0 -and
        @($roadClasses | Select-Object -Unique).Count -eq $roadClasses.Count -and
        [double]$Layer.settings.surface_offset_m -gt 0.0 -and
        [double]$Layer.settings.surface_offset_m -le 2.0 -and
        [double]$Layer.settings.maximum_segment_length_m -gt 0.0 -and
        [double]$Layer.settings.maximum_segment_length_m -le 30.0 -and
        [bool]$Layer.settings.nanite -eq $true -and
        [string]$Layer.settings.collision -ceq 'complex_as_simple' -and
        [string]$Layer.settings.structure_fallback -ceq 'terrain_drape' -and
        [string]$Layer.settings.intersection_policy -ceq 'overlap_same_owner'
    [object[]]$vegetationMeshes = @()
    if ($Layer.settings.PSObject.Properties.Name -contains 'mesh_assets') {
        $vegetationMeshes = @($Layer.settings.mesh_assets)
    }
    [object[]]$vegetationDependencies = @()
    if ($Layer.PSObject.Properties.Name -contains 'depends_on') {
        $vegetationDependencies = @($Layer.depends_on)
    }
    $vegetation = $common -and
        [string]$Layer.generator_id -ceq 'project_vegetation_instances' -and
        $selectors.Count -eq 1 -and [string]$selectors[0] -ceq 'vegetation' -and
        $vegetationDependencies.Count -eq 3 -and
        [string]$vegetationDependencies[0] -ceq 'terrain' -and
        [string]$vegetationDependencies[1] -ceq 'water' -and
        [string]$vegetationDependencies[2] -ceq 'roads' -and
        [string]$Layer.spatial_ownership -ceq 'cell_local' -and
        [string]$Layer.runtime_mapping -ceq 'world_partition_spatial' -and
        [int]$Layer.dependency_halo_cells -eq 0 -and
        $vegetationMeshes.Count -gt 0 -and
        @($vegetationMeshes | Select-Object -Unique).Count -eq $vegetationMeshes.Count -and
        @($vegetationMeshes | Where-Object {
            [string]$_ -notmatch '^/Project[A-Za-z0-9]+/[A-Za-z0-9_/]+\.[A-Za-z0-9_]+$'
        }).Count -eq 0 -and
        [double]$Layer.settings.area_spacing_m -ge 5.0 -and
        [double]$Layer.settings.area_spacing_m -le 100.0 -and
        [double]$Layer.settings.area_jitter_fraction -ge 0.0 -and
        [double]$Layer.settings.area_jitter_fraction -lt 0.5 -and
        [double]$Layer.settings.minimum_scale -gt 0.0 -and
        [double]$Layer.settings.minimum_scale -le [double]$Layer.settings.maximum_scale -and
        [double]$Layer.settings.maximum_scale -le 3.0 -and
        [int]$Layer.settings.maximum_instances_per_cell -ge 1 -and
        [int]$Layer.settings.maximum_instances_per_cell -le 8192 -and
        [int64]$Layer.settings.deterministic_seed -ge 0 -and
        [int64]$Layer.settings.deterministic_seed -le 2147483647 -and
        [double]$Layer.settings.surface_offset_m -ge 0.0 -and
        [double]$Layer.settings.surface_offset_m -le 2.0 -and
        [bool]$Layer.settings.nanite -eq $true -and
        [string]$Layer.settings.collision -ceq 'no_collision' -and
        [string]$Layer.settings.placement_policy -ceq 'canonical_points_and_lattice_areas'
    [object[]]$buildingDependencies = @()
    if ($Layer.PSObject.Properties.Name -contains 'depends_on') {
        $buildingDependencies = @($Layer.depends_on)
    }
    $buildingVersion = [int]$Layer.generator_version
    $buildingTopology = if ($buildingVersion -eq 2) {
        'logical_building_classify_v2'
    } else {
        'cell_local_classify_v1'
    }
    $buildings = [string]$Layer.layer_kind -ceq 'generated_geography' -and
        $buildingVersion -in @(1, 2) -and
        [string]$Layer.dirty_granularity -ceq 'canonical_cell' -and
        [string]$Layer.generator_id -ceq 'project_building_massing' -and
        $selectors.Count -eq 1 -and [string]$selectors[0] -ceq 'buildings' -and
        $buildingDependencies.Count -eq 1 -and
        [string]$buildingDependencies[0] -ceq 'terrain' -and
        [string]$Layer.spatial_ownership -ceq 'cell_local' -and
        [string]$Layer.runtime_mapping -ceq 'world_partition_spatial' -and
        [int]$Layer.dependency_halo_cells -eq 0 -and
        [double]$Layer.settings.maximum_height_m -ge 50.0 -and
        [double]$Layer.settings.maximum_height_m -le 1000.0 -and
        [string]$Layer.settings.terrain_anchor_policy -ceq 'owner_cell_clamped_bounds_center' -and
        [string]$Layer.settings.topology_policy -ceq $buildingTopology -and
        [string]$Layer.settings.duplicate_policy -ceq 'stable_feature_id' -and
        [string]$Layer.settings.contained_policy -ceq 'associate_with_container' -and
        [string]$Layer.settings.conflict_policy -ceq 'reject_affected_fragments' -and
        [bool]$Layer.settings.nanite -eq $true -and
        [string]$Layer.settings.collision -ceq 'complex_as_simple' -and
        [string]$Layer.settings.navigation -ceq 'no_navigation'
    [object[]]$gameplayDependencies = @()
    if ($Layer.PSObject.Properties.Name -contains 'depends_on') {
        $gameplayDependencies = @($Layer.depends_on)
    }
    $gameplay = [string]$Layer.layer_kind -ceq 'generated_gameplay_placement' -and
        [int]$Layer.generator_version -eq 1 -and
        [string]$Layer.generator_id -ceq 'project_gameplay_placement' -and
        $selectors.Count -eq 1 -and [string]$selectors[0] -ceq 'gameplay_placements' -and
        $gameplayDependencies.Count -eq 1 -and
        [string]$gameplayDependencies[0] -ceq 'terrain' -and
        [string]$Layer.spatial_ownership -ceq 'object_local' -and
        [string]$Layer.dirty_granularity -ceq 'object_id' -and
        [int]$Layer.dependency_halo_cells -eq 0 -and
        [string]$Layer.runtime_mapping -ceq 'world_partition_spatial' -and
        [string]$Layer.settings.placement_source -match '^GameplayPlacement/[a-z0-9_]+\.json$' -and
        [string]$Layer.settings.surface_policy -ceq 'canonical_terrain_snap' -and
        [string]$Layer.settings.runtime_state_policy -ceq 'external_to_generation'
    if (-not $meshTerrain -and -not $water -and -not $roads -and -not $vegetation -and
        -not $buildings -and -not $gameplay) {
        throw "Realization layer does not match a registered executable tuple: $($Layer.layer_id)"
    }
}

function Resolve-ProjectWorldRealizationLayers {
    param(
        [object]$RealizationDocument,
        [Parameter(Mandatory = $true)][object]$WorldDataRoots
    )

    $definitions = [ordered]@{}
    $scopePaths = @{}
    if ($null -eq $RealizationDocument) {
        return [pscustomobject]@{ Definitions = $definitions; ScopePaths = $scopePaths }
    }
    foreach ($layer in @($RealizationDocument.layers)) {
        if ([string]$layer.layer_id -notmatch '^[a-z0-9_]+$') {
            throw "Realization layer has an invalid ID: $($layer.layer_id)"
        }
        Assert-ProjectWorldExecutableLayerTuple -Layer $layer
        $scopeId = "layer_$($RealizationDocument.profile_id)_$($layer.layer_id)"
        $artifactRoot = [string]$layer.artifact_root
        if ($definitions.Contains($scopeId) -or
            $artifactRoot.Length -le $WorldDataRoots.GeneratedPackageRoot.Length -or
            -not $artifactRoot.StartsWith(
                $WorldDataRoots.GeneratedPackageRoot,
            [System.StringComparison]::Ordinal) -or
            -not $artifactRoot.EndsWith('/')) {
            throw "Generated realization layer has an invalid or duplicate owned root: $scopeId"
        }
        $relativeRoot = $artifactRoot.Substring($WorldDataRoots.MountRoot.Length).Replace(
            '/',
            [System.IO.Path]::DirectorySeparatorChar).TrimEnd('\', '/')
        $diskRoot = Assert-ProjectWorldOwnedPath `
            -ContentRoot $WorldDataRoots.ContentRoot `
            -Path (Join-Path $WorldDataRoots.ContentRoot $relativeRoot)
        $definitions[$scopeId] = $layer
        $scopePaths[$scopeId] = @($diskRoot)
    }
    return [pscustomobject]@{ Definitions = $definitions; ScopePaths = $scopePaths }
}

function Get-ProjectWorldLayerScopePath {
    param(
        [Parameter(Mandatory = $true)][object]$WorldDataRoots,
        [Parameter(Mandatory = $true)][object]$LayerContract
    )

    $packageRoot = [string]$LayerContract.artifact_root
    if ($packageRoot.Length -le $WorldDataRoots.GeneratedPackageRoot.Length -or
        -not $packageRoot.StartsWith($WorldDataRoots.GeneratedPackageRoot, [System.StringComparison]::Ordinal)) {
        throw "Accepted layer root escapes the generated owner boundary: $packageRoot"
    }
    $relativeRoot = $packageRoot.Substring($WorldDataRoots.MountRoot.Length).Replace(
        '/',
        [System.IO.Path]::DirectorySeparatorChar).TrimEnd('\', '/')
    return Assert-ProjectWorldOwnedPath `
        -ContentRoot $WorldDataRoots.ContentRoot `
        -Path (Join-Path $WorldDataRoots.ContentRoot $relativeRoot)
}

function New-ProjectWorldLayerDirtyInput {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$OutputDirectory,
        [Parameter(Mandatory = $true)][object]$RealizationDocument,
        [Parameter(Mandatory = $true)][object]$LayerDefinitions,
        [object]$ActiveSet = $null,
        [AllowEmptyCollection()][string[]]$OperatorDirtyUnits = @()
    )

    $operatorUnits = [ordered]@{}
    foreach ($entry in $OperatorDirtyUnits) {
        $separator = $entry.IndexOf('=')
        if ($separator -lt 1 -or $separator -eq $entry.Length - 1) {
            throw "DirtyUnit must use layer_id=unit_id: $entry"
        }
        $layerId = $entry.Substring(0, $separator)
        $matchingScopes = @($LayerDefinitions.Keys | Where-Object {
            [string]$LayerDefinitions[$_].layer_id -ceq $layerId
        })
        if ($matchingScopes.Count -ne 1) {
            throw "DirtyUnit targets an unknown generated layer: $entry"
        }
        if (-not $operatorUnits.Contains($layerId)) {
            $operatorUnits[$layerId] = [System.Collections.Generic.List[string]]::new()
        }
        $operatorUnits[$layerId].Add($entry.Substring($separator + 1))
    }

    $baseLayers = @()
    if ($null -ne $ActiveSet) {
        foreach ($scopeId in $LayerDefinitions.Keys) {
            if (-not $ActiveSet.Manifests.Contains($scopeId)) { continue }
            $priorManifest = $ActiveSet.Manifests[$scopeId]
            $definition = $LayerDefinitions[$scopeId]
            $layerId = [string]$definition.layer_id
            $producerId = "$([string]$definition.generator_id):v$([int]$definition.generator_version)"
            $currentFingerprint = Get-ProjectWorldGeneratorFingerprint `
                -ProjectRoot $ProjectRoot -ProducerId $producerId
            if ([string]$priorManifest.generator_fingerprint -cne $currentFingerprint) {
                if (-not $operatorUnits.Contains($layerId)) {
                    $operatorUnits[$layerId] = [System.Collections.Generic.List[string]]::new()
                }
                $operatorUnits[$layerId].Add('*')
            }
            $contract = $priorManifest.layer_contract
            $baseLayers += , ([ordered]@{
                layer_id = $layerId
                normalized_layer_contract_sha256 = [string]$contract.normalized_layer_contract_sha256
                canonical_inputs = @($contract.canonical_inputs)
            })
        }
    }

    $schemaPath = Join-Path $ProjectRoot 'Plugins\World\ProjectWorld\Data\Schemas\project_world_layer_dirty_input.schema.json'
    $fromUri = [System.Uri]::new([System.IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar)
    $toUri = [System.Uri]::new([System.IO.Path]::GetFullPath($schemaPath))
    $schemaReference = [System.Uri]::UnescapeDataString($fromUri.MakeRelativeUri($toUri).ToString()).Replace('\', '/')
    $path = Join-Path $OutputDirectory 'layer_dirty_input.json'
    Write-ProjectWorldJson -Path $path -Document ([ordered]@{
        '$schema' = $schemaReference
        schema_version = 1
        realization_profile_id = [string]$RealizationDocument.profile_id
        base_layers = $baseLayers
        operator_additions = @($operatorUnits.Keys | Sort-Object | ForEach-Object {
            [ordered]@{ layer_id = $_; units = @($operatorUnits[$_] | Sort-Object -Unique) }
        })
    })
    return [pscustomobject]@{
        Path = $path
        Sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Test-ProjectWorldManifestSemanticallyUnchanged {
    param(
        [object]$PriorManifest,
        [Parameter(Mandatory = $true)][object]$CandidateManifest,
        [Parameter(Mandatory = $true)][string]$GeneratorFingerprint,
        [switch]$CompareLayerContract
    )

    if ($null -eq $PriorManifest -or
        [string]$PriorManifest.generator_fingerprint -cne $GeneratorFingerprint -or
        (($PriorManifest.artifacts | ConvertTo-Json -Depth 6 -Compress) -cne
         ($CandidateManifest.artifacts | ConvertTo-Json -Depth 6 -Compress))) {
        return $false
    }
    if (-not $CompareLayerContract) {
        return $true
    }
    $stableFields = @(
        'realization_profile_id',
        'normalized_layer_contract_sha256', 'generator_id', 'generator_version',
        'artifact_root', 'canonical_inputs', 'dependency_inputs', 'semantic_outputs')
    $priorStable = [ordered]@{}
    $candidateStable = [ordered]@{}
    foreach ($field in $stableFields) {
        $priorStable[$field] = $PriorManifest.layer_contract.$field
        $candidateStable[$field] = $CandidateManifest.layer_contract.$field
    }
    return ($priorStable | ConvertTo-Json -Depth 8 -Compress) -ceq
        ($candidateStable | ConvertTo-Json -Depth 8 -Compress)
}

function Test-ProjectWorldReconstructionScopeState {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][object]$ActiveSet,
        [Parameter(Mandatory = $true)][hashtable]$ScopePathsById
    )

    foreach ($scopeId in $ScopePathsById.Keys) {
        if (-not $ActiveSet.Manifests.Contains($scopeId)) { continue }
        $current = @(Get-ProjectWorldScopeArtifactRecords `
            -ProjectRoot $ProjectRoot -ScopePaths $ScopePathsById[$scopeId])
        if ($current.Count -eq 0) { continue }
        Test-ProjectWorldScopeDrift `
            -ProjectRoot $ProjectRoot -ActiveSet $ActiveSet `
            -ScopePathsById @{ $scopeId = $ScopePathsById[$scopeId] }
    }
}

function Remove-ProjectWorldSupersededLayerArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [object]$PriorManifest,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$CandidateRecords
    )

    if ($null -eq $PriorManifest) { return @() }

    $projectPrefix = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/') +
        [System.IO.Path]::DirectorySeparatorChar
    $candidatePaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($record in $CandidateRecords) {
        [void]$candidatePaths.Add([string]$record.path)
    }

    $removed = [System.Collections.Generic.List[string]]::new()
    foreach ($artifact in @($PriorManifest.artifacts)) {
        $relative = [string]$artifact.path
        if ($candidatePaths.Contains($relative)) { continue }
        if ($relative -match '\.\.' -or $relative -match '^[/\\]' -or
            $relative -match ':' -or $relative -notmatch '^[A-Za-z0-9_./ -]+$') {
            throw "Superseded layer artifact is not a confined repository-relative path: $relative"
        }
        $fullPath = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $relative.Replace('/', '\')))
        if (-not $fullPath.StartsWith($projectPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Superseded layer artifact escapes the repository: $relative"
        }
        if (Test-Path -LiteralPath $fullPath -PathType Container) {
            throw "Superseded layer artifact unexpectedly resolves to a directory: $relative"
        }
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            Remove-Item -LiteralPath $fullPath -Force
            $removed.Add($relative)
        }
    }
    return @($removed | Sort-Object)
}

function Merge-ProjectWorldMeshTerrainAuditArtifacts {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$TerrainArtifacts,
        [AllowEmptyCollection()][object[]]$PriorSections = @(),
        [Parameter(Mandatory = $true)][object[]]$Sections,
        [Parameter(Mandatory = $true)][string]$ProjectRoot
    )

    $records = [ordered]@{}
    $priorSectionPaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($section in $PriorSections) {
        [void]$priorSectionPaths.Add([string]$section.artifact_path)
    }
    foreach ($artifact in $TerrainArtifacts) {
        $path = [string]$artifact.path
        if ($priorSectionPaths.Contains($path)) { continue }
        $fullPath = Join-Path $ProjectRoot $path.Replace('/', '\')
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            throw "Mesh Terrain builder removed a declared terrain artifact: $path"
        }
        $artifact.digest = Get-ProjectWorldFileSha256 -Path $fullPath
        $records[$path] = $artifact
    }

    $sectionPaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($section in $Sections) {
        $path = [string]$section.artifact_path
        if ($path -notmatch '^[A-Za-z0-9_./ -]+$' -or $path -match '\.\.' -or
            -not $sectionPaths.Add($path)) {
            throw "Mesh Terrain audit emitted an invalid or duplicate artifact path: $path"
        }
        $fullPath = Join-Path $ProjectRoot $path.Replace('/', '\')
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            throw "Mesh Terrain compiled-section artifact is missing: $path"
        }
        $digest = Get-ProjectWorldFileSha256 -Path $fullPath
        $records[$path] = [pscustomobject]@{
            path = $path
            kind = 'external_actor'
            digest_kind = 'sha256'
            digest = $digest
            semantic_sha256 = $digest
        }
    }
    return @($records.Values | Sort-Object -Property path)
}

function Test-ProjectWorldMeshTerrainBuildRequired {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet('validate', 'apply', 'delete')]
        [string]$Mode,
        [Parameter(Mandatory = $true)][object]$Result
    )

    if ($Mode -cne 'apply') {
        return $false
    }
    return @($Result.layer_inventories | Where-Object {
        [string]$_.layer_id -ceq 'terrain' -and
        [string]$_.generator_id -ceq 'project_mesh_terrain'
    }).Count -gt 0
}

function Invoke-ProjectWorldMeshTerrainBuildIfRequired {
    param(
        [Parameter(Mandatory = $true)][string]$EditorCommand,
        [Parameter(Mandatory = $true)][string]$ProjectFile,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$MapPackage,
        [Parameter(Mandatory = $true)][string]$CompileResult,
        [Parameter(Mandatory = $true)][string]$EvidenceDirectory,
        [Parameter(Mandatory = $true)][object]$Result
    )

    $terrain = @($Result.layer_inventories | Where-Object {
        [string]$_.layer_id -ceq 'terrain' -and
        [string]$_.generator_id -ceq 'project_mesh_terrain'
    })
    if ($terrain.Count -eq 0) { return }
    if ($terrain.Count -ne 1) { throw 'Mesh Terrain build requires exactly one terrain inventory.' }

    $prebuildAuditPath = Join-Path $EvidenceDirectory 'mesh-partition-prebuild-inventory.json'
    $prebuildAuditLog = Join-Path $EvidenceDirectory 'mesh-partition-prebuild-inventory.log'
    $prebuildAuditArguments = @(
        $ProjectFile, '-run=ProjectWorldMeshTerrainAudit',
        "-Map=$MapPackage", "-CompileResult=$CompileResult",
        '-ExpectedSectionsPerVariant=0', "-Result=$prebuildAuditPath",
        '-AllowCommandletRendering', '-unattended', '-nop4', '-nosplash',
        '-stdout', '-FullStdOutLogOutput', "-abslog=$prebuildAuditLog"
    )
    & $EditorCommand @prebuildAuditArguments | Out-Null
    if ($LASTEXITCODE -notin @(0, 1) -or
        -not (Test-Path -LiteralPath $prebuildAuditPath -PathType Leaf)) {
        throw "Mesh Terrain pre-build inventory failed with exit code $LASTEXITCODE. See $prebuildAuditLog"
    }
    $prebuildAudit = Get-Content -LiteralPath $prebuildAuditPath -Raw | ConvertFrom-Json
    if ([string]$prebuildAudit.schema -cne 'project-world-mesh-terrain-audit:v1' -or
        [string]$prebuildAudit.map -cne $MapPackage -or $null -eq $prebuildAudit.sections) {
        throw "Mesh Terrain pre-build inventory is invalid. See $prebuildAuditPath"
    }

    $builderLog = Join-Path $EvidenceDirectory 'mesh-partition-builder-transaction.log'
    $builderArguments = @(
        $ProjectFile, $MapPackage,
        '-run=WorldPartitionBuilderCommandlet',
        '-Builder=WorldPartitionMeshPartitionBuilder',
        '-AllowCommandletRendering',
        '-ini:Engine:[SystemSettings]:r.VolumetricFog=0',
        '-SCCProvider=None', '-unattended', '-nop4', '-nosplash',
        '-stdout', '-FullStdOutLogOutput', "-abslog=$builderLog"
    )
    & $EditorCommand @builderArguments | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Mesh Terrain builder failed with exit code $LASTEXITCODE. See $builderLog"
    }

    $auditPath = Join-Path $EvidenceDirectory 'mesh-partition-transaction-audit.json'
    $auditLog = Join-Path $EvidenceDirectory 'mesh-partition-transaction-audit.log'
    $auditArguments = @(
        $ProjectFile, '-run=ProjectWorldMeshTerrainAudit',
        "-Map=$MapPackage", "-CompileResult=$CompileResult",
        '-ExpectedSectionsPerVariant=0', "-Result=$auditPath",
        '-AllowCommandletRendering', '-unattended', '-nop4', '-nosplash',
        '-stdout', '-FullStdOutLogOutput', "-abslog=$auditLog"
    )
    & $EditorCommand @auditArguments | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $auditPath -PathType Leaf)) {
        throw "Mesh Terrain transaction audit failed with exit code $LASTEXITCODE. See $auditLog"
    }
    $audit = Get-Content -LiteralPath $auditPath -Raw | ConvertFrom-Json
    if ([string]$audit.status -cne 'accepted') {
        throw "Mesh Terrain transaction audit rejected the compiled sections. See $auditPath"
    }

    $terrain[0].artifacts = @(Merge-ProjectWorldMeshTerrainAuditArtifacts `
        -TerrainArtifacts @($terrain[0].artifacts) `
        -PriorSections @($prebuildAudit.sections) -Sections @($audit.sections) `
        -ProjectRoot $ProjectRoot)
}
