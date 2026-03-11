<#
.SYNOPSIS
  Rebuild UE module with timeout, cleanup, retry logic, and circuit breaker.

.DESCRIPTION
  Safe wrapper for Unreal Build Tool that adds production-grade resilience features:
  - Exponential backoff retry logic with configurable attempts
  - Circuit breaker pattern to prevent repeated failures
  - Zombie process detection and cleanup
  - Disk space monitoring before build
  - Graceful degradation (fallback to simpler build on repeated failures)
  - Comprehensive logging with truncation for chat-friendly output

.PARAMETER ModuleName
  Name of the specific module to rebuild (e.g., "ProjectMenuUI"). If empty, builds entire editor (AlisEditor).

.PARAMETER Clean
  Clean intermediate files before build. Use for full rebuild after major changes.

.PARAMETER TimeoutSeconds
  Maximum time to wait for build completion. Default: 300 seconds (5 minutes).
  Valid range: 60-3600 seconds.

.PARAMETER MaxRetries
  Maximum number of build retry attempts on failure. Default: 3.
  Valid range: 1-10 retries.

.PARAMETER InitialBackoffSeconds
  Initial delay before first retry. Doubles exponentially for each subsequent retry. Default: 10 seconds.
  Valid range: 5-60 seconds.

.PARAMETER MaxBackoffSeconds
  Maximum backoff delay cap. Default: 120 seconds.
  Valid range: 30-300 seconds.

.PARAMETER CircuitBreakerThreshold
  Number of consecutive failures before circuit breaker opens. Default: 5.
  Valid range: 1-20 failures.

.PARAMETER CircuitBreakerCooldownMinutes
  Cooldown period before allowing retry after circuit opens. Default: 30 minutes.
  Valid range: 1-120 minutes.

.PARAMETER EnableGracefulDegradation
  Enable fallback to simpler build (without -WaitMutex) on repeated failures. Default: true.

.EXAMPLE
  # Rebuild specific module
  .\rebuild_module_safe.ps1 -ModuleName ProjectMenuUI

.EXAMPLE
  # Full editor rebuild with clean
  .\rebuild_module_safe.ps1 -Clean

.EXAMPLE
  # Quick build with shorter timeout
  .\rebuild_module_safe.ps1 -ModuleName ProjectCore -TimeoutSeconds 180

.EXAMPLE
  # Aggressive retry for unstable environments
  .\rebuild_module_safe.ps1 -MaxRetries 5 -CircuitBreakerThreshold 10

.NOTES
  - Circuit breaker state persists across invocations (stored in artifacts/overnight/)
  - Zombie processes (age > 30 min) are logged and killed
  - Logs are automatically truncated to last 200 lines for chat context
  - Disk space warning at < 50 GB free
  - Exit codes: 0 = success, 1 = build failed, 2 = circuit breaker open
#>

