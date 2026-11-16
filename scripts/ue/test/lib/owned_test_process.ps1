# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

function Initialize-ProjectOwnedTestProcess {
    param([Parameter(Mandatory)][string]$ScratchRoot)
    if ('Project.Testing.ProjectOwnedTestProcess' -as [type]) { return }
    New-Item -ItemType Directory -Force -Path $ScratchRoot | Out-Null
    $previousTemp = $env:TEMP
    $previousTmp = $env:TMP
    try {
        $env:TEMP = $ScratchRoot
        $env:TMP = $ScratchRoot
        Add-Type -Path (Join-Path $PSScriptRoot 'owned_test_process.cs')
    } finally {
        $env:TEMP = $previousTemp
        $env:TMP = $previousTmp
    }
}

function Stop-ProjectOwnedTestProcess {
    param([Parameter(Mandatory)]$Process)
    return $Process.Stop(5000)
}

function Invoke-ProjectOwnedTestProcess {
    param(
        [Parameter(Mandatory)][Diagnostics.ProcessStartInfo]$StartInfo,
        [Parameter(Mandatory)][ValidateRange(1, 86400)][int]$TimeoutSeconds,
        [Parameter(Mandatory)][string]$LogPath,
        [Parameter(Mandatory)][string]$ScratchRoot,
        [switch]$RetireDescendantsOnRootExit
    )
    Initialize-ProjectOwnedTestProcess -ScratchRoot $ScratchRoot
    $process = $null
    $missedDeadline = $false
    $cleanupFailure = ''
    $infrastructureError = ''
    $retiredDescendants = $false
    $exitCode = 1
    try {
        $process = [Project.Testing.ProjectOwnedTestProcess]::Start($StartInfo)
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $missedDeadline = -not $process.WaitForExit($TimeoutSeconds * 1000)
        if ($missedDeadline) {
            Write-Warning "Owned test process timed out after $TimeoutSeconds seconds."
            try {
                if (-not (Stop-ProjectOwnedTestProcess $process)) { $cleanupFailure = 'Owned Job did not become empty within termination grace.' }
            } catch { $cleanupFailure = "Owned Job termination failed: $($_.Exception.Message)" }
        }
        if (-not $missedDeadline -and -not $process.WaitForEmpty(250)) {
            if ($RetireDescendantsOnRootExit) { $retiredDescendants = $true }
            else { $infrastructureError = 'Owned descendants remain after root exit.' }
            try {
                if (-not (Stop-ProjectOwnedTestProcess $process)) { $cleanupFailure = 'Owned Job did not become empty within termination grace.' }
            } catch { $cleanupFailure = "Owned Job termination failed: $($_.Exception.Message)" }
        }
        $drained = $false
        $drainError = ''
        try {
            $drained = [Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdoutTask, $stderrTask), 5000)
        } catch { $drainError = $_.Exception.Message }
        if (-not $drained) {
            $infrastructureError = "Redirected output did not drain within grace. $drainError"
            Set-Content -LiteralPath $LogPath -Value "Infrastructure failure: $infrastructureError"
        } else {
            Set-Content -LiteralPath $LogPath -Value ($stdoutTask.Result + "`n`n--- STDERR ---`n" + $stderrTask.Result)
            if (-not $missedDeadline -and -not $infrastructureError) { $exitCode = $process.ExitCode }
        }
    } finally {
        if ($null -ne $process) {
            try {
                if (-not (Stop-ProjectOwnedTestProcess $process)) { $cleanupFailure = 'Owned Job did not become empty within termination grace.' }
            } catch { $cleanupFailure = "Owned Job termination failed: $($_.Exception.Message)" }
            finally { $process.Dispose() }
        }
    }
    if ($cleanupFailure) {
        $retiredDescendants = $false
        Add-Content -LiteralPath $LogPath -Value "Infrastructure failure: $cleanupFailure"
        Write-Warning $cleanupFailure
        $infrastructureError = $cleanupFailure
        $exitCode = 1
    }
    elseif ($infrastructureError) {
        Add-Content -LiteralPath $LogPath -Value "Infrastructure failure: $infrastructureError"
        Write-Warning $infrastructureError
    }
    elseif ($retiredDescendants) {
        Add-Content -LiteralPath $LogPath -Value 'Retired owned native workers after root exit; Job verified empty.'
    }
    if ($missedDeadline) { $exitCode = 124 }
    return [pscustomobject]@{ ExitCode = $exitCode; ProcessDeadlineMissed = $missedDeadline; InfrastructureError = $infrastructureError; RetiredDescendantsOnExit = $retiredDescendants }
}
