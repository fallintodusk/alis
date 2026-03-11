# Clean all project and plugin artifacts
# Usage: .\clean_all.ps1 [-Verbose]

param(
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

# Script location
$ScriptDir = Split-Path -Parent $PSScriptRoot

Write-Host "Cleaning all project artifacts..." -ForegroundColor Cyan
Write-Host ""

# Call clean_plugins.ps1
$CleanPluginsScript = Join-Path $PSScriptRoot "clean_plugins.ps1"
if (Test-Path $CleanPluginsScript) {
    if ($Verbose) {
        & $CleanPluginsScript -Verbose
    } else {
        & $CleanPluginsScript
    }
} else {
    Write-Host "[ERROR] clean_plugins.ps1 not found at: $CleanPluginsScript" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Call clean_project.ps1
$CleanProjectScript = Join-Path $PSScriptRoot "clean_project.ps1"
if (Test-Path $CleanProjectScript) {
    if ($Verbose) {
        & $CleanProjectScript -Verbose
    } else {
        & $CleanProjectScript
    }
} else {
    Write-Host "[ERROR] clean_project.ps1 not found at: $CleanProjectScript" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "All artifacts cleaned successfully!" -ForegroundColor Green

exit 0
