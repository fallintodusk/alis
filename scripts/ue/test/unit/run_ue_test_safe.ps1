# UE GameFeature Registration Test - Safe Wrapper with Timeout & Process Kill
# Usage: .\run_ue_test_safe.ps1 [-PluginName ProjectMenuExperienceGF]

param(
    [string]$PluginName = "ProjectMenuExperienceGF",
    [int]$TimeoutSeconds = 180  # 3 minutes
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$stepN = $env:OVERNIGHT_STEP
if (-not $stepN) { $stepN = "manual" }
$logDir = Join-Path $root "artifacts\overnight\step-$stepN"
New-Item -Force -ItemType Directory -Path $logDir | Out-Null

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "UE GameFeature Registration Test (Safe)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Plugin:  $PluginName"
Write-Host "Timeout: $TimeoutSeconds seconds"
Write-Host "LogDir:  $logDir"
Write-Host ""

# Step 1: Kill any existing UE processes
Write-Host "[1/5] Cleaning existing UE processes..." -ForegroundColor Yellow
Get-Process | Where-Object {
    $_.ProcessName -like "UnrealEditor*" -or
    $_.ProcessName -like "UEBuildWorker*" -or
    $_.ProcessName -like "ShaderCompileWorker*"
} | Stop-Process -Force -ErrorAction SilentlyContinue
Write-Host "  Done" -ForegroundColor Green

# Step 2: Clean old log
$projectRoot = (Resolve-Path "$PSScriptRoot\..\..\..\..\").Path
$ueLogPath = Join-Path $projectRoot "Saved\Logs\Alis.log"
Write-Host "[2/5] Cleaning old UE log..." -ForegroundColor Yellow
if (Test-Path $ueLogPath) {
    Remove-Item $ueLogPath -Force
}
Write-Host "  Done" -ForegroundColor Green

# Step 3: Start UnrealEditor with timeout
Write-Host "[3/5] Starting Unreal Editor..." -ForegroundColor Yellow

# Read UE_PATH from config (single source of truth)
$configPath = Join-Path $PSScriptRoot "..\..\..\config\ue_path.conf"
$uePath = $null
if (Test-Path $configPath) {
    $uePath = (Get-Content $configPath | Where-Object { $_ -match "^UE_PATH=(.+)$" } | Select-Object -First 1) -replace "^UE_PATH=", "" | ForEach-Object { $_.Trim().Replace("/", "\") }
}
if (-not $uePath) {
    Write-Host "  ERROR: UE_PATH not found in $configPath" -ForegroundColor Red
    exit 1
}
Write-Host "  UE_PATH: $uePath" -ForegroundColor Gray
$editorPath = Join-Path $uePath "Engine\Binaries\Win64\UnrealEditor.exe"
if (-not (Test-Path $editorPath)) { throw "UnrealEditor not found: $editorPath" }
$projectRoot = (Resolve-Path "$PSScriptRoot\..\..\..\..\").Path
$projectPath = Join-Path $projectRoot "Alis.uproject"

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $editorPath
$psi.Arguments = "`"$projectPath`" -log -stdout"
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true

$proc = New-Object System.Diagnostics.Process
$proc.StartInfo = $psi
$proc.Start() | Out-Null

# Wait for editor to initialize (check for LogGameFeatures in log)
$elapsed = 0
$checkInterval = 2
$initialized = $false

while ($elapsed -lt $TimeoutSeconds) {
    Start-Sleep -Seconds $checkInterval
    $elapsed += $checkInterval

    if (Test-Path $ueLogPath) {
        $logContent = Get-Content $ueLogPath -Raw
        if ($logContent -match "LogGameFeatures") {
            Write-Host "  Editor initialized after $elapsed seconds" -ForegroundColor Green
            $initialized = $true
            break
        }
    }

    Write-Host "  Waiting... ($elapsed/$TimeoutSeconds seconds)" -ForegroundColor Gray
}

if (-not $initialized) {
    Write-Host "  TIMEOUT: Editor did not initialize in $TimeoutSeconds seconds" -ForegroundColor Red
    # Kill process tree
    taskkill /T /F /PID $proc.Id 2>$null | Out-Null
    taskkill /T /F /IM "UnrealEditor*.exe" 2>$null | Out-Null
    exit 124  # Timeout exit code
}

# Step 4: Check logs for registration errors
Write-Host "[4/5] Checking GameFeature registration..." -ForegroundColor Yellow
Start-Sleep -Seconds 2  # Let registration complete

$logContent = Get-Content $ueLogPath -Raw
$ourLog = Join-Path $logDir "Alis.log"
Copy-Item $ueLogPath $ourLog -Force

# Parse for errors
$hasPlugin = $logContent -match $PluginName
$hasMissingData = $logContent -match "Plugin_Missing_GameFeatureData.*$PluginName"
$hasError = $logContent -match "LogGameFeatures: Error.*$PluginName"
$hasLoaded = $logContent -match "Loaded GameFeatureData.*$PluginName"

Write-Host ""
if (-not $hasPlugin) {
    Write-Host "  FAIL: No logs found for $PluginName" -ForegroundColor Red
    $exitCode = 1
} elseif ($hasMissingData) {
    Write-Host "  FAIL: Plugin_Missing_GameFeatureData error" -ForegroundColor Red
    $logContent -split "`n" | Where-Object { $_ -match "Plugin_Missing_GameFeatureData.*$PluginName" } | Write-Host -ForegroundColor Red
    $exitCode = 1
} elseif ($hasError) {
    Write-Host "  FAIL: GameFeature error detected" -ForegroundColor Red
    $logContent -split "`n" | Where-Object { $_ -match "LogGameFeatures: Error.*$PluginName" } | Write-Host -ForegroundColor Red
    $exitCode = 1
} elseif ($hasLoaded) {
    Write-Host "  SUCCESS: GameFeatureData loaded successfully" -ForegroundColor Green
    $logContent -split "`n" | Where-Object { $_ -match "Loaded GameFeatureData.*$PluginName" } | Write-Host -ForegroundColor Green
    $exitCode = 0
} else {
    Write-Host "  WARNING: Plugin registered but no explicit load confirmation" -ForegroundColor Yellow
    $exitCode = 2
}

# Step 5: Kill editor and cleanup
Write-Host "[5/5] Cleaning up..." -ForegroundColor Yellow
if (-not $proc.HasExited) {
    $proc.Kill($true)  # Kill entire tree
}
Get-Process | Where-Object { $_.ProcessName -like "UnrealEditor*" } | Stop-Process -Force -ErrorAction SilentlyContinue

Write-Host "  Done" -ForegroundColor Green
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Full log: $ourLog" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan

# Truncate log for chat (keep last 200 lines)
$logLines = Get-Content $ourLog
if ($logLines.Count -gt 200) {
    $truncated = $logLines | Select-Object -Last 200
    Set-Content -Path "$ourLog.truncated" -Value $truncated
    Write-Host "Truncated log for chat: $ourLog.truncated (last 200 lines)" -ForegroundColor Gray
}

exit $exitCode
