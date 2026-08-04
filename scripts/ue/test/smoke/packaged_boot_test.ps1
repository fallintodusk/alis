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

# Keep standalone invocations compatible with UE's default saved location.
if (-not $LogDir) {
    $LogDir = Join-Path $env:LOCALAPPDATA "Alis\Saved\Logs"
}
New-Item -ItemType Directory -Path $LogDir -Force | Out-Null

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

    $launchExe = $Exe
    $launchArgs = @()
    $gameName = [System.IO.Path]::GetFileNameWithoutExtension($Exe)
    $binaryDir = Join-Path (Split-Path -Parent $Exe) "$gameName\Binaries\Win64"
    if (Test-Path $binaryDir) {
        $innerCandidates = @(
            @(
                "$gameName.exe",
                "$gameName-Win64-Shipping.exe",
                "$gameName-Win64-Test.exe",
                "$gameName-Win64-Development.exe"
            ) | ForEach-Object { Join-Path $binaryDir $_ } |
                Where-Object { Test-Path -LiteralPath $_ }
        )
        if ($innerCandidates.Count -ne 1) {
            throw "Expected one staged game executable under ${binaryDir}; found $($innerCandidates.Count)"
        }

        # Own the monolithic game process instead of its short-lived bootstrap.
        $launchExe = $innerCandidates[0]
        $launchArgs += $gameName
    }

    if ($ForceResolution) {
        $launchArgs += @("-windowed", "-ResX=1280", "-ResY=720")
    }
    # Shipping uses the per-user saved directory by default. Force each gate's
    # log into its caller-owned evidence root so analysis cannot read stale data.
    $launchArgs += @("-log", ('-abslog="{0}"' -f $logFile))

    $process = Start-Process -FilePath $launchExe -ArgumentList $launchArgs -PassThru

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

    $results = [pscustomobject]@{
        Label = $Label
        Crashed = $false
        PSOHitches = 0
        PSOUncached = 0
        ShaderMapErrors = 0
        MutableOverflows = 0
        CvarWarnings = 0
        ContractErrors = 0
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

    # Runtime content must target the active engine line.
    $results.ContractErrors = ([regex]::Matches(
        $logContent, "engine_build_id incompatible")).Count

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
    Write-Host "    Contract errors  : $($r.ContractErrors)"
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
$gatedRuns = @($run1)
if ($run2) {
    $gatedRuns += $run2
}
$crashedRuns = @($gatedRuns | Where-Object { $_.Crashed })
$shaderMapErrors = ($gatedRuns | Measure-Object -Property ShaderMapErrors -Sum).Sum
$contractErrors = ($gatedRuns | Measure-Object -Property ContractErrors -Sum).Sum
$cvarWarnings = ($gatedRuns | Measure-Object -Property CvarWarnings -Sum).Sum
$mutableOverflows = ($gatedRuns | Measure-Object -Property MutableOverflows -Sum).Sum

if ($crashedRuns.Count -gt 0) {
    Write-Host "  TEST RESULT: FAILED (crash detected)" -ForegroundColor Red
    $failed = $true
} elseif ($shaderMapErrors -gt 0) {
    Write-Host "  TEST RESULT: FAILED ($shaderMapErrors invalid ShaderMap errors)" -ForegroundColor Red
    $failed = $true
} elseif ($contractErrors -gt 0) {
    Write-Host "  TEST RESULT: FAILED (runtime engine contract rejected)" -ForegroundColor Red
    $failed = $true
} elseif ($cvarWarnings -gt 100) {
    Write-Host "  TEST RESULT: FAILED ($cvarWarnings CVar warnings - per-frame spam)" -ForegroundColor Red
    $failed = $true
} elseif ($mutableOverflows -gt 0) {
    Write-Host "  TEST RESULT: WARNING ($mutableOverflows Mutable budget overflows)" -ForegroundColor Yellow
} else {
    Write-Host "  TEST RESULT: PASSED" -ForegroundColor Green
}

Write-Host "============================================================================"

if ($failed) { exit 1 }
exit 0