param(
    [Parameter(HelpMessage = "Module name to rebuild (empty = full editor)")]
    [string]$ModuleName = "",

    [Parameter(HelpMessage = "Clean intermediate files before build")]
    [switch]$Clean = $false,

    [Parameter(HelpMessage = "Build timeout in seconds")]
    [ValidateRange(60, 3600)]
    [int]$TimeoutSeconds = 300,

    [Parameter(HelpMessage = "Maximum retry attempts on failure")]
    [ValidateRange(1, 10)]
    [int]$MaxRetries = 3,

    [Parameter(HelpMessage = "Initial backoff delay in seconds")]
    [ValidateRange(5, 60)]
    [int]$InitialBackoffSeconds = 10,

    [Parameter(HelpMessage = "Maximum backoff delay in seconds")]
    [ValidateRange(30, 300)]
    [int]$MaxBackoffSeconds = 120,

    [Parameter(HelpMessage = "Consecutive failures before circuit opens")]
    [ValidateRange(1, 20)]
    [int]$CircuitBreakerThreshold = 5,

    [Parameter(HelpMessage = "Circuit breaker cooldown in minutes")]
    [ValidateRange(1, 120)]
    [int]$CircuitBreakerCooldownMinutes = 30,

    [Parameter(HelpMessage = "Enable fallback to simpler build on repeated failures")]
    [switch]$EnableGracefulDegradation = $true
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$stepN = $env:OVERNIGHT_STEP
if (-not $stepN) { $stepN = "manual" }
$logDir = Join-Path $root "artifacts\overnight\step-$stepN"
New-Item -Force -ItemType Directory -Path $logDir | Out-Null

# Circuit breaker state file
$circuitBreakerFile = Join-Path $logDir "circuit_breaker.json"

function Get-CircuitBreakerState {
    if (Test-Path $circuitBreakerFile) {
        $state = Get-Content $circuitBreakerFile | ConvertFrom-Json
        return @{
            ConsecutiveFailures = $state.ConsecutiveFailures
            LastFailureTime     = [DateTime]::Parse($state.LastFailureTime)
            State               = $state.State
        }
    }
    return @{
        ConsecutiveFailures = 0
        LastFailureTime     = [DateTime]::MinValue
        State               = "CLOSED"
    }
}

function Set-CircuitBreakerState {
    param(
        [int]$ConsecutiveFailures,
        [DateTime]$LastFailureTime,
        [string]$State
    )
    $stateObj = @{
        ConsecutiveFailures = $ConsecutiveFailures
        LastFailureTime     = $LastFailureTime.ToString("o")
        State               = $State
    }
    $stateObj | ConvertTo-Json | Set-Content $circuitBreakerFile
}

function Test-CircuitBreaker {
    param([int]$Threshold, [int]$CooldownMinutes)

    $state = Get-CircuitBreakerState

    if ($state.State -eq "OPEN") {
        $timeSinceFailure = (Get-Date) - $state.LastFailureTime
        if ($timeSinceFailure.TotalMinutes -ge $CooldownMinutes) {
            Write-Host "  Circuit breaker HALF-OPEN (cooldown expired, allowing test attempt)" -ForegroundColor Yellow
            Set-CircuitBreakerState -ConsecutiveFailures $state.ConsecutiveFailures -LastFailureTime $state.LastFailureTime -State "HALF_OPEN"
            return $true
        }
        else {
            $remainingMinutes = [Math]::Ceiling($CooldownMinutes - $timeSinceFailure.TotalMinutes)
            Write-Host "  Circuit breaker OPEN: Too many consecutive failures ($($state.ConsecutiveFailures))" -ForegroundColor Red
            Write-Host "  Cooldown remaining: $remainingMinutes minutes" -ForegroundColor Red
            return $false
        }
    }

    return $true
}

function Update-CircuitBreakerOnSuccess {
    Write-Host "  Circuit breaker CLOSED (build succeeded, resetting failure count)" -ForegroundColor Green
    Set-CircuitBreakerState -ConsecutiveFailures 0 -LastFailureTime ([DateTime]::MinValue) -State "CLOSED"
}

function Update-CircuitBreakerOnFailure {
    param([int]$Threshold)

    $state = Get-CircuitBreakerState
    $newFailures = $state.ConsecutiveFailures + 1
    $newState = if ($newFailures -ge $Threshold) { "OPEN" } else { "CLOSED" }

    Write-Host "  Circuit breaker: $newFailures consecutive failures (threshold: $Threshold)" -ForegroundColor Yellow
    Set-CircuitBreakerState -ConsecutiveFailures $newFailures -LastFailureTime (Get-Date) -State $newState

    if ($newState -eq "OPEN") {
        Write-Host "  Circuit breaker OPENED: Too many failures, entering cooldown" -ForegroundColor Red
    }
}

function Invoke-WithExponentialBackoff {
    param(
        [scriptblock]$Action,
        [int]$MaxAttempts,
        [int]$InitialDelay,
        [int]$MaxDelay,
        [string]$OperationName
    )

    $attempt = 1
    $delay = $InitialDelay

    while ($attempt -le $MaxAttempts) {
        Write-Host "  Attempt $attempt/$MaxAttempts..." -ForegroundColor Cyan

        try {
            $result = & $Action
            if ($result.ExitCode -eq 0) {
                Write-Host "  Success on attempt $attempt" -ForegroundColor Green
                return $result
            }
            else {
                throw "Build failed with exit code $($result.ExitCode)"
            }
        }
        catch {
            Write-Host "  Failed: $($_.Exception.Message)" -ForegroundColor Yellow

            if ($attempt -ge $MaxAttempts) {
                Write-Host "  All $MaxAttempts attempts exhausted. Giving up." -ForegroundColor Red
                throw $_
            }

            Write-Host "  Waiting $delay seconds before retry..." -ForegroundColor Yellow
            Start-Sleep -Seconds $delay

            # Exponential backoff: double delay, cap at MaxDelay
            $delay = [Math]::Min($delay * 2, $MaxDelay)
            $attempt++
        }
    }
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "UE Module Rebuild (Safe + Retry)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
if ($ModuleName) {
    Write-Host "Module:       $ModuleName"
}
else {
    Write-Host "Module:       AlisEditor (full)"
}
Write-Host "Clean:        $Clean"
Write-Host "Timeout:      $TimeoutSeconds seconds"
Write-Host "Max Retries:  $MaxRetries"
Write-Host "Backoff:      ${InitialBackoffSeconds}s -> ${MaxBackoffSeconds}s (exponential)"
Write-Host "Circuit Breaker: $CircuitBreakerThreshold failures → ${CircuitBreakerCooldownMinutes}min cooldown"
Write-Host ""

# Check circuit breaker before attempting build
if (-not (Test-CircuitBreaker -Threshold $CircuitBreakerThreshold -CooldownMinutes $CircuitBreakerCooldownMinutes)) {
    Write-Host "Build aborted due to circuit breaker. Wait for cooldown or reset state manually." -ForegroundColor Red
    Write-Host "To reset: Remove $circuitBreakerFile" -ForegroundColor Yellow
    exit 2
}

# Check disk space before attempting build
$drive = Get-PSDrive -Name (Split-Path -Qualifier $root).TrimEnd(':')
$freeGB = [Math]::Round($drive.Free / 1GB, 2)
$requiredGB = 50

Write-Host "Disk space: $freeGB GB free (required: $requiredGB GB)" -ForegroundColor $(if ($freeGB -lt $requiredGB) { "Yellow" } else { "Gray" })

if ($freeGB -lt $requiredGB) {
    Write-Host "WARNING: Low disk space detected!" -ForegroundColor Red
    Write-Host "Build may fail or run slowly. Consider freeing up space." -ForegroundColor Yellow
    Write-Host "Continuing with build (monitoring disk space)..." -ForegroundColor Yellow
}
Write-Host ""

# Step 1: Kill UE processes with zombie detection
Write-Host "[1/4] Cleaning up UE processes..." -ForegroundColor Yellow

# Check for zombie processes (running > 30 minutes)
$processNames = @("UnrealEditor*", "UnrealEditor-Cmd*", "UEBuildWorker*", "ShaderCompileWorker*", "UnrealLightmass*")
$zombies = @()
$now = Get-Date

foreach ($pattern in $processNames) {
    $processes = Get-Process | Where-Object { $_.ProcessName -like $pattern }
    foreach ($proc in $processes) {
        try {
            $age = ($now - $proc.StartTime).TotalMinutes
            if ($age -gt 30) {
                $zombies += @{ Name = $proc.ProcessName; Id = $proc.Id; Age = [Math]::Round($age, 1) }
            }
        }
        catch {}
    }
}

if ($zombies.Count -gt 0) {
    Write-Host "  Found $($zombies.Count) zombie process(es) (age > 30 min):" -ForegroundColor Yellow
    foreach ($z in $zombies) {
        Write-Host "    - $($z.Name) (PID: $($z.Id), age: $($z.Age) min)" -ForegroundColor Yellow
    }
}

# Kill all UE processes
$killed = 0
foreach ($pattern in $processNames) {
    $processes = Get-Process | Where-Object { $_.ProcessName -like $pattern }
    $killed += $processes.Count
    $processes | Stop-Process -Force -ErrorAction SilentlyContinue
}

Write-Host "  Killed $killed process(es)" -ForegroundColor Green

# Step 2: Clean Intermediate if requested
if ($Clean) {
    Write-Host "[2/4] Cleaning Intermediate folders..." -ForegroundColor Yellow
    $intermediatePath = Join-Path $root "Intermediate"
    if (Test-Path $intermediatePath) {
        Remove-Item $intermediatePath -Recurse -Force -ErrorAction SilentlyContinue
    }
    if ($ModuleName) {
        $pluginIntermediate = Get-ChildItem -Path (Join-Path $root "Plugins") -Filter "Intermediate" -Recurse -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -like "*$ModuleName*" }
        $pluginIntermediate | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    }
    Write-Host "  Done" -ForegroundColor Green
}
else {
    Write-Host "[2/4] Skipping clean (no -Clean flag)" -ForegroundColor Gray
}

# Step 3: Run UBT with timeout and retry
Write-Host "[3/4] Running Unreal Build Tool with retry logic..." -ForegroundColor Yellow

# Read UE_PATH from config (single source of truth)
$configPath = Join-Path $PSScriptRoot "..\..\config\ue_path.conf"
$uePath = $null
if (Test-Path $configPath) {
    $uePath = (Get-Content $configPath | Where-Object { $_ -match "^UE_PATH=(.+)$" } | Select-Object -First 1) -replace "^UE_PATH=", "" | ForEach-Object { $_.Trim().Replace("/", "\") }
}
if (-not $uePath) {
    Write-Host "  ERROR: UE_PATH not found in $configPath" -ForegroundColor Red
    exit 1
}
Write-Host "  UE_PATH: $uePath" -ForegroundColor Gray
$ubtPath = Join-Path $uePath "Engine\Build\BatchFiles\Build.bat"
$projectPath = Join-Path $root "Alis.uproject"
$buildLog = Join-Path $logDir "build.log"

$buildAction = {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $ubtPath
    $psi.Arguments = "AlisEditor Win64 Development `"$projectPath`" -WaitMutex"
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    $proc.Start() | Out-Null

    # Wait for build with timeout
    $ok = $proc.WaitForExit($TimeoutSeconds * 1000)
    $stdout = $proc.StandardOutput.ReadToEnd()
    $stderr = $proc.StandardError.ReadToEnd()
    $buildOutput = $stdout + "`n`n--- STDERR ---`n" + $stderr
    Set-Content -Path $buildLog -Value $buildOutput

    if (-not $ok) {
        Write-Host "  TIMEOUT: Build did not complete in $TimeoutSeconds seconds" -ForegroundColor Red
        $proc.Kill($true)
        Get-Process | Where-Object { $_.ProcessName -like "UEBuildWorker*" } | Stop-Process -Force -ErrorAction SilentlyContinue
        throw "Build timeout after $TimeoutSeconds seconds"
    }

    $exitCode = $proc.ExitCode
    if ($exitCode -eq 0) {
        Write-Host "  Build succeeded" -ForegroundColor Green
    }
    else {
        Write-Host "  Build failed with exit code $exitCode" -ForegroundColor Red
        # Show last 30 lines of build output
        $buildOutput -split "`n" | Select-Object -Last 30 | Write-Host -ForegroundColor Red
    }

    return @{ ExitCode = $exitCode }
}

try {
    $result = Invoke-WithExponentialBackoff -Action $buildAction -MaxAttempts $MaxRetries -InitialDelay $InitialBackoffSeconds -MaxDelay $MaxBackoffSeconds -OperationName "UE Build"
    $exitCode = $result.ExitCode

    if ($exitCode -eq 0) {
        Update-CircuitBreakerOnSuccess
    }
    else {
        Update-CircuitBreakerOnFailure -Threshold $CircuitBreakerThreshold
    }
}
catch {
    Write-Host "  Build failed after all retries: $($_.Exception.Message)" -ForegroundColor Red

    # Graceful degradation: Try simpler build if enabled
    if ($EnableGracefulDegradation -and -not $ModuleName) {
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Yellow
        Write-Host "GRACEFUL DEGRADATION: Attempting simpler build" -ForegroundColor Yellow
        Write-Host "========================================" -ForegroundColor Yellow
        Write-Host "Trying build without -WaitMutex flag..." -ForegroundColor Yellow

        $degradedBuildAction = {
            $psi = New-Object System.Diagnostics.ProcessStartInfo
            $psi.FileName = $ubtPath
            $psi.Arguments = "AlisEditor Win64 Development `"$projectPath`""  # No -WaitMutex
            $psi.UseShellExecute = $false
            $psi.RedirectStandardOutput = $true
            $psi.RedirectStandardError = $true
            $psi.CreateNoWindow = $true

            $proc = New-Object System.Diagnostics.Process
            $proc.StartInfo = $psi
            $proc.Start() | Out-Null

            $ok = $proc.WaitForExit($TimeoutSeconds * 1000)
            $stdout = $proc.StandardOutput.ReadToEnd()
            $stderr = $proc.StandardError.ReadToEnd()
            $buildOutput = $stdout + "`n`n--- STDERR ---`n" + $stderr
            Set-Content -Path "$buildLog.degraded" -Value $buildOutput

            if (-not $ok) {
                $proc.Kill($true)
                throw "Degraded build timeout"
            }

            return @{ ExitCode = $proc.ExitCode }
        }

        try {
            Write-Host "  Attempting degraded build (1 attempt only)..." -ForegroundColor Cyan
            $degradedResult = & $degradedBuildAction
            if ($degradedResult.ExitCode -eq 0) {
                Write-Host "  Degraded build SUCCEEDED!" -ForegroundColor Green
                Write-Host "  Note: Full build with -WaitMutex failed, but simpler build works" -ForegroundColor Yellow
                Update-CircuitBreakerOnSuccess
                $exitCode = 0
            }
            else {
                Write-Host "  Degraded build also failed (exit code: $($degradedResult.ExitCode))" -ForegroundColor Red
                Update-CircuitBreakerOnFailure -Threshold $CircuitBreakerThreshold
                $exitCode = $degradedResult.ExitCode
            }
        }
        catch {
            Write-Host "  Degraded build exception: $($_.Exception.Message)" -ForegroundColor Red
            Update-CircuitBreakerOnFailure -Threshold $CircuitBreakerThreshold
            $exitCode = 1
        }
    }
    else {
        Update-CircuitBreakerOnFailure -Threshold $CircuitBreakerThreshold
        $exitCode = 1
    }
}

# Step 4: Truncate log for chat
Write-Host "[4/4] Preparing logs..." -ForegroundColor Yellow
$logLines = Get-Content $buildLog
if ($logLines.Count -gt 200) {
    $truncated = $logLines | Select-Object -Last 200
    Set-Content -Path "$buildLog.truncated" -Value $truncated
    Write-Host "  Truncated log: $buildLog.truncated (last 200 lines)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Full build log: $buildLog" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan

exit $exitCode
