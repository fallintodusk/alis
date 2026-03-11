# Clean project-level Binaries and Intermediate directories
# Usage: .\clean_project.ps1

param(
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

# Project root (3 levels up from this script)
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))

Write-Host "Cleaning project artifacts..." -ForegroundColor Cyan

$CleanedCount = 0
$TotalSize = 0

# Clean project Binaries
$BinariesPath = Join-Path $ProjectRoot "Binaries"
if (Test-Path $BinariesPath) {
    if ($Verbose) {
        $Size = (Get-ChildItem -Path $BinariesPath -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB
        Write-Host "  Removing $BinariesPath ($([math]::Round($Size, 2)) MB)" -ForegroundColor Gray
        $TotalSize += $Size
    }
    Remove-Item -Path $BinariesPath -Recurse -Force
    $CleanedCount++
}

# Clean project Intermediate
$IntermediatePath = Join-Path $ProjectRoot "Intermediate"
if (Test-Path $IntermediatePath) {
    if ($Verbose) {
        $Size = (Get-ChildItem -Path $IntermediatePath -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB
        Write-Host "  Removing $IntermediatePath ($([math]::Round($Size, 2)) MB)" -ForegroundColor Gray
        $TotalSize += $Size
    }
    Remove-Item -Path $IntermediatePath -Recurse -Force
    $CleanedCount++
}

# Clean DerivedDataCache (optional, can be large)
# Uncomment to enable:
# $DDCPath = Join-Path $ProjectRoot "DerivedDataCache"
# if (Test-Path $DDCPath) {
#     if ($Verbose) {
#         $Size = (Get-ChildItem -Path $DDCPath -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB
#         Write-Host "  Removing $DDCPath ($([math]::Round($Size, 2)) MB)" -ForegroundColor Gray
#         $TotalSize += $Size
#     }
#     Remove-Item -Path $DDCPath -Recurse -Force
#     $CleanedCount++
# }

if ($CleanedCount -eq 0) {
    Write-Host "No project artifacts to clean" -ForegroundColor Green
} else {
    if ($Verbose -and $TotalSize -gt 0) {
        Write-Host "Cleaned $CleanedCount project artifact directories ($([math]::Round($TotalSize, 2)) MB freed)" -ForegroundColor Green
    } else {
        Write-Host "Cleaned $CleanedCount project artifact directories" -ForegroundColor Green
    }
}

exit 0
