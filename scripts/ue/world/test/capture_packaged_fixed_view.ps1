# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][string]$Experience,
    [Parameter(Mandatory = $true)][string]$Map,
    [Parameter(Mandatory = $true)][string]$PlayerLocation,
    [Parameter(Mandatory = $true)][string]$LookAtLocation,
    [Parameter(Mandatory = $true)][string]$SubjectLocation,
    [Parameter(Mandatory = $true)][string]$SubjectClass,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [int]$TimeoutSeconds = 240,
    [double]$SubjectToleranceCentimeters = 200.0,
    [string]$FixtureMesh = '',
    [string]$FixtureMaterial = '',
    [string]$FixtureStartLocation = '',
    [string]$FixtureScale = '1,1,1',
    [string]$FixtureStartRotation = '0,0,0',
    [string]$FixtureFinalRotation = '0,0,0',
    [string[]]$ScalarOverrides = @(),
    [string]$ScalarMaterial = '',
    [double]$ScalarRadiusCentimeters = 20000.0,
    [switch]$BaseColor
)

$ErrorActionPreference = 'Stop'
$executablePath = [System.IO.Path]::GetFullPath($Executable)
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw "Packaged executable does not exist: $executablePath"
}
if ($outputRoot -match '\s') {
    throw "Fixed-view output path must not contain spaces: $outputRoot"
}
$normalizedScalarOverrides = @()
$scalarNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($override in $ScalarOverrides) {
    if ($override -notmatch '^([A-Za-z0-9_-]+):([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)$') {
        throw "Scalar override must be ParameterName:FiniteNumber: $override"
    }
    $parameterName = $Matches[1]
    $scalarValue = [double]::Parse($Matches[2], [System.Globalization.CultureInfo]::InvariantCulture)
    if ([double]::IsNaN($scalarValue) -or [double]::IsInfinity($scalarValue) -or
        [math]::Abs($scalarValue) -gt [single]::MaxValue) {
        throw "Scalar override value is outside the finite float range: $override"
    }
    if (-not $scalarNames.Add($parameterName)) {
        throw "Scalar parameter is specified more than once: $parameterName"
    }
    $normalizedScalarOverrides += "${parameterName}:$($Matches[2])"
}
if ($ScalarMaterial -and ($normalizedScalarOverrides.Count -eq 0 -or
    $ScalarMaterial -notmatch '^/' -or $ScalarRadiusCentimeters -le 0)) {
    throw 'ScalarMaterial requires at least one override, an asset path, and a positive radius.'
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$operationId = 'fixed_view_' + (Get-Date).ToUniversalTime().ToString('yyyyMMdd_HHmmss_fff')
$resultPath = Join-Path $outputRoot 'fixed-view.json'
$screenshotPath = Join-Path $outputRoot 'fixed-view.png'
$logPath = Join-Path $outputRoot 'fixed-view.log'
Remove-Item -LiteralPath $resultPath, $screenshotPath, $logPath -Force -ErrorAction SilentlyContinue

$arguments = @(
    "-ProjectMenuPlayAutoExperience=$Experience",
    '-ProjectMenuPlayAutoMode=SinglePlayer',
    '-ProjectWorldFixedViewGate',
    "-ProjectWorldFixedViewOperation=$operationId",
    "-ProjectWorldFixedViewResult=$resultPath",
    "-ProjectWorldFixedViewScreenshot=$screenshotPath",
    "-ProjectWorldFixedViewMap=$Map",
    "-ProjectWorldFixedViewPlayer=$PlayerLocation",
    "-ProjectWorldFixedViewLookAt=$LookAtLocation",
    "-ProjectWorldFixedViewSubject=$SubjectLocation",
    "-ProjectWorldFixedViewSubjectClass=$SubjectClass",
    "-ProjectWorldFixedViewTolerance=$SubjectToleranceCentimeters",
    '-ResX=1920', '-ResY=1080', '-Windowed', '-ForceRes',
    '-RenderOffScreen', '-unattended', '-nosplash', '-NoSound', '-NoMessaging',
    "-abslog=$logPath"
)
if ($BaseColor) {
    $arguments += '-ProjectWorldFixedViewBaseColor'
}
if ($FixtureMesh -or $FixtureMaterial -or $FixtureStartLocation) {
    if (-not $FixtureMesh -or -not $FixtureMaterial -or -not $FixtureStartLocation) {
        throw 'FixtureMesh, FixtureMaterial, and FixtureStartLocation must be supplied together.'
    }
    $arguments += "-ProjectWorldFixedViewFixtureMesh=$FixtureMesh"
    $arguments += "-ProjectWorldFixedViewFixtureMaterial=$FixtureMaterial"
    $arguments += "-ProjectWorldFixedViewFixtureStart=$FixtureStartLocation"
    $arguments += "-ProjectWorldFixedViewFixtureScale=$FixtureScale"
    $arguments += "-ProjectWorldFixedViewFixtureStartRotation=$FixtureStartRotation"
    $arguments += "-ProjectWorldFixedViewFixtureFinalRotation=$FixtureFinalRotation"
}
if ($normalizedScalarOverrides.Count -gt 0) {
    $arguments += "-ProjectWorldFixedViewScalars=$($normalizedScalarOverrides -join ';')"
    if ($ScalarMaterial) {
        $arguments += "-ProjectWorldFixedViewScalarMaterial=$ScalarMaterial"
        $arguments += "-ProjectWorldFixedViewScalarRadius=$ScalarRadiusCentimeters"
    }
}

$process = Start-Process -FilePath $executablePath -ArgumentList $arguments `
    -WorkingDirectory (Split-Path -Parent $executablePath) -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    $process.WaitForExit()
    throw "Packaged fixed-view proof exceeded ${TimeoutSeconds}s. Log: $logPath"
}
if (-not (Test-Path -LiteralPath $resultPath -PathType Leaf)) {
    throw "Packaged fixed-view proof produced no receipt. Exit=$($process.ExitCode) Log=$logPath"
}
$receipt = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
if ($process.ExitCode -ne 0 -or [string]$receipt.status -cne 'accepted' -or
    [string]$receipt.operation_id -cne $operationId -or [string]$receipt.map_package -cne $Map -or
    -not (Test-Path -LiteralPath $screenshotPath -PathType Leaf)) {
    throw "Packaged fixed-view proof rejected. Exit=$($process.ExitCode) Code=$($receipt.error_code) Message=$($receipt.error_message) Log=$logPath"
}
if ($normalizedScalarOverrides.Count -gt 0) {
    if (@($receipt.scalar_overrides).Count -ne $normalizedScalarOverrides.Count) {
        throw "Scalar override receipt count does not match the request. Log=$logPath"
    }
    foreach ($override in $normalizedScalarOverrides) {
        $parts = $override.Split(':', 2)
        $received = @($receipt.scalar_overrides | Where-Object { [string]$_.parameter -ceq $parts[0] })
        if ($received.Count -ne 1 -or
            [math]::Abs([double]$received[0].value - [double]::Parse($parts[1], [System.Globalization.CultureInfo]::InvariantCulture)) -gt 0.00001) {
            throw "Scalar override receipt does not match the request for $($parts[0]). Log=$logPath"
        }
    }
}
if ($ScalarMaterial -and [int]$receipt.scalar_override_slot_count -le 0) {
    throw "Scalar override receipt matched no material slots. Log=$logPath"
}

Write-Host "[OK] Packaged fixed-view evidence: $resultPath"
Write-Host "[OK] Screenshot: $screenshotPath"
if (Test-Path -LiteralPath $logPath -PathType Leaf) {
    Write-Host "[OK] Runtime log: $logPath"
}
else {
    Write-Host "[i] This Shipping target does not emit a runtime log; the receipt and screenshot remain authoritative."
}
