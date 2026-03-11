# UE Test Orchestrator for Overnight CI
# Wraps existing UE test scripts with resilience features
#
# Usage:
#   .\UE_Tests.ps1 -TestSuite smoke    # Run smoke tests (boot, gamefeature)
#   .\UE_Tests.ps1 -TestSuite unit     # Run C++ unit tests
#   .\UE_Tests.ps1 -TestSuite full     # Run all tests

param(
    [ValidateSet("smoke", "unit", "integration", "full")]
    [string]$TestSuite = "smoke",
    [int]$TimeoutSeconds = 300,  # 5 minutes per test
    [switch]$EnableRetry = $true
)

$ErrorActionPreference = "Stop"

# Import utilities
$ProjectRoot = (Resolve-Path "$PSScriptRoot/../../../../..").Path
Import-Module (Join-Path $ProjectRoot "scripts/autonomous/common/Common.psm1") -Force

# Setup logging
$stepN = $env:OVERNIGHT_STEP
if (-not $stepN) { $stepN = "manual" }
$logDir = Join-Path $ProjectRoot "artifacts/overnight/step-$stepN"
$testLog = Join-Path $logDir "ue_tests.json"

New-Item -ItemType Directory -Path $logDir -Force | Out-Null

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  UE Test Orchestrator (Overnight CI)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Test Suite:  $TestSuite"
Write-Host "Timeout:     $TimeoutSeconds seconds per test"
Write-Host "Retry:       $($EnableRetry.IsPresent)"
Write-Host "Log Dir:     $logDir"
Write-Host ""

# Track results
$allResults = @()

# Helper: Run test with resilience
function Invoke-UETestWithResilience {
    param(
        [string]$TestName,
        [scriptblock]$TestCommand,
        [int]$Timeout = 300
    )

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host "  TEST: $TestName" -ForegroundColor Yellow
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host ""

    $startTime = Get-Date
    $attempt = 0
    $maxAttempts = if ($EnableRetry) { 3 } else { 1 }
    $success = $false
    $lastError = $null

    while ($attempt -lt $maxAttempts -and -not $success) {
        $attempt++
        if ($attempt -gt 1) {
            Write-Host "Retry attempt $attempt/$maxAttempts..." -ForegroundColor Yellow
            Start-Sleep -Seconds 10  # Brief pause between retries
        }

        try {
            # Kill any zombie UE processes before test
            Write-Host "[Pre-test] Cleaning zombie processes..." -ForegroundColor Gray
            Stop-ZombieProcesses -MaxAgeMinutes 30 -Force | Out-Null

            # Check disk space
            $diskOk = Test-DiskSpace -RequiredGB 30
            if (-not $diskOk) {
                throw "Insufficient disk space (< 30GB)"
            }

            # Run test with timeout (pass ProjectRoot to scriptblock)
            $testJob = Start-Job -ScriptBlock $TestCommand -ArgumentList $ProjectRoot
            $completed = Wait-Job -Job $testJob -Timeout $Timeout

            if ($completed) {
                $output = Receive-Job -Job $testJob
                $exitCode = $output.ExitCode

                if ($exitCode -eq 0) {
                    Write-Host ""
                    Write-Host "  ✅ TEST PASSED" -ForegroundColor Green
                    $success = $true
                } else {
                    throw "Test failed with exit code: $exitCode"
                }
            } else {
                # Timeout
                Remove-Job -Job $testJob -Force
                throw "Test timed out after $Timeout seconds"
            }
        } catch {
            $lastError = $_.Exception.Message
            Write-Host ""
            Write-Host "  ❌ TEST FAILED: $lastError" -ForegroundColor Red

            if ($attempt -lt $maxAttempts) {
                Write-Host "  Will retry..." -ForegroundColor Yellow
            }
        } finally {
            # Always clean up
            Stop-AllUEProcesses
        }
    }

    $endTime = Get-Date
    $duration = ($endTime - $startTime).TotalSeconds

    # Record result
    $result = @{
        TestName = $TestName
        Success = $success
        Attempts = $attempt
        Duration = [Math]::Round($duration, 2)
        Error = $lastError
        Timestamp = (Get-Date -Format "o")
    }

    # Log to structured log
    $level = if ($success) { "INFO" } else { "ERROR" }
    Write-StructuredLog -Level $level -Message "UE Test: $TestName" -Data $result -LogFile $testLog

    # Add metric
    Add-Metric -MetricName "test_$($TestName)_duration" -Value $duration -MetricsFile (Join-Path $logDir "metrics.json")

    return $result
}

