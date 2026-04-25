# Strict exact-only dispatch for one automation test.
#
# Contract (docs/agents/canonical.md "Dev Loop Contract"):
#   - accepts exactly one full test name as a positional or -TestFilter argument
#   - rejects broad shapes (wildcards, unions, Group:/Filter:, short prefix,
#     tag expressions)  - see Test-FilterShape.ps1 for the rule set
#   - prefers the persistent editor when alive (delegates to
#     persistent_editor_run.ps1); falls back to cold one-shot via
#     run_cpp_tests_safe.ps1 -Mode Gate otherwise
#
# Deliberately minimal: no compile selection, no tag handling. For the full
# dev entrypoint (compile-mode selection, -Tags, -Mode) use iterate.ps1.
#
# Usage:
#   .\run_single.ps1 "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.BuilderWrapsEveryCell"
#   .\run_single.ps1 -TestFilter "..." -TimeoutSeconds 240
#
# Exit codes (match persistent_editor_run.ps1 / run_cpp_tests_safe.ps1):
#   0   = test passed
#   1   = test failed
#   2   = no tests matched (also used when the rejection fires in this script)
#   124 = timeout
#   3   = persistent editor declared alive but unreachable

param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$TestFilter,

    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "Test-FilterShape.ps1")

# Validate filter shape before doing anything expensive.
$shape = Test-ExactFilter -Filter $TestFilter
if (-not $shape.IsExact) {
    # run_single.ps1 is intentionally strict-only (no override flags). Point
    # users at the wrappers that do accept -Mode Gate / -AllowBroadFilter.
    Write-BroadFilterRejection -Filter $TestFilter -Reason $shape.Reason `
        -Example $shape.Example -ScriptName "run_single.ps1" `
        -OverrideCommands @(
            "iterate.ps1             -Mode Gate        -TestFilter `"{0}`"",
            "iterate.ps1             -AllowBroadFilter -TestFilter `"{0}`"",
            "run_cpp_tests_safe.ps1 -Mode Gate        -TestFilter `"{0}`""
        )
    exit 2
}

Write-Host "[run_single] Exact filter accepted: $TestFilter" -ForegroundColor Green

# Prefer the persistent editor. Same detection logic as run_cpp_tests_safe.ps1.
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
if ($env:ALIS_NO_PERSISTENT_EDITOR -ne "1") {
    $persistentPid = Join-Path $root "artifacts\persistent\editor.pid"
    if (Test-Path $persistentPid) {
        $editorPid = (Get-Content $persistentPid -ErrorAction SilentlyContinue | Select-Object -First 1)
        if ($editorPid) {
            $aliveProc = $null
            try { $aliveProc = Get-Process -Id $editorPid -ErrorAction Stop } catch { }
            if ($aliveProc -and ($aliveProc.ProcessName -like "UnrealEditor*")) {
                Write-Host "[run_single] Persistent editor detected (PID $editorPid); dispatching warm." -ForegroundColor Cyan
                $persistentRun = Join-Path $PSScriptRoot "persistent_editor_run.ps1"
                $persistentTimeout = [Math]::Max($TimeoutSeconds, 300)
                & $persistentRun -TestFilter $TestFilter -TimeoutSeconds $persistentTimeout
                exit $LASTEXITCODE
            }
        }
    }
}

# Cold fallback. Run through run_cpp_tests_safe.ps1 in Gate mode so the
# generic wrapper does not double-check the filter shape (we already did).
Write-Host "[run_single] No warm editor; falling back to cold one-shot." -ForegroundColor Yellow
$coldRun = Join-Path $PSScriptRoot "run_cpp_tests_safe.ps1"
& $coldRun -TestFilter $TestFilter -TimeoutSeconds $TimeoutSeconds -Mode Gate
exit $LASTEXITCODE
