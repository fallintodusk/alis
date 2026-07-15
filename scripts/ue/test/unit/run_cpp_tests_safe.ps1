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
    [string]$PreExecCmds = "",  # Console commands to run before Automation RunTests
    [string[]]$RequiredLogPatterns = @(),  # Regex markers that must appear in the cold-run log
    [string[]]$ExpectedTestNames = @(),  # Exact test paths expected to start and complete
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
$requiresColdRunEvidence = $RequiredLogPatterns.Count -gt 0 -or $ExpectedTestNames.Count -gt 0
if ($env:ALIS_NO_PERSISTENT_EDITOR -ne "1" -and -not $requiresColdRunEvidence) {
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
if ($ExpectedTestNames.Count -gt 0) {
    Write-Host "Expected:   $($ExpectedTestNames.Count) exact test(s)" -ForegroundColor Gray
}
if ($RequiredLogPatterns.Count -gt 0) {
    Write-Host "Markers:    $($RequiredLogPatterns.Count) required log pattern(s)" -ForegroundColor Gray
}
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
$automationCmds = if ($UseTestExitOnly) { "Automation RunTests $TestFilter" } else { "Automation RunTests $TestFilter; Quit" }
$execCmds = if ($PreExecCmds) { "$PreExecCmds, $automationCmds" } else { $automationCmds }
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

# Parse discovery, start, and completion independently. A process exit code or
# an empty automation queue is not evidence that the requested tests ran.
$discoveryMatches = [regex]::Matches(
    $logContent,
    "Found\s+(?<Count>\d+)\s+automation tests based on")
$discoveredCount = if ($discoveryMatches.Count -gt 0) {
    [int]$discoveryMatches[$discoveryMatches.Count - 1].Groups["Count"].Value
} else {
    $null
}

$startedMatches = [regex]::Matches(
    $logContent,
    "Test Started\..*?Path=\{(?<Path>[^}]+)\}")
$startedPaths = @($startedMatches | ForEach-Object { $_.Groups["Path"].Value })

$completedMatches = [regex]::Matches(
    $logContent,
    "Test Completed\. Result=\{(?<Result>Success|Fail)\}.*?Path=\{(?<Path>[^}]+)\}")
$completedPaths = @($completedMatches | ForEach-Object { $_.Groups["Path"].Value })

$passedCount = @($completedMatches | Where-Object { $_.Groups["Result"].Value -eq "Success" }).Count
$failedCount = @($completedMatches | Where-Object { $_.Groups["Result"].Value -eq "Fail" }).Count
$totalCount = $passedCount + $failedCount
$uniqueStartedPaths = @($startedPaths | Sort-Object -Unique)
$uniqueCompletedPaths = @($completedPaths | Sort-Object -Unique)
$duplicateStartedPaths = @($startedPaths | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name })
$duplicateCompletedPaths = @($completedPaths | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name })

$validationErrors = [System.Collections.Generic.List[string]]::new()

if ($proc.ExitCode -ne 0) {
    $validationErrors.Add("Editor process exited with code $($proc.ExitCode)")
}

if ($null -eq $discoveredCount) {
    $validationErrors.Add("Automation discovery marker was not found")
} elseif ($discoveredCount -le 0) {
    $validationErrors.Add("Automation discovery matched zero tests")
}

if ($startedPaths.Count -eq 0) {
    $validationErrors.Add("No automation test start marker was found")
}

if ($totalCount -eq 0) {
    $validationErrors.Add("No automation test completion marker was found")
}

if ($startedPaths.Count -ne $totalCount) {
    $validationErrors.Add(
        "Started/completed count mismatch: started=$($startedPaths.Count), completed=$totalCount")
}

if ($null -ne $discoveredCount -and $discoveredCount -ne $startedPaths.Count) {
    $validationErrors.Add(
        "Discovered/started count mismatch: discovered=$discoveredCount, started=$($startedPaths.Count)")
}

if ($null -ne $discoveredCount -and $discoveredCount -ne $totalCount) {
    $validationErrors.Add(
        "Discovered/completed count mismatch: discovered=$discoveredCount, completed=$totalCount")
}

if ($duplicateStartedPaths.Count -gt 0) {
    $validationErrors.Add("Duplicate test start markers: $($duplicateStartedPaths -join ', ')")
}

