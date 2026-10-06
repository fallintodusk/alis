# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# The only resolver of a pending generated-content outer operation (the
# outer-recovery marker beside the content lock). It proves quiescence, takes
# the lock as its owner, restores World and ProjectMaterial through their own
# routes, verifies both against the marker's pre-operation identities, and
# removes the marker last. Rerunnable: every step is idempotent.
#
# Exit codes (meaning and operator action: README.md in this directory):
# 0 resolved or nothing pending; 5 refused, nothing changed; 6 invalid marker,
# nothing changed; 7 owner restore or identity check failed, marker kept.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [string]$ProjectRoot = ''
)

$ErrorActionPreference = 'Stop'
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDirectory 'generated_content_outer_recovery.ps1')
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $scriptDirectory))
}
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)

function Invoke-ProjectGeneratedContentOuterRecoveryEntry {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$ProjectFile
    )
    if (-not (Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $ProjectRoot)) {
        Write-ProjectGeneratedContentOuterLog -Message 'state=no_outer_operation'
        return 0
    }
    try {
        $lock = Enter-ProjectGeneratedContentRecoveryLock -ProjectRoot $ProjectRoot -ProjectFile $ProjectFile
    }
    catch {
        Write-ProjectGeneratedContentOuterLog -Message "REFUSED: $($_.Exception.Message)"
        return 5
    }
    $delegated = $false
    $prior = $null
    try {
        try {
            $marker = Read-ProjectGeneratedContentOuterMarker -ProjectRoot $ProjectRoot
        }
        catch {
            Write-ProjectGeneratedContentOuterLog -Message "INVALID: $($_.Exception.Message)"
            return 6
        }
        if ($null -eq $marker) {
            Write-ProjectGeneratedContentOuterLog -Message 'state=no_outer_operation'
            return 0
        }
        $operationId = [string]$marker.operation_id
        Write-ProjectGeneratedContentOuterLog -Message "recovering operation=$operationId phase=$($marker.phase)"
        $prior = Enable-ProjectGeneratedContentLockDelegation -Lock $lock
        $delegated = $true
        try {
            $evidenceRoot = Get-ProjectGeneratedContentOuterEvidenceRoot -ProjectRoot $ProjectRoot -OperationId $operationId
            $restore = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $ProjectRoot -Marker $marker -EvidenceRoot $evidenceRoot
            Complete-ProjectGeneratedContentOuterOperation -Lock $lock -ProjectRoot $ProjectRoot -Marker $marker -Result 'restored'
        }
        catch {
            Write-ProjectGeneratedContentOuterLog -Message "FAILED: $($_.Exception.Message)"
            return 7
        }
        Write-ProjectGeneratedContentOuterLog -Message "state=restored operation=$operationId material_source=$($restore.MaterialSource)"
        return 0
    }
    finally {
        if ($delegated) {
            Disable-ProjectGeneratedContentLockDelegation -Prior $prior
        }
        $lock.Dispose()
    }
}

$exitCode = @(Invoke-ProjectGeneratedContentOuterRecoveryEntry -ProjectRoot $ProjectRoot `
    -ProjectFile (Join-Path $ProjectRoot 'Alis.uproject'))[-1]
exit $exitCode
