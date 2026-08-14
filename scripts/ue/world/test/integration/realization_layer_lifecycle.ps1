# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CompileResult,

    [string]$PresentationProfile =
        'Plugins/World/ProjectWorldTestData/Data/Presentation/synthetic_representative_v1.json',

    [string]$AuthoredOverlayProfile =
        'Plugins/World/ProjectWorldTestData/Data/Authored/synthetic_landscape_water_twin_v1.json',

    [string]$RealizationProfile =
        'Plugins/World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_landscape_water_twin.realization.json',

    [int]$ExpectedCellCount = 0,

    [double]$ExpectedSampleSpacingMeters = 0.0,

    [double]$MaximumGeoReferenceErrorMeters = 0.01,

    [switch]$RequireWater,

    [switch]$AllowProductionIsolation,

    [switch]$ProveReconstruction
)

$ErrorActionPreference = 'Stop'
$worldRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $worldRoot))
$wrapper = Join-Path $worldRoot 'realize_canonical_world.ps1'
. (Join-Path $worldRoot 'generated_content_transaction.ps1')
. (Join-Path $worldRoot 'generated_manifest.ps1')

function Resolve-ProjectPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Assert-ProjectWorldIntegration {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Get-ProjectWorldIntegrationDigest {
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    $records = [System.Collections.Generic.List[string]]::new()
    foreach ($path in @($Paths | Sort-Object -Unique)) {
        $fullPath = [System.IO.Path]::GetFullPath($path)
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            $records.Add("$fullPath|$((Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash.ToLowerInvariant())")
            continue
        }
        if (-not (Test-Path -LiteralPath $fullPath -PathType Container)) {
            $records.Add("$fullPath|absent")
            continue
        }
        foreach ($file in Get-ChildItem -LiteralPath $fullPath -Recurse -File | Sort-Object FullName) {
            $records.Add("$($file.FullName)|$((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())")
        }
    }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($records -join "`n"))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Get-ProjectWorldLayerSemanticDigest {
    param([Parameter(Mandatory = $true)][object]$Result)

    $records = [System.Collections.Generic.List[string]]::new()
    foreach ($inventory in @($Result.layer_inventories | Sort-Object layer_id)) {
        foreach ($artifact in @($inventory.artifacts | Sort-Object path)) {
            $records.Add("$($inventory.layer_id)|$($artifact.path)|$($artifact.semantic_sha256)")
        }
    }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($records -join "`n"))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

