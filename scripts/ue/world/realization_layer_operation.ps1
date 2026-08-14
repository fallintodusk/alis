# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Assert-ProjectWorldExecutableLayerTuple {
    param([Parameter(Mandatory = $true)][object]$Layer)

    $selectors = @($Layer.canonical_selectors)
    $common = [string]$Layer.layer_kind -ceq 'generated_geography' -and
        [int]$Layer.generator_version -eq 1 -and
        [string]$Layer.dirty_granularity -ceq 'canonical_cell'
    $landscape = $common -and
        [string]$Layer.generator_id -ceq 'project_landscape' -and
        $selectors.Count -eq 1 -and [string]$selectors[0] -ceq 'terrain' -and
        [string]$Layer.spatial_ownership -ceq 'logical_landscape_with_cell_proxies' -and
        [string]$Layer.runtime_mapping -ceq 'world_partition_owner' -and
        [int]$Layer.settings.components_per_proxy -eq 1
    $water = $common -and
        [string]$Layer.generator_id -ceq 'project_water_mesh' -and
        $selectors.Count -eq 1 -and [string]$selectors[0] -ceq 'water' -and
        [string]$Layer.spatial_ownership -ceq 'cell_local' -and
        [string]$Layer.runtime_mapping -ceq 'world_partition_spatial' -and
        [string]$Layer.settings.material_shading_model -ceq 'single_layer_water' -and
        [bool]$Layer.settings.nanite -eq $false -and
        @($Layer.settings.scattering_coefficients).Count -eq 3 -and
        @($Layer.settings.absorption_coefficients).Count -eq 3 -and
        @($Layer.settings.scattering_coefficients | Where-Object { [double]$_ -lt 0 }).Count -eq 0 -and
        @($Layer.settings.absorption_coefficients | Where-Object { [double]$_ -lt 0 }).Count -eq 0
    if (-not $landscape -and -not $water) {
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
            $contract = $ActiveSet.Manifests[$scopeId].layer_contract
            $baseLayers += , ([ordered]@{
                layer_id = [string]$LayerDefinitions[$scopeId].layer_id
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
        'realization_profile_id', 'realization_profile_sha256',
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
