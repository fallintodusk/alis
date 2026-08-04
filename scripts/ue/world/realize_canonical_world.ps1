# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CompileResult,

    [ValidateSet("Validate", "Apply", "Delete")]
    [string]$Mode = "Validate",

    [string]$Map = "/ProjectWorld/Generated/P0/L_ProjectWorldSynthetic",

	[ValidateRange(0, 1000)]
	[int]$MaxRoads = 1,

	[ValidateRange(0, 1000)]
	[int]$MaxBuildings = 4,

    [switch]$RequireLandscape
)

$ErrorActionPreference = "Stop"
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $scriptDirectory))
$configDirectory = Join-Path $projectRoot "scripts\config"
. (Join-Path $configDirectory "Resolve-UEConfig.ps1")
$config = Resolve-UEConfig -ConfigDir $configDirectory

$editorCommand = Join-Path $config.UE_PATH "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$projectFile = Join-Path $projectRoot "Alis.uproject"
$compileResultPath = [System.IO.Path]::GetFullPath($CompileResult)
if (-not (Test-Path -LiteralPath $compileResultPath -PathType Leaf)) {
    throw "Compile result does not exist: $compileResultPath"
}
if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
    throw "UnrealEditor-Cmd.exe does not exist under the configured UE_PATH."
}

$receiptHash = (Get-FileHash -LiteralPath $compileResultPath -Algorithm SHA256).Hash.ToLowerInvariant()
$evidenceDirectory = Join-Path $projectRoot "Saved\Validation\WorldRealization\$receiptHash"
New-Item -ItemType Directory -Path $evidenceDirectory -Force | Out-Null
$modeName = $Mode.ToLowerInvariant()
$resultPath = Join-Path $evidenceDirectory "$modeName.json"
$logPath = Join-Path $evidenceDirectory "$modeName.log"
if (Test-Path -LiteralPath $resultPath -PathType Leaf) {
    Remove-Item -LiteralPath $resultPath -Force
}

$unrealArguments = @(
    $projectFile
    "-run=ProjectWorldRealize"
    "-CompileResult=$compileResultPath"
    "-Result=$resultPath"
    "-Mode=$modeName"
    "-Map=$Map"
	"-MaxRoads=$MaxRoads"
	"-MaxBuildings=$MaxBuildings"
    "-abslog=$logPath"
    "-unattended"
    "-nop4"
    "-nosplash"
    "-NullRHI"
    "-FullStdOutLogOutput"
)
if ($RequireLandscape) {
    $unrealArguments += "-RequireLandscape"
}

Write-Host "[WorldRealization] mode=$modeName"
Write-Host "[WorldRealization] input=$compileResultPath"
Write-Host "[WorldRealization] evidence=$resultPath"
& $editorCommand @unrealArguments | Out-Null
$engineExitCode = $LASTEXITCODE
if (-not (Test-Path -LiteralPath $resultPath -PathType Leaf)) {
    throw "World realization emitted no structured result. See $logPath"
}

$result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
Write-Host "[WorldRealization] status=$($result.status) engine_exit=$engineExitCode"
Write-Host "[WorldRealization] roundtrip_m=$($result.coordinate_roundtrip_error_m)"
if ($engineExitCode -ne 0) {
    exit $engineExitCode
}
if ($result.status -ne "accepted") {
    exit 4
}
exit 0
