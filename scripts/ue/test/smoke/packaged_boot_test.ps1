# Packaged Build Hitch Smoke Test
# Launches the packaged Shipping exe, monitors log for PSO hitches, errors,
# and crash indicators. Optionally runs twice to compare first-run vs cached.
#
# NOTE: This is a smoke test, not a benchmark. "Cold" run only clears the log,
# not PSO disk caches or other persistent caches. For a true first-run test,
# manually delete <local-app-data>\Alis\Saved\ before running.
#
# Usage:
#   .\packaged_boot_test.ps1 -ExePath "C:\builds\Alis\Alis.exe"
#   .\packaged_boot_test.ps1 -ExePath "C:\builds\Alis\Alis.exe" -SecondRun
#   .\packaged_boot_test.ps1 -ExePath "C:\builds\Alis\Alis.exe" -TimeoutSeconds 90
#   .\packaged_boot_test.ps1 -ExePath "C:\builds\Alis\Alis.exe" -NoForceResolution

param(
    [Parameter(Mandatory=$true)]
    [string]$ExePath,

    [int]$TimeoutSeconds = 60,

    [switch]$SecondRun,

    [switch]$NoForceResolution,

    [string]$LogDir = ""
)

$ErrorActionPreference = "Stop"

# ============================================================================
# Validate inputs
# ============================================================================
if (-not (Test-Path $ExePath)) {
    Write-Host "ERROR: Executable not found: $ExePath" -ForegroundColor Red
    exit 1
}

# Packaged logs go to <local-app-data>\Alis\Saved\Logs\
if (-not $LogDir) {
    $LogDir = Join-Path $env:LOCALAPPDATA "Alis\Saved\Logs"
}

Write-Host "============================================================================"
Write-Host "  Packaged Build Hitch Smoke Test"
Write-Host "============================================================================"
Write-Host "  Exe      : $ExePath"
Write-Host "  Timeout  : ${TimeoutSeconds}s"
Write-Host "  Log dir  : $LogDir"
Write-Host "  Mode     : $(if ($SecondRun) { 'first + second run' } else { 'single run' })"
Write-Host "  Resolution: $(if ($NoForceResolution) { 'packaged default' } else { 'forced 1280x720' })"
Write-Host ""

# ============================================================================
# Helper: run one session and collect metrics
# ============================================================================
function Run-Session {
    param(
        [string]$Label,
        [string]$Exe,
        [int]$Timeout,
        [bool]$ForceResolution
    )

    Write-Host "--- $Label ---"
    Write-Host "[1/4] Launching packaged build..."

    # Clear old log
    $logFile = Join-Path $LogDir "Alis.log"
    if (Test-Path $logFile) {
        Remove-Item $logFile -Force
    }

    $launchArgs = "-log"
    if ($ForceResolution) {
        $launchArgs = "-windowed -ResX=1280 -ResY=720 -log"
    }

    $process = Start-Process -FilePath $Exe -ArgumentList $launchArgs -PassThru

    Write-Host "[2/4] Waiting ${Timeout}s for boot and gameplay..."
    $elapsed = 0
    $pollInterval = 5
    while ($elapsed -lt $Timeout) {
        Start-Sleep -Seconds $pollInterval
        $elapsed += $pollInterval

        if ($process.HasExited) {
            Write-Host "  Process exited early (code $($process.ExitCode)) at ${elapsed}s"
            break
        }
    }

    # Kill if still running
    if (-not $process.HasExited) {
        Write-Host "[3/4] Stopping process..."
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 3
    }

    Write-Host "[4/4] Analyzing log..."

    $results = @{
        Label = $Label
        Crashed = $false
        PSOHitches = 0
        PSOUncached = 0
        ShaderMapErrors = 0
        MutableOverflows = 0
        CvarWarnings = 0
        ExitCode = $process.ExitCode
    }

    if (-not (Test-Path $logFile)) {
        Write-Host "  [X] Log file not found: $logFile" -ForegroundColor Red
        $results.Crashed = $true
        return $results
    }

    $logContent = Get-Content $logFile -Raw -ErrorAction SilentlyContinue

    # Check crash
    if ($logContent -match "EXCEPTION_ACCESS_VIOLATION|Fatal error|Assertion failed|CrashReportClient") {
        $results.Crashed = $true
    }

    # PSO hitches: capture ALL matches, keep the highest total (final cumulative report)
    $psoMatches = [regex]::Matches($logContent, "Encountered (\d+) PSO creation hitches.*?(\d+) of them were precached")
    foreach ($m in $psoMatches) {
        $total = [int]$m.Groups[1].Value
        $precached = [int]$m.Groups[2].Value
        if ($total -gt $results.PSOHitches) {
            $results.PSOHitches = $total
            $results.PSOUncached = $total - $precached
        }
    }

    # Invalid ShaderMap
    $results.ShaderMapErrors = ([regex]::Matches($logContent, "invalid ShaderMap")).Count

    # Mutable overflow
    $results.MutableOverflows = ([regex]::Matches($logContent, "Failed to keep memory budget")).Count

    # CVar spam
    $results.CvarWarnings = ([regex]::Matches($logContent, "Failed to find console variable")).Count

    return $results
}

