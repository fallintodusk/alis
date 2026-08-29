# Default dev entrypoint for the test iteration loop.
#
# One command for normal iteration. Enforces the Dev Loop Contract
# (docs/agents/canonical.md):
#   - default mode = Dev -> exact filter required, broad filters refused
#   - Gate / -AllowBroadFilter = explicit override for end-of-slice, CI
#   - -CompileMode Auto|LiveCoding|None:
#       Auto       - try LiveCoding.CompileSync when warm editor is alive.
#                    On any non-OK Live Coding status (FAILED / TIMEOUT /
#                    UNAVAILABLE), ABORT with exit 5 - never dispatch
#                    against a potentially-stale editor state. Caller must
#                    rebuild and rerun.
#       LiveCoding - strict: try LiveCoding.CompileSync; any non-OK aborts
#                    with exit 5.
#       None       - no compile; caller is responsible for binaries.
#
# The old `Full` mode (rebuild-then-dispatch) was a placeholder that
# silently degraded to None. Removed from the public interface until a
# real full-rebuild path is wired.
#
# Phase 1 hard invariant: no test dispatch is allowed before a Live Coding
# terminal status is observed (when LC is invoked). Enforced via
# livecoding_sync.ps1 exit codes (0=LC_OK, 1=LC_FAILED, 2=LC_UNAVAILABLE,
# 3=LC_TIMEOUT). Auto and LiveCoding both abort on any non-OK. Only None
# skips the compile step.
#
# Dispatch:
#   - prefer persistent editor via persistent_editor_run.ps1 when alive
#   - fall back to cold run_cpp_tests_safe.ps1 -Mode Gate (shape already
#     validated here)
#
# Tag filtering (UE 5.7 BasicString substring match; single tag only):
#   -Tags "Fast"        - run tests whose registered tag string contains "Fast"
#   -Tags "Inventory"   - narrow to inventory tests
#   Boolean operators are NOT supported by the CVar - see canonical.md.
#
# Usage:
#   .\iterate.ps1 -TestFilter "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.BuilderWrapsEveryCell"
#   .\iterate.ps1 -TestFilter "..." -Mode Gate           # end-of-slice gate
#   .\iterate.ps1 -TestFilter "..." -AllowBroadFilter    # one-off override
#   .\iterate.ps1 -TestFilter "..." -CompileMode LiveCoding
#   .\iterate.ps1 -TestFilter "..." -CompileMode None    # skip compile
#   .\iterate.ps1 -Mode Gate -Tags "Fast"                # single-token tag
#
# Exit codes: same as run_single.ps1 / persistent_editor_run.ps1, plus:
#   5 = Live Coding did not reach LC_OK (stale-state protection)

param(
    [string]$TestFilter = "",

    # Phase 2: UE 5.7 automation tag boolean expression. When provided,
    # TestFilter is ignored and the editor runs every test matching the
    # expression. Examples: "[Fast] && [Inventory]", "[Inventory] && ![Slow]".
    # Tags are registered per-test via REGISTER_SIMPLE_AUTOMATION_TEST_TAGS;
    # see docs/agents/canonical.md "Tag taxonomy" and
    # "Single-token CLI filter" for the wrapper contract.
    [string]$Tags = "",

    [ValidateSet("Dev", "Gate")]
    [string]$Mode = "Dev",

    [switch]$AllowBroadFilter = $false,

    # Full was removed - it was a no-op placeholder that silently degraded
    # to None, which allowed dispatch against stale binaries after a
    # compile failure. Reintroduce only when a real rebuild-then-dispatch
    # path is wired.
    [ValidateSet("Auto", "LiveCoding", "None")]
    [string]$CompileMode = "Auto",

    [int]$TimeoutSeconds = 180,

    # Live Coding hard-cap. Typical body-only patches finish in <10s; 60s is
    # generous. If a compile legitimately takes longer, the CompileMode
    # LiveCoding caller should pass a larger value.
    [int]$LiveCodingTimeoutSeconds = 60
)

# One of TestFilter or Tags must be provided, but not both.
if (-not $TestFilter -and -not $Tags) {
    Write-Host "ERROR: pass either -TestFilter <exact id> or -Tags <single tag token>." -ForegroundColor Red
    exit 2
}
if ($TestFilter -and $Tags) {
    Write-Host "ERROR: pass either -TestFilter OR -Tags, not both. Tag runs implicitly select many tests." -ForegroundColor Red
    exit 2
}

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "Test-FilterShape.ps1")

# --- Filter-shape gate ----------------------------------------------------
# Tag runs are implicitly broad (they select a tag-matched set, not one
# test). They are allowed only when the caller explicitly opted in via
# -Mode Gate or -AllowBroadFilter, matching the Dev Loop Contract's
# "broad verification requires explicit override" rule.
$isBroadOverrideUsed = $false
$useTagRun = [bool]$Tags

