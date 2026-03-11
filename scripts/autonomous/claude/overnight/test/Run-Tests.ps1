# Run-Tests.ps1 - Test runner for overnight CI unit tests

param(
    [switch]$InstallPester = $false,
    [switch]$Detailed = $false
)

$ErrorActionPreference = "Stop"

Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Overnight CI Unit Tests (Pester)" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# Check if Pester is installed
$pesterModules = Get-Module -ListAvailable -Name Pester
$pesterModule = $pesterModules | Sort-Object Version -Descending | Select-Object -First 1
if (-not $pesterModule) {
    Write-Host "Pester module not found" -ForegroundColor Yellow

    if ($InstallPester) {
        Write-Host "Installing Pester..." -ForegroundColor Yellow
        Install-Module -Name Pester -Force -SkipPublisherCheck -Scope CurrentUser
        Write-Host "Pester installed successfully" -ForegroundColor Green
    }
    else {
        Write-Host "To install Pester, run:" -ForegroundColor Yellow
        Write-Host "  powershell scripts/autonomous/claude/overnight/test/Run-Tests.ps1 -InstallPester" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Or manually:" -ForegroundColor Yellow
        Write-Host "  Install-Module -Name Pester -Force -SkipPublisherCheck -Scope CurrentUser" -ForegroundColor Yellow
        exit 1
    }
}
else {
    Write-Host "Pester version: $($pesterModule.Version)" -ForegroundColor Green
}

Write-Host ""
Write-Host "Running tests..." -ForegroundColor Cyan
Write-Host ""

# Run Pester tests
$testPath = Join-Path $PSScriptRoot "*.Tests.ps1"

$config = New-PesterConfiguration
$config.Run.Path = $testPath
$config.Output.Verbosity = if ($Detailed) { "Detailed" } else { "Normal" }
$config.Should.ErrorAction = "Stop"

$config.Run.PassThru = $true
$result = Invoke-Pester -Configuration $config

Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Test Results" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Total:   $($result.TotalCount)" -ForegroundColor Gray
Write-Host "Passed:  $($result.PassedCount)" -ForegroundColor Green
Write-Host "Failed:  $($result.FailedCount)" -ForegroundColor $(if ($result.FailedCount -gt 0) { "Red" } else { "Green" })
Write-Host "Skipped: $($result.SkippedCount)" -ForegroundColor Yellow
Write-Host ""

if ($result.FailedCount -gt 0) {
    Write-Host "Tests FAILED" -ForegroundColor Red
    exit 1
}
else {
    Write-Host "All tests PASSED" -ForegroundColor Green
    exit 0
}