if ($duplicateCompletedPaths.Count -gt 0) {
    $validationErrors.Add("Duplicate test completion markers: $($duplicateCompletedPaths -join ', ')")
}

$startedWithoutCompletion = @($uniqueStartedPaths | Where-Object { $uniqueCompletedPaths -notcontains $_ })
$completedWithoutStart = @($uniqueCompletedPaths | Where-Object { $uniqueStartedPaths -notcontains $_ })
if ($startedWithoutCompletion.Count -gt 0) {
    $validationErrors.Add("Started tests did not complete: $($startedWithoutCompletion -join ', ')")
}
if ($completedWithoutStart.Count -gt 0) {
    $validationErrors.Add("Completed tests had no start marker: $($completedWithoutStart -join ', ')")
}

$foundRequiredPatternCount = 0
foreach ($pattern in $RequiredLogPatterns) {
    if (-not [regex]::IsMatch($logContent, $pattern)) {
        $validationErrors.Add("Required log pattern was not found: $pattern")
    } else {
        $foundRequiredPatternCount++
    }
}

$expectedNames = @($ExpectedTestNames | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
if ($expectedNames.Count -gt 0) {
    $uniqueExpectedNames = @($expectedNames | Sort-Object -Unique)
    if ($uniqueExpectedNames.Count -ne $expectedNames.Count) {
        $validationErrors.Add("Expected test names contain duplicates")
    }

    if ($null -ne $discoveredCount -and $discoveredCount -ne $expectedNames.Count) {
        $validationErrors.Add(
            "Discovered test count mismatch: expected=$($expectedNames.Count), discovered=$discoveredCount")
    }

    $missingStarts = @($expectedNames | Where-Object { $startedPaths -notcontains $_ })
    $missingCompletions = @($expectedNames | Where-Object { $completedPaths -notcontains $_ })
    $unexpectedStarts = @($startedPaths | Where-Object { $expectedNames -notcontains $_ })
    $unexpectedCompletions = @($completedPaths | Where-Object { $expectedNames -notcontains $_ })

    if ($missingStarts.Count -gt 0) {
        $validationErrors.Add("Expected tests did not start: $($missingStarts -join ', ')")
    }
    if ($missingCompletions.Count -gt 0) {
        $validationErrors.Add("Expected tests did not complete: $($missingCompletions -join ', ')")
    }
    if ($unexpectedStarts.Count -gt 0) {
        $validationErrors.Add("Unexpected tests started: $($unexpectedStarts -join ', ')")
    }
    if ($unexpectedCompletions.Count -gt 0) {
        $validationErrors.Add("Unexpected tests completed: $($unexpectedCompletions -join ', ')")
    }
}

if ($failedCount -gt 0) {
    $validationErrors.Add("$failedCount automation test(s) failed")
}

Write-Host ""
Write-Host "  Discovered:   $(if ($null -eq $discoveredCount) { '(missing)' } else { $discoveredCount })" -ForegroundColor Gray
Write-Host "  Started:      $($startedPaths.Count)" -ForegroundColor Gray
Write-Host "  Completed:    $totalCount" -ForegroundColor Gray
Write-Host "  Passed:       $passedCount" -ForegroundColor Green
Write-Host "  Failed:       $failedCount" -ForegroundColor $(if ($failedCount -eq 0) { "Green" } else { "Red" })
Write-Host "  Markers:      $foundRequiredPatternCount/$($RequiredLogPatterns.Count)" -ForegroundColor Gray
Write-Host "  Duplicates:   starts=$($duplicateStartedPaths.Count), completions=$($duplicateCompletedPaths.Count)" -ForegroundColor Gray
Write-Host ""

# Show failures if any
if ($validationErrors.Count -gt 0) {
    Write-Host "  Validation failures:" -ForegroundColor Red
    foreach ($validationError in $validationErrors) {
        Write-Host "    - $validationError" -ForegroundColor Red
    }

    if ($failedCount -gt 0) {
        Write-Host "  Failed test log lines:" -ForegroundColor Red
    }
    $logContent -split "`n" |
        Where-Object { $_ -match "LogAutomationController: Error:.*failed" } |
        ForEach-Object { Write-Host "    $_" -ForegroundColor Red }
    $exitCode = if ($totalCount -eq 0) { 2 } else { 1 }
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
