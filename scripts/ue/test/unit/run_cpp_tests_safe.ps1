# UE C++ Unit Tests - Safe Wrapper (No Editor UI Required)
# Usage: .\run_cpp_tests_safe.ps1 [-TestFilter "Project.GameFeatures.*"] [-Map "/Game/Maps/MyMap"]
# For integration tests needing a world: -Map "/MainMenuWorld/Maps/MainMenu_Persistent"

param(
    [string]$TestFilter = "Project.*",  # Test name pattern
    [string]$Map = "",  # Optional map to load (required for integration tests needing a world)
    [switch]$NoRHI = $false,  # Use -NullRHI (headless, no GPU). Disable for tests needing a world.
    [switch]$Game = $false,  # Run in standalone game mode (creates real game world)
    [int]$TimeoutSeconds = 120,  # 2 minutes
    [string]$ExtraArgs = "",  # Additional command-line args (e.g. -ProjectSkipFrontEnd)
    [switch]$UseTestExitOnly = $false,  # Omit Quit from ExecCmds; rely on -testexit sentinel only
    [ValidateSet("Dev", "Gate")]
    [string]$Mode = "Dev",  # Dev (default) rejects broad filters; Gate accepts them (CI/end-of-slice).
    [switch]$AllowBroadFilter = $false  # Explicit per-call override even in Dev mode.
)

$ErrorActionPreference = "Stop"

# --------------------------------------------------------------------------
# Dev Loop Contract:
# In Dev mode (default) the wrapper refuses broad filters. Override via
# -Mode Gate (end-of-slice / CI) or -AllowBroadFilter (one-off). Full
# contract: docs/agents/canonical.md "Dev Loop Contract" + AGENTS.md
# "Dev Loop Rule".
# --------------------------------------------------------------------------
. (Join-Path $PSScriptRoot "Test-FilterShape.ps1")
if ($Mode -eq "Dev" -and -not $AllowBroadFilter) {
    $shape = Test-ExactFilter -Filter $TestFilter
    if (-not $shape.IsExact) {
        Write-BroadFilterRejection -Filter $TestFilter -Reason $shape.Reason `
            -Example $shape.Example -ScriptName "run_cpp_tests_safe.ps1"
        exit 2
    }
}
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$stepN = $env:OVERNIGHT_STEP
if (-not $stepN) { $stepN = "manual" }
$logDir = Join-Path $root "artifacts\overnight\step-$stepN"
New-Item -Force -ItemType Directory -Path $logDir | Out-Null

# --------------------------------------------------------------------------
# Persistent-editor fast path. If a persistent editor is running (PID file
# exists AND points to a live UnrealEditor process), dispatch via the
# file-watcher instead of cold-booting. The cold path below stays the
# single-source-of-truth for first-run / CI / no-warm-editor callers.
#
# Env opt-out: set ALIS_NO_PERSISTENT_EDITOR=1 to force the cold path.
# --------------------------------------------------------------------------
if ($env:ALIS_NO_PERSISTENT_EDITOR -ne "1") {
    $persistentPid = Join-Path $root "artifacts\persistent\editor.pid"
    if (Test-Path $persistentPid) {
        $editorPid = (Get-Content $persistentPid -ErrorAction SilentlyContinue | Select-Object -First 1)
        if ($editorPid) {
            $aliveProc = $null
            try {
                $aliveProc = Get-Process -Id $editorPid -ErrorAction Stop
            } catch { }
            if ($aliveProc -and ($aliveProc.ProcessName -like "UnrealEditor*")) {
                Write-Host "[run_cpp_tests_safe] Persistent editor detected (PID $editorPid); dispatching via persistent_editor_run." -ForegroundColor Cyan
                $persistentRun = Join-Path $PSScriptRoot "persistent_editor_run.ps1"
                # Forward at least 300s hard cap to persistent_editor_run. The
                # caller's $TimeoutSeconds default (120) was sized for the
                # cold-boot path where the editor exits between runs; a
                # persistent dispatch can legitimately exceed that on its
                # first post-boot call (JIT + first Slate paint). The idle-
                # timeout guard inside persistent_editor_run still catches
                # real hangs.
                $persistentTimeout = [Math]::Max($TimeoutSeconds, 300)
                & $persistentRun -TestFilter $TestFilter -TimeoutSeconds $persistentTimeout
                exit $LASTEXITCODE
            }
        }
    }
}

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

# Read UE_PATH from config (SOT: Resolve-UEConfig.ps1)
$configDir = Join-Path $PSScriptRoot "..\..\..\config"
. (Join-Path $configDir "Resolve-UEConfig.ps1")
$config = Resolve-UEConfig -ConfigDir $configDir
$uePath = $config.UE_PATH.Replace("/", "\")
if (-not $uePath) {
    Write-Host "  ERROR: UE_PATH not found. Create scripts\config\ue_path.local.conf" -ForegroundColor Red
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
$extraArg = if ($ExtraArgs) { " $ExtraArgs" } else { "" }
$execCmds = if ($UseTestExitOnly) { "Automation RunTests $TestFilter" } else { "Automation RunTests $TestFilter; Quit" }
$psi.Arguments = "`"$projectPath`"$mapArg$gameArg -ExecCmds=`"$execCmds`" -unattended -nopause$rhiArg -nosplash -nosound -log -stdout -FullStdOutLogOutput -testexit=`"Automation Test Queue Empty`"$extraArg"
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
