# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [string]$Map = "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory",

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string[]]$RuntimeProfiles,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$SelectedProfile,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$EvidencePath
)

$ErrorActionPreference = "Stop"
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $scriptDirectory))
$configDirectory = Join-Path $projectRoot "scripts\config"
. (Join-Path $configDirectory "Resolve-UEConfig.ps1")
$config = Resolve-UEConfig -ConfigDir $configDirectory

$editorCommand = Join-Path $config.UE_PATH "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$projectFile = Join-Path $projectRoot "Alis.uproject"
if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
    throw "Launcher UnrealEditor-Cmd does not exist: $editorCommand"
}

$profilePaths = @($RuntimeProfiles | ForEach-Object {
    $path = [System.IO.Path]::GetFullPath($_)
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Runtime profile does not exist: $path"
    }
    $path
})
if ($profilePaths.Count -eq 0) {
    throw "At least one runtime profile is required."
}

$resultPath = [System.IO.Path]::GetFullPath($EvidencePath)
$evidenceRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $projectRoot "Saved\Validation\WorldRealization"))
if (-not $resultPath.StartsWith(
    $evidenceRoot + [System.IO.Path]::DirectorySeparatorChar,
    [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Evidence must stay under $evidenceRoot"
}

$resultDirectory = Split-Path -Parent $resultPath
[System.IO.Directory]::CreateDirectory($resultDirectory) | Out-Null
if (Test-Path -LiteralPath $resultPath -PathType Leaf) {
    Remove-Item -LiteralPath $resultPath -Force
}

$joinedProfiles = $profilePaths -join "|"
$arguments = @(
    $projectFile,
    "-run=ProjectWorldStaticPartitionAudit",
    "-Map=$Map",
    "-RuntimeProfiles=$joinedProfiles",
    "-SelectedProfile=$SelectedProfile",
    "-Result=$resultPath",
    "-unattended",
    "-nop4",
    "-nosplash",
    "-NullRHI"
)

Write-Host "World static partition audit"
Write-Host "ENGINE           = $($config.UE_PATH)"
Write-Host "MAP              = $Map"
Write-Host "PROFILES         = $($profilePaths.Count)"
Write-Host "SELECTED_PROFILE = $SelectedProfile"
Write-Host "EVIDENCE         = $resultPath"

& $editorCommand @arguments
$exitCode = $LASTEXITCODE
if (-not (Test-Path -LiteralPath $resultPath -PathType Leaf)) {
    throw "Static partition audit emitted no receipt (exit=$exitCode)."
}
$receipt = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
if ($exitCode -ne 0 -or [string]$receipt.status -ne "accepted") {
    throw "Static partition audit rejected (exit=$exitCode status=$($receipt.status)). See $resultPath"
}

Write-Host "[OK] Static partition audit accepted the selected profile."
Write-Host "Receipt: $resultPath"
