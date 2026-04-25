# Dispatch an automation filter into the persistent editor and wait for
# the resulting test run to complete. Completion is detected by watching
# the editor's rolling log for the "Automation Test Queue Empty N tests
# performed" marker, not the Saved/Automation/Reports/index.json file
# (the report is only written in specific CLI-exit paths; the persistent
# editor does not exit, so it never writes that file).
#
# Preconditions:
#   - persistent_editor_start.ps1 has been called successfully.
#   - PID file at scripts/ue/artifacts/persistent/editor.pid is alive.
#
# Usage:
#   .\persistent_editor_run.ps1 -TestFilter "ProjectIntegrationTests.UI.*"
#     [-TimeoutSeconds 300] [-IdleTimeoutSeconds 120]
#
# Timeout model (two-tier):
#   -TimeoutSeconds     = hard wall-clock cap for the whole dispatch. Default
#                         300s is generous because the FIRST automation run
#                         after an editor boot pays extra costs (JIT, module
#                         load, first Slate paint for layout-dependent tests).
#                         Subsequent warm runs complete in ~5-15s.
#   -IdleTimeoutSeconds = max time with NO forward progress before aborting.
#                         "Forward progress" = a new `Test Completed.` line
#                         appears in the log tail slice. This keeps slow-but-
#                         progressing runs alive while still catching actual
#                         hangs. Default 120s.
#
# Exit codes (match run_cpp_tests_safe.ps1 for drop-in compatibility):
#   0   = all tests passed
#   1   = at least one test failed
#   2   = no tests matched the filter
#   124 = timeout (either hard wall-clock or idle)
#   3   = persistent editor not running

