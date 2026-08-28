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

    [string]$ChangedAuthoredOverlayProfile =
        'Plugins/World/ProjectWorldTestData/Data/Authored/synthetic_landscape_water_twin_changed_v1.json',

    [string]$RealizationProfile =
        'Plugins/World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_landscape_water_twin.realization.json'
)

$ErrorActionPreference = 'Stop'
$worldRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $worldRoot))
$wrapper = Join-Path $worldRoot 'realize_canonical_world.ps1'
$audit = Join-Path $worldRoot 'audit_generated_authority.ps1'
. (Join-Path $worldRoot 'generated_content_transaction.ps1')
. (Join-Path $worldRoot 'generated_manifest.ps1')

function Resolve-ProjectPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Assert-ProjectWorldPersistence {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) { throw $Message }
}

function Get-ProjectWorldTreeDigest {
    param([Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$Paths)

    $records = [System.Collections.Generic.List[string]]::new()
    foreach ($path in @($Paths | Sort-Object -Unique)) {
        $fullPath = [System.IO.Path]::GetFullPath($path)
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            $records.Add("$fullPath|$((Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash.ToLowerInvariant())")
        }
        elseif (Test-Path -LiteralPath $fullPath -PathType Container) {
            foreach ($file in Get-ChildItem -LiteralPath $fullPath -Recurse -File | Sort-Object FullName) {
                $records.Add("$($file.FullName)|$((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())")
            }
        }
        else {
            $records.Add("$fullPath|absent")
        }
    }
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes(($records -join "`n"))
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}

function Find-ProjectWorldAnchorPackage {
    param(
        [Parameter(Mandatory = $true)][string]$ExternalRoot,
        [Parameter(Mandatory = $true)][string]$OverlayId
    )

    $needle = "ProjectWorld.AuthoredOverlay=$OverlayId"
    $matches = @()
    foreach ($file in Get-ChildItem -LiteralPath $ExternalRoot -Recurse -File -Filter '*.uasset') {
        $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
        $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
        $unicode = [System.Text.Encoding]::Unicode.GetString($bytes)
        if ($ascii.Contains($needle) -or $unicode.Contains($needle)) {
            $matches += $file.FullName
        }
    }
    Assert-ProjectWorldPersistence -Condition ($matches.Count -eq 1) `
        -Message "Expected one external package for authored overlay '$OverlayId'; found $($matches.Count)."
    return [System.IO.Path]::GetFullPath($matches[0])
}

$compileResultPath = Resolve-ProjectPath -Path $CompileResult
$presentationPath = Resolve-ProjectPath -Path $PresentationProfile
$authoredPath = Resolve-ProjectPath -Path $AuthoredOverlayProfile
$changedAuthoredPath = Resolve-ProjectPath -Path $ChangedAuthoredOverlayProfile
$realizationPath = Resolve-ProjectPath -Path $RealizationProfile
$realization = Get-Content -LiteralPath $realizationPath -Raw | ConvertFrom-Json
Assert-ProjectWorldPersistence -Condition ([string]$realization.world_data_plugin -ceq 'ProjectWorldTestData') `
    -Message 'Authored-overlay persistence proof is confined to ProjectWorldTestData.'
$roots = Resolve-ProjectWorldDataRoots -ProjectRoot $projectRoot -PluginName 'ProjectWorldTestData'
$mapPackage = [string]$realization.map_package
$runId = [System.Guid]::NewGuid().ToString('N')
$evidenceRoot = Join-Path $projectRoot "Saved\Validation\WorldRealization\authored-overlay-persistence\$runId"
$manifestRoot = Join-Path $evidenceRoot 'manifests'
$workParent = Join-Path $projectRoot 'tmp\world\authored_overlay_persistence'
$workRoot = Join-Path $workParent $runId
$snapshotRoot = Join-Path $workRoot 'outer-snapshot'
$layerPaths = @($realization.layers | ForEach-Object {
    $relative = ([string]$_.artifact_root).Substring($roots.MountRoot.Length).Replace(
        '/', [System.IO.Path]::DirectorySeparatorChar).TrimEnd('\', '/')
    Join-Path $roots.ContentRoot $relative
})
$mapRelative = $mapPackage.Substring($roots.MountRoot.Length).Replace(
    '/', [System.IO.Path]::DirectorySeparatorChar)
$externalRoot = Join-Path $roots.ContentRoot "__ExternalActors__\$mapRelative"
$durableActiveSet = Join-Path $roots.ManifestRoot 'active_set.json'
$durableAuthorityBefore = Get-ProjectWorldTreeDigest -Paths @($durableActiveSet)
$powerShellExe = (Get-Process -Id $PID).Path
$priorDelegatedToken = $env:ALIS_WORLD_CONTENT_LOCK_TOKEN
$contentLock = $null
$snapshotRecords = @()

function Invoke-ProjectWorldPersistenceRun {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Profile,
        [string[]]$ExtraArguments = @()
    )

    $evidencePath = Join-Path $evidenceRoot "$Name.json"
    $arguments = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $wrapper,
        '-CompileResult', $compileResultPath,
        '-Mode', 'Apply',
        '-Map', $mapPackage,
        '-PresentationProfile', $presentationPath,
        '-AuthoredOverlayProfile', $Profile,
        '-RealizationProfile', $realizationPath,
        '-ManifestRoot', $manifestRoot,
        '-EvidencePath', $evidencePath,
        '-MaxRoads', '0',
        '-MaxBuildings', '0',
        '-RequireLandscape',
        '-NonInteractive'
    ) + $ExtraArguments
    & $powerShellExe @arguments | Out-Host
    $exitCode = $LASTEXITCODE
    $result = if (Test-Path -LiteralPath $evidencePath -PathType Leaf) {
        Get-Content -LiteralPath $evidencePath -Raw | ConvertFrom-Json
    }
    else { $null }
    return [pscustomobject]@{ ExitCode = $exitCode; Path = $evidencePath; Result = $result }
}