# Define test suites
$smokeTests = @(
    @{
        Name = "GameFeature_Registration"
        Command = {
            param($ProjectRoot)
            $script = Join-Path $ProjectRoot "scripts/ue/test/smoke/gamefeature_registration.bat"

            # Run and capture exit code
            cmd.exe /c "`"$script`"" 2>&1
            return @{ ExitCode = $LASTEXITCODE }
        }.GetNewClosure()
    },
    @{
        Name = "Standalone_Boot_15s"
        Command = {
            param($ProjectRoot)
            $script = Join-Path $ProjectRoot "scripts/ue/test/smoke/boot_test.bat"

            # Run and capture exit code
            cmd.exe /c "`"$script`"" 2>&1
            return @{ ExitCode = $LASTEXITCODE }
        }.GetNewClosure()
    }
)

$unitTests = @(
    @{
        Name = "CPP_Unit_Tests"
        Command = {
            param($ProjectRoot)
            $script = Join-Path $ProjectRoot "scripts/ue/test/unit/run_cpp_tests_safe.ps1"

            # Run with PowerShell
            $result = & powershell.exe -ExecutionPolicy Bypass -File $script -TestFilter "Project.*"
            return @{ ExitCode = $LASTEXITCODE }
        }.GetNewClosure()
    }
)

$integrationTests = @(
    @{
        Name = "Autonomous_Boot_Flow"
        Command = {
            param($ProjectRoot)
            $script = Join-Path $ProjectRoot "scripts/ue/test/integration/autonomous_boot_test.bat"

            # Run and capture exit code
            cmd.exe /c "`"$script`"" 2>&1
            return @{ ExitCode = $LASTEXITCODE }
        }.GetNewClosure()
    }
)

# Select tests based on suite
$testsToRun = @()
switch ($TestSuite) {
    "smoke" {
        $testsToRun = $smokeTests
    }
    "unit" {
        $testsToRun = $unitTests
    }
    "integration" {
        $testsToRun = $integrationTests
    }
    "full" {
        $testsToRun = $smokeTests + $unitTests + $integrationTests
    }
}

Write-Host "Running $($testsToRun.Count) test(s) in '$TestSuite' suite..." -ForegroundColor Cyan
Write-Host ""

# Run all tests
foreach ($test in $testsToRun) {
    $result = Invoke-UETestWithResilience `
        -TestName $test.Name `
        -TestCommand $test.Command `
        -Timeout $TimeoutSeconds

    $allResults += $result
}

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  TEST SUMMARY" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$passedCount = ($allResults | Where-Object { $_.Success }).Count
$failedCount = ($allResults | Where-Object { -not $_.Success }).Count
$totalDuration = ($allResults | Measure-Object -Property Duration -Sum).Sum

Write-Host "Total Tests:    $($allResults.Count)"
Write-Host "Passed:         $passedCount" -ForegroundColor Green
Write-Host "Failed:         $failedCount" -ForegroundColor $(if ($failedCount -eq 0) { 'Green' } else { 'Red' })
Write-Host "Total Duration: $([Math]::Round($totalDuration, 2))s"
Write-Host ""

# Show failed tests
if ($failedCount -gt 0) {
    Write-Host "Failed Tests:" -ForegroundColor Red
    foreach ($result in $allResults | Where-Object { -not $_.Success }) {
        Write-Host "  ❌ $($result.TestName)" -ForegroundColor Red
        Write-Host "     Error: $($result.Error)" -ForegroundColor Gray
        Write-Host "     Attempts: $($result.Attempts)" -ForegroundColor Gray
    }
    Write-Host ""
}

# Write summary to metrics
Add-Metric -MetricName "test_suite_total" -Value $allResults.Count -MetricsFile (Join-Path $logDir "metrics.json")
Add-Metric -MetricName "test_suite_passed" -Value $passedCount -MetricsFile (Join-Path $logDir "metrics.json")
Add-Metric -MetricName "test_suite_failed" -Value $failedCount -MetricsFile (Join-Path $logDir "metrics.json")
Add-Metric -MetricName "test_suite_duration" -Value $totalDuration -MetricsFile (Join-Path $logDir "metrics.json")

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Structured log: $testLog" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Exit with appropriate code
if ($failedCount -gt 0) {
    exit 1
} else {
    exit 0
}
