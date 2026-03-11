#Requires -Version 5.1
<#
.SYNOPSIS
    Build AlisEditor target for standalone game testing

.DESCRIPTION
    Builds the AlisEditor target using configuration from scripts/config/ue_path.conf.
    Produces UnrealEditor-Alis.dll for fast iterative testing with UnrealEditor.exe -game.
    Supports incremental builds by default (fast!).

.PARAMETER Clean
    Clean all binaries and intermediate files before building (slow, full rebuild)

.EXAMPLE
    .\build.ps1
    Incremental build (fast, default)

.EXAMPLE
    .\build.ps1 -Clean
    Clean + full rebuild (slow, use when needed)
#>

param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# ============================================================
# Configuration (Single Source of Truth: scripts/config/ue_path.conf)
# ============================================================

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ConfigFile = Join-Path (Split-Path -Parent (Split-Path -Parent $ScriptDir)) "config\ue_path.conf"

# Default fallbacks (non-UE)
$DefaultTarget = "AlisEditor"
$DefaultConfig = "Development"
$DefaultPlatform = "Win64"

# ============================================================
# Load Configuration from File
# ============================================================

$UEPath = $null
$Target = $null
$BuildConfig = $null
$Platform = $null

if (Test-Path $ConfigFile) {
    # Support both shell (=) and Make (:=) syntax for ue_path.conf
    Get-Content $ConfigFile | Where-Object { $_ -match '^([A-Z_]+)\s*:?=\s*(.+)$' } | ForEach-Object {
        $key = $matches[1]
        $value = $matches[2].Trim().Trim('"', "'")

        switch ($key) {
            "UE_PATH" { $UEPath = $value }
            "BUILD_TARGET" { $Target = $value }
            "BUILD_CONFIG" { $BuildConfig = $value }
            "BUILD_PLATFORM" { $Platform = $value }
        }
    }
}

# Require UE_PATH from config
if (-not $UEPath) {
    Write-Host "[ERROR] UE_PATH not found in config: $ConfigFile" -ForegroundColor Red
    Write-Host "Set UE_PATH in scripts\\config\\ue_path.conf." -ForegroundColor Yellow
    exit 1
}

# Apply defaults for non-UE values if not loaded
if (-not $Target) { $Target = $DefaultTarget }
if (-not $BuildConfig) { $BuildConfig = $DefaultConfig }
if (-not $Platform) { $Platform = $DefaultPlatform }

# Export to environment so all child tools share same config
$Env:UE_PATH        = $UEPath
$Env:BUILD_TARGET   = $Target
$Env:BUILD_CONFIG   = $BuildConfig
$Env:BUILD_PLATFORM = $Platform

# Validate UE path
$UEBuildBat = Join-Path $UEPath "Engine\Build\BatchFiles\Build.bat"
if (-not (Test-Path $UEBuildBat)) {
    Write-Host "[ERROR] UE_PATH invalid or not found: $UEPath" -ForegroundColor Red
    Write-Host ""
    Write-Host "Create scripts\\config\\ue_path.conf with:" -ForegroundColor Yellow
    Write-Host "  UE_PATH=<path to source-built engine>"
    Write-Host "  BUILD_TARGET=AlisEditor"
    Write-Host "  BUILD_CONFIG=Development"
    Write-Host "  BUILD_PLATFORM=Win64"
    exit 1
}

# Resolve project file
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$ProjectFile = Join-Path $ProjectRoot "Alis.uproject"

# ============================================================
# Print Configuration
# ============================================================

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " Alis - Build AlisEditor target for standalone testing" -ForegroundColor Cyan
if ($Clean) {
    Write-Host " Clean + full rebuild" -ForegroundColor Yellow
} else {
    Write-Host " Incremental build (use -Clean for full rebuild)" -ForegroundColor Green
}
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Configuration loaded from: $ConfigFile"
Write-Host "UE_PATH      = `"$UEPath`""
Write-Host "PROJECT_FILE = `"$ProjectFile`""
Write-Host "TARGET       = `"$Target`""
Write-Host "CONFIG       = `"$BuildConfig`""
Write-Host "PLATFORM     = `"$Platform`""
Write-Host ""

# ============================================================
# Clean (if requested)
# ============================================================

if ($Clean) {
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host " Cleaning binaries and intermediate files..." -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host ""

    # Clean plugins
    Write-Host "Cleaning plugins..." -ForegroundColor Yellow
    $CleanPluginsScript = Join-Path $ProjectRoot "scripts\ue\clean\clean_plugins.ps1"
    if (Test-Path $CleanPluginsScript) {
        & $CleanPluginsScript
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[WARNING] Plugin clean failed, continuing anyway..." -ForegroundColor Yellow
        }
    }

    # Clean project
    Write-Host "Cleaning project..." -ForegroundColor Yellow
    $CleanProjectScript = Join-Path $ProjectRoot "scripts\ue\clean\clean_project.ps1"
    if (Test-Path $CleanProjectScript) {
        & $CleanProjectScript
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[WARNING] Project clean failed, continuing anyway..." -ForegroundColor Yellow
        }
    }

    Write-Host ""
} else {
    Write-Host "[INFO] Using incremental build (use -Clean for full rebuild)" -ForegroundColor Green
    Write-Host ""
}

# ============================================================
# Build
# ============================================================

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " Building $Target ($BuildConfig)..." -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

$BuildArgs = @(
    $Target,
    $Platform,
    $BuildConfig,
    "`"$ProjectFile`"",
    "-NoHotReloadFromIDE"
)

& $UEBuildBat @BuildArgs

$ExitCode = $LASTEXITCODE

Write-Host ""
if ($ExitCode -ne 0) {
    Write-Host "*** BUILD FAILED: exit code $ExitCode ***" -ForegroundColor Red
    exit $ExitCode
} else {
    Write-Host "*** BUILD OK ***" -ForegroundColor Green
}

exit 0
