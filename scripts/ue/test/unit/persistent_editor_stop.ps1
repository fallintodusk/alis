# Gracefully terminate the persistent editor, removing its PID file and
# optionally keeping the rolling log for post-mortem.
#
# Usage:
#   .\persistent_editor_stop.ps1 [-KeepLog]

param(
    [switch]$KeepLog = $false
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$artifactDir = Join-Path $root "artifacts\persistent"
$pidFile    = Join-Path $artifactDir "editor.pid"
$readyFile  = Join-Path $artifactDir "ready.marker"
$cmdFile    = Join-Path $artifactDir "command.txt"
$logFile    = Join-Path $artifactDir "editor.log"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Persistent Editor Stop" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if (-not (Test-Path $pidFile)) {
    Write-Host "  No PID file at $pidFile - nothing to stop." -ForegroundColor Yellow
} else {
    $editorPid = (Get-Content $pidFile -ErrorAction SilentlyContinue | Select-Object -First 1)
    if ($editorPid) {
        try {
            $proc = Get-Process -Id $editorPid -ErrorAction Stop
            if ($proc.ProcessName -like "UnrealEditor*") {
                # Try a graceful Quit via the command file first, then fall back
                # to Stop-Process if the editor doesn't exit within 10 sec.
                "Quit" | Out-File -FilePath $cmdFile -Encoding ASCII -Force
                Write-Host "  Sent graceful Quit command to PID $editorPid" -ForegroundColor Yellow
                $deadline = (Get-Date).AddSeconds(10)
                while (-not $proc.HasExited -and (Get-Date) -lt $deadline) {
                    Start-Sleep -Milliseconds 300
                }
                if (-not $proc.HasExited) {
                    Write-Host "  Graceful Quit did not exit in 10s; forcing Stop-Process..." -ForegroundColor Yellow
                    Stop-Process -Id $editorPid -Force -ErrorAction SilentlyContinue
                }
                Write-Host "  PID $editorPid terminated." -ForegroundColor Green
            } else {
                Write-Host "  PID $editorPid is not an UnrealEditor process; ignoring." -ForegroundColor Yellow
            }
        } catch {
            Write-Host "  PID $editorPid is not alive." -ForegroundColor Yellow
        }
    }
}

Remove-Item -Force $pidFile, $readyFile, $cmdFile -ErrorAction SilentlyContinue
if (-not $KeepLog -and (Test-Path $logFile)) {
    # Keep the log but rotate it so a fresh start gets a clean file while
    # post-mortem is still possible.
    $stamp = (Get-Date).ToString("yyyyMMdd_HHmmss")
    $rotated = Join-Path $artifactDir "editor.prev_$stamp.log"
    try {
        Move-Item -Force $logFile $rotated
        Write-Host "  Rotated log -> $rotated" -ForegroundColor Gray
    } catch { }
}

Write-Host "========================================" -ForegroundColor Cyan
exit 0
