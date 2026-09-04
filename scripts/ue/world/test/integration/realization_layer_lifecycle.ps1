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

    [switch]$ProveReconstruction,

    [switch]$ProvePackageLocality
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

function Get-ProjectWorldIntegrationLayer {
    param(
        [Parameter(Mandatory = $true)][object]$Result,
        [Parameter(Mandatory = $true)][string]$LayerId
    )

    $matches = @($Result.layer_inventories | Where-Object { $_.layer_id -ceq $LayerId })
    Assert-ProjectWorldIntegration -Condition ($matches.Count -eq 1) `
        -Message "Expected one layer inventory: $LayerId"
    return $matches[0]
}

function Get-ProjectWorldIntegrationFileHashes {
    param([Parameter(Mandatory = $true)][object[]]$Artifacts)

    $hashes = @{}
    foreach ($artifact in $Artifacts) {
        $path = Resolve-ProjectPath -Path ([string]$artifact.path)
        Assert-ProjectWorldIntegration -Condition (
            $path.StartsWith($projectRoot, [System.StringComparison]::OrdinalIgnoreCase) -and
            (Test-Path -LiteralPath $path -PathType Leaf)) `
            -Message "Layer artifact is unavailable: $path"
        $hashes[[string]$artifact.path] =
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $hashes
}

