# Layer C of the Phase 5 inventory verification harness: capture
# per-checkpoint screenshots of the inventory UI.
#
# SOLID contract (enforced by the Phase5 fitness test, deferred):
#   - This script does NOT parse dumps or assert state. It only captures
#     pixels. If the capture succeeds, exit 0 even if the UI is visually
#     broken - downstream review (human, or future pixel-diff v2) decides.
#   - Output directory is deterministic: Saved/Screenshots/Phase5/<run-id>/.
#   - Filenames are deterministic: <checkpoint>_<NNN>.png.
#
# Mechanism: UE's console command `HighResShot <multiplier>` captures the
# current viewport at the requested resolution. Dispatched via the
# persistent editor's command.txt IPC (reuses the generic GEngine->Exec
# pass-through in ProjectIntegrationTestsModule).
#
# Checkpoints (v1; extend as Layer A drives more flows):
#   inventory_open  - inventory panel initial display
#   drag_midway     - mid-drag with cursor decorator visible
#   drop_complete   - after-drop final state
#
# Each checkpoint fires a HighResShot, waits for the file to appear under
# Saved/Screenshots/WindowsEditor/, then renames/moves it into the
# Phase5/<run-id>/ structure.
#
# Precondition: persistent editor warm. Without a warm editor there is no
# viewport to snapshot and we exit 2 (LC_UNAVAILABLE-style, consistent with
# livecoding_sync.ps1 semantics).
#
# Exit codes:
#   0  = all checkpoints captured
#   1  = at least one capture failed (file never appeared)
#   2  = no warm editor
#   3  = timeout waiting for a specific capture

param(
    [int]$CaptureTimeoutSeconds = 15,
    [int]$HighResMultiplier = 1,

    # Checkpoint names. Extend in lockstep with Layer A flows.
    [string[]]$Checkpoints = @("inventory_open", "drag_midway", "drop_complete"),

    # Override output dir (default: Saved/Screenshots/Phase5/<timestamp>/)
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
$artifactDir = Join-Path $projectRoot "scripts\ue\artifacts\persistent"
$pidFile = Join-Path $artifactDir "editor.pid"
$cmdFile = Join-Path $artifactDir "command.txt"

$shotsIn = Join-Path $projectRoot "Saved\Screenshots\WindowsEditor"
if (-not $OutputDir) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputDir = Join-Path $projectRoot "Saved\Screenshots\Phase5\$stamp"
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

Write-Host ""
Write-Host "--- capture_phase5 ---" -ForegroundColor Cyan
Write-Host " OutputDir:   $OutputDir" -ForegroundColor Gray
Write-Host " Checkpoints: $($Checkpoints -join ', ')" -ForegroundColor Gray

# Precondition: warm editor.
if (-not (Test-Path $pidFile)) {
    Write-Host " SKIP: no warm persistent editor (run persistent_editor_start.ps1)" -ForegroundColor Yellow
    exit 2
}
$editorPid = (Get-Content $pidFile -ErrorAction SilentlyContinue | Select-Object -First 1)
if (-not $editorPid) { Write-Host " SKIP: PID file empty" -ForegroundColor Yellow; exit 2 }
try {
    $proc = Get-Process -Id $editorPid -ErrorAction Stop
    if (-not ($proc.ProcessName -like "UnrealEditor*")) {
        Write-Host " SKIP: PID $editorPid is not UnrealEditor" -ForegroundColor Yellow
        exit 2
    }
} catch {
    Write-Host " SKIP: PID $editorPid not alive" -ForegroundColor Yellow
    exit 2
}

# Capture loop.
$ix = 0
$anyFailed = $false
foreach ($cp in $Checkpoints) {
    $ix++
    # Marker file we will watch for. UE writes HighresScreenshot<N>.png by
    # default; easier to detect is "any new .png in $shotsIn since now".
    $baseline = @()
    if (Test-Path $shotsIn) {
        $baseline = Get-ChildItem $shotsIn -Filter "*.png" -File | ForEach-Object { $_.FullName }
    }

    # Dispatch the HighResShot console command. The module Execs the line.
    "HighResShot $HighResMultiplier" | Out-File -FilePath $cmdFile -Encoding ASCII -Force

    # Poll for a new .png file.
    $deadline = (Get-Date).AddSeconds($CaptureTimeoutSeconds)
    $newFile = $null
    while ((Get-Date) -lt $deadline) {
        if (Test-Path $shotsIn) {
            $now = Get-ChildItem $shotsIn -Filter "*.png" -File | ForEach-Object { $_.FullName }
            $diff = $now | Where-Object { $baseline -notcontains $_ }
            if ($diff.Count -gt 0) {
                # Take the newest added file.
                $newFile = ($diff | ForEach-Object { Get-Item $_ } | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
                break
            }
        }
        Start-Sleep -Milliseconds 300
    }

    if (-not $newFile) {
        Write-Host " [${cp}] TIMEOUT after ${CaptureTimeoutSeconds}s - no new .png" -ForegroundColor Red
        $anyFailed = $true
        continue
    }

    $destName = "{0:D3}_{1}.png" -f $ix, $cp
    $destPath = Join-Path $OutputDir $destName
    Move-Item -LiteralPath $newFile -Destination $destPath -Force
    Write-Host " [${cp}] -> $destName" -ForegroundColor Green
}

if ($anyFailed) {
    Write-Host " Some captures timed out" -ForegroundColor Red
    exit 1
}
Write-Host " All $($Checkpoints.Count) checkpoints captured" -ForegroundColor Green
exit 0
