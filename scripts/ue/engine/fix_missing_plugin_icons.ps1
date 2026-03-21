# fix_missing_plugin_icons.ps1
# Copies Paper2D icon to all engine plugins missing Icon128.png
# Silences "Failed to read file Icon128.png" warnings in editor
#
# Usage:
#   .\scripts\ue\engine\fix_missing_plugin_icons.ps1              # Uses ue_path.conf
#   .\scripts\ue\engine\fix_missing_plugin_icons.ps1 -DryRun      # Preview only
#   .\scripts\ue\engine\fix_missing_plugin_icons.ps1 -EngineRoot "<engine root path>"

param(
    [string]$EngineRoot,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Resolve-Path (Join-Path $scriptDir "..\..\..")

# Read UE path from config if not provided
if (-not $EngineRoot) {
    $configDir = Join-Path $projectRoot "scripts\config"
    . (Join-Path $configDir "Resolve-UEConfig.ps1")
    $config = Resolve-UEConfig -ConfigDir $configDir
    $EngineRoot = $config.UE_PATH
}

if (-not $EngineRoot -or -not (Test-Path $EngineRoot)) {
    Write-Error "Engine not found. Specify -EngineRoot or create scripts/config/ue_path.local.conf"
    exit 1
}

$sourceIcon = Join-Path $EngineRoot "Engine\Plugins\2D\Paper2D\Resources\Icon128.png"
$pluginsRoot = Join-Path $EngineRoot "Engine\Plugins"

if (-not (Test-Path $sourceIcon)) {
    Write-Error "Source icon not found: $sourceIcon"
    exit 1
}

Write-Host "Engine: $EngineRoot" -ForegroundColor Cyan
Write-Host "Source icon: $sourceIcon" -ForegroundColor Cyan
if ($DryRun) {
    Write-Host "[DRY RUN - no files will be copied]" -ForegroundColor Yellow
}
Write-Host ""

# Find all .uplugin files
$uplugins = Get-ChildItem -Path $pluginsRoot -Recurse -Filter "*.uplugin" -File

$fixed = 0
$skipped = 0

foreach ($uplugin in $uplugins) {
    $pluginDir = $uplugin.DirectoryName
    $resourcesDir = Join-Path $pluginDir "Resources"
    $iconPath = Join-Path $resourcesDir "Icon128.png"

    if (Test-Path $iconPath) {
        $skipped++
        continue
    }

    $pluginName = $uplugin.BaseName

    if ($DryRun) {
        Write-Host "[DRY] Would copy icon to: $pluginName" -ForegroundColor Gray
    } else {
        # Create Resources folder if needed
        if (-not (Test-Path $resourcesDir)) {
            New-Item -ItemType Directory -Path $resourcesDir -Force | Out-Null
        }

        # Copy icon
        Copy-Item -Path $sourceIcon -Destination $iconPath -Force
        Write-Host "Fixed: $pluginName" -ForegroundColor Green
    }

    $fixed++
}

Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Cyan
Write-Host "Plugins with icons: $skipped"
Write-Host "Plugins fixed: $fixed"

if ($DryRun -and $fixed -gt 0) {
    Write-Host ""
    Write-Host "Run without -DryRun to apply changes" -ForegroundColor Yellow
}