$compileResultPath = Resolve-ProjectPath -Path $CompileResult
$presentationPath = Resolve-ProjectPath -Path $PresentationProfile
$authoredPath = Resolve-ProjectPath -Path $AuthoredOverlayProfile
$realizationPath = Resolve-ProjectPath -Path $RealizationProfile
$realization = Get-Content -LiteralPath $realizationPath -Raw | ConvertFrom-Json
$worldDataPlugin = [string]$realization.world_data_plugin
$isProductionIsolation = $worldDataPlugin -ceq 'ProjectWorldData'
Assert-ProjectWorldIntegration -Condition (
    $worldDataPlugin -ceq 'ProjectWorldTestData' -or
    ($isProductionIsolation -and $AllowProductionIsolation)) `
    -Message 'Production isolation requires ProjectWorldData plus -AllowProductionIsolation.'
$mapPackage = [string]$realization.map_package
$roots = Resolve-ProjectWorldDataRoots -ProjectRoot $projectRoot -PluginName $worldDataPlugin
$runId = [System.Guid]::NewGuid().ToString('N')
$evidenceBucket = if ($isProductionIsolation) { '3a-territory-isolation' } else { '3-core-lifecycle' }
$evidenceRoot = Join-Path $projectRoot "Saved\Validation\WorldRealization\$evidenceBucket\$runId"
$manifestRoot = Join-Path $evidenceRoot 'manifests'
$workParent = Join-Path $projectRoot 'tmp\world\3-core-lifecycle'
$workRoot = Join-Path $workParent $runId
$snapshotRoot = Join-Path $workRoot 'outer-snapshot'
$layerPaths = @($realization.layers | ForEach-Object {
    $relative = ([string]$_.artifact_root).Substring($roots.MountRoot.Length).Replace(
        '/',
        [System.IO.Path]::DirectorySeparatorChar).TrimEnd('\', '/')
    Join-Path $roots.ContentRoot $relative
})
$statePaths = @(
    $layerPaths
    (Join-Path $roots.ContentRoot 'Generated\Presentation')
    (Join-Path $roots.ContentRoot 'Authored')
    (Join-Path $manifestRoot 'active_set.json')
)
$powerShellExe = (Get-Process -Id $PID).Path
$priorDelegatedToken = $env:ALIS_WORLD_CONTENT_LOCK_TOKEN
$contentLock = $null
$snapshotRecords = @()

function Invoke-ProjectWorldIntegrationRun {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][ValidateSet('Apply', 'Delete')][string]$Mode,
        [string[]]$ExtraArguments = @()
    )

    $evidencePath = Join-Path $evidenceRoot "$Name.json"
    $arguments = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $wrapper,
        '-CompileResult', $compileResultPath,
        '-Mode', $Mode,
        '-Map', $mapPackage,
        '-RealizationProfile', $realizationPath,
        '-ManifestRoot', $manifestRoot,
        '-EvidencePath', $evidencePath,
        '-MaxRoads', '0',
        '-MaxBuildings', '0',
        '-NonInteractive'
    )
    if ($Mode -eq 'Apply') {
        $arguments += @(
            '-PresentationProfile', $presentationPath,
            '-AuthoredOverlayProfile', $authoredPath,
            '-RequireLandscape'
        )
    }
    $arguments += $ExtraArguments
    & $powerShellExe @arguments | Out-Host
    $exitCode = $LASTEXITCODE
    $result = if (Test-Path -LiteralPath $evidencePath -PathType Leaf) {
        Get-Content -LiteralPath $evidencePath -Raw | ConvertFrom-Json
    }
    else {
        $null
    }
    return [pscustomobject]@{ ExitCode = $exitCode; Path = $evidencePath; Result = $result }
}

try {
    $contentLock = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
    if ([string]::IsNullOrWhiteSpace($priorDelegatedToken)) {
        $lockPath = Join-Path $projectRoot 'tmp\world\world_realization\content_mutation.lock'
        $env:ALIS_WORLD_CONTENT_LOCK_TOKEN = (Get-Content -LiteralPath $lockPath -Raw).Trim()
    }
    $snapshotRecords = @(New-ProjectWorldGeneratedSnapshot `
        -ContentRoot $roots.ContentRoot `
        -MapPackage $mapPackage `
        -GeneratedPackageRoot $roots.GeneratedPackageRoot `
        -SnapshotRoot $snapshotRoot `
        -AdditionalPaths $layerPaths)
    $protectedAuthoredSha256 = Get-ProjectWorldIntegrationDigest `
        -Paths @((Join-Path $roots.ContentRoot 'Authored'))

    $first = Invoke-ProjectWorldIntegrationRun `
        -Name '01-first-apply' -Mode Apply -ExtraArguments @('-EnrollManifests')
    Assert-ProjectWorldIntegration -Condition ($first.ExitCode -eq 0 -and $first.Result.status -ceq 'accepted') `
        -Message 'First layer enrollment did not commit.'
    $firstTerrain = @($first.Result.layer_inventories | Where-Object { $_.layer_id -ceq 'terrain' })
    Assert-ProjectWorldIntegration -Condition ($firstTerrain.Count -eq 1) `
        -Message 'First Apply did not emit one terrain inventory.'
    if ($ExpectedCellCount -gt 0) {
        Assert-ProjectWorldIntegration -Condition (
            [int]$first.Result.canonical_cell_count -eq $ExpectedCellCount -and
            [int]$first.Result.changes.landscape_components -eq $ExpectedCellCount -and
            [int]$first.Result.changes.landscape_proxies -eq $ExpectedCellCount -and
            @($firstTerrain[0].canonical_inputs).Count -eq $ExpectedCellCount) `
            -Message "Landscape topology is not $ExpectedCellCount/$ExpectedCellCount."
    }
    if ($ExpectedSampleSpacingMeters -gt 0.0) {
        Assert-ProjectWorldIntegration -Condition (
            [double]$first.Result.sample_spacing_m[0] -eq $ExpectedSampleSpacingMeters -and
            [double]$first.Result.sample_spacing_m[1] -eq $ExpectedSampleSpacingMeters) `
            -Message 'Canonical sample spacing does not match the production contract.'
    }
    Assert-ProjectWorldIntegration -Condition (
        [double]$first.Result.georeferencing_placement_error_m -le $MaximumGeoReferenceErrorMeters -and
        [int]$first.Result.hlod_proxy_actor_count -eq 0 -and
        [int]$first.Result.hlod_layer_reference_count -eq 0 -and
        [int]$first.Result.hlod_eligible_generated_actor_count -eq 0) `
        -Message 'Georeferencing tolerance or the zero-HLOD contract failed.'
    if ($RequireWater) {
        Assert-ProjectWorldIntegration -Condition (
            [int]$first.Result.changes.water_cell_actors -gt 0 -and
            [int]$first.Result.changes.water_cell_actors -eq [int]$first.Result.changes.water_mesh_assets) `
            -Message 'Persistent cell-local water inventory is incomplete.'
    }
    Assert-ProjectWorldIntegration -Condition (
        (Get-ProjectWorldIntegrationDigest -Paths @((Join-Path $roots.ContentRoot 'Authored'))) -ceq
        $protectedAuthoredSha256) -Message 'First Apply changed protected authored bytes.'
    $firstLayerSemanticSha256 = Get-ProjectWorldLayerSemanticDigest -Result $first.Result
    $firstActive = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
    $firstScopeHashes = @{}
    foreach ($entry in @($firstActive.Record.scopes)) {
        $firstScopeHashes[[string]$entry.scope_id] = [string]$entry.manifest_sha256
    }
    $firstLayerHashes = @{}
    foreach ($scopeId in @($firstActive.Manifests.Keys | Where-Object { $_ -like 'layer_*' })) {
        $entry = @($firstActive.Record.scopes | Where-Object { $_.scope_id -ceq $scopeId })
        Assert-ProjectWorldIntegration -Condition ($entry.Count -eq 1) `
            -Message "Active-set entry is missing for layer scope: $scopeId"
        $firstLayerHashes[$scopeId] = [string]$entry[0].manifest_sha256
    }

    $unchanged = Invoke-ProjectWorldIntegrationRun -Name '02-unchanged-apply' -Mode Apply
    Assert-ProjectWorldIntegration -Condition ($unchanged.ExitCode -eq 0 -and $unchanged.Result.status -ceq 'accepted') `
        -Message 'Unchanged Apply did not commit.'
    foreach ($inventory in @($unchanged.Result.layer_inventories)) {
        Assert-ProjectWorldIntegration -Condition (@($inventory.final_dirty_units).Count -eq 0) `
            -Message "Unchanged layer was not a semantic no-op: $($inventory.layer_id)"
    }
    Assert-ProjectWorldIntegration -Condition (
        [int]$unchanged.Result.changes.updated_actors -eq 0 -and
        [int]$unchanged.Result.changes.updated_landscape_components -eq 0 -and
        [int]$unchanged.Result.changes.water_triangles -eq 0) `
        -Message 'Unchanged Apply rewrote generated map or layer state.'
    Assert-ProjectWorldIntegration -Condition (
        (Get-ProjectWorldIntegrationDigest -Paths @((Join-Path $roots.ContentRoot 'Authored'))) -ceq
        $protectedAuthoredSha256) -Message 'Unchanged Apply changed protected authored bytes.'
    $unchangedActive = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
    foreach ($scopeId in $firstScopeHashes.Keys) {
        $entry = @($unchangedActive.Record.scopes | Where-Object { $_.scope_id -ceq $scopeId })
        Assert-ProjectWorldIntegration -Condition (
            $entry.Count -eq 1 -and [string]$entry[0].manifest_sha256 -ceq $firstScopeHashes[$scopeId]) `
            -Message "Unchanged Apply advanced generated authority: $scopeId"
    }

    $cellId = [string]$firstTerrain[0].canonical_inputs[0].unit_id
    $incremental = Invoke-ProjectWorldIntegrationRun `
        -Name '03-one-cell-apply' -Mode Apply `
        -ExtraArguments @('-DirtyUnit', "terrain=$cellId")
    Assert-ProjectWorldIntegration -Condition ($incremental.ExitCode -eq 0 -and $incremental.Result.status -ceq 'accepted') `
        -Message 'One-cell Apply did not commit.'
    foreach ($layerId in @('terrain', 'water')) {
        $inventory = @($incremental.Result.layer_inventories | Where-Object { $_.layer_id -ceq $layerId })
        Assert-ProjectWorldIntegration `
            -Condition ($inventory.Count -eq 1 -and @($inventory[0].final_dirty_units).Count -eq 1 -and
                [string]$inventory[0].final_dirty_units[0] -ceq $cellId) `
            -Message "One-cell dependency closure is wrong for layer: $layerId"
    }
    $incrementalActive = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
    foreach ($scopeId in $firstLayerHashes.Keys) {
        $entry = @($incrementalActive.Record.scopes | Where-Object { $_.scope_id -ceq $scopeId })
        Assert-ProjectWorldIntegration `
            -Condition ($entry.Count -eq 1 -and
                [string]$entry[0].manifest_sha256 -ceq $firstLayerHashes[$scopeId]) `
            -Message "Semantically unchanged layer authority advanced: $scopeId"
    }
    Assert-ProjectWorldIntegration -Condition (
        (Get-ProjectWorldIntegrationDigest -Paths @((Join-Path $roots.ContentRoot 'Authored'))) -ceq
        $protectedAuthoredSha256) -Message 'One-cell Apply changed protected authored bytes.'

    $currentMapPaths = @(Get-ProjectWorldGeneratedPaths `
        -ContentRoot $roots.ContentRoot -MapPackage $mapPackage `
        -GeneratedPackageRoot $roots.GeneratedPackageRoot -IncludePresentation $false)
    $beforeRejected = Get-ProjectWorldIntegrationDigest -Paths @($statePaths + $currentMapPaths)
    $rejected = Invoke-ProjectWorldIntegrationRun `
        -Name '04-rejected-out-of-domain' -Mode Apply `
        -ExtraArguments @('-DirtyUnit', "water=$(([string]$first.Result.grid_id)):x2147483647:y2147483647")
    Assert-ProjectWorldIntegration `
        -Condition ($rejected.ExitCode -ne 0 -and $rejected.Result.status -ceq 'rejected') `
        -Message 'The out-of-domain dirty operation was not rejected.'
    $afterRejected = Get-ProjectWorldIntegrationDigest -Paths @($statePaths + $currentMapPaths)
    Assert-ProjectWorldIntegration -Condition ($afterRejected -ceq $beforeRejected) `
        -Message 'Rejected mutation did not restore exact content and authority state.'

    $lifecycle = [System.Collections.Generic.List[string]]::new()
    @('first_apply', 'unchanged_apply', 'one_cell_apply', 'rejected_rollback') |
        ForEach-Object { $lifecycle.Add($_) }
    if ($ProveReconstruction) {
        Remove-ProjectWorldGeneratedPaths -ContentRoot $roots.ContentRoot `
            -MapPackage $mapPackage -GeneratedPackageRoot $roots.GeneratedPackageRoot
        foreach ($path in $layerPaths) {
            $ownedPath = Assert-ProjectWorldOwnedPath -ContentRoot $roots.ContentRoot -Path $path
            if (Test-Path -LiteralPath $ownedPath) {
                Remove-Item -LiteralPath $ownedPath -Recurse -Force
            }
        }
        Assert-ProjectWorldIntegration -Condition (
            @(Get-ProjectWorldGeneratedPaths -ContentRoot $roots.ContentRoot `
                -MapPackage $mapPackage -GeneratedPackageRoot $roots.GeneratedPackageRoot).Count -eq 0 -and
            @($layerPaths | Where-Object { Test-Path -LiteralPath $_ }).Count -eq 0) `
            -Message 'Clean reconstruction precondition did not remove every generated scope.'
        $reconstructed = Invoke-ProjectWorldIntegrationRun `
            -Name '05-clean-reconstruction' -Mode Apply -ExtraArguments @('-Reconstruct')
        Assert-ProjectWorldIntegration -Condition (
            $reconstructed.ExitCode -eq 0 -and $reconstructed.Result.status -ceq 'accepted') `
            -Message 'Clean reconstruction did not commit.'
        Assert-ProjectWorldIntegration -Condition (
            (Get-ProjectWorldLayerSemanticDigest -Result $reconstructed.Result) -ceq $firstLayerSemanticSha256) `
            -Message 'Clean reconstruction changed generated-layer semantics.'
        Assert-ProjectWorldIntegration -Condition (
            (Get-ProjectWorldIntegrationDigest -Paths @((Join-Path $roots.ContentRoot 'Authored'))) -ceq
            $protectedAuthoredSha256) -Message 'Clean reconstruction changed protected authored bytes.'
        $lifecycle.Add('clean_reconstruction')
    }

    $deleteName = if ($ProveReconstruction) { '06-delete' } else { '05-delete' }
    $delete = Invoke-ProjectWorldIntegrationRun -Name $deleteName -Mode Delete
    Assert-ProjectWorldIntegration -Condition ($delete.ExitCode -eq 0 -and $delete.Result.status -ceq 'accepted') `
        -Message 'Delete did not commit.'
    $deletedActive = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
    Assert-ProjectWorldIntegration -Condition ($deletedActive.Record.scopes.Count -eq 0) `
        -Message 'Delete left active realization scopes behind.'
    Assert-ProjectWorldIntegration -Condition (
        (Get-ProjectWorldIntegrationDigest -Paths @((Join-Path $roots.ContentRoot 'Authored'))) -ceq
        $protectedAuthoredSha256) -Message 'Delete changed protected authored bytes.'
    $lifecycle.Add('delete')

    Write-ProjectWorldJson -Path (Join-Path $evidenceRoot 'summary.json') -Document ([ordered]@{
        schema_version = 1
        status = 'accepted'
        world_data_plugin = $worldDataPlugin
        realization_profile_id = [string]$realization.profile_id
        compile_result_sha256 = (Get-FileHash -LiteralPath $compileResultPath -Algorithm SHA256).Hash.ToLowerInvariant()
        lifecycle = @($lifecycle)
        one_cell_dirty_unit = $cellId
        canonical_cell_count = [int]$first.Result.canonical_cell_count
        sample_spacing_m = @($first.Result.sample_spacing_m)
        landscape_proxy_count = [int]$first.Result.changes.landscape_proxies
        water_cell_actor_count = [int]$first.Result.changes.water_cell_actors
        maximum_georeference_error_m = $MaximumGeoReferenceErrorMeters
        observed_georeference_error_m = [double]$first.Result.georeferencing_placement_error_m
        hlod_proxy_actor_count = [int]$first.Result.hlod_proxy_actor_count
        rejected_state_sha256 = $afterRejected
        unchanged_active_set_sha256 = $unchangedActive.Sha256
        protected_authored_sha256 = $protectedAuthoredSha256
        layer_semantic_reconstruction_sha256 = $(if ($ProveReconstruction) { $firstLayerSemanticSha256 } else { $null })
        active_scope_count_after_delete = 0
    })
}
finally {
    if ($snapshotRecords.Count -gt 0) {
        Restore-ProjectWorldGeneratedSnapshot `
            -ContentRoot $roots.ContentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $roots.GeneratedPackageRoot `
            -Records $snapshotRecords
    }
    if (Test-Path -LiteralPath $workRoot) {
        Remove-ProjectWorldGeneratedSnapshot `
            -TransactionParent $workParent `
            -TransactionRoot $workRoot
    }
    if ([string]::IsNullOrWhiteSpace($priorDelegatedToken)) {
        Remove-Item Env:ALIS_WORLD_CONTENT_LOCK_TOKEN -ErrorAction SilentlyContinue
    }
    if ($null -ne $contentLock) {
        $contentLock.Dispose()
    }
}

Write-Host "[ProjectWorldLayerLifecycle] accepted: $(Join-Path $evidenceRoot 'summary.json')"
