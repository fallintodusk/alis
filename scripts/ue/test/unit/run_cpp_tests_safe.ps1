# UE C++ Unit Tests - Safe Wrapper (No Editor UI Required)
# Usage: .\run_cpp_tests_safe.ps1 [-TestFilter "Project.GameFeatures.*"] [-Map "/Game/Maps/MyMap"]
# For integration tests needing a world: -Map "/MainMenuWorld/Maps/MainMenu_Persistent"

param(
    [string]$TestFilter = "Project.*",  # Test name pattern
    [string]$Map = "",  # Optional map to load (required for integration tests needing a world)
    [switch]$NoRHI = $false,  # Use -NullRHI (headless, no GPU). Disable for tests needing a world.
    [switch]$Game = $false,  # Run in standalone game mode (creates real game world)
    [int]$TimeoutSeconds = 120  # 2 minutes
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$stepN = $env:OVERNIGHT_STEP
if (-not $stepN) { $stepN = "manual" }
$logDir = Join-Path $root "artifacts\overnight\step-$stepN"
New-Item -Force -ItemType Directory -Path $logDir | Out-Null

$startTime = Get-Date
$warnThresholdSeconds = 60  # Warn if test takes longer than this

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "UE C++ Unit Tests (Safe)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TestFilter: $TestFilter"
if ($Map) { Write-Host "Map:        $Map" -ForegroundColor Gray }
Write-Host "NullRHI:    $NoRHI" -ForegroundColor $(if ($NoRHI) { "Gray" } else { "Yellow" })
if ($Game) { Write-Host "Mode:       GAME (standalone)" -ForegroundColor Green }
Write-Host "Timeout:    $TimeoutSeconds seconds"
Write-Host "WarnAfter:  $warnThresholdSeconds seconds"
Write-Host "LogDir:     $logDir"
Write-Host "Started:    $($startTime.ToString('HH:mm:ss'))"
Write-Host ""

# Step 1: Kill any existing UE processes
Write-Host "[1/4] Cleaning existing UE processes..." -ForegroundColor Yellow
Get-Process | Where-Object {
    $_.ProcessName -like "UnrealEditor*" -or
    $_.ProcessName -like "UEBuildWorker*" -or
    $_.ProcessName -like "ShaderCompileWorker*"
} | Stop-Process -Force -ErrorAction SilentlyContinue
Write-Host "  Done" -ForegroundColor Green

# Step 2: Run tests via UnrealEditor-Cmd (headless, no UI)
Write-Host "[2/4] Running C++ unit tests (headless)..." -ForegroundColor Yellow

# Read UE_PATH from config (single source of truth)
$configPath = Join-Path $PSScriptRoot "..\..\..\config\ue_path.conf"
$uePath = $null
if (Test-Path $configPath) {
    $uePath = (Get-Content $configPath | Where-Object { $_ -match "^UE_PATH=(.+)$" } | Select-Object -First 1) -replace "^UE_PATH=", "" | ForEach-Object { $_.Trim().Replace("/", "\") }
}
if (-not $uePath) {
    Write-Host "  ERROR: UE_PATH not found in $configPath" -ForegroundColor Red
    exit 1
}
Write-Host "  UE_PATH: $uePath" -ForegroundColor Gray
$editorCmdPath = Join-Path $uePath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path $editorCmdPath)) { throw "UnrealEditor-Cmd not found: $editorCmdPath" }
$projectRoot = (Resolve-Path "$PSScriptRoot\..\..\..\..\").Path
$projectPath = Join-Path $projectRoot "Alis.uproject"
$testLog = Join-Path $logDir "tests.log"

# UnrealEditor-Cmd runs tests without UI
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $editorCmdPath
$mapArg = if ($Map) { " `"$Map`"" } else { "" }
$rhiArg = if ($NoRHI) { " -NullRHI" } else { "" }
$gameArg = if ($Game) { " -game" } else { "" }
$psi.Arguments = "`"$projectPath`"$mapArg$gameArg -ExecCmds=`"Automation RunTests $TestFilter; Quit`" -unattended -nopause$rhiArg -nosplash -nosound -log -stdout -FullStdOutLogOutput -testexit=`"Automation Test Queue Empty`""
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.CreateNoWindow = $true

$proc = New-Object System.Diagnostics.Process
$proc.StartInfo = $psi
$proc.Start() | Out-Null

# Read stdout/stderr asynchronously to avoid deadlock
# (Must read BEFORE WaitForExit when both stdout and stderr are redirected)
$stdoutTask = $proc.StandardOutput.ReadToEndAsync()
$stderrTask = $proc.StandardError.ReadToEndAsync()

# Wait with timeout
$ok = $proc.WaitForExit($TimeoutSeconds * 1000)

# Get output (tasks should be complete after WaitForExit)
$stdout = $stdoutTask.Result
$stderr = $stderrTask.Result
$output = $stdout + "`n`n--- STDERR ---`n" + $stderr
Set-Content -Path $testLog -Value $output

if (-not $ok) {
    Write-Host "  TIMEOUT: Tests did not complete in $TimeoutSeconds seconds" -ForegroundColor Red
    try { $proc.Kill() } catch { }
    Get-Process | Where-Object { $_.ProcessName -like "UnrealEditor*" } | Stop-Process -Force -ErrorAction SilentlyContinue
    exit 124  # Timeout exit code
}

$exitCode = $proc.ExitCode
Write-Host "  Tests completed (exit code: $exitCode)" -ForegroundColor $(if ($exitCode -eq 0) { "Green" } else { "Red" })

# Step 3: Parse test results
Write-Host "[3/4] Parsing test results..." -ForegroundColor Yellow
$logContent = Get-Content $testLog -Raw

# Count test results (UE format: "Test Completed. Result={Success}" or "Result={Fail}")
$passedCount = ([regex]::Matches($logContent, "Test Completed\. Result=\{Success\}")).Count
$failedCount = ([regex]::Matches($logContent, "Test Completed\. Result=\{Fail\}")).Count
$totalCount = $passedCount + $failedCount

Write-Host ""
Write-Host "  Total tests:  $totalCount" -ForegroundColor Gray
Write-Host "  Passed:       $passedCount" -ForegroundColor Green
Write-Host "  Failed:       $failedCount" -ForegroundColor $(if ($failedCount -eq 0) { "Green" } else { "Red" })
Write-Host ""

# Show failures if any
if ($failedCount -gt 0) {
    Write-Host "  Failed tests:" -ForegroundColor Red
    $logContent -split "`n" |
        Where-Object { $_ -match "LogAutomationController: Error:.*failed" } |
        ForEach-Object { Write-Host "    $_" -ForegroundColor Red }
    $exitCode = 1
} elseif ($totalCount -eq 0) {
    Write-Host "  WARNING: No tests found matching '$TestFilter'" -ForegroundColor Yellow
    $exitCode = 2
} else {
    Write-Host "  All tests passed!" -ForegroundColor Green
    $exitCode = 0
}

# Step 4: Cleanup and truncate logs
Write-Host "[4/4] Cleaning up..." -ForegroundColor Yellow

# Truncate log for chat (keep last 200 lines)
$logLines = Get-Content $testLog
if ($logLines.Count -gt 200) {
    $truncated = $logLines | Select-Object -Last 200
    Set-Content -Path "$testLog.truncated" -Value $truncated
    Write-Host "  Truncated log: $testLog.truncated (last 200 lines)" -ForegroundColor Gray
}

Write-Host "  Done" -ForegroundColor Green

# Calculate and display timing
$endTime = Get-Date
$elapsed = $endTime - $startTime
$elapsedSeconds = [math]::Round($elapsed.TotalSeconds, 1)

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Elapsed:  $elapsedSeconds seconds" -ForegroundColor $(if ($elapsedSeconds -le $warnThresholdSeconds) { "Green" } else { "Yellow" })
if ($elapsedSeconds -gt $warnThresholdSeconds) {
    Write-Host "WARNING:  Test exceeded $warnThresholdSeconds sec threshold!" -ForegroundColor Yellow
    Write-Host "          Expected: 30-60 sec. Investigate if this persists." -ForegroundColor Yellow
}
Write-Host "Full log: $testLog" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan

exit $exitCode
