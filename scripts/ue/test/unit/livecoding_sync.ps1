# Send LiveCoding.CompileSync to the warm editor and wait for a terminal
# status. Contract + pitfalls: docs/agents/canonical.md "Dev Loop Contract"
# and "Dev loop pitfalls (verified ...)".
#
# Why a separate helper (SOLID):
#   iterate.ps1 orchestrates the dev loop (filter gate -> compile -> dispatch).
#   The Live Coding compile step is a self-contained sub-responsibility with
#   its own preconditions, IPC, log-scrape, and terminal-status set. Splitting
#   it out keeps iterate.ps1 a thin composer.
#
# Reuses (no C++ changes needed):
#   - ProjectIntegrationTestsModule routes arbitrary GEngine->Exec via
#     command.txt (verified: Private/ProjectIntegrationTestsModule.cpp:74
#     `GEngine->Exec(nullptr, *Line)`).
#   - persistent_editor_run.ps1 already wrote the pattern of "snapshot log
#     offset -> write command.txt -> tail slice for terminal marker" for
#     automation dispatch. This script mirrors that shape.
#
# UE 5.7 Live Coding terminal log markers (observed against the actual
# shipped editor; engine source in LiveCodingModule2.cpp has different
# strings because that module is not what ships by default).
#
# Real markers from a warm editor's editor.log:
#   - success: "LogLiveCoding: Warning: Live coding succeeded" (ALWAYS appears on a successful patch, even when no data types changed)
#   - failure: "LogLiveCoding: Warning: Live coding failed"    (observed failure path)
#   - server spam we must IGNORE: "LogLiveCodingServer: Error: ... Cannot find image section .voltbl"  (harmless startup plugin-load noise)
#
# Other potentially-visible markers:
#   - "Patch creation for module ... successful" (per-module, fires even when the editor later rejects the patch)
#   - "---------- Finished" (end-of-patch wrapper; doesn't discriminate success vs fail)
#   - "Compile error:" / "Link error:" (toolchain-level failure)
# We watch for the decisive LogLiveCoding warning first; the compile/link
# markers are fallback signals for the toolchain-fail case.
#
# Usage:
#   .\livecoding_sync.ps1
#   .\livecoding_sync.ps1 -TimeoutSeconds 90
#
# Exit codes (map to Phase 1 terminal-status set in the todo):
#   0   = LC_OK          - patching complete, safe to dispatch test
#   1   = LC_FAILED      - compile / link / hard error (reason logged)
#   2   = LC_UNAVAILABLE - no warm persistent editor (no LC session)
#   3   = LC_TIMEOUT     - no terminal marker within TimeoutSeconds
#
# The caller (iterate.ps1) maps any non-zero exit to "fall back to full
# compile before dispatching the test". No test dispatch is allowed before
# one of these terminal statuses is observed (Phase 1 hard invariant).

