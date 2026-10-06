# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Fixtures for the outer-recovery suites: a fake World authority (World's own
# test helper), a stand-in ProjectMaterial host at the fake root's host path,
# child processes, and bounded file-based synchronization. Nothing here touches
# the repository's lock or generated content.

. (Join-Path $PSScriptRoot '..\..\world\test\outer_snapshot_test_helpers.ps1')

function Install-OuterRecoveryMaterialHostStub {
    # The library runs <ProjectRoot>\scripts\ue\material\run_material_generation.ps1
    # as a delegated child. This stand-in joins the content lock as every host
    # run does, records the call, and keeps the fake Material state in
    # material_state.txt: RestorePrevious puts back material_bundle_<id>.txt,
    # Validate reports the state's digest as a rejected verdict.
    param([Parameter(Mandatory = $true)][string]$Root)
    $lockFile = (Resolve-Path (Join-Path $PSScriptRoot '..\generated_content_mutation_lock.ps1')).Path
    $hostPath = Join-Path $Root 'scripts\ue\material\run_material_generation.ps1'
    New-Item -ItemType Directory -Path (Split-Path -Parent $hostPath) -Force | Out-Null
    $text = @'
param(
    [string]$Domain = 'Surface', [string]$Mode = 'Validate', [string]$OperationId = '',
    [string]$RestoreOperationId = '', [string]$LayoutReceipt = '', [string]$EvidencePath = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
. '__LOCK_FILE__'
$lock = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $projectRoot
$accepted = $Mode -eq 'RestorePrevious'
try {
    Add-Content -LiteralPath (Join-Path $projectRoot 'material_host_calls.log') `
        -Value "$Mode|$RestoreOperationId|delegated=$(-not $lock.CanWrite)"
    $state = Join-Path $projectRoot 'material_state.txt'
    $source = 'none'
    if ($accepted) {
        $bundle = Join-Path $projectRoot "material_bundle_$RestoreOperationId.txt"
        if (Test-Path -LiteralPath $bundle) {
            Copy-Item -LiteralPath $bundle -Destination $state -Force
            $source = 'bundle'
        }
    }
    $digest = if (Test-Path -LiteralPath $state) {
        (Get-FileHash -LiteralPath $state -Algorithm SHA256).Hash.ToLowerInvariant()
    } else { 'none' }
    $receipt = [ordered]@{
        schema_version = '2'
        operation_id = $OperationId
        status = $(if ($accepted) { 'accepted' } else { 'rejected' })
        commandlet_status = $(if ($accepted) { 'none' } else { 'rejected' })
        commandlet_error = $(if ($accepted) { '' } else { 'Accepted surface manifest is stale: stand-in' })
        manifest_tree_sha256 = $digest
        output_tree_sha256 = 'none'
        source = $source
    }
    $folder = Join-Path $EvidencePath $(if ($accepted) { 'Current' } else { 'Rejected' })
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    $receipt | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $folder 'host.receipt.json') -Encoding UTF8
}
finally {
    $lock.Dispose()
}
if (-not $accepted) { exit 1 }
'@
    Set-Content -LiteralPath $hostPath -Value $text.Replace('__LOCK_FILE__', $lockFile) -Encoding UTF8
}

function New-OuterRecoveryTestProject {
    param([Parameter(Mandatory = $true)][string]$Root)
    $project = New-ProjectWorldOuterTestProject -Root $Root
    $layout = Join-Path $Root 'inputs\layout-receipt.json'
    New-Item -ItemType Directory -Path (Split-Path -Parent $layout) -Force | Out-Null
    Set-Content -LiteralPath $layout -Value ('{"layout_sha256":"' + ('1' * 64) + '"}') -NoNewline
    $materialState = Join-Path $Root 'material_state.txt'
    Set-Content -LiteralPath $materialState -Value 'material-before' -NoNewline
    Install-OuterRecoveryMaterialHostStub -Root $Root
    $project | Add-Member -NotePropertyName LayoutReceipt -NotePropertyValue $layout
    $project | Add-Member -NotePropertyName MaterialState -NotePropertyValue $materialState
    $project | Add-Member -NotePropertyName MaterialCalls -NotePropertyValue (Join-Path $Root 'material_host_calls.log')
    return $project
}

function Get-OuterRecoveryTestState {
    # World generated roots and authority plus the stand-in Material state.
    param([Parameter(Mandatory = $true)][object]$Project)
    return (Get-ProjectWorldOuterTestState -Project $Project) + "`n" +
        (Get-ProjectWorldOuterTestDigest -Paths @($Project.MaterialState))
}

function Invoke-OuterRecoveryChild {
    # One Windows PowerShell child, synchronously; it inherits this process's
    # environment, including a delegated token.
    param(
        [Parameter(Mandatory = $true)][string]$Script,
        [string[]]$Arguments = @()
    )
    $ErrorActionPreference = 'Continue'
    $powershell = (Get-Process -Id $PID).Path
    $output = & $powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $Script @Arguments 2>&1
    return [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output = (@($output | ForEach-Object { [string]$_ }) -join "`n")
    }
}

function Start-OuterRecoveryChild {
    # One Windows PowerShell child that outlives this call; synchronize through files.
    param(
        [Parameter(Mandatory = $true)][string]$Script,
        [string[]]$Arguments = @()
    )
    $quoted = @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', "`"$Script`"") +
        @($Arguments | ForEach-Object { if ($_ -match '^-') { $_ } else { "`"$_`"" } })
    return Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList $quoted -PassThru -WindowStyle Hidden
}

function Wait-OuterRecoveryFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [int]$TimeoutSeconds = 120
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not (Test-Path -LiteralPath $Path)) {
        if ([DateTime]::UtcNow -gt $deadline) {
            throw "Timed out after $TimeoutSeconds s waiting for $Path"
        }
        Start-Sleep -Milliseconds 100
    }
}

function Start-OuterRecoveryTestOperation {
    # An in-process stand-in coordinator: owner lock, delegation, and an open
    # outer operation, as the S5 coordinator composes them.
    param(
        [Parameter(Mandatory = $true)][object]$Project,
        [string]$Label = 'test_operation'
    )
    $lock = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $Project.Root `
        -OwnerName 'test coordinator' -RequireOwnership
    $prior = $null
    try {
        $prior = Enable-ProjectGeneratedContentLockDelegation -Lock $lock
        $marker = New-ProjectGeneratedContentOuterOperation -Lock $lock -ProjectRoot $Project.Root `
            -WorldDataPlugin 'ProjectWorldTestData' -Label $Label -LayoutReceipt $Project.LayoutReceipt
    }
    catch {
        Disable-ProjectGeneratedContentLockDelegation -Prior $prior
        $lock.Dispose()
        throw
    }
    return [pscustomobject]@{ Lock = $lock; Prior = $prior; Marker = $marker }
}

function Stop-OuterRecoveryTestOperation {
    # The coordinator ends without resolving: its handle goes, its marker stays.
    param([Parameter(Mandatory = $true)][object]$Operation)
    Disable-ProjectGeneratedContentLockDelegation -Prior $Operation.Prior
    $Operation.Lock.Dispose()
}