# ============================================================================
# Run sessions
# ============================================================================
$forceRes = -not $NoForceResolution
$run1 = Run-Session -Label "Run 1 (smoke)" -Exe $ExePath -Timeout $TimeoutSeconds -ForceResolution $forceRes

$run2 = $null
if ($SecondRun) {
    Write-Host ""
    $run2 = Run-Session -Label "Run 2 (warm cache)" -Exe $ExePath -Timeout $TimeoutSeconds -ForceResolution $forceRes
}

# ============================================================================
# Report
# ============================================================================
Write-Host ""
Write-Host "============================================================================"
Write-Host "  RESULTS"
Write-Host "============================================================================"

function Show-Results {
    param($r)

    Write-Host "  $($r.Label):"
    Write-Host "    Crashed          : $(if ($r.Crashed) { '[X] YES' } else { '[OK] No' })"
    Write-Host "    PSO hitches      : $($r.PSOHitches) total, $($r.PSOUncached) uncached"
    Write-Host "    ShaderMap errors : $($r.ShaderMapErrors)"
    Write-Host "    Mutable overflows: $($r.MutableOverflows)"
    Write-Host "    CVar warnings    : $($r.CvarWarnings)"
    Write-Host "    Exit code        : $($r.ExitCode)"
}

Show-Results $run1

if ($run2) {
    Write-Host ""
    Show-Results $run2

    # Compare
    Write-Host ""
    Write-Host "  Comparison:"
    if ($run2.PSOUncached -lt $run1.PSOUncached) {
        Write-Host "    [OK] PSO uncached hitches reduced: $($run1.PSOUncached) -> $($run2.PSOUncached)"
    } elseif ($run1.PSOUncached -eq 0 -and $run2.PSOUncached -eq 0) {
        Write-Host "    [OK] No PSO uncached hitches in either run"
    } else {
        Write-Host "    [!] PSO uncached hitches not reduced: $($run1.PSOUncached) -> $($run2.PSOUncached)"
    }
}

Write-Host "============================================================================"

# Determine pass/fail
$failed = $false

if ($run1.Crashed) {
    Write-Host "  TEST RESULT: FAILED (crash detected)" -ForegroundColor Red
    $failed = $true
} elseif ($run1.ShaderMapErrors -gt 0) {
    Write-Host "  TEST RESULT: FAILED ($($run1.ShaderMapErrors) invalid ShaderMap errors)" -ForegroundColor Red
    $failed = $true
} elseif ($run1.CvarWarnings -gt 100) {
    Write-Host "  TEST RESULT: FAILED ($($run1.CvarWarnings) CVar warnings - per-frame spam)" -ForegroundColor Red
    $failed = $true
} elseif ($run1.MutableOverflows -gt 0) {
    Write-Host "  TEST RESULT: WARNING ($($run1.MutableOverflows) Mutable budget overflows)" -ForegroundColor Yellow
} else {
    Write-Host "  TEST RESULT: PASSED" -ForegroundColor Green
}

Write-Host "============================================================================"

if ($failed) { exit 1 }
exit 0
