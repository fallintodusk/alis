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

    [string]$PresentationProfile = "",

	[string]$RuntimeProfile = "",

    [string]$EvidencePath = "",

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
. (Join-Path $scriptDirectory "generated_content_transaction.ps1")
$config = Resolve-UEConfig -ConfigDir $configDirectory

$editorCommand = Join-Path $config.UE_PATH "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$projectFile = Join-Path $projectRoot "Alis.uproject"
$compileResultPath = [System.IO.Path]::GetFullPath($CompileResult)
$modeName = $Mode.ToLowerInvariant()
$presentationProfilePath = if ($modeName -eq "delete") {
    ""
}
elseif ([string]::IsNullOrWhiteSpace($PresentationProfile)) {
    Join-Path $projectRoot "Plugins\World\ProjectWorld\Data\Presentation\kazan_representative_v1.json"
}
else {
    [System.IO.Path]::GetFullPath($PresentationProfile)
}
$runtimeProfilePath = if ($modeName -eq "delete" -or [string]::IsNullOrWhiteSpace($RuntimeProfile)) {
	""
}
else {
	[System.IO.Path]::GetFullPath($RuntimeProfile)
}
if (-not (Test-Path -LiteralPath $compileResultPath -PathType Leaf)) {
    throw "Compile result does not exist: $compileResultPath"
}
if ($modeName -ne "delete" -and -not (Test-Path -LiteralPath $presentationProfilePath -PathType Leaf)) {
    throw "Presentation profile does not exist: $presentationProfilePath"
}
if (-not [string]::IsNullOrWhiteSpace($runtimeProfilePath) -and
	-not (Test-Path -LiteralPath $runtimeProfilePath -PathType Leaf)) {
	throw "Runtime profile does not exist: $runtimeProfilePath"
}
if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
    throw "UnrealEditor-Cmd.exe does not exist under the configured UE_PATH."
}

$compileResultHash = (Get-FileHash -LiteralPath $compileResultPath -Algorithm SHA256).Hash.ToLowerInvariant()
$presentationProfileHash = if ($modeName -eq "delete") {
    "none"
}
else {
    (Get-FileHash -LiteralPath $presentationProfilePath -Algorithm SHA256).Hash.ToLowerInvariant()
}
$runtimeProfileHash = if ([string]::IsNullOrWhiteSpace($runtimeProfilePath)) {
	"none"
}
else {
	(Get-FileHash -LiteralPath $runtimeProfilePath -Algorithm SHA256).Hash.ToLowerInvariant()
}
if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $identity = [ordered]@{
        schema_version = 1
        compile_result_sha256 = $compileResultHash
        presentation_profile_sha256 = $presentationProfileHash
		runtime_profile_sha256 = $runtimeProfileHash
        map = $Map
        max_roads = $MaxRoads
        max_buildings = $MaxBuildings
        require_landscape = [bool]$RequireLandscape
    }
    $identityBytes = [System.Text.Encoding]::UTF8.GetBytes(($identity | ConvertTo-Json -Compress))
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $invocationHash = ([System.BitConverter]::ToString($sha256.ComputeHash($identityBytes))).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
    $evidenceDirectory = Join-Path $projectRoot "Saved\Validation\WorldRealization\$invocationHash"
    $resultPath = Join-Path $evidenceDirectory "$modeName.json"
}
else {
    $resultPath = [System.IO.Path]::GetFullPath($EvidencePath)
    $evidenceDirectory = Split-Path -Parent $resultPath
}
$evidenceRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "Saved\Validation\WorldRealization"))
$evidencePrefix = $evidenceRoot + [System.IO.Path]::DirectorySeparatorChar
if (-not $resultPath.StartsWith($evidencePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Evidence path must stay under $evidenceRoot"
}
New-Item -ItemType Directory -Path $evidenceDirectory -Force | Out-Null
$logPath = [System.IO.Path]::ChangeExtension($resultPath, ".log")
if (Test-Path -LiteralPath $resultPath -PathType Leaf) {
    Remove-Item -LiteralPath $resultPath -Force
}

$contentRoot = Join-Path $projectRoot "Plugins\World\ProjectWorld\Content"
$transactionParent = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "tmp\world\world_realization\transactions"))
$transactionRoot = Join-Path $transactionParent ([System.Guid]::NewGuid().ToString("N"))
$transactionRecords = @()
$transactionActive = $modeName -eq "apply" -or $modeName -eq "delete"
if ($transactionActive) {
    $transactionRecords = @(New-ProjectWorldGeneratedSnapshot `
        -ContentRoot $contentRoot `
        -MapPackage $Map `
        -SnapshotRoot $transactionRoot)
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
if ($modeName -ne "delete") {
    $unrealArguments += "-PresentationProfile=$presentationProfilePath"
}
if (-not [string]::IsNullOrWhiteSpace($runtimeProfilePath)) {
	$unrealArguments += "-RuntimeProfile=$runtimeProfilePath"
}
if ($RequireLandscape) {
    $unrealArguments += "-RequireLandscape"
}

Write-Host "[WorldRealization] mode=$modeName"
Write-Host "[WorldRealization] input=$compileResultPath"
Write-Host "[WorldRealization] presentation=$($presentationProfilePath.Trim())"
Write-Host "[WorldRealization] runtime=$($runtimeProfilePath.Trim())"
Write-Host "[WorldRealization] evidence=$resultPath"
$engineExitCode = -1
$result = $null
$childStatus = "missing"
$invocationFailure = $null
try {
    & $editorCommand @unrealArguments | Out-Null
    $engineExitCode = $LASTEXITCODE
}
catch {
    $invocationFailure = $_
}
if (Test-Path -LiteralPath $resultPath -PathType Leaf) {
    try {
        $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
        $childStatus = [string]$result.status
    }
    catch {
        if ($null -eq $invocationFailure) {
            $invocationFailure = $_
        }
        $childStatus = "invalid"
    }
}
elseif ($null -eq $invocationFailure) {
    $invocationFailure = [System.InvalidOperationException]::new(
        "World realization emitted no structured result. See $logPath")
}

if ($transactionActive) {
    $transactionResult = Complete-ProjectWorldGeneratedTransaction `
        -ContentRoot $contentRoot `
        -MapPackage $Map `
        -Records $transactionRecords `
        -TransactionParent $transactionParent `
        -TransactionRoot $transactionRoot `
        -ResultPath $resultPath `
        -EngineExitCode $engineExitCode `
        -ChildStatus $childStatus
    Write-Host "[WorldRealization] transaction=$($transactionResult.State)"
}
if ($null -ne $invocationFailure) {
    throw $invocationFailure
}

Write-Host "[WorldRealization] child_status=$($result.status) engine_exit=$engineExitCode"
Write-Host "[WorldRealization] roundtrip_m=$($result.coordinate_roundtrip_error_m)"
if ($engineExitCode -ne 0) {
    exit $engineExitCode
}
if ($result.status -ne "accepted") {
    exit 4
}
exit 0
