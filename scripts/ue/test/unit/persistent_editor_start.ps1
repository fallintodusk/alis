# Launch a persistent UnrealEditor-Cmd that stays warm for repeated test runs.
#
# Why: see `ProjectIntegrationTestsModule.cpp`. Boot + map-load is ~60s per
# cold invocation; persistent mode pays that once and each subsequent filter
# dispatches in ~5-10s.
#
# Usage:
#   .\persistent_editor_start.ps1 [-Map "/MainMenuWorld/Maps/MainMenu_Persistent"]
#     [-TimeoutSeconds 180] [-Force]
#
# Exit codes:
#   0 = editor is ready to accept commands (ready marker present)
#   1 = timeout or launch failure
#   2 = already running (unless -Force)

param(
    [string]$Map = "/MainMenuWorld/Maps/MainMenu_Persistent",
    [int]$TimeoutSeconds = 180,
    [switch]$Force = $false
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$projectRoot = (Resolve-Path "$PSScriptRoot\..\..\..\..\").Path
$artifactDir = Join-Path $root "artifacts\persistent"
$pidFile   = Join-Path $artifactDir "editor.pid"
$logFile   = Join-Path $artifactDir "editor.log"
$readyFile = Join-Path $artifactDir "ready.marker"
$cmdFile   = Join-Path $artifactDir "command.txt"

New-Item -Force -ItemType Directory -Path $artifactDir | Out-Null

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Persistent Editor Start" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Map:         $Map"
Write-Host "Timeout:     $TimeoutSeconds sec"
Write-Host "ArtifactDir: $artifactDir"

# Step 0: Detect an already-running persistent editor.
if (Test-Path $pidFile) {
    $existingPid = (Get-Content $pidFile -ErrorAction SilentlyContinue | Select-Object -First 1)
    if ($existingPid) {
        try {
            $proc = Get-Process -Id $existingPid -ErrorAction Stop
            if ($proc.ProcessName -like "UnrealEditor*") {
                if (-not $Force) {
                    Write-Host "  Editor already running (PID $existingPid). Use -Force to restart." -ForegroundColor Yellow
                    exit 2
                }
                Write-Host "  Killing existing editor PID $existingPid (forced)..." -ForegroundColor Yellow
                Stop-Process -Id $existingPid -Force -ErrorAction SilentlyContinue
            }
        } catch {
            # Stale PID file; ignore.
        }
    }
}

# Clear stale artifacts so readiness detection is unambiguous.
Remove-Item -Force $pidFile, $readyFile, $cmdFile -ErrorAction SilentlyContinue

# Kill any rogue editors that do not own a PID file (they would eat focus
# and warm up worker processes we don't want to track).
Get-Process | Where-Object {
    $_.ProcessName -like "UnrealEditor*"
} | Stop-Process -Force -ErrorAction SilentlyContinue

# Step 1: Resolve UE path.
$configDir = Join-Path $PSScriptRoot "..\..\..\config"
. (Join-Path $configDir "Resolve-UEConfig.ps1")
$config = Resolve-UEConfig -ConfigDir $configDir
$uePath = $config.UE_PATH.Replace("/", "\")
if (-not $uePath) {
    Write-Host "  ERROR: UE_PATH not found. Create scripts\config\ue_path.local.conf" -ForegroundColor Red
    exit 1
}
$editorCmdPath = Join-Path $uePath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path $editorCmdPath)) {
    Write-Host "  ERROR: UnrealEditor-Cmd not found: $editorCmdPath" -ForegroundColor Red
    exit 1
}
$projectPath = Join-Path $projectRoot "Alis.uproject"

# Step 2: Launch the editor WITHOUT -ExecCmds or -testexit. It boots,
# loads the map, and stays alive. The project-side file watcher
# (ProjectIntegrationTestsModule) picks up commands written to command.txt.
$mapArg = if ($Map) { "`"$Map`"" } else { "" }
$argsLine = "`"$projectPath`" $mapArg -unattended -nopause -nosplash -nosound -NoMessaging -log -stdout -FullStdOutLogOutput"

Write-Host "[1/2] Launching UnrealEditor-Cmd..." -ForegroundColor Yellow
Write-Host "  $editorCmdPath $argsLine" -ForegroundColor Gray

# Launch with Start-Process redirections. UnrealEditor-Cmd writes all its
# log output to stdout when -log -stdout is set; redirecting captures it
# into editor.log without tying up this PowerShell session.
if (Test-Path $logFile) {
    try { Remove-Item -Force $logFile } catch { }
}
$stderrLog = $logFile + ".err"
if (Test-Path $stderrLog) {
    try { Remove-Item -Force $stderrLog } catch { }
}

# Build ArgumentList carefully: Start-Process handles quoting per-element.
# UE project path must be quoted because it may contain spaces.
$argList = @(
    "`"$projectPath`"",
    $Map,
    "-unattended",
    "-nopause",
    "-nosplash",
    "-nosound",
    "-NoMessaging",
    "-log",
    "-stdout",
    "-FullStdOutLogOutput"
)
# Filter empty (no Map case).
$argList = $argList | Where-Object { $_ -ne "" -and $_ -ne $null }

$proc = Start-Process -FilePath $editorCmdPath `
    -ArgumentList $argList `
    -WorkingDirectory $projectRoot `
    -WindowStyle Hidden `
    -RedirectStandardOutput $logFile `
    -RedirectStandardError  $stderrLog `
    -PassThru

$proc.Id | Out-File -FilePath $pidFile -Encoding ASCII -Force

Write-Host "  PID: $($proc.Id)" -ForegroundColor Green
Write-Host "  Log: $logFile" -ForegroundColor Gray

# Step 3: Block until the ready marker appears OR the process exits OR we
# hit the timeout. We poll the filesystem rather than tailing the log so
# the contract survives log-format changes.
Write-Host "[2/2] Waiting for editor to become ready..." -ForegroundColor Yellow
$startTime = Get-Date
$lastDot = Get-Date
while ($true) {
    if ($proc.HasExited) {
        Write-Host "  FAIL: editor exited with code $($proc.ExitCode) before ready." -ForegroundColor Red
        Remove-Item -Force $pidFile -ErrorAction SilentlyContinue
        Write-Host "  Log tail:" -ForegroundColor Yellow
        Get-Content $logFile -Tail 30 -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "    $_" }
        exit 1
    }
    if (Test-Path $readyFile) {
        $elapsed = [math]::Round(((Get-Date) - $startTime).TotalSeconds, 1)
        Write-Host "  READY after $elapsed sec" -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Cyan
        exit 0
    }
    $elapsed = ((Get-Date) - $startTime).TotalSeconds
    if ($elapsed -ge $TimeoutSeconds) {
        Write-Host "  TIMEOUT after $TimeoutSeconds sec - editor did not become ready." -ForegroundColor Red
        try { $proc.Kill() } catch { }
        Remove-Item -Force $pidFile -ErrorAction SilentlyContinue
        Write-Host "  Log tail:" -ForegroundColor Yellow
        Get-Content $logFile -Tail 30 -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "    $_" }
        exit 1
    }
    if (((Get-Date) - $lastDot).TotalSeconds -ge 5) {
        Write-Host "  ... $([math]::Round($elapsed, 0))s elapsed" -ForegroundColor DarkGray
        $lastDot = Get-Date
    }
    Start-Sleep -Milliseconds 500
}
