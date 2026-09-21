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
    [double]$SubjectToleranceCentimeters = 200.0
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

$process = Start-Process -FilePath $executablePath -ArgumentList $arguments `
    -WorkingDirectory (Split-Path -Parent $executablePath) -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
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

Write-Host "[OK] Packaged fixed-view evidence: $resultPath"
Write-Host "[OK] Screenshot: $screenshotPath"
if (Test-Path -LiteralPath $logPath -PathType Leaf) {
    Write-Host "[OK] Runtime log: $logPath"
}
else {
    Write-Host "[i] This Shipping target does not emit a runtime log; the receipt and screenshot remain authoritative."
}
