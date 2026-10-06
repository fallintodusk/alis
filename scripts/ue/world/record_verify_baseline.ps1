# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

<#
.SYNOPSIS
Records or probes the baseline of one World producer verify.

.DESCRIPTION
Record mode is the only writer of Data/TestFixtures/Verify/<generator_id>.verify.json. It runs the
exact verify in a cold editor with -ProjectWorldVerifyRecord and succeeds only when the log carries
the verify's record line. The verify itself refuses to record an unchanged verification identity,
so a refused record exits non-zero with the baseline untouched.

-Probe runs the verify twice in fresh cold editors with -ProjectWorldVerifyProbe=<dir>, compares the
two output projections and the package digests, and writes no baseline. Equal projections with
different package bytes is the expected result.

Ordinary test runs and the common checks never pass either switch.

.EXAMPLE
scripts/ue/world/record_verify_baseline.ps1 -Verify Water
scripts/ue/world/record_verify_baseline.ps1 -Verify Road -Probe
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Z][A-Za-z0-9]*$')]
    [string]$Verify,

    [switch]$Probe,

    [int]$TimeoutSeconds = 900
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$runId = Get-Date -Format 'yyyyMMdd-HHmmss'
$evidenceRoot = Join-Path $repoRoot "tmp\world\realization_verify\record\$Verify-$runId"
New-Item -ItemType Directory -Force -Path $evidenceRoot | Out-Null

if ($Verify -eq 'Twin') {
    # Map, terrain, and pipeline share one twin realization and are recorded by its script.
    $twin = Join-Path $repoRoot 'scripts\ue\world\test\integration\verify_twin_realization.ps1'
    if (-not (Test-Path -LiteralPath $twin -PathType Leaf)) {
        throw "The twin verify script is missing: $twin"
    }
    if ($Probe) {
        & $twin -Probe (Join-Path $evidenceRoot 'probe')
    } else {
        & $twin -Record
    }
    exit $LASTEXITCODE
}

$testName = "Project.World.Realization.Verify.$Verify"
$runner = Join-Path $repoRoot 'scripts\ue\test\unit\run_cpp_tests_safe.ps1'
$step = if ($env:OVERNIGHT_STEP) { $env:OVERNIGHT_STEP } else { 'manual' }
$runnerLog = Join-Path $repoRoot "scripts\ue\artifacts\overnight\step-$step\tests.log"

function Invoke-ColdVerify {
    param(
        [Parameter(Mandatory = $true)][string]$Switch,
        [Parameter(Mandatory = $true)][string]$Marker,
        [Parameter(Mandatory = $true)][string]$LogName
    )

    # Naming the expected test forces the cold path: a warm editor would run without the switch.
    & $runner -TestFilter $testName -ExpectedTestNames @($testName) -RequiredLogPatterns @($Marker) `
        -ExtraArgs $Switch -TimeoutSeconds $TimeoutSeconds | Out-Host
    $exitCode = $LASTEXITCODE
    $copy = Join-Path $evidenceRoot $LogName
    if (Test-Path -LiteralPath $runnerLog -PathType Leaf) {
        Copy-Item -LiteralPath $runnerLog -Destination $copy -Force
    }
    $lines = if (Test-Path -LiteralPath $copy -PathType Leaf) {
        @(Select-String -LiteralPath $copy -Pattern '\[ProjectWorldVerify\]|record refused|nothing to record|identity differs|output differs' |
            ForEach-Object { $_.Line.Trim() })
    } else { @() }
    foreach ($line in $lines) { Write-Host "  $line" }
    return [pscustomobject]@{ ExitCode = $exitCode; Log = $copy }
}

function Get-ProbeRecords {
    param([Parameter(Mandatory = $true)][string]$Path)

    $records = @{}
    foreach ($record in @((Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json).records)) {
        $fields = @{}
        foreach ($field in $record.fields.PSObject.Properties) { $fields[$field.Name] = [string]$field.Value }
        $records["$($record.kind):$($record.key)"] = $fields
    }
    return $records
}

if (-not $Probe) {
    $marker = [regex]::Escape("[ProjectWorldVerify] Recorded baseline - verify=$testName ")
    $result = Invoke-ColdVerify -Switch '-ProjectWorldVerifyRecord' -Marker $marker -LogName 'record.log'
    if ($result.ExitCode -ne 0) {
        Write-Host "Record did not complete for $testName (exit $($result.ExitCode)); log: $($result.Log)" -ForegroundColor Red
        exit $result.ExitCode
    }
    Write-Host "Recorded $testName; log: $($result.Log)" -ForegroundColor Green
    & git -C $repoRoot status --short -- 'Plugins/*/*/Data/TestFixtures/Verify'
    & git -C $repoRoot diff --stat -- 'Plugins/*/*/Data/TestFixtures/Verify'
    exit 0
}

$probeRoot = Join-Path $evidenceRoot 'probe'
New-Item -ItemType Directory -Force -Path $probeRoot | Out-Null
$marker = [regex]::Escape("[ProjectWorldVerify] Probe written - verify=$testName ")
foreach ($run in 1..2) {
    $result = Invoke-ColdVerify -Switch "-ProjectWorldVerifyProbe=$($probeRoot.Replace('\', '/'))" `
        -Marker $marker -LogName "probe-$run.log"
    if ($result.ExitCode -ne 0) {
        Write-Host "Probe run $run failed for $testName (exit $($result.ExitCode)); log: $($result.Log)" -ForegroundColor Red
        exit $result.ExitCode
    }
}