function Get-ProjectWorldChangedHashPaths {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Before,
        [Parameter(Mandatory = $true)][hashtable]$After
    )

    Assert-ProjectWorldIntegration -Condition (
        @($Before.Keys | Where-Object { -not $After.ContainsKey($_) }).Count -eq 0 -and
        @($After.Keys | Where-Object { -not $Before.ContainsKey($_) }).Count -eq 0) `
        -Message 'Layer artifact paths changed during a package-local update.'
    return @($Before.Keys | Where-Object { $Before[$_] -cne $After[$_] } | Sort-Object)
}

function Get-ProjectWorldIntegrationTextSha256 {
    param([Parameter(Mandatory = $true)][string]$Value)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Get-ProjectWorldIntegrationScopeEntry {
    param(
        [Parameter(Mandatory = $true)][object]$ActiveSet,
        [Parameter(Mandatory = $true)][string]$ScopeId
    )

    $entries = @($ActiveSet.Record.scopes | Where-Object { $_.scope_id -ceq $ScopeId })
    Assert-ProjectWorldIntegration -Condition (
        $entries.Count -eq 1 -and $ActiveSet.Manifests.Contains($ScopeId)) `
        -Message "Expected one active scope: $ScopeId"
    return [pscustomobject]@{
        scope_id = [string]$entries[0].scope_id
        manifest_path = [string]$entries[0].manifest_path
        manifest_sha256 = [string]$entries[0].manifest_sha256
        generation = [int]$ActiveSet.Manifests[$ScopeId].generation
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
if ($ProvePackageLocality) {
    Assert-ProjectWorldIntegration -Condition (
        $worldDataPlugin -ceq 'ProjectWorldTestData' -and $ProveReconstruction) `
        -Message 'Package-locality proof requires TestData plus -ProveReconstruction.'
}
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
    (Get-ProjectWorldPresentationRoot -ContentRoot $roots.ContentRoot)
    (Join-Path $roots.ContentRoot 'Authored')
    (Join-Path $manifestRoot 'active_set.json')
)
$powerShellExe = (Get-Process -Id $PID).Path
$priorDelegatedToken = $env:ALIS_WORLD_CONTENT_LOCK_TOKEN
$contentLock = $null
$snapshotRecords = @()
$packageLocality = $null
$packageLocalityProof = $null

function Invoke-ProjectWorldIntegrationRun {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][ValidateSet('Apply', 'Delete')][string]$Mode,
        [string]$RunCompileResult = $compileResultPath,
        [string[]]$ExtraArguments = @()
    )

    $evidencePath = Join-Path $evidenceRoot "$Name.json"
    $arguments = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $wrapper,
        '-CompileResult', $RunCompileResult,
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

function Invoke-ProjectWorldPackageLocalityProof {
    param(
        [Parameter(Mandatory = $true)][object]$BaseResult,
        [Parameter(Mandatory = $true)][object]$BaseActive,
        [Parameter(Mandatory = $true)][object]$Variants
    )

    $baseTerrain = Get-ProjectWorldIntegrationLayer -Result $BaseResult -LayerId 'terrain'
    $baseWater = Get-ProjectWorldIntegrationLayer -Result $BaseResult -LayerId 'water'
    $mapScopeId = Get-ProjectWorldMapScopeId `
        -MapPackage $mapPackage -GeneratedPackageRoot $roots.GeneratedPackageRoot
    $mapRelative = $mapPackage.Substring($roots.MountRoot.Length).Replace(
        '/', [System.IO.Path]::DirectorySeparatorChar)
    $mapPath = Join-Path $roots.ContentRoot ($mapRelative + '.umap')
    $baseTerrainHashes = Get-ProjectWorldIntegrationFileHashes -Artifacts $baseTerrain.artifacts

    $terrainRun = Invoke-ProjectWorldIntegrationRun `
        -Name '03a-genuine-terrain-change' -Mode Apply `
        -RunCompileResult ([string]$Variants.terrain_compile_result)
    Assert-ProjectWorldIntegration -Condition (
        $terrainRun.ExitCode -eq 0 -and $terrainRun.Result.status -ceq 'accepted') `
        -Message 'Genuine one-cell terrain Apply did not commit.'
    $terrainInventory = Get-ProjectWorldIntegrationLayer -Result $terrainRun.Result -LayerId 'terrain'
    Assert-ProjectWorldIntegration -Condition (
        @($terrainInventory.final_dirty_units).Count -eq 1 -and
        [string]$terrainInventory.final_dirty_units[0] -ceq [string]$Variants.terrain_changed_cell_id) `
        -Message 'Genuine terrain change did not select exactly its canonical cell.'

    $terrainHashes = Get-ProjectWorldIntegrationFileHashes -Artifacts $terrainInventory.artifacts
    $changedTerrainPaths = @(Get-ProjectWorldChangedHashPaths `
        -Before $baseTerrainHashes -After $terrainHashes)
    Assert-ProjectWorldIntegration -Condition ($changedTerrainPaths.Count -eq 1) `
        -Message 'A genuine terrain change did not rewrite exactly one proxy package.'

    $cellId = [string]$Variants.terrain_changed_cell_id
    Assert-ProjectWorldIntegration -Condition ($cellId -match ':x(-?\d+):y(-?\d+)$') `
        -Message 'Terrain variant cell ID is malformed.'
    $cellX = [int]$Matches[1]
    $cellY = [int]$Matches[2]
    $allCoordinates = @($terrainInventory.canonical_inputs | ForEach-Object {
        Assert-ProjectWorldIntegration -Condition ([string]$_.unit_id -match ':x(-?\d+):y(-?\d+)$') `
            -Message 'Terrain canonical-input cell ID is malformed.'
        [pscustomobject]@{ X = [int]$Matches[1]; Y = [int]$Matches[2] }
    })
    $coveragePath = Join-Path (Split-Path ([string]$Variants.terrain_compile_result)) 'canonical\coverage.json'
    $coverage = Get-Content -LiteralPath $coveragePath -Raw | ConvertFrom-Json
    $componentQuads = [int]$coverage.grid.cell_quads[0]
    $minimumX = [int](($allCoordinates | Measure-Object -Property X -Minimum).Minimum)
    $maximumY = [int](($allCoordinates | Measure-Object -Property Y -Maximum).Maximum)
    $canonicalInput = @($terrainInventory.canonical_inputs | Where-Object { $_.unit_id -ceq $cellId })
    Assert-ProjectWorldIntegration -Condition ($canonicalInput.Count -eq 1) `
        -Message 'Changed terrain cell has no canonical input identity.'
    $sectionX = ($cellX - $minimumX) * $componentQuads
    $sectionY = ($maximumY - $cellY) * $componentQuads
    $semanticText = "project_landscape_proxy_v2|$([string]$realization.landscape.logical_landscape_id)|" +
        "$cellId|$([string]$canonicalInput[0].sha256)|$sectionX,$sectionY"
    $expectedSemantic = Get-ProjectWorldIntegrationTextSha256 -Value $semanticText
    $expectedArtifacts = @($terrainInventory.artifacts | Where-Object {
        $_.semantic_sha256 -ceq $expectedSemantic
    })
    Assert-ProjectWorldIntegration -Condition (
        $expectedArtifacts.Count -eq 1 -and
        [string]$expectedArtifacts[0].path -ceq [string]$changedTerrainPaths[0]) `
        -Message 'Changed proxy package does not belong to the changed canonical terrain cell.'

    $terrainActive = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
    $baseTerrainScope = Get-ProjectWorldIntegrationScopeEntry `
        -ActiveSet $BaseActive -ScopeId ([string]$baseTerrain.scope_id)
    $terrainScope = Get-ProjectWorldIntegrationScopeEntry `
        -ActiveSet $terrainActive -ScopeId ([string]$terrainInventory.scope_id)
    $baseMapScope = Get-ProjectWorldIntegrationScopeEntry -ActiveSet $BaseActive -ScopeId $mapScopeId
    $terrainMapScope = Get-ProjectWorldIntegrationScopeEntry -ActiveSet $terrainActive -ScopeId $mapScopeId
    Assert-ProjectWorldIntegration -Condition (
        [int]$terrainScope.generation -gt [int]$baseTerrainScope.generation -and
        [string]$terrainScope.manifest_sha256 -cne [string]$baseTerrainScope.manifest_sha256) `
        -Message 'Actual terrain semantics did not advance terrain authority.'
    Assert-ProjectWorldIntegration -Condition (
        [int]$terrainMapScope.generation -gt [int]$baseMapScope.generation -and
        [string]$terrainMapScope.manifest_sha256 -cne [string]$baseMapScope.manifest_sha256) `
        -Message 'Changed compile-result identity did not advance map-scope authority.'

    $waterRun = Invoke-ProjectWorldIntegrationRun `
        -Name '03b-genuine-water-only-change' -Mode Apply `
        -RunCompileResult ([string]$Variants.water_compile_result)
    Assert-ProjectWorldIntegration -Condition (
        $waterRun.ExitCode -eq 0 -and $waterRun.Result.status -ceq 'accepted') `
        -Message 'Genuine water-only Apply did not commit.'
    $waterTerrain = Get-ProjectWorldIntegrationLayer -Result $waterRun.Result -LayerId 'terrain'
    $waterInventory = Get-ProjectWorldIntegrationLayer -Result $waterRun.Result -LayerId 'water'
    $sourceChangedWaterCells = @($Variants.water_changed_cell_ids | Sort-Object -Unique)
    $expectedWaterCells = @($waterInventory.final_dirty_units | Sort-Object -Unique)
    $actualWaterTerrainCells = @($waterTerrain.final_dirty_units | Sort-Object -Unique)
    Assert-ProjectWorldIntegration -Condition (
        ($actualWaterTerrainCells -join '|') -ceq ($expectedWaterCells -join '|') -and
        $expectedWaterCells.Count -gt 0 -and
        @($sourceChangedWaterCells | Where-Object { $_ -notin $expectedWaterCells }).Count -eq 0) `
        -Message 'Water-only change did not select the same hydrologic Landscape cells.'
    $waterMapHash = (Get-FileHash -LiteralPath $mapPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $waterTerrainHashes = Get-ProjectWorldIntegrationFileHashes -Artifacts $waterTerrain.artifacts
    Assert-ProjectWorldIntegration -Condition (
        @(Get-ProjectWorldChangedHashPaths -Before $terrainHashes -After $waterTerrainHashes).Count -eq
            $expectedWaterCells.Count) `
        -Message 'A genuine water-only change did not rewrite exactly its hydrologic Landscape proxies.'

    $waterActive = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
    $waterTerrainScope = Get-ProjectWorldIntegrationScopeEntry `
        -ActiveSet $waterActive -ScopeId ([string]$waterTerrain.scope_id)
    $terrainWaterScope = Get-ProjectWorldIntegrationScopeEntry `
        -ActiveSet $terrainActive -ScopeId ([string]$baseWater.scope_id)
    $waterScope = Get-ProjectWorldIntegrationScopeEntry `
        -ActiveSet $waterActive -ScopeId ([string]$waterInventory.scope_id)
    $waterMapScope = Get-ProjectWorldIntegrationScopeEntry -ActiveSet $waterActive -ScopeId $mapScopeId
    Assert-ProjectWorldIntegration -Condition (
        [int]$waterTerrainScope.generation -gt [int]$terrainScope.generation -and
        [string]$waterTerrainScope.manifest_sha256 -cne [string]$terrainScope.manifest_sha256) `
        -Message 'Water-only change did not advance hydrologic Landscape authority.'
    Assert-ProjectWorldIntegration -Condition (
        [int]$waterScope.generation -gt [int]$terrainWaterScope.generation -and
        [string]$waterScope.manifest_sha256 -cne [string]$terrainWaterScope.manifest_sha256) `
        -Message 'Actual water semantics did not advance water authority.'
    Assert-ProjectWorldIntegration -Condition (
        [int]$waterMapScope.generation -gt [int]$terrainMapScope.generation -and
        [string]$waterMapScope.manifest_sha256 -cne [string]$terrainMapScope.manifest_sha256) `
        -Message 'Water compile-result identity did not advance map-scope authority.'

    return [pscustomobject]@{
        TerrainCellId = $cellId
        TerrainProxyPath = [string]$changedTerrainPaths[0]
        LogicalMapSha256 = $waterMapHash
        TerrainManifestGeneration = [int]$terrainScope.generation
        WaterManifestGeneration = [int]$waterScope.generation
    }
}

try {
    if ($ProvePackageLocality) {
        $variantBuilder = Join-Path $PSScriptRoot 'generate_package_locality_variants.py'
        $variantRoot = Join-Path $workRoot 'canonical-variants'
        $variantOutput = & python $variantBuilder --output-root $variantRoot
        Assert-ProjectWorldIntegration -Condition ($LASTEXITCODE -eq 0) `
            -Message 'Canonical package-locality variants failed to compile.'
        $packageLocality = $variantOutput | ConvertFrom-Json
        $compileResultPath = [string]$packageLocality.base_compile_result
    }
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
        $incrementalInventory = @($incremental.Result.layer_inventories | Where-Object {
            [string]$_.scope_id -ceq $scopeId
        })
        Assert-ProjectWorldIntegration -Condition ($incrementalInventory.Count -eq 1) `
            -Message "Incremental Apply has no inventory for layer scope: $scopeId"
        if (@($incrementalInventory[0].final_dirty_units).Count -gt 0) {
            # Explicit/downstream dirtiness authorizes regeneration. Immutable authority must
            # follow the actual bytes; only a zero-dirty layer is required to retain its manifest.
            continue
        }
        $entry = @($incrementalActive.Record.scopes | Where-Object { $_.scope_id -ceq $scopeId })
        Assert-ProjectWorldIntegration `
            -Condition ($entry.Count -eq 1 -and
                [string]$entry[0].manifest_sha256 -ceq $firstLayerHashes[$scopeId]) `
            -Message "Zero-dirty layer authority advanced: $scopeId"
    }
    Assert-ProjectWorldIntegration -Condition (
        (Get-ProjectWorldIntegrationDigest -Paths @((Join-Path $roots.ContentRoot 'Authored'))) -ceq
        $protectedAuthoredSha256) -Message 'One-cell Apply changed protected authored bytes.'

    if ($ProvePackageLocality) {
        $packageLocalityProof = Invoke-ProjectWorldPackageLocalityProof `
            -BaseResult $incremental.Result -BaseActive $incrementalActive `
            -Variants $packageLocality
    }

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
    @('first_apply', 'unchanged_apply', 'one_cell_apply') |
        ForEach-Object { $lifecycle.Add($_) }
    if ($ProvePackageLocality) {
        $lifecycle.Add('genuine_terrain_change')
        $lifecycle.Add('genuine_water_only_change')
    }
    $lifecycle.Add('rejected_rollback')
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
        package_locality = $packageLocalityProof
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
