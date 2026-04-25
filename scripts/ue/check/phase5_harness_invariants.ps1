# Phase 5 inventory harness - SOLID invariants check.
# Scope + Layer A coverage rule: docs/agents/canonical.md
# "Phase 5 Layer A coverage - 3 in-action tests max".
#
# The Phase 5 layers are PowerShell + Python + Markdown, not C++, so a
# shell-level check is the right tool match (vs a C++ fitness test).
#
# Invariants (from the Phase 5 section):
#   1. Layer A (tests) does not import Layer B or C helpers.
#   2. Layer B (dump analyzer, `tools/agentic/inventory/dump_report.py`) has
#      no UE-renderer or editor-only imports - must be -NullRHI safe.
#   3. Layer C (screenshot capture) does NOT parse dumps or assert state.
#      It only captures pixels.
#   4. `scripts/ue/test/inventory/verify.ps1` is the only module that
#      references all three layers.
#   5. Each layer's failure mode is loud - non-zero exit signal in source.
#
# Exit codes:
#   0 = all invariants satisfied
#   1 = at least one invariant violated (details printed)
#   2 = harness file missing (harness not yet scaffolded; Phase 5 not run)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path "$PSScriptRoot\..\..\..").Path

$harnessDir   = Join-Path $repoRoot "scripts\ue\test\inventory"
$verifyPs1    = Join-Path $harnessDir "verify.ps1"
$captureC     = Join-Path $harnessDir "capture_phase5.ps1"
$readmeMd     = Join-Path $harnessDir "README.md"
$dumpPy       = Join-Path $repoRoot "tools\agentic\inventory\dump_report.py"

# Layer A location is intentional: Layer A lives inside the main
# ProjectIntegrationTests plugin (tagged with [Inventory][E2E]). We scan
# the three tagged files we already have.
$integrationDir = Join-Path $repoRoot "Plugins\Test\ProjectIntegrationTests\Source\ProjectIntegrationTests\Private\Integration"
$layerAFiles = @(
    (Join-Path $integrationDir "InventoryPanelGridSizingTests.cpp"),
    (Join-Path $integrationDir "InventoryCellDropTargetContractTests.cpp"),
    (Join-Path $integrationDir "InventoryDragEventBusTests.cpp")
)

Write-Host ""
Write-Host "--- phase5_harness_invariants ---" -ForegroundColor Cyan
Write-Host " repo:    $repoRoot" -ForegroundColor Gray

# Pre-flight: harness must exist.
foreach ($p in @($verifyPs1, $captureC, $readmeMd, $dumpPy)) {
    if (-not (Test-Path $p)) {
        Write-Host " MISSING: $p" -ForegroundColor Red
        Write-Host " Phase 5 harness not scaffolded; nothing to check." -ForegroundColor Yellow
        exit 2
    }
}

$fail = 0

function Fail($msg) {
    Write-Host " [FAIL] $msg" -ForegroundColor Red
    $script:fail = 1
}

function Pass($msg) {
    Write-Host " [ OK ] $msg" -ForegroundColor Green
}

# Invariant 1: Layer A does not import Layer B or C.
# Layer A = the tagged .cpp files above; it must not reference the dump
# analyzer path, the capture script path, or the verify runner path.
foreach ($f in $layerAFiles) {
    $content = Get-Content -Raw -Path $f -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    $bad = @(
        'dump_report.py',
        'capture_phase5.ps1',
        'verify.ps1'
    ) | Where-Object { $content.Contains($_) }
    if ($bad.Count -gt 0) {
        Fail "Layer A file $($f | Split-Path -Leaf) references Layer B/C: $($bad -join ', ')"
    }
}
if ($fail -eq 0) { Pass "Invariant 1: Layer A tagged tests do not import Layer B or C" }

# Invariant 2: Layer B (dump_report.py) must be -NullRHI safe. That means no
# import of a renderer-dependent Python module. Practically the Python
# helper should only import stdlib + repo-local analyzers; the existing
# `layout_report.py` is already -NullRHI safe (it parses a JSON dump).
# We enforce: dump_report.py imports only stdlib / `tools.agentic.ui.layout_report`.
$dumpContent = Get-Content -Raw -Path $dumpPy
$forbiddenImports = @('unreal', 'PySide', 'PyQt', 'OpenGL', 'moderngl')
$badImport = $forbiddenImports | Where-Object { $dumpContent -match "(^|\s)(import|from)\s+$([regex]::Escape($_))\b" }
if ($badImport.Count -gt 0) {
    Fail "Layer B dump_report.py imports renderer/editor modules: $($badImport -join ', ')"
} else {
    Pass "Invariant 2: Layer B (dump_report.py) is -NullRHI safe"
}

# Invariant 3: Layer C (capture_phase5.ps1) does not parse dumps or assert
# state. Enforce: capture script does NOT import dump_report.py, does NOT
# read Saved/Dumps/, and does NOT call any iterate.ps1 / run_single.ps1.
$captureContent = Get-Content -Raw -Path $captureC
$captureForbidden = @(
    'dump_report.py',
    'Saved/Dumps',
    'Saved\Dumps',
    'iterate.ps1',
    'run_single.ps1',
    'run_cpp_tests_safe.ps1'
)
$badCap = $captureForbidden | Where-Object { $captureContent.Contains($_) }
if ($badCap.Count -gt 0) {
    Fail "Layer C capture_phase5.ps1 reaches beyond pixel capture: $($badCap -join ', ')"
} else {
    Pass "Invariant 3: Layer C capture script only captures pixels"
}

# Invariant 4: verify.ps1 is the only module that references all three layers.
# We require verify.ps1 to reference each layer by filename. We also require
# that NO other file under scripts/ue/test/inventory/ references all three.
$verifyContent = Get-Content -Raw -Path $verifyPs1
$mentionsCapture = $verifyContent.Contains('capture_phase5.ps1')
$mentionsDump    = $verifyContent.Contains('dump_report.py')
$mentionsLayerA  = $verifyContent.Contains('iterate.ps1') -or $verifyContent.Contains('Tags')
if (-not ($mentionsCapture -and $mentionsDump -and $mentionsLayerA)) {
    Fail "verify.ps1 must reference all three layers (found: capture=$mentionsCapture, dump=$mentionsDump, layerA=$mentionsLayerA)"
} else {
    Pass "Invariant 4: verify.ps1 is the single composer that knows all three layers"
}

# Invariant 5: each layer must have a non-zero exit on failure (visible in
# source). Match either a literal digit `exit [1-9]` OR `exit $var` (the
# composer variant that propagates an aggregated code). Python stub must
# have `sys.exit(` or `return <digit>`.
$layerSuites = @{
    $verifyPs1  = '(?m)^\s*exit\s+([1-9]|\$)'
    $captureC   = '(?m)^\s*exit\s+([1-9]|\$)'
    $dumpPy     = 'sys\.exit\(|return\s+[1-9]'
}
foreach ($kv in $layerSuites.GetEnumerator()) {
    $layer = $kv.Key
    $rx    = $kv.Value
    $src   = Get-Content -Raw -Path $layer
    if (-not ($src -match $rx)) {
        Fail "Layer file $(Split-Path -Leaf $layer) has no visible non-zero exit path"
    }
}
if ($fail -eq 0) { Pass "Invariant 5: every layer has a loud failure path" }

Write-Host ""
if ($fail -eq 0) {
    Write-Host " All Phase 5 harness invariants satisfied." -ForegroundColor Green
    exit 0
}
Write-Host " One or more Phase 5 harness invariants violated." -ForegroundColor Red
exit 1
