# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$CompileResult,
    [string]$BaselineRuntimeProfile = '',
    [string]$CandidateRuntimeProfile =
        'Plugins/World/ProjectWorldData/Data/Runtime/kazan_territory_128_768_v1.json',
    [string]$PresentationProfile =
        'Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json',
    [string]$AuthoredOverlayProfile =
        'Plugins/World/ProjectWorldData/Data/Authored/kazan_slice0_v1.json',
    [string]$RealizationProfile =
        'Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json'
)

$ErrorActionPreference = 'Stop'
$worldRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $worldRoot))
$wrapper = Join-Path $worldRoot 'realize_canonical_world.ps1'
. (Join-Path $worldRoot 'generated_content_transaction.ps1')
. (Join-Path $worldRoot 'generated_manifest.ps1')
. (Join-Path $worldRoot 'realization_layer_operation.ps1')
. (Join-Path $PSScriptRoot '..\runtime_profile_test_helpers.ps1')

function Resolve-LocalityPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Assert-Locality {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) { throw $Message }
}

function Get-LocalitySha256 {
    param([Parameter(Mandatory = $true)][string[]]$Lines)
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($Lines -join "`n"))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}

function Initialize-LocalityAuthority {
    param(
        [Parameter(Mandatory = $true)][object]$Active,
        [Parameter(Mandatory = $true)][string]$DestinationRoot
    )
    New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null
    $operationId = [System.Guid]::NewGuid().ToString('N')
    $candidates = [System.Collections.Generic.List[object]]::new()
    foreach ($scopeId in @($Active.Manifests.Keys | Sort-Object)) {
        $prior = $Active.Manifests[$scopeId]
        $producerId = Get-ProjectWorldManifestProducerId -Manifest $prior
        $currentFingerprint = Get-ProjectWorldGeneratorFingerprint `
            -ProjectRoot $projectRoot -ProducerId $producerId
        $candidate = New-ProjectWorldFingerprintMigrationCandidate `
            -PriorManifest $prior -Generation 1 -OperationId $operationId `
            -GeneratorFingerprint $currentFingerprint
        $candidate.'$schema' = $script:ManifestSchemaId
        $candidates.Add($candidate)
    }
    Publish-ProjectWorldActiveSet `
        -ManifestRoot $DestinationRoot -ProjectRoot $projectRoot `
        -TransactionId $operationId -OperationId $operationId `
        -CandidateManifests @($candidates) | Out-Null
}

function Get-LocalityLayerState {
    param(
        [Parameter(Mandatory = $true)][object]$Active,
        [Parameter(Mandatory = $true)][string[]]$ScopeIds
    )
    $entries = [ordered]@{}
    $artifactLines = [System.Collections.Generic.List[string]]::new()
    foreach ($scopeId in @($ScopeIds | Sort-Object)) {
        $record = @($Active.Record.scopes | Where-Object { $_.scope_id -ceq $scopeId })
        Assert-Locality ($record.Count -eq 1 -and $Active.Manifests.Contains($scopeId)) `
            "Missing active generated-layer scope: $scopeId"
        $manifest = $Active.Manifests[$scopeId]
        Assert-Locality ([string]$manifest.input_identity.runtime_profile_sha256 -ceq 'none') `
            "Generated layer claims runtime-profile ownership: $scopeId"
        $entries[$scopeId] = [string]$record[0].manifest_sha256
        foreach ($artifact in @($manifest.artifacts | Sort-Object path)) {
            $fullPath = Resolve-LocalityPath -Path ([string]$artifact.path)
            Assert-Locality (Test-Path -LiteralPath $fullPath -PathType Leaf) `
                "Generated-layer artifact is missing: $($artifact.path)"
            $actual = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash.ToLowerInvariant()
            Assert-Locality ($actual -ceq [string]$artifact.digest) `
                "Generated-layer artifact drifted: $($artifact.path)"
            $artifactLines.Add("$scopeId|$($artifact.path)|$actual")
        }
    }
    return [pscustomobject]@{
        Entries = $entries
        ArtifactSetSha256 = Get-LocalitySha256 -Lines @($artifactLines)
    }
}

$compilePath = Resolve-LocalityPath $CompileResult
$presentationPath = Resolve-LocalityPath $PresentationProfile
$authoredPath = Resolve-LocalityPath $AuthoredOverlayProfile
$realizationPath = Resolve-LocalityPath $RealizationProfile
$realization = Get-Content -LiteralPath $realizationPath -Raw | ConvertFrom-Json
$baselinePath = if ([string]::IsNullOrWhiteSpace($BaselineRuntimeProfile)) {
    Resolve-LocalityPath "Plugins/World/ProjectWorldData/Data/Runtime/$($realization.runtime_profile_id).json"
}
else { Resolve-LocalityPath $BaselineRuntimeProfile }
$candidatePath = Resolve-LocalityPath $CandidateRuntimeProfile
Assert-Locality ([string]$realization.world_data_plugin -ceq 'ProjectWorldData') `
    'Runtime-profile locality is a production-isolated ProjectWorldData proof.'
$roots = Resolve-ProjectWorldDataRoots -ProjectRoot $projectRoot -PluginName 'ProjectWorldData'
$resolvedLayers = Resolve-ProjectWorldRealizationLayers `
    -RealizationDocument $realization -WorldDataRoots $roots
$layerScopeIds = @($resolvedLayers.Definitions.Keys | Sort-Object)
Assert-Locality ($layerScopeIds.Count -eq 6) 'The Kazan locality proof requires exactly six generated layers.'
$layerPaths = @($layerScopeIds | ForEach-Object { $resolvedLayers.ScopePaths[$_] })
$mapPackage = [string]$realization.map_package
$mapScopeId = Get-ProjectWorldMapScopeId `
    -MapPackage $mapPackage -GeneratedPackageRoot $roots.GeneratedPackageRoot
$runId = [System.Guid]::NewGuid().ToString('N')
$evidenceRoot = Join-Path $projectRoot "Saved\Validation\WorldRealization\runtime-profile-locality\$runId"
$summaryPath = Join-Path $evidenceRoot 'summary.json'
$transactionParent = Join-Path $projectRoot 'tmp\world\runtime_profile_locality'
$workRoot = Join-Path $transactionParent $runId
$snapshotRoot = Join-Path $workRoot 'snapshot'
$transientManifestRoot = Join-Path $workRoot 'manifests'
$powerShellExe = (Get-Process -Id $PID).Path
$priorToken = $env:ALIS_WORLD_CONTENT_LOCK_TOKEN
$contentLock = $null
$snapshotRecords = @()
$summary = $null

function Invoke-LocalityApply {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$RuntimeProfile,
        [Parameter(Mandatory = $true)][string]$RealizationProfile
    )
    $receiptPath = Join-Path $evidenceRoot "$Name.json"
    $arguments = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $wrapper,
        '-CompileResult', $compilePath, '-Mode', 'Apply',
        '-Map', $mapPackage, '-WorldDataPlugin', 'ProjectWorldData',
        '-PresentationProfile', $presentationPath,
        '-RuntimeProfile', $RuntimeProfile,
        '-AuthoredOverlayProfile', $authoredPath,
        '-RealizationProfile', $RealizationProfile,
        '-TransientRealizationProfileRoot', $workRoot,
        '-ManifestRoot', $transientManifestRoot,
        '-EvidencePath', $receiptPath,
        '-MaxRoads', '1000', '-MaxBuildings', '1000',
        '-RequireLandscape', '-NonInteractive')
    & $powerShellExe @arguments | Out-Host
    $exitCode = $LASTEXITCODE
    $result = if (Test-Path -LiteralPath $receiptPath -PathType Leaf) {
        Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    }
    Assert-Locality ($exitCode -eq 0 -and $null -ne $result -and $result.status -ceq 'accepted') `
        "Runtime-profile locality Apply failed: $Name"
    foreach ($inventory in @($result.layer_inventories)) {
        Assert-Locality (@($inventory.final_dirty_units).Count -eq 0) `
            "Runtime-only switch dirtied generated layer: $($inventory.layer_id)"
    }
    return $result
}

function New-LocalityRealizationProfile {
    param(
        [Parameter(Mandatory = $true)][string]$RuntimeProfile,
        [Parameter(Mandatory = $true)][string]$OutputPath
    )
    $runtime = Get-Content -LiteralPath $RuntimeProfile -Raw | ConvertFrom-Json
    $document = $realization | ConvertTo-Json -Depth 20 | ConvertFrom-Json
    $document.runtime_profile_id = [string]$runtime.profile_id
    Set-ProjectWorldTransientSchemaReference -Document $document `
        -DocumentPath $OutputPath -SchemaPath (Join-Path $projectRoot `
            'Plugins\World\ProjectWorld\Data\Schemas\project_world_realization_profile.schema.json')
    Write-ProjectWorldJson -Path $OutputPath -Document $document
    return $OutputPath
}

try {
    New-Item -ItemType Directory -Path $evidenceRoot, $workRoot -Force | Out-Null
    $contentLock = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
    if ([string]::IsNullOrWhiteSpace($priorToken)) {
        $lockPath = Join-Path $projectRoot 'tmp\world\world_realization\content_mutation.lock'
        $env:ALIS_WORLD_CONTENT_LOCK_TOKEN = (Get-Content -LiteralPath $lockPath -Raw).Trim()
    }
    $durableActive = Read-ProjectWorldActiveSet `
        -ManifestRoot $roots.ManifestRoot -ProjectRoot $projectRoot
    Initialize-LocalityAuthority -Active $durableActive `
        -DestinationRoot $transientManifestRoot
    $snapshotRecords = @(New-ProjectWorldGeneratedSnapshot `
        -ContentRoot $roots.ContentRoot -MapPackage $mapPackage `
        -GeneratedPackageRoot $roots.GeneratedPackageRoot `
        -SnapshotRoot $snapshotRoot -AdditionalPaths $layerPaths)
    $candidateRealization = New-LocalityRealizationProfile `
        -RuntimeProfile $candidatePath -OutputPath (Join-Path $workRoot 'candidate.realization.json')
    $baselineRealization = New-LocalityRealizationProfile `
        -RuntimeProfile $baselinePath -OutputPath (Join-Path $workRoot 'baseline.realization.json')

    $before = Read-ProjectWorldActiveSet `
        -ManifestRoot $transientManifestRoot -ProjectRoot $projectRoot
    $beforeLayers = Get-LocalityLayerState -Active $before -ScopeIds $layerScopeIds
    $candidateResult = Invoke-LocalityApply -Name '01-candidate' `
        -RuntimeProfile $candidatePath -RealizationProfile $candidateRealization
    $candidate = Read-ProjectWorldActiveSet `
        -ManifestRoot $transientManifestRoot -ProjectRoot $projectRoot
    $candidateLayers = Get-LocalityLayerState -Active $candidate -ScopeIds $layerScopeIds
    Assert-Locality (($beforeLayers.Entries | ConvertTo-Json -Compress) -ceq
        ($candidateLayers.Entries | ConvertTo-Json -Compress)) `
        'Runtime candidate advanced a generated-layer manifest.'
    Assert-Locality ($beforeLayers.ArtifactSetSha256 -ceq $candidateLayers.ArtifactSetSha256) `
        'Runtime candidate changed generated-layer artifact bytes.'
    Assert-Locality ([string]$candidate.Manifests[$mapScopeId].input_identity.runtime_profile_sha256 -ceq
        (Get-FileHash -LiteralPath $candidatePath -Algorithm SHA256).Hash.ToLowerInvariant()) `
        'Candidate runtime identity did not advance through the map owner.'

    $baselineResult = Invoke-LocalityApply -Name '02-baseline-restore' `
        -RuntimeProfile $baselinePath -RealizationProfile $baselineRealization
    $restored = Read-ProjectWorldActiveSet `
        -ManifestRoot $transientManifestRoot -ProjectRoot $projectRoot
    $restoredLayers = Get-LocalityLayerState -Active $restored -ScopeIds $layerScopeIds
    Assert-Locality (($beforeLayers.Entries | ConvertTo-Json -Compress) -ceq
        ($restoredLayers.Entries | ConvertTo-Json -Compress)) `
        'Baseline restoration advanced a generated-layer manifest.'
    Assert-Locality ($beforeLayers.ArtifactSetSha256 -ceq $restoredLayers.ArtifactSetSha256) `
        'Baseline restoration changed generated-layer artifact bytes.'

    $summary = [ordered]@{
        schema_version = 1
        status = 'accepted'
        map_scope_id = $mapScopeId
        layer_scope_ids = $layerScopeIds
        layer_artifact_set_sha256 = $beforeLayers.ArtifactSetSha256
        baseline_runtime_profile_sha256 = (Get-FileHash $baselinePath -Algorithm SHA256).Hash.ToLowerInvariant()
        candidate_runtime_profile_sha256 = (Get-FileHash $candidatePath -Algorithm SHA256).Hash.ToLowerInvariant()
        realization_sot_field_switched = $true
        candidate_runtime_cell_size_m = [int]$candidateResult.runtime_cell_size_m
        restored_runtime_cell_size_m = [int]$baselineResult.runtime_cell_size_m
        durable_active_set_sha256 = $durableActive.Sha256
    }
}
finally {
    if ($snapshotRecords.Count -gt 0) {
        Restore-ProjectWorldGeneratedSnapshot `
            -ContentRoot $roots.ContentRoot -MapPackage $mapPackage `
            -GeneratedPackageRoot $roots.GeneratedPackageRoot -Records $snapshotRecords
    }
    if (Test-Path -LiteralPath $workRoot) {
        Remove-ProjectWorldGeneratedSnapshot `
            -TransactionParent $transactionParent -TransactionRoot $workRoot
    }
    if ([string]::IsNullOrWhiteSpace($priorToken)) {
        Remove-Item Env:ALIS_WORLD_CONTENT_LOCK_TOKEN -ErrorAction SilentlyContinue
    }
    if ($null -ne $contentLock) { $contentLock.Dispose() }
}

Write-ProjectWorldJson -Path $summaryPath -Document $summary
Write-Host "[ProjectWorldRuntimeProfileLocality] accepted: $summaryPath"