$projections = @(Get-ChildItem -LiteralPath $probeRoot -Filter '*.projection.json' | Sort-Object LastWriteTime)
$packages = @(Get-ChildItem -LiteralPath $probeRoot -Filter '*.packages.json' | Sort-Object LastWriteTime)
if ($projections.Count -ne 2 -or $packages.Count -ne 2) {
    throw "Expected two probe projections and two package lists under $probeRoot."
}

$first = Get-ProbeRecords -Path $projections[0].FullName
$second = Get-ProbeRecords -Path $projections[1].FullName
$fieldDifferences = [System.Collections.Generic.List[string]]::new()
foreach ($key in @(@($first.Keys) + @($second.Keys) | Sort-Object -Unique)) {
    if (-not $first.ContainsKey($key) -or -not $second.ContainsKey($key)) {
        $fieldDifferences.Add("$key (record present in one run)")
        continue
    }
    foreach ($name in @(@($first[$key].Keys) + @($second[$key].Keys) | Sort-Object -Unique)) {
        if ($first[$key][$name] -cne $second[$key][$name]) { $fieldDifferences.Add("$key.$name") }
    }
}

$firstPackages = Get-Content -LiteralPath $packages[0].FullName -Raw | ConvertFrom-Json
$secondPackages = Get-Content -LiteralPath $packages[1].FullName -Raw | ConvertFrom-Json
$names = @(@($firstPackages.PSObject.Properties.Name) + @($secondPackages.PSObject.Properties.Name) | Sort-Object -Unique)
$byteDifferences = @($names | Where-Object {
    $a = $firstPackages.PSObject.Properties[$_]
    $b = $secondPackages.PSObject.Properties[$_]
    $null -eq $a -or $null -eq $b -or [string]$a.Value -cne [string]$b.Value
})

if ($fieldDifferences.Count -eq 0) {
    Write-Host "projection equal ($($first.Count) records)" -ForegroundColor Green
} else {
    Write-Host "projection different ($($fieldDifferences.Count) fields): $($fieldDifferences -join ', ')" -ForegroundColor Red
}
Write-Host ("bytes {0} ({1} of {2} packages differ)" -f $(if ($byteDifferences.Count -eq 0) { 'equal' } else { 'different' }),
    $byteDifferences.Count, $names.Count)
Write-Host "Probe evidence: $probeRoot"
exit $(if ($fieldDifferences.Count -eq 0) { 0 } else { 1 })
