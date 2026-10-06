# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [switch]$Record,
    [string]$Probe,
    [switch]$ProbeRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$worldRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $worldRoot))
if ($Record -and $Probe) { throw 'Record and Probe are mutually exclusive.' }
if ($Probe -and -not $ProbeRun) {
    $probeRoot = [System.IO.Path]::GetFullPath($Probe)
    $allowedRoot = Join-Path $projectRoot 'tmp\world\realization_verify'
    if (-not $probeRoot.StartsWith($allowedRoot + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'Twin probe output must be under tmp/world/realization_verify.'
    }
    New-Item -ItemType Directory -Force -Path $probeRoot | Out-Null
    foreach ($run in 1..2) { & $PSCommandPath -Probe $probeRoot -ProbeRun }
    foreach ($name in @('map', 'project_mesh_terrain', 'realization_pipeline')) {
        $projections = @(Get-ChildItem -LiteralPath $probeRoot -Filter "$name.*.projection.json" |
            Sort-Object LastWriteTime)
        $packages = @(Get-ChildItem -LiteralPath $probeRoot -Filter "$name.*.packages.json" |
            Sort-Object LastWriteTime)
        if ($projections.Count -ne 2 -or $packages.Count -ne 2) {
            throw "Twin probe expected two projections and package lists for $name."
        }
        $first = Get-Content -LiteralPath $projections[0].FullName -Raw | ConvertFrom-Json
        $second = Get-Content -LiteralPath $projections[1].FullName -Raw | ConvertFrom-Json
        if ($first.projection_sha256 -cne $second.projection_sha256) {
            throw "Twin probe projection changed for $name."
        }
        Write-Host "[ProjectWorldTwinVerify] $name projection equal: $($first.projection_sha256)"
    }
    $packageLists = @(Get-ChildItem -LiteralPath $probeRoot -Filter 'realization_pipeline.*.packages.json' |
        Sort-Object LastWriteTime)
    $firstPackages = Get-Content -LiteralPath $packageLists[0].FullName -Raw | ConvertFrom-Json
    $secondPackages = Get-Content -LiteralPath $packageLists[1].FullName -Raw | ConvertFrom-Json
    $firstValues = @($firstPackages.PSObject.Properties | ForEach-Object { "$($_.Name)=$($_.Value)" } | Sort-Object)
    $secondValues = @($secondPackages.PSObject.Properties | ForEach-Object { "$($_.Name)=$($_.Value)" } | Sort-Object)
    if (-not (Compare-Object $firstValues $secondValues)) {
        throw 'Twin probe expected different saved package bytes across fresh runs.'
    }
    Write-Host "[ProjectWorldTwinVerify] package bytes differ; probe: $probeRoot"
    return
}
. (Join-Path $worldRoot 'generated_content_transaction.ps1')
. (Join-Path $worldRoot 'generated_manifest.ps1')
. (Join-Path $worldRoot 'execution_envelope.ps1')
. (Join-Path $projectRoot 'scripts\config\Resolve-UEConfig.ps1')

$testData = Join-Path $projectRoot 'Plugins\World\ProjectWorldTestData\Data'
$presentation = Join-Path $testData 'Presentation\synthetic_representative_v1.json'
$runtime = Join-Path $testData 'Runtime\synthetic_territory_twin_v1.json'
$authored = Join-Path $testData 'Authored\synthetic_territory_twin_v1.json'
$realization = Join-Path $testData 'Profiles\Realization\synthetic_territory_twin.realization.json'
$realizationDocument = Get-Content -LiteralPath $realization -Raw | ConvertFrom-Json
if ([string]$realizationDocument.world_data_plugin -cne 'ProjectWorldTestData') {
    throw 'Twin verify can only realize into ProjectWorldTestData.'
}
$mapPackage = [string]$realizationDocument.map_package
$roots = Resolve-ProjectWorldDataRoots -ProjectRoot $projectRoot -PluginName 'ProjectWorldTestData'
$runId = [System.Guid]::NewGuid().ToString('N')
$evidenceRoot = Join-Path $projectRoot "Saved\Validation\WorldRealization\verify-twin\$runId"
$manifestRoot = Join-Path $evidenceRoot 'manifests'
$workParent = Join-Path $projectRoot 'tmp\world\realization_verify\twin'
$workRoot = Join-Path $workParent $runId
$snapshotRoot = Join-Path $workRoot 'outer-snapshot'
New-Item -ItemType Directory -Force -Path $evidenceRoot, $workRoot | Out-Null
$compileScript = Join-Path $PSScriptRoot 'compile_twin_fixture.py'
$compileResult = & python $compileScript --output-root (Join-Path $workRoot 'canonical')
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $compileResult -PathType Leaf)) {
    throw 'Twin fixture compilation failed.'
}
$layerPaths = @($realizationDocument.layers | ForEach-Object {
    $relative = ([string]$_.artifact_root).Substring($roots.MountRoot.Length).Replace(
        '/', [System.IO.Path]::DirectorySeparatorChar).TrimEnd('\', '/')
    Join-Path $roots.ContentRoot $relative
})
$contentLock = $null
$priorDelegation = $null
$snapshotRecords = @()
try {
    $contentLock = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
    $priorDelegation = Enable-ProjectGeneratedContentLockDelegation -Lock $contentLock
    $snapshotRecords = @(New-ProjectWorldGeneratedSnapshot `
        -ContentRoot $roots.ContentRoot -MapPackage $mapPackage `
        -GeneratedPackageRoot $roots.GeneratedPackageRoot `
        -SnapshotRoot $snapshotRoot -AdditionalPaths $layerPaths)
    Remove-ProjectWorldGeneratedPaths -ContentRoot $roots.ContentRoot `
        -MapPackage $mapPackage -GeneratedPackageRoot $roots.GeneratedPackageRoot

    $shell = (Get-Process -Id $PID).Path
    $applyArguments = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $worldRoot 'realize_canonical_world.ps1'),
        '-CompileResult', $compileResult,
        '-Mode', 'Apply', '-Map', $mapPackage,
        '-PresentationProfile', $presentation,
        '-RuntimeProfile', $runtime,
        '-AuthoredOverlayProfile', $authored,
        '-RealizationProfile', $realization,
        '-ManifestRoot', $manifestRoot,
        '-EvidencePath', (Join-Path $evidenceRoot 'apply.json'),
        '-MaxRoads', '0', '-MaxBuildings', '0',
        '-EnrollManifests', '-NonInteractive'
    )
    & $shell @applyArguments | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Twin realization failed; evidence: $(Join-Path $evidenceRoot 'apply.json')"
    }

    $config = Resolve-UEConfig -ConfigDir (Join-Path $projectRoot 'scripts\config')
    $editor = Join-Path $config.UE_PATH 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $verifyResult = Join-Path $evidenceRoot 'verify.json'
    $verifyLog = Join-Path $evidenceRoot 'verify.log'
    $arguments = @(
        (Join-Path $projectRoot 'Alis.uproject'),
        '-run=ProjectWorldTwinVerify',
        "-Map=$mapPackage",
        "-CompileResult=$compileResult",
        "-RealizationProfile=$realization",
        "-PresentationProfile=$presentation",
        "-RuntimeProfile=$runtime",
        "-AuthoredOverlayProfile=$authored",
        "-ManifestRoot=$manifestRoot",
        "-Result=$verifyResult",
        '-EnablePlugins=ProjectWorldTestData',
        "-abslog=$verifyLog", '-unattended', '-nop4', '-nosplash',
        '-FullStdOutLogOutput'
    )
    $arguments += Get-ProjectWorldExecutionEnvelopeArguments -Rendering Required
    if ($Record) { $arguments += '-ProjectWorldVerifyRecord' }
    if ($Probe) { $arguments += "-ProjectWorldVerifyProbe=$($Probe.Replace('\', '/'))" }
    Assert-ProjectWorldExecutionEnvelope -Rendering Required -Arguments $arguments
    $consoleLog = Join-Path $evidenceRoot 'verify.console.log'
    & $editor @arguments *> $consoleLog
    if ($LASTEXITCODE -ne 0) {
        Get-Content -LiteralPath $consoleLog -Tail 12 | Out-Host
        throw "Twin verify failed; evidence: $verifyResult"
    }
    Write-Host "[ProjectWorldTwinVerify] accepted: $verifyResult"
}
finally {
    if ($snapshotRecords.Count -gt 0) {
        Restore-ProjectWorldGeneratedSnapshot `
            -ContentRoot $roots.ContentRoot -MapPackage $mapPackage `
            -GeneratedPackageRoot $roots.GeneratedPackageRoot -Records $snapshotRecords
    }
    if (Test-Path -LiteralPath $workRoot) {
        Remove-ProjectWorldGeneratedSnapshot `
            -TransactionParent $workParent -TransactionRoot $workRoot
    }
    if ($null -ne $contentLock) {
        Disable-ProjectGeneratedContentLockDelegation -Prior $priorDelegation
        $contentLock.Dispose()
    }
}
