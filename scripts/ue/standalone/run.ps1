#Requires -Version 5.1
<#
.SYNOPSIS
    Launch game in standalone mode

.DESCRIPTION
    Runs UnrealEditor.exe in standalone game mode (-game flag) with optional
    auto-quit and custom flags. Reads UE path from scripts/config/ue_path.conf.

.PARAMETER AutoQuitSeconds
    Auto-quit after N seconds (for automated testing)

.PARAMETER ExtraArgs
    Additional command-line arguments to pass to the game

.EXAMPLE
    .\run.ps1
    Manual run, no auto quit

.EXAMPLE
    .\run.ps1 -AutoQuitSeconds 60
    Auto quit after 60 seconds

.EXAMPLE
    .\run.ps1 -AutoQuitSeconds 120 -ExtraArgs @("-log", "-NoSound")
    Auto quit 120s with extra flags
#>

param(
    [int]$AutoQuitSeconds = 0,
    [string[]]$ExtraArgs = @()
)

$ErrorActionPreference = "Stop"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " run.ps1 - Launching Alis Standalone Game" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "Script location: $($MyInvocation.MyCommand.Path)"
Write-Host "Current dir:     $(Get-Location)"
Write-Host "Date/Time:       $(Get-Date)"
Write-Host ""

# ============================================================
# Load Configuration
# ============================================================

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ConfigDir = Join-Path (Split-Path -Parent (Split-Path -Parent $ScriptDir)) "config"
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$ProjectFile = Join-Path $ProjectRoot "Alis.uproject"

. (Join-Path $ConfigDir "Resolve-UEConfig.ps1")
$config = Resolve-UEConfig -ConfigDir $ConfigDir

$UEPath      = $config.UE_PATH
$Target      = $config.BUILD_TARGET
$BuildConfig = $config.BUILD_CONFIG
$Platform    = $config.BUILD_PLATFORM

Write-Host "Config: $($config.ConfigFile)" -ForegroundColor Green

if (-not $UEPath) {
    Write-Host "[ERROR] UE_PATH not found." -ForegroundColor Red
    Write-Host "Create scripts\\config\\ue_path.local.conf with UE_PATH=<path>." -ForegroundColor Yellow
    exit 1
}

# Apply defaults for missing values and export environment (shared with build.ps1)
if (-not $Target) { $Target = $DefaultTarget }
if (-not $BuildConfig) { $BuildConfig = $DefaultConfig }
if (-not $Platform) { $Platform = $DefaultPlatform }

$Env:UE_PATH        = $UEPath
$Env:BUILD_TARGET   = $Target
$Env:BUILD_CONFIG   = $BuildConfig
$Env:BUILD_PLATFORM = $Platform


# ============================================================
# Resolve paths
# ============================================================

$GameExe = Join-Path $UEPath "Engine\Binaries\Win64\UnrealEditor.exe"
$TmpDir = Join-Path $ProjectRoot "tmp"
$LogTimestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$LogFile = Join-Path $TmpDir "run_standalone_$LogTimestamp.log"

# Create tmp directory
if (-not (Test-Path $TmpDir)) {
    New-Item -ItemType Directory -Path $TmpDir | Out-Null
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " PATHS RESOLVED" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "UE_PATH      = $UEPath"
Write-Host "PROJECT_ROOT = $ProjectRoot"
Write-Host "PROJECT_FILE = $ProjectFile"
Write-Host "GAME_EXE     = $GameExe"
Write-Host "TMP_DIR      = $TmpDir"
Write-Host "LOG_FILE     = $LogFile"
Write-Host ""

# Validate UnrealEditor.exe exists
if (-not (Test-Path $GameExe)) {
    Write-Host "[ERROR] UnrealEditor.exe not found: $GameExe" -ForegroundColor Red
    Write-Host "Create scripts\config\ue_path.local.conf with UE_PATH=<path>" -ForegroundColor Yellow
    exit 1
}

# ============================================================
# Build command line
# ============================================================

$AutoQuitArg = ""
if ($AutoQuitSeconds -gt 0) {
    Write-Host "Auto-quit enabled: $AutoQuitSeconds seconds" -ForegroundColor Yellow
    $AutoQuitArg = "-AutoQuitSeconds=$AutoQuitSeconds"
}

$AllArgs = @("`"$ProjectFile`"", "-game", "-windowed", "-ResX=1280", "-ResY=720", "-log")

if ($AutoQuitArg) {
    $AllArgs += $AutoQuitArg
}

if ($ExtraArgs) {
    $AllArgs += $ExtraArgs
    Write-Host "EXTRA_ARGS   = $($ExtraArgs -join ' ')"
}

Write-Host ""

# ============================================================
# Launch game
# ============================================================

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " LAUNCHING GAME" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Boot Map:    (uses GameDefaultMap from Config/DefaultEngine.ini)"
Write-Host "             = /Game/Project/Maps/Boot/L_OrchestratorBoot"
Write-Host "Mode:        Standalone Game"
Write-Host "Window Size: 1280x720 (windowed)"
Write-Host ""
Write-Host "Full command:"
Write-Host "`"$GameExe`" $($AllArgs -join ' ')"
Write-Host ""

# Write run info to log file
@"
============================================================
run.ps1 execution log
Date: $(Get-Date)
UE_PATH: $UEPath
PROJECT_FILE: $ProjectFile
GAME_EXE: $GameExe
Boot Map: GameDefaultMap from config (L_OrchestratorBoot)
AUTO_QUIT: $AutoQuitSeconds
EXTRA_ARGS: $($ExtraArgs -join ' ')
============================================================

"@ | Out-File -FilePath $LogFile -Encoding UTF8

# Launch game and wait for it to finish
$process = Start-Process -FilePath $GameExe -ArgumentList $AllArgs -PassThru -NoNewWindow

# Wait for game to close
Write-Host "Game is running... (close game window to exit)" -ForegroundColor Green
$process.WaitForExit()

$ExitCode = $process.ExitCode

# Append UE saved log to our combined log
$UESavedLog = Join-Path $ProjectRoot "Saved\Logs\Alis.log"

"`n============================================================" | Out-File -FilePath $LogFile -Append -Encoding UTF8
"Exit code: $ExitCode" | Out-File -FilePath $LogFile -Append -Encoding UTF8
"============================================================" | Out-File -FilePath $LogFile -Append -Encoding UTF8

if (Test-Path $UESavedLog) {
    "`n============================================================" | Out-File -FilePath $LogFile -Append -Encoding UTF8
    "UE Saved Log (Alis.log)" | Out-File -FilePath $LogFile -Append -Encoding UTF8
    "============================================================" | Out-File -FilePath $LogFile -Append -Encoding UTF8
    Get-Content $UESavedLog | Out-File -FilePath $LogFile -Append -Encoding UTF8
} else {
    "WARNING: UE saved log not found at `"$UESavedLog`"" | Out-File -FilePath $LogFile -Append -Encoding UTF8
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " GAME CLOSED" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "Exit code: $ExitCode"
Write-Host ""
Write-Host "Log saved: $LogFile"
Write-Host "  (contains script metadata + complete UE output)"
Write-Host ""

exit $ExitCode
