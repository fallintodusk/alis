# Phase 5 top-level runner: composes the 3-layer inventory in-action
# verification harness (Layer A tests, Layer B dump, Layer C screenshot).
#
# Each layer is independent and produces its own artifact. -Layers selects
# a subset. Exit code is aggregated: non-zero if any selected layer fails.
#
# Layers:
#   A - automation tests (in-action, tag-narrowed via Phase 2 single-tag
#       CVar filter; UE 5.7 tag CVar is BasicString substring match ONLY,
#       no boolean operators - see canonical.md "Dev Loop Contract").
#   B - UI state dump:
#       wraps tools/agentic/inventory/dump_report.py, which extends the
#       existing tools/agentic/ui/layout_report.py with inventory-specific
#       checkpoints.
#   C - screenshot capture:
#       scripts/ue/test/inventory/capture_phase5.ps1 (warm editor only;
#       uses UE's HighResShot console command).
#
# SOLID invariants enforced by `scripts/ue/check/phase5_harness_invariants.ps1`:
#   1. Layer A does not import Layer B or C helpers.
#   2. Layer B runs headless (must work with -NullRHI).
#   3. Layer C does NOT parse dumps or assert state; only captures pixels.
#   4. `verify.ps1` is the only module that knows all three exist.
#   5. Each layer's failure mode is loud: non-zero exit + one-line summary.
#
# Usage:
#   .\verify.ps1                          # all three layers
#   .\verify.ps1 -Layers A,B              # just tests + dump
#   .\verify.ps1 -Layers A                # tests only
#   .\verify.ps1 -TagExpression "Fast"    # narrow Layer A to Fast tests
#
# Scope + Layer A rule: docs/agents/canonical.md
#   "Phase 5 Layer A coverage - 3 in-action tests max".

param(
    [ValidateSet("A", "B", "C")]
    [string[]]$Layers = @("A", "B", "C"),

    # Single-token tag used by Layer A. Default "Inventory" narrows the
    # pool to the 10 currently-tagged inventory tests. When Phase 5 Layer
    # A E2E tests land with a dedicated "[Phase5]" tag, switch this default
    # to "Phase5".
    # UE 5.7 limitation: tag filter is BasicString substring; single token
    # only. Do NOT pass boolean expressions like "Fast && Inventory" -
    # they become literal substrings and match zero tests.
    [string]$TagExpression = "Inventory",

    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = "Stop"
# verify.ps1 lives at scripts/ue/test/inventory/; repo root is 4 levels up.
$root = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$unitScripts = Join-Path $root "scripts\ue\test\unit"

$results = @{}

Write-Host ""
Write-Host "================ verify (Phase 5) ================" -ForegroundColor Cyan
Write-Host " Layers:  $($Layers -join ',')"                    -ForegroundColor White
Write-Host " Tag:     $TagExpression"                          -ForegroundColor White
Write-Host "===================================================" -ForegroundColor Cyan

# --- Layer A: automation tests ---
if ($Layers -contains "A") {
    Write-Host ""
    Write-Host "[A] automation tests..." -ForegroundColor Cyan
    $iterate = Join-Path $unitScripts "iterate.ps1"
    & $iterate -Mode Gate -Tags $TagExpression -TimeoutSeconds $TimeoutSeconds -CompileMode None
    $results["A"] = $LASTEXITCODE
    Write-Host "[A] exit: $($results['A'])" -ForegroundColor $(if ($results["A"] -eq 0) { "Green" } else { "Red" })
}

# --- Layer B: UI state dump ---
if ($Layers -contains "B") {
    Write-Host ""
    Write-Host "[B] UI state dump..." -ForegroundColor Cyan
    $dumpScript = Join-Path $root "tools\agentic\inventory\dump_report.py"
    if (-not (Test-Path $dumpScript)) {
        Write-Host "[B] SKIP: $dumpScript not present yet (Phase 5 Layer B pending)" -ForegroundColor Yellow
        $results["B"] = 0  # not a failure; the layer is documented-but-unfinished
    } else {
        # Reuse existing Python resolver (preferred: system python > py > UE bundled).
        $pythonExe = $null
        foreach ($candidate in @("python", "python3", "py")) {
            if (Get-Command $candidate -ErrorAction SilentlyContinue) { $pythonExe = $candidate; break }
        }
        if (-not $pythonExe) {
            $uePy = "<ue-path>\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
            if (Test-Path $uePy) { $pythonExe = $uePy }
        }
        if (-not $pythonExe) {
            Write-Host "[B] FAIL: no python found" -ForegroundColor Red
            $results["B"] = 1
        } else {
            & $pythonExe $dumpScript
            $results["B"] = $LASTEXITCODE
            Write-Host "[B] exit: $($results['B'])" -ForegroundColor $(if ($results["B"] -eq 0) { "Green" } else { "Red" })
        }
    }
}

# --- Layer C: screenshot capture ---
if ($Layers -contains "C") {
    Write-Host ""
    Write-Host "[C] screenshot capture..." -ForegroundColor Cyan
    $captureScript = Join-Path $PSScriptRoot "capture_phase5.ps1"
    & $captureScript
    $results["C"] = $LASTEXITCODE
    Write-Host "[C] exit: $($results['C'])" -ForegroundColor $(if ($results["C"] -eq 0) { "Green" } else { "Red" })
}

# --- Aggregate ---
Write-Host ""
Write-Host "================ verify summary ================" -ForegroundColor Cyan
foreach ($key in @("A","B","C")) {
    if ($results.ContainsKey($key)) {
        $v = $results[$key]
        Write-Host " Layer ${key}: exit $v" -ForegroundColor $(if ($v -eq 0) { "Green" } else { "Red" })
    }
}
Write-Host "=================================================" -ForegroundColor Cyan

$overall = 0
foreach ($v in $results.Values) { if ($v -ne 0) { $overall = 1; break } }
exit $overall