if ($useTagRun) {
    if ($Mode -ne "Gate" -and -not $AllowBroadFilter) {
        # Pass the raw tag as Filter so the Override hints print correctly.
        # UE 5.7's tag CVar is BasicString substring; single-token only.
        Write-BroadFilterRejection -Filter $Tags `
            -Reason "tag-based runs are broad by design; require -Mode Gate or -AllowBroadFilter" `
            -Example 'Fast' `
            -ScriptName "iterate.ps1 (tag mode)" `
            -OverrideCommands @(
                'iterate.ps1 -Mode Gate         -Tags "{0}"',
                'iterate.ps1 -AllowBroadFilter  -Tags "{0}"'
            )
        exit 2
    }
    $isBroadOverrideUsed = $true
    $shape = [PSCustomObject]@{ IsExact = $false; Reason = "tag (single token)"; Example = "" }
} else {
    $shape = Test-ExactFilter -Filter $TestFilter
    if (-not $shape.IsExact) {
        if ($Mode -eq "Gate" -or $AllowBroadFilter) {
            $isBroadOverrideUsed = $true
        } else {
            Write-BroadFilterRejection -Filter $TestFilter -Reason $shape.Reason `
                -Example $shape.Example -ScriptName "iterate.ps1"
            exit 2
        }
    }
}

# --- Chosen-path banner ---------------------------------------------------
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$persistentPid = Join-Path $root "artifacts\persistent\editor.pid"
$warm = $false
$editorPid = $null
if ($env:ALIS_NO_PERSISTENT_EDITOR -ne "1" -and (Test-Path $persistentPid)) {
    $editorPid = (Get-Content $persistentPid -ErrorAction SilentlyContinue | Select-Object -First 1)
    if ($editorPid) {
        try {
            $aliveProc = Get-Process -Id $editorPid -ErrorAction Stop
            if ($aliveProc.ProcessName -like "UnrealEditor*") { $warm = $true }
        } catch { $warm = $false }
    }
}

Write-Host ""
Write-Host "================ iterate ================" -ForegroundColor Cyan
if ($useTagRun) {
    Write-Host " Tags:             $Tags"               -ForegroundColor White
    Write-Host " FilterShape:      tag-mode (implicitly broad)" -ForegroundColor Yellow
} else {
    Write-Host " TestFilter:       $TestFilter"         -ForegroundColor White
    Write-Host " FilterShape:      $(if ($shape.IsExact) { 'exact' } else { 'broad ('+$shape.Reason+')' })" `
        -ForegroundColor $(if ($shape.IsExact) { "Green" } else { "Yellow" })
}
Write-Host " Mode:             $Mode"                   -ForegroundColor White
Write-Host " BroadOverride:    $isBroadOverrideUsed"    -ForegroundColor $(if ($isBroadOverrideUsed) { "Yellow" } else { "Gray" })
Write-Host " Dispatch:         $(if ($warm) { 'persistent editor (PID '+$editorPid+')' } else { 'cold one-shot' })" `
    -ForegroundColor $(if ($warm) { "Green" } else { "Gray" })
Write-Host " CompileMode:      $CompileMode"           -ForegroundColor White
Write-Host "=========================================" -ForegroundColor Cyan
if (-not $warm) {
    # Measured 2026-04-23 on this repo: cold 42.9s median vs warm 13.5s
    # (3.2x faster). Numbers + pitfalls: docs/agents/canonical.md
    # "Persistent editor trio" and "Dev loop pitfalls".
    Write-Host " TIP: start persistent editor for ~3x faster iteration:" -ForegroundColor DarkYellow
    Write-Host "      scripts/ue/test/unit/persistent_editor_start.ps1"  -ForegroundColor DarkYellow
}
Write-Host ""

# --- Compile step ---------------------------------------------------------
# Hard invariant: no test dispatch until a Live Coding terminal status
# is observed (LC_OK | LC_UNAVAILABLE | LC_FAILED | LC_TIMEOUT). See
# docs/agents/canonical.md "Dev Loop Contract" + "Dev loop pitfalls"
# for the stale-state protection rationale and the manual recovery path.

$tryLiveCoding = $false
switch ($CompileMode) {
    # Auto uses LC only when a warm editor is alive. If the editor is
    # cold, there is nothing for LC to patch, so this wrapper performs
    # the normal launcher-engine incremental build before dispatch.
    "Auto"       { $tryLiveCoding = $warm }
    "LiveCoding" { $tryLiveCoding = $true }   # strict: try regardless; fail loud
    "None"       { $tryLiveCoding = $false }  # caller compiled already
}

if (-not $warm -and $CompileMode -eq "Auto") {
    Write-Host "[compile] Cold incremental AlisEditor build..." -ForegroundColor Cyan
    $buildScript = Join-Path $PSScriptRoot "..\..\standalone\build.ps1"
    & $buildScript
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[compile] Cold build failed - refusing to dispatch stale binaries." -ForegroundColor Red
        exit 5
    }
    Write-Host "[compile] Cold build OK -> proceeding to dispatch" -ForegroundColor Green
}

if ($tryLiveCoding) {
    Write-Host "[compile] LiveCoding.CompileSync (timeout ${LiveCodingTimeoutSeconds}s)..." -ForegroundColor Cyan
    $lcScript = Join-Path $PSScriptRoot "livecoding_sync.ps1"
    & $lcScript -TimeoutSeconds $LiveCodingTimeoutSeconds
    $lcExit = $LASTEXITCODE

    # Stale-state protection: if Live Coding did NOT reach LC_OK, refuse
    # to dispatch. Dispatching against a warm editor whose binaries are
    # behind the current source would produce fake-green test results.
    # This applies to both Auto and LiveCoding modes - the only way to
    # proceed is LC_OK or an explicit -CompileMode None (caller accepts
    # responsibility for the editor state).
    if ($lcExit -ne 0) {
        $reasonLabel = switch ($lcExit) {
            1       { "LC_FAILED (compile/link/hard error)" }
            2       { "LC_UNAVAILABLE (no warm LC session)" }
            3       { "LC_TIMEOUT (no terminal marker)" }
            default { "LC exit $lcExit" }
        }
        Write-Host ""
        Write-Host "[compile] $reasonLabel - refusing to dispatch against stale editor state." -ForegroundColor Red
        Write-Host "[compile] To fix:" -ForegroundColor Yellow
        Write-Host "  1. Stop the editor: scripts/ue/test/unit/persistent_editor_stop.ps1" -ForegroundColor Yellow
        Write-Host "  2. Full rebuild:    scripts/ue/build/rebuild_module_safe.ps1 -ModuleName <name>" -ForegroundColor Yellow
        Write-Host "  3. Restart editor:  scripts/ue/test/unit/persistent_editor_start.ps1" -ForegroundColor Yellow
        Write-Host "  4. Rerun iterate.ps1" -ForegroundColor Yellow
        Write-Host "  Or bypass compile entirely: -CompileMode None (you own the state)" -ForegroundColor Gray
        exit 5
    }
    Write-Host "[compile] LC_OK -> proceeding to dispatch" -ForegroundColor Green
}

# --- Dispatch -------------------------------------------------------------

if ($warm) {
    $persistentRun = Join-Path $PSScriptRoot "persistent_editor_run.ps1"
    $persistentStart = Join-Path $PSScriptRoot "persistent_editor_start.ps1"
    $persistentTimeout = [Math]::Max($TimeoutSeconds, 300)

    function Invoke-TestDispatch {
        if ($script:useTagRun) {
            & $persistentRun -TestFilter "ProjectIntegrationTests" -TagExpression $script:Tags `
                -TimeoutSeconds $persistentTimeout
        } else {
            & $persistentRun -TestFilter $script:TestFilter -TimeoutSeconds $persistentTimeout
        }
        return $LASTEXITCODE
    }

    $dispatchExit = Invoke-TestDispatch

    # Auto-recover from editor crash (exit 1 from persistent_editor_run
    # = "editor exited during run"). The test probably hit a check()/
    # assertion; restart editor once and rerun. Second crash = real bug,
    # bubble up. Avoids manual "restart editor -> rerun" drudgery that
    # otherwise stalls every crash-inducing test iteration.
    if ($dispatchExit -eq 1) {
        Write-Host ""
        Write-Host "[iterate] Editor crashed during test. Auto-restarting + rerunning once." -ForegroundColor Yellow

        # Editor process is already gone; stale pid/ready files may linger.
        # persistent_editor_start.ps1 with -Force clears them and relaunches.
        & $persistentStart -Force -TimeoutSeconds 240
        $startExit = $LASTEXITCODE
        if ($startExit -ne 0) {
            Write-Host "[iterate] Editor restart failed (exit $startExit). Leaving original crash exit code." -ForegroundColor Red
            exit 1
        }

        Write-Host "[iterate] Editor restarted. Rerunning test." -ForegroundColor Cyan
        $dispatchExit = Invoke-TestDispatch
        if ($dispatchExit -eq 1) {
            Write-Host "[iterate] Editor crashed AGAIN on retry. Treating as real bug; check Saved/Logs/Alis.log for the assertion." -ForegroundColor Red
        }
    }

    exit $dispatchExit
}

# Cold fallback. run_cpp_tests_safe does not yet support tag expressions;
# tag runs require the warm editor's SetTagFilter console command. If we
# fell through to cold and -Tags was given, surface the gap clearly rather
# than silently ignoring the tag expression.
if ($useTagRun) {
    Write-Host "ERROR: tag-based runs require a warm persistent editor (no cold path)." -ForegroundColor Red
    Write-Host "       Start the editor: .\scripts\ue\test\unit\persistent_editor_start.ps1" -ForegroundColor Yellow
    exit 2
}
$coldRun = Join-Path $PSScriptRoot "run_cpp_tests_safe.ps1"
& $coldRun -TestFilter $TestFilter -TimeoutSeconds $TimeoutSeconds -Mode Gate
exit $LASTEXITCODE
