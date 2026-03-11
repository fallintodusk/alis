# Clean all plugin Binaries and Intermediate directories
# Usage: .\clean_plugins.ps1

param(
    [string]$PluginName = "",  # Optional: clean specific plugin only
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

# Project root (3 levels up from this script)
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))

Write-Host "Cleaning plugin artifacts..." -ForegroundColor Cyan

if ($PluginName) {
    # Clean specific plugin
    $PluginPaths = @(
        "$ProjectRoot\Plugins\Boot\$PluginName",
        "$ProjectRoot\Plugins\Core\$PluginName",
        "$ProjectRoot\Plugins\Foundation\$PluginName",
        "$ProjectRoot\Plugins\Systems\$PluginName",
        "$ProjectRoot\Plugins\UI\$PluginName",
        "$ProjectRoot\Plugins\Features\$PluginName"
    ) | Where-Object { Test-Path $_ }

    if ($PluginPaths.Count -eq 0) {
        Write-Host "Plugin '$PluginName' not found" -ForegroundColor Yellow
        exit 1
    }
} else {
    # Clean all plugins
    $PluginPaths = @(
        Get-ChildItem -Path "$ProjectRoot\Plugins\Boot" -Directory -ErrorAction SilentlyContinue
        Get-ChildItem -Path "$ProjectRoot\Plugins\Core" -Directory -ErrorAction SilentlyContinue
        Get-ChildItem -Path "$ProjectRoot\Plugins\Foundation" -Directory -ErrorAction SilentlyContinue
        Get-ChildItem -Path "$ProjectRoot\Plugins\Systems" -Directory -ErrorAction SilentlyContinue
        Get-ChildItem -Path "$ProjectRoot\Plugins\UI" -Directory -ErrorAction SilentlyContinue
        Get-ChildItem -Path "$ProjectRoot\Plugins\Features" -Directory -ErrorAction SilentlyContinue
    )
}

$CleanedCount = 0
$TotalSize = 0

foreach ($Plugin in $PluginPaths) {
    $PluginPath = if ($Plugin -is [string]) { $Plugin } else { $Plugin.FullName }

    # Clean Binaries
    $BinariesPath = Join-Path $PluginPath "Binaries"
    if (Test-Path $BinariesPath) {
        if ($Verbose) {
            $Size = (Get-ChildItem -Path $BinariesPath -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB
            Write-Host "  Removing $BinariesPath ($([math]::Round($Size, 2)) MB)" -ForegroundColor Gray
            $TotalSize += $Size
        }
        Remove-Item -Path $BinariesPath -Recurse -Force
        $CleanedCount++
    }

    # Clean Intermediate
    $IntermediatePath = Join-Path $PluginPath "Intermediate"
    if (Test-Path $IntermediatePath) {
        if ($Verbose) {
            $Size = (Get-ChildItem -Path $IntermediatePath -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB
            Write-Host "  Removing $IntermediatePath ($([math]::Round($Size, 2)) MB)" -ForegroundColor Gray
            $TotalSize += $Size
        }
        Remove-Item -Path $IntermediatePath -Recurse -Force
        $CleanedCount++
    }
}

if ($CleanedCount -eq 0) {
    Write-Host "No plugin artifacts to clean" -ForegroundColor Green
} else {
    if ($Verbose -and $TotalSize -gt 0) {
        Write-Host "Cleaned $CleanedCount plugin artifact directories ($([math]::Round($TotalSize, 2)) MB freed)" -ForegroundColor Green
    } else {
        Write-Host "Cleaned $CleanedCount plugin artifact directories" -ForegroundColor Green
    }
}

exit 0
