# Rerun only the tests that failed in the most recent run. Failure-
# recovery entrypoint paired with iterate.ps1 (the dev-loop entrypoint).
# Dev Loop Contract: docs/agents/canonical.md "Dev Loop Contract".
#
# Why (SOLID):
#   iterate.ps1 is the dev-loop entrypoint; rerun_failed is the failure-
#   recovery entrypoint. They share no logic - this script only scrapes
#   logs and composes a filter expression, then delegates dispatch back
#   to iterate.ps1 -Mode Gate (broad by nature -> explicit override).
#
# Reuse (nothing reinvented):
#   - Pass/Fail regex is the same one already used in
#     `run_cpp_tests_safe.ps1:149-150` and `persistent_editor_run.ps1:192-193`
#     (`Test Completed\. Result=\{Fail\}`).
#   - Dispatch is delegated to iterate.ps1 -Mode Gate so rerun runs through
#     the same code path as any other gate run (no duplicate polling logic).
#
# Source log preference (stop at first found):
#   1. persistent editor rolling log  scripts/ue/artifacts/persistent/editor.log
#   2. cold run artifact log          scripts/ue/artifacts/overnight/step-<N>/tests.log
#   3. user-provided -LogPath
#
# Exit codes:
#   0 = all previously-failed tests now pass (or no failures to rerun; exits 0 with a note)
#   1 = at least one test still fails
#   2 = could not locate a log / could not parse any RunTests sentinel
#   other = propagated from iterate.ps1

param(
    # Explicit log path override; if not provided, auto-detection is used.
    [string]$LogPath = "",

    # How many RunTests sentinels to walk back. Default 1 = the most recent
    # run. Increase to rerun failures from a suite of runs.
    [int]$NSentinelsBack = 1,

    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path

# ---- Pick source log ----
if (-not $LogPath) {
    $candidates = @(
        (Join-Path $root "artifacts\persistent\editor.log"),
        (Get-ChildItem (Join-Path $root "artifacts\overnight") -Recurse -Filter "tests.log" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName)
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) { $LogPath = $c; break }
    }
}
if (-not $LogPath -or -not (Test-Path $LogPath)) {
    Write-Host "ERROR: no test log found." -ForegroundColor Red
    Write-Host "  Checked:" -ForegroundColor Gray
    Write-Host "    artifacts/persistent/editor.log" -ForegroundColor Gray
    Write-Host "    artifacts/overnight/**/tests.log" -ForegroundColor Gray
    Write-Host "  Pass -LogPath to override." -ForegroundColor Gray
    exit 2
}

Write-Host ""
Write-Host "--- rerun_failed ---" -ForegroundColor Cyan
Write-Host " SourceLog: $LogPath" -ForegroundColor Gray

# ---- Locate the N-th most recent RunTests sentinel ----
# UE's AutomationController emits "Session Started" and later "Automation
# Test Queue Empty" markers around each run. We scan backwards.
$logLines = Get-Content -Path $LogPath -ErrorAction Stop
$queueEmpties = @()
for ($i = 0; $i -lt $logLines.Count; $i++) {
    if ($logLines[$i] -match "Automation Test Queue Empty") {
        $queueEmpties += $i
    }
}
if ($queueEmpties.Count -eq 0) {
    Write-Host "ERROR: no 'Automation Test Queue Empty' marker in log. No run to rerun from." -ForegroundColor Red
    exit 2
}

# Pick the target run's end-index. For NSentinelsBack=1 this is the last
# marker. For larger N we walk backwards.
$targetIdx = [Math]::Max(0, $queueEmpties.Count - $NSentinelsBack)
$runEndLine = $queueEmpties[$targetIdx]

# Run start: the line immediately after the previous "Queue Empty" (or the
# start of the log). That gives us a slice containing exactly one run.
$runStartLine = 0
if ($targetIdx -gt 0) {
    $runStartLine = $queueEmpties[$targetIdx - 1] + 1
}

$slice = $logLines[$runStartLine..$runEndLine] -join "`n"

# ---- Extract failed test paths ----
# UE format: `Test Completed. Result={Fail} Name={Class} Path={FullDotted}`
$failedPaths = [System.Collections.Generic.HashSet[string]]::new()
foreach ($m in [regex]::Matches($slice, 'Test Completed\. Result=\{Fail\} Name=\{[^}]+\} Path=\{([^}]+)\}')) {
    [void]$failedPaths.Add($m.Groups[1].Value.Trim())
}

$count = $failedPaths.Count
Write-Host " Failed tests found: $count" -ForegroundColor $(if ($count -gt 0) { "Yellow" } else { "Green" })

if ($count -eq 0) {
    Write-Host " Nothing to rerun." -ForegroundColor Green
    exit 0
}

# Print the reconstructed list so the operator sees what is being rerun.
foreach ($p in $failedPaths) { Write-Host "   - $p" -ForegroundColor Gray }

# ---- Compose filter and delegate to iterate.ps1 ----
# UE accepts ';'-separated filters. Many failures => broad run => -Mode Gate.
$filter = ($failedPaths -join ';')

Write-Host ""
Write-Host " Dispatching via iterate.ps1 -Mode Gate ..." -ForegroundColor Cyan
$iterate = Join-Path $PSScriptRoot "iterate.ps1"
& $iterate -TestFilter $filter -Mode Gate -TimeoutSeconds $TimeoutSeconds -CompileMode None
exit $LASTEXITCODE