try {
    $contentLock = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
    if ([string]::IsNullOrWhiteSpace($priorDelegatedToken)) {
        $lockPath = Join-Path $projectRoot 'tmp\world\world_realization\content_mutation.lock'
        $env:ALIS_WORLD_CONTENT_LOCK_TOKEN = (Get-Content -LiteralPath $lockPath -Raw).Trim()
    }
    $evidenceParent = Split-Path -Parent $evidenceRoot
    Get-ChildItem -LiteralPath $evidenceParent -Directory -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -cne $evidenceRoot -and
            -not (Test-Path -LiteralPath (Join-Path $_.FullName 'summary.json') -PathType Leaf)
        } |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName -Recurse -Force }
    $snapshotRecords = @(New-ProjectWorldGeneratedSnapshot `
        -ContentRoot $roots.ContentRoot `
        -MapPackage $mapPackage `
        -GeneratedPackageRoot $roots.GeneratedPackageRoot `
        -SnapshotRoot $snapshotRoot `
        -AdditionalPaths $layerPaths)

    $first = Invoke-ProjectWorldPersistenceRun `
        -Name '01-first-apply' -Profile $authoredPath -ExtraArguments @('-EnrollManifests')
    Assert-ProjectWorldPersistence -Condition (
        $first.ExitCode -eq 0 -and $first.Result.status -ceq 'accepted' -and
        [int]$first.Result.changes.self_saved_actor_mutations -ge 1) `
        -Message 'Initial synthetic Apply did not persist its authored-overlay package.'

    $anchorPackage = Find-ProjectWorldAnchorPackage `
        -ExternalRoot $externalRoot -OverlayId 'persistence_marker'
    $anchorBefore = (Get-FileHash -LiteralPath $anchorPackage -Algorithm SHA256).Hash.ToLowerInvariant()
    $anchorLength = (Get-Item -LiteralPath $anchorPackage).Length
    $externalBefore = Get-ProjectWorldTreeDigest -Paths @($externalRoot)
    $activeBefore = Get-ProjectWorldTreeDigest -Paths @($manifestRoot)

    $unchanged = Invoke-ProjectWorldPersistenceRun -Name '02-cold-reload-no-op' -Profile $authoredPath
    Assert-ProjectWorldPersistence -Condition (
        $unchanged.ExitCode -eq 0 -and $unchanged.Result.status -ceq 'accepted' -and
        [int]$unchanged.Result.changes.self_saved_actor_mutations -eq 0 -and
        [int]$unchanged.Result.changes.preserved_actors -ge 1) `
        -Message 'Cold reload did not preserve the existing authored-overlay actor as a no-op.'
    Assert-ProjectWorldPersistence -Condition (
        (Get-FileHash -LiteralPath $anchorPackage -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $anchorBefore -and
        (Get-Item -LiteralPath $anchorPackage).Length -eq $anchorLength -and
        (Get-ProjectWorldTreeDigest -Paths @($externalRoot)) -ceq $externalBefore -and
        (Get-ProjectWorldTreeDigest -Paths @($manifestRoot)) -ceq $activeBefore) `
        -Message 'Accepted unchanged regeneration rewrote the authored-overlay package.'

    $activeBeforeFailure = Get-ProjectWorldTreeDigest -Paths @($manifestRoot)
    $rejected = Invoke-ProjectWorldPersistenceRun `
        -Name '03-rejected-after-self-save' -Profile $changedAuthoredPath `
        -ExtraArguments @('-TestFailAfterSelfSavedActor')
    Assert-ProjectWorldPersistence -Condition (
        $rejected.ExitCode -ne 0 -and $rejected.Result.status -ceq 'rejected' -and
        @($rejected.Result.errors).Count -eq 1 -and
        [string]$rejected.Result.errors[0].code -ceq 'test-after-self-save' -and
        [int]$rejected.Result.changes.self_saved_actor_mutations -eq 1) `
        -Message 'Late failure did not occur after exactly one real external-actor self-save.'
    Assert-ProjectWorldPersistence -Condition (
        (Get-FileHash -LiteralPath $anchorPackage -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $anchorBefore -and
        (Get-Item -LiteralPath $anchorPackage).Length -eq $anchorLength -and
        (Get-ProjectWorldTreeDigest -Paths @($externalRoot)) -ceq $externalBefore) `
        -Message 'Rejected late mutation did not restore the exact prior external-actor tree.'
    Assert-ProjectWorldPersistence -Condition (
        (Get-ProjectWorldTreeDigest -Paths @($manifestRoot)) -ceq $activeBeforeFailure -and
        (Get-ProjectWorldTreeDigest -Paths @($durableActiveSet)) -ceq $durableAuthorityBefore) `
        -Message 'Rejected late mutation changed transient or durable active authority.'

    $auditPath = Join-Path $evidenceRoot '04-read-only-audit.json'
    $generatedRoots = @(
        (Join-Path $roots.ContentRoot 'Generated\Twin'),
        $externalRoot,
        (Join-Path $roots.ContentRoot '__ExternalObjects__\Generated\Twin\L_ProjectWorldLandscapeWaterTwin'),
        (Get-ProjectWorldPresentationRoot -ContentRoot $roots.ContentRoot)
    )
    $rootLiteral = '@(' + (($generatedRoots | ForEach-Object {
        "'$($_.Replace("'", "''"))'"
    }) -join ',') + ')'
    $auditCommand = "& '$audit' -ProjectRoot '$projectRoot' " +
        "-WorldDataPlugin 'ProjectWorldTestData' -ManifestRoot '$manifestRoot' " +
        "-GeneratedRoots $rootLiteral -EvidencePath '$auditPath'"
    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -Command $auditCommand | Out-Host
    Assert-ProjectWorldPersistence -Condition (
        $LASTEXITCODE -eq 0 -and
        (Get-Content -LiteralPath $auditPath -Raw | ConvertFrom-Json).status -ceq 'accepted') `
        -Message 'Read-only authority audit rejected the restored synthetic state.'

    Write-ProjectWorldJson -Path (Join-Path $evidenceRoot 'summary.json') -Document ([ordered]@{
        schema_version = 1
        status = 'accepted'
        world_data_plugin = 'ProjectWorldTestData'
        map_package = $mapPackage
        authored_overlay_id = 'persistence_marker'
        external_actor_package = $anchorPackage.Substring($projectRoot.Length + 1).Replace('\', '/')
        external_actor_sha256 = $anchorBefore
        external_actor_bytes = $anchorLength
        first_self_saved_actor_mutations = [int]$first.Result.changes.self_saved_actor_mutations
        cold_reload_self_saved_actor_mutations = 0
        rejected_self_saved_actor_mutations = 1
        rejected_error_code = 'test-after-self-save'
        external_tree_sha256 = $externalBefore
        active_authority_sha256 = $activeBefore
        read_only_audit = $auditPath.Substring($projectRoot.Length + 1).Replace('\', '/')
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
    $journalPath = Join-Path $manifestRoot 'journal.json'
    if (Test-Path -LiteralPath $journalPath -PathType Leaf) {
        $journal = Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
        Remove-ProjectWorldGeneratedSnapshot `
            -TransactionParent (Join-Path $projectRoot 'tmp\world\world_realization\transactions') `
            -TransactionRoot ([string]$journal.snapshot_root)
        Remove-Item -LiteralPath $journalPath -Force
    }
    if (Test-Path -LiteralPath $workRoot) {
        Remove-ProjectWorldGeneratedSnapshot `
            -TransactionParent $workParent `
            -TransactionRoot $workRoot
    }
    if ([string]::IsNullOrWhiteSpace($priorDelegatedToken)) {
        Remove-Item Env:ALIS_WORLD_CONTENT_LOCK_TOKEN -ErrorAction SilentlyContinue
    }
    if ($null -ne $contentLock) { $contentLock.Dispose() }
}

Write-Host "[ProjectWorldAuthoredOverlayPersistence] accepted: $(Join-Path $evidenceRoot 'summary.json')"