param(
    [int]$TimeoutSeconds = 60,
    [int]$PollMs = 500
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$artifactDir = Join-Path $root "artifacts\persistent"
$pidFile   = Join-Path $artifactDir "editor.pid"
$cmdFile   = Join-Path $artifactDir "command.txt"
$editorLog = Join-Path $artifactDir "editor.log"

Write-Host ""
Write-Host "--- livecoding_sync ---" -ForegroundColor Cyan

# -- Precondition: warm editor with an alive PID. Otherwise LC_UNAVAILABLE.
if (-not (Test-Path $pidFile)) {
    Write-Host " LC_UNAVAILABLE: no PID file at $pidFile" -ForegroundColor Yellow
    exit 2
}
$editorPid = (Get-Content $pidFile -ErrorAction SilentlyContinue | Select-Object -First 1)
if (-not $editorPid) {
    Write-Host " LC_UNAVAILABLE: PID file empty" -ForegroundColor Yellow
    exit 2
}
try {
    $proc = Get-Process -Id $editorPid -ErrorAction Stop
    if (-not ($proc.ProcessName -like "UnrealEditor*")) {
        Write-Host " LC_UNAVAILABLE: PID $editorPid not UnrealEditor (name=$($proc.ProcessName))" -ForegroundColor Yellow
        exit 2
    }
} catch {
    Write-Host " LC_UNAVAILABLE: PID $editorPid not alive" -ForegroundColor Yellow
    exit 2
}

# -- Snapshot log offset BEFORE dispatch; markers appear after this offset.
$startOffset = 0
if (Test-Path $editorLog) {
    $startOffset = (Get-Item $editorLog).Length
}

# -- Dispatch: the module reads one command line and Exec()s it.
"LiveCoding.CompileSync" | Out-File -FilePath $cmdFile -Encoding ASCII -Force
Write-Host " dispatched: LiveCoding.CompileSync (baseline byte offset $startOffset)" -ForegroundColor Gray

# -- Poll the log slice for a terminal marker. Order of checks matters:
# patching-done is the only true success; error markers beat it if present.
$startTime = Get-Date
$terminal = $null
$reason = $null

while ($true) {
    if ($proc.HasExited) {
        Write-Host " LC_FAILED: editor exited during compile (code $($proc.ExitCode))" -ForegroundColor Red
        exit 1
    }

    if (Test-Path $editorLog) {
        try {
            $fs = [System.IO.File]::Open($editorLog, [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            try {
                if ($fs.Length -gt $startOffset) {
                    $fs.Seek($startOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
                    $reader = [System.IO.StreamReader]::new($fs)
                    $slice = $reader.ReadToEnd()
                    $reader.Dispose()

                    # Decisive markers first. LogLiveCoding Warning line is
                    # the ground-truth signal the editor emits after the
                    # patch is applied (or after it has been rejected).
                    # IMPORTANT: "LogLiveCodingServer: Error: Cannot find
                    # image section .voltbl" is HARMLESS startup noise from
                    # the LC agent's plugin scanner - do NOT treat it as
                    # LC_FAILED.
                    # Verbosity varies: body-only edits emit "Display:",
                    # edits that change reflected types emit "Warning:".
                    # Match either.
                    if ($slice -match 'LogLiveCoding: (?:Display|Warning|Error): Live coding failed[^\r\n]*') {
                        $terminal = "LC_FAILED"
                        $reason = $Matches[0].Trim()
                        break
                    }
                    if ($slice -match 'LogLiveCoding: (?:Display|Warning): Live coding succeeded[^\r\n]*') {
                        $terminal = "LC_OK"
                        break
                    }
                    # Toolchain-level failure fallbacks. These fire before
                    # the LogLiveCoding warning in some scenarios (failed
                    # compile or link never gets to the patch stage).
                    if ($slice -match 'Compile error:[^\r\n]*') {
                        $terminal = "LC_FAILED"
                        $reason = "compile error: " + $Matches[0]
                        break
                    }
                    if ($slice -match 'Link error:[^\r\n]*') {
                        $terminal = "LC_FAILED"
                        $reason = "link error: " + $Matches[0]
                        break
                    }
                }
            } finally {
                $fs.Dispose()
            }
        } catch {
            # Log rotated or locked; retry next poll.
        }
    }

    $elapsed = ((Get-Date) - $startTime).TotalSeconds
    if ($elapsed -ge $TimeoutSeconds) {
        Write-Host " LC_TIMEOUT: no terminal marker in ${TimeoutSeconds}s" -ForegroundColor Red
        exit 3
    }

    Start-Sleep -Milliseconds $PollMs
}

$elapsed = [math]::Round(((Get-Date) - $startTime).TotalSeconds, 1)
switch ($terminal) {
    "LC_OK" {
        Write-Host " LC_OK: patching done in ${elapsed}s" -ForegroundColor Green
        exit 0
    }
    "LC_FAILED" {
        Write-Host " LC_FAILED in ${elapsed}s: $reason" -ForegroundColor Red
        exit 1
    }
    default {
        Write-Host " LC_FAILED in ${elapsed}s: unknown terminal state '$terminal'" -ForegroundColor Red
        exit 1
    }
}