param(
    [Parameter(Mandatory = $true)]
    [string]$TestFilter,

    # Optional UE 5.7 automation tag filter. When provided, the editor is
    # told to SetTagFilter first, then RunTests <pool>. $TestFilter becomes
    # the pool that the tag CVar narrows (caller picks the right root).
    # UE 5.7 limitation: tag CVar uses FTextFilterExpressionEvaluator in
    # BasicString mode - LITERAL SUBSTRING MATCH ONLY. Pass a single token:
    #   "Fast"       - all tests whose tag string contains "Fast"
    #   "Inventory"  - narrow to inventory tests
    # Do NOT pass boolean operators - `&&` / `||` / `!` become literal
    # characters and match zero tests. See canonical.md for the full
    # Dev Loop Contract discussion.
    [string]$TagExpression = "",

    [int]$TimeoutSeconds = 300,

    [int]$IdleTimeoutSeconds = 120,

    [int]$PollMs = 500
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$artifactDir = Join-Path $root "artifacts\persistent"
$pidFile   = Join-Path $artifactDir "editor.pid"
$cmdFile   = Join-Path $artifactDir "command.txt"
$editorLog = Join-Path $artifactDir "editor.log"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Persistent Editor Run" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TestFilter: $TestFilter"
Write-Host "Timeout:    $TimeoutSeconds sec"

# Step 1: Verify the persistent editor process is alive.
if (-not (Test-Path $pidFile)) {
    Write-Host "  ERROR: no PID file at $pidFile (run persistent_editor_start.ps1 first)" -ForegroundColor Red
    exit 3
}
$editorPid = (Get-Content $pidFile -ErrorAction SilentlyContinue | Select-Object -First 1)
if (-not $editorPid) {
    Write-Host "  ERROR: PID file empty" -ForegroundColor Red
    exit 3
}
try {
    $proc = Get-Process -Id $editorPid -ErrorAction Stop
    if (-not ($proc.ProcessName -like "UnrealEditor*")) {
        Write-Host "  ERROR: PID $editorPid is not an UnrealEditor process (name=$($proc.ProcessName))" -ForegroundColor Red
        exit 3
    }
} catch {
    Write-Host "  ERROR: PID $editorPid is not alive" -ForegroundColor Red
    exit 3
}
Write-Host "  Editor alive: PID $editorPid" -ForegroundColor Green

# Step 2: Snapshot the current log size BEFORE dispatching. Test-completion
# markers will appear AFTER this offset; counting completed-test lines
# from the snapshot forward gives us per-run pass/fail counts.
$startOffset = 0
if (Test-Path $editorLog) {
    $startOffset = (Get-Item $editorLog).Length
}
Write-Host "  Log baseline offset: $startOffset bytes" -ForegroundColor DarkGray

# Step 3: Dispatch the command. The module's file watcher picks up
# command.txt within ~0.5s. One line per command; the module Exec()s each.
if ($TagExpression) {
    # UE 5.7 tag filtering via the CVar Automation.TestTagGlobalFilter.
    # `SetTagFilter` only sets that CVar; `RunTests <pool>` then runs the
    # candidate pool narrowed by the CVar's substring match.
    # IMPORTANT: the CVar uses FTextFilterExpressionEvaluator in
    # BasicString mode - that means LITERAL SUBSTRING MATCH only. Boolean
    # operators like `&&` / `||` / `!` are NOT evaluated; they become
    # literal characters that will never match. Pass a single tag (e.g.
    # "Fast" or "Inventory") for predictable narrowing. Multiple bracketed
    # tags in a single REGISTER call ([Fast][Inventory]) are fine - the
    # single-tag filter matches on substring.
    $pool = if ($TestFilter) { $TestFilter } else { "ProjectIntegrationTests" }
    $command = @(
        "Automation SetTagFilter `"$TagExpression`"",
        "Automation RunTests $pool"
    ) -join "`n"
    Write-Host "[1/2] Tag-filtered dispatch:" -ForegroundColor Yellow
    Write-Host "        SetTagFilter $TagExpression"      -ForegroundColor Yellow
    Write-Host "        RunTests $pool (narrowed by tag CVar, BasicString substring)" -ForegroundColor Yellow
} else {
    # Auto-clear any tag CVar left from a prior tag-mode run so non-tag
    # dispatches don't get silently filtered. The module Exec()s each line.
    $command = @(
        'Automation SetTagFilter ""',
        "Automation RunTests $TestFilter"
    ) -join "`n"
    Write-Host "[1/2] Command dispatched (with tag-CVar auto-clear):" -ForegroundColor Yellow
    Write-Host "        SetTagFilter `"`"" -ForegroundColor DarkGray
    Write-Host "        RunTests $TestFilter" -ForegroundColor Yellow
}
$command | Out-File -FilePath $cmdFile -Encoding ASCII -Force

# Step 4: Tail the log starting at $startOffset. Completion signal:
#   "Automation Test Queue Empty N tests performed"
# Sabotage signal: the editor process exits.
#
# Progress tracking: count `Test Completed.` occurrences in the slice. When
# the count grows, reset $lastProgress. If no progress for $IdleTimeoutSeconds
# we abort as an idle-timeout. This lets genuinely-slow-but-alive runs
# (e.g. first post-boot dispatch that pays JIT + first-paint cost) run to
# completion without the hard cap killing them prematurely.
$startTime      = Get-Date
$lastDot        = Get-Date
$lastProgress   = Get-Date
$lastCompletedCount = 0
$expectedCount  = -1

while ($true) {
    if ($proc.HasExited) {
        Write-Host "  FAIL: editor exited during run (exit code $($proc.ExitCode))" -ForegroundColor Red
        Remove-Item -Force $pidFile -ErrorAction SilentlyContinue
        exit 1
    }

    if (Test-Path $editorLog) {
        # Stream-read the tail-slice. Opening with FileShare.ReadWrite lets
        # us read while the editor process still writes.
        try {
            $fs = [System.IO.File]::Open($editorLog, [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            try {
                if ($fs.Length -gt $startOffset) {
                    $fs.Seek($startOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
                    $reader = [System.IO.StreamReader]::new($fs)
                    $slice = $reader.ReadToEnd()
                    $reader.Dispose()
                    if ($slice -match "Automation Test Queue Empty (\d+) tests performed") {
                        $expectedCount = [int]$Matches[1]
                        break
                    }
                    # Fail-fast: if the command was rejected, Automation prints
                    # "No tests matching" or similar. Don't block the full
                    # timeout in that case.
                    if ($slice -match "No automation tests currently loaded" -or
                        $slice -match "Could not find automation test") {
                        $expectedCount = 0
                        break
                    }
                    # Progress detection: number of completed tests in the
                    # slice. We treat a positive delta as forward progress
                    # and reset the idle clock.
                    $completedNow = ([regex]::Matches($slice, 'Test Completed\.')).Count
                    if ($completedNow -gt $lastCompletedCount) {
                        $lastCompletedCount = $completedNow
                        $lastProgress = Get-Date
                    }
                }
            } finally {
                $fs.Dispose()
            }
        } catch {
            # Log was rotated out from under us; try again next poll.
        }
    }

    $elapsed     = ((Get-Date) - $startTime).TotalSeconds
    $idleElapsed = ((Get-Date) - $lastProgress).TotalSeconds
    if ($elapsed -ge $TimeoutSeconds) {
        Write-Host "  TIMEOUT after $TimeoutSeconds sec (hard cap; last progress $([math]::Round($idleElapsed,0))s ago, $lastCompletedCount tests completed)" -ForegroundColor Red
        exit 124
    }
    if ($idleElapsed -ge $IdleTimeoutSeconds) {
        Write-Host "  TIMEOUT idle-$IdleTimeoutSeconds sec after $([math]::Round($elapsed,0))s total ($lastCompletedCount tests completed before stall)" -ForegroundColor Red
        exit 124
    }
    if (((Get-Date) - $lastDot).TotalSeconds -ge 5) {
        Write-Host "  ... $([math]::Round($elapsed, 0))s elapsed (completed=$lastCompletedCount, idle=$([math]::Round($idleElapsed,0))s)" -ForegroundColor DarkGray
        $lastDot = Get-Date
    }
    Start-Sleep -Milliseconds $PollMs
}

# Step 5: Count pass/fail markers in the per-run slice.
$slice = ""
try {
    $fs = [System.IO.File]::Open($editorLog, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        $fs.Seek($startOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $reader = [System.IO.StreamReader]::new($fs)
        $slice = $reader.ReadToEnd()
        $reader.Dispose()
    } finally {
        $fs.Dispose()
    }
} catch { }

$passed = ([regex]::Matches($slice, 'Test Completed\. Result=\{Success\}')).Count
$failed = ([regex]::Matches($slice, 'Test Completed\. Result=\{Fail\}')).Count
$total = $passed + $failed

$totalElapsed = [math]::Round(((Get-Date) - $startTime).TotalSeconds, 1)

Write-Host "[2/2] Parsing log slice..." -ForegroundColor Yellow
Write-Host ""
Write-Host "  Automation reported: $expectedCount tests performed" -ForegroundColor Gray
Write-Host "  Total tests:  $total" -ForegroundColor Gray
Write-Host "  Passed:       $passed" -ForegroundColor Green
Write-Host "  Failed:       $failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Host ""

if ($failed -gt 0) {
    Write-Host "  Failed tests:" -ForegroundColor Red
    foreach ($m in [regex]::Matches($slice, 'Test Completed\. Result=\{Fail\} Name=\{([^}]+)\} Path=\{([^}]+)\}')) {
        Write-Host "    - $($m.Groups[2].Value)" -ForegroundColor Red
    }
    $exitCode = 1
} elseif ($total -eq 0 -or $expectedCount -eq 0) {
    Write-Host "  WARNING: no tests matched '$TestFilter'" -ForegroundColor Yellow
    $exitCode = 2
} else {
    Write-Host "  All tests passed." -ForegroundColor Green
    $exitCode = 0
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Elapsed:  $totalElapsed sec" -ForegroundColor $(if ($totalElapsed -le 30) { "Green" } else { "Yellow" })
Write-Host "Log slice: bytes $startOffset .. EOF" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan

exit $exitCode
