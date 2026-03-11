# UI Layout Check - Inventory Panel
# Runs widget tree dump test + validates with Python analyzer
#
# Usage: .\check_inventory_layout.ps1 [-SkipDump] [-Verbose]
# See: docs/testing/agent_ue_inspection.md

param(
    [switch]$SkipDump = $false,  # Skip UE test, just analyze existing dump
    [string]$DumpPath = "",      # Custom dump path (default: Saved/Dumps/Inventory.json)
    [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = "Stop"
$scriptRoot = $PSScriptRoot
$projectRoot = (Resolve-Path "$scriptRoot\..\..\..\..").Path
$toolsRoot = Join-Path $projectRoot "tools"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "UI Layout Check - Inventory" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Project:  $projectRoot"
Write-Host "SkipDump: $SkipDump"
Write-Host ""

# Determine dump path
if (-not $DumpPath) {
    $DumpPath = Join-Path $projectRoot "Saved\Dumps\Inventory.json"
}
Write-Host "DumpPath: $DumpPath" -ForegroundColor Gray

# Step 1: Run widget tree dump test (unless skipped)
if (-not $SkipDump) {
    Write-Host ""
    Write-Host "[1/2] Running widget tree dump test..." -ForegroundColor Yellow

    $testScript = Join-Path $scriptRoot "..\unit\run_cpp_tests_safe.ps1"
    if (-not (Test-Path $testScript)) {
        Write-Host "  ERROR: Test script not found: $testScript" -ForegroundColor Red
        exit 2
    }

    # Run the DumpTree test in game mode with a real map
    & $testScript `
        -TestFilter "ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree" `
        -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent" `
        -Game `
        -TimeoutSeconds $TimeoutSeconds

    $testExitCode = $LASTEXITCODE

    if ($testExitCode -eq 124) {
        Write-Host "  TIMEOUT: Test did not complete" -ForegroundColor Red
        exit 124
    }
    elseif ($testExitCode -eq 2) {
        Write-Host "  WARNING: No tests found - check test name" -ForegroundColor Yellow
        Write-Host "  Expected: ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree" -ForegroundColor Gray
        exit 2
    }
    elseif ($testExitCode -ne 0) {
        Write-Host "  ERROR: Test failed (exit code: $testExitCode)" -ForegroundColor Red
        exit $testExitCode
    }

    Write-Host "  Test completed successfully" -ForegroundColor Green
}
else {
    Write-Host ""
    Write-Host "[1/2] Skipping dump test (using existing file)..." -ForegroundColor Gray
}

# Step 2: Analyze dump with Python tool
Write-Host ""
Write-Host "[2/2] Analyzing widget tree dump..." -ForegroundColor Yellow

if (-not (Test-Path $DumpPath)) {
    Write-Host "  ERROR: Dump file not found: $DumpPath" -ForegroundColor Red
    Write-Host "  Run without -SkipDump to generate, or check path" -ForegroundColor Gray
    exit 2
}

$pythonTool = Join-Path $toolsRoot "agentic\ui\layout_report.py"
if (-not (Test-Path $pythonTool)) {
    Write-Host "  ERROR: Python tool not found: $pythonTool" -ForegroundColor Red
    exit 2
}

# Check Python availability
# Priority: system PATH > UE bundled Python
$pythonCmd = $null
foreach ($cmd in @("python", "python3", "py")) {
    try {
        $null = & $cmd --version 2>&1
        $pythonCmd = $cmd
        break
    }
    catch { }
}

# Fallback to UE bundled Python
if (-not $pythonCmd) {
    # Read UE_PATH from config (handles both Make := and Shell = syntax)
    $uePathConf = Join-Path $projectRoot "scripts\config\ue_path.conf"
    if (Test-Path $uePathConf) {
        $uePath = $null
        Get-Content $uePathConf | ForEach-Object {
            $line = $_.Trim()
            if ($line -match "^UE_PATH\s*(?::=|=)\s*([^#]+)") {
                # Handles both Make (:=) and Shell (=) syntax
                $uePath = $Matches[1].Trim().Replace("/", "\").Trim('"', "'")
            }
        }
        if ($uePath) {
            $uePython = Join-Path $uePath "Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
            if (Test-Path $uePython) {
                $pythonCmd = $uePython
                Write-Host "  Using UE bundled Python" -ForegroundColor Gray
            }
        }
    }
}

if (-not $pythonCmd) {
    Write-Host "  ERROR: Python not found in PATH or UE installation" -ForegroundColor Red
    Write-Host "  See AGENTS.md for Python configuration" -ForegroundColor Gray
    exit 2
}

Write-Host "  Python: $pythonCmd" -ForegroundColor Gray
Write-Host "  Tool:   $pythonTool" -ForegroundColor Gray
Write-Host ""

# Run analysis
& $pythonCmd $pythonTool $DumpPath
$analysisExitCode = $LASTEXITCODE

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
if ($analysisExitCode -eq 0) {
    Write-Host "PASS: No layout issues detected" -ForegroundColor Green
}
elseif ($analysisExitCode -eq 1) {
    Write-Host "FAIL: Layout issues found (see above)" -ForegroundColor Red
}
else {
    Write-Host "ERROR: Analysis failed (exit code: $analysisExitCode)" -ForegroundColor Red
}
Write-Host "========================================" -ForegroundColor Cyan

exit $analysisExitCode
