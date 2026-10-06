# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [ValidateSet('Surface', 'Texture')]
    [string]$Domain = 'Surface',

    # RestorePrevious runs no commandlet: it restores the pending journal or the retained
    # RollbackPrevious bundle of exactly the operation named by -RestoreOperationId.
    [ValidateSet('Validate', 'Regenerate', 'RestorePrevious')]
    [string]$Mode = 'Validate',

    [string]$TestRoot = '',

    [string]$EvidencePath = '',

    [string]$LayoutReceipt = '',

    [string]$PatternTestRoot = '',

    [switch]$CleanupOrphans,

    [ValidateSet('', 'post-save', 'pre-commit')]
    [string]$InjectFailure = '',

    # A caller that records this run before it mutates passes the id; it binds the journal,
    # the commandlet receipt, the host receipt, and the rollback bundle.
    [ValidatePattern('^([a-f0-9]{32})?$')]
    [string]$OperationId = '',

    [ValidatePattern('^([a-f0-9]{32})?$')]
    [string]$RestoreOperationId = '',

    [ValidateRange(30, 3600)]
    [int]$TimeoutSeconds = 600
)

$ErrorActionPreference = 'Stop'
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptDirectory '..\..\..'))
$projectFile = Join-Path $projectRoot 'Alis.uproject'
$configDirectory = Join-Path $projectRoot 'scripts\config'
. (Join-Path $configDirectory 'Resolve-UEConfig.ps1')
. (Join-Path $projectRoot 'scripts\ue\generated_content\generated_content_mutation_lock.ps1')
. (Join-Path $scriptDirectory 'material_host_recovery.ps1')

function Get-FileSha256OrNone {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return 'none'
    }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-PendingRestoreSources {
    # Each result can still restore packages that reference an orphan this host would otherwise
    # delete; references held in a snapshot are invisible to the Asset Registry. World transactions
    # keep their snapshots under one folder whatever the manifest root, World test harnesses and
    # generated-content outer operations keep snapshots named 'snapshot' or 'outer-snapshot' under
    # tmp/world, and each is removed once its content is restored or committed.
    $pending = @()
    $worldScratch = Join-Path $projectRoot 'tmp\world'
    $worldSnapshots = Join-Path $worldScratch 'world_realization\transactions'
    if (Test-Path -LiteralPath $worldSnapshots -PathType Container) {
        $pending += @(Get-ChildItem -LiteralPath $worldSnapshots -Force | Select-Object -ExpandProperty FullName)
    }
    if (Test-Path -LiteralPath $worldScratch -PathType Container) {
        $pending += @(Get-ChildItem -LiteralPath $worldScratch -Recurse -Directory |
            Where-Object { $_.Name -in @('snapshot', 'outer-snapshot') } |
            Select-Object -ExpandProperty FullName)
    }
    $releaseWork = Join-Path $projectRoot 'tmp\release\work'
    if (Test-Path -LiteralPath $releaseWork -PathType Container) {
        $pending += @(Get-ChildItem -LiteralPath $releaseWork -Directory |
            ForEach-Object { Join-Path $_.FullName 'world-projection-rollback' } |
            Where-Object { Test-Path -LiteralPath $_ })
    }
    $siblingDomain = if ($Domain -eq 'Texture') { 'material' } else { 'texture' }
    $siblingJournal = Join-Path $projectRoot "tmp\$siblingDomain\generation\transactions\journal.json"
    if (Test-Path -LiteralPath $siblingJournal -PathType Leaf) {
        $pending += $siblingJournal
    }
    return $pending
}

$isRestore = $Mode -eq 'RestorePrevious'
if ($isRestore) {
    if ([string]::IsNullOrWhiteSpace($RestoreOperationId)) {
        throw 'RestorePrevious requires -RestoreOperationId.'
    }
    if ($RestoreOperationId -ceq $OperationId) {
        throw 'RestorePrevious cannot restore its own operation id.'
    }
    if ($CleanupOrphans -or $InjectFailure -or $LayoutReceipt -or $PatternTestRoot) {
        throw 'RestorePrevious runs no commandlet; cleanup, failure injection, a layout receipt, and a pattern test root are refused.'
    }
}
elseif (-not [string]::IsNullOrWhiteSpace($RestoreOperationId)) {
    throw '-RestoreOperationId is valid only for RestorePrevious.'
}
elseif ($Mode -eq 'Validate' -and (-not [string]::IsNullOrWhiteSpace($InjectFailure) -or $CleanupOrphans)) {
    throw 'Cleanup and failure injection are valid only for Regenerate.'
}
$editorCommand = $null
if (-not $isRestore) {
    $config = Resolve-UEConfig -ConfigDir $configDirectory
    $editorCommand = Join-Path $config.UE_PATH 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
        throw "Launcher UnrealEditor-Cmd does not exist: $editorCommand"
    }
}

$isTexture = $Domain -eq 'Texture'
$domainName = if ($isTexture) { 'ProjectTexture' } else { 'ProjectMaterial' }
$commandletName = if ($isTexture) {
    'ProjectTextureGenerate'
}
else {
    'ProjectMaterialSurfaceGenerate'
}
$scratchDomain = if ($isTexture) { 'texture' } else { 'material' }
$contentFolder = if ($isTexture) { 'Patterns' } else { 'Surfaces' }
$manifestFolder = if ($isTexture) { 'Patterns' } else { 'Surfaces' }
$validationFolder = if ($isTexture) {
    'TexturePatternGeneration'
}
else {
    'MaterialSurfaceGeneration'
}
$isTest = -not [string]::IsNullOrWhiteSpace($TestRoot)
if (-not [string]::IsNullOrWhiteSpace($PatternTestRoot) -and ($isTexture -or -not $isTest)) {
    throw 'A pattern test root is valid only for a Surface test run.'
}
if ($InjectFailure -eq 'pre-commit' -and -not $isTest) {
    throw 'Pre-commit failure injection is valid only for a test run.'
}
if ($isTest) {
    $testOwnerRoot = Assert-PathWithin `
        -Path $TestRoot `
        -Root (Join-Path $projectRoot "tmp\$scratchDomain\generation") `
        -Label 'Test root'
    $outputRoot = Join-Path $testOwnerRoot "content\$contentFolder"
    $manifestRoot = Join-Path $testOwnerRoot 'manifests'
    $transactionRoot = Join-Path $testOwnerRoot 'transaction'
    $allowedTargetRoot = $testOwnerRoot
    $rollbackRoot = Join-Path $testOwnerRoot 'RollbackPrevious'
}
else {
    $pluginRoot = Join-Path $projectRoot "Plugins\Resources\$domainName"
    $outputRoot = Join-Path $pluginRoot "Content\$contentFolder"
    $manifestRoot = Join-Path $pluginRoot "Data\Manifests\$manifestFolder"
    $transactionRoot = Join-Path $projectRoot "tmp\$scratchDomain\generation\transactions"
    $allowedTargetRoot = $pluginRoot
    $rollbackRoot = Join-Path $projectRoot "Saved\Validation\$validationFolder\RollbackPrevious"
}
$operationId = if ([string]::IsNullOrWhiteSpace($OperationId)) {
    [System.Guid]::NewGuid().ToString('N')
}
else {
    $OperationId
}
$modeName = if ($isRestore) { 'restore_previous' } else { $Mode.ToLowerInvariant() }
$operationRoot = Join-Path $transactionRoot $operationId
$snapshotRoot = Join-Path $operationRoot 'snapshot'
$receiptPath = Join-Path $operationRoot 'commandlet.receipt.json'
$commandletLogPath = Join-Path $operationRoot 'commandlet.log'
$journalPath = Join-Path $transactionRoot 'journal.json'
$evidenceRoot = if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    if ($isTest) {
        Join-Path $testOwnerRoot 'evidence'
    }
    else {
        Join-Path $projectRoot "Saved\Validation\$validationFolder"
    }
}
else {
    Assert-PathWithin -Path $EvidencePath -Root $projectRoot -Label 'Evidence path'
}

function New-HostReceipt {
    # One structured verdict per run, accepted or rejected, with digests of the content this
    # run leaves on disk (after its own rollback when rejected).
    param([Parameter(Mandatory = $true)][string]$Status, [string]$Rejection = '')
    $hostReceipt = [ordered]@{
        schema_version = '2'
        operation_id = $operationId
        mode = $modeName
        status = $Status
        commandlet_status = $script:commandletStatus
        commandlet_error = $script:commandletError
        commandlet_exit_code = $script:commandletExitCode
        receipt_sha256 = Get-FileSha256OrNone -Path $receiptPath
        manifest_sha256 = $script:commandletManifest
        generated = $script:commandletGenerated
        skipped = $script:commandletSkipped
        shader_compiles = $script:commandletShaderCompiles
        retained_orphans = @($script:commandletRetainedOrphans)
        manifest_tree_sha256 = Get-MaterialTreeSha256 -Root $manifestRoot
        output_tree_sha256 = Get-MaterialTreeSha256 -Root $outputRoot
    }
    if ($isRestore) {
        $hostReceipt['restored_operation_id'] = $RestoreOperationId
        $hostReceipt['source'] = $script:restoreSource
    }
    if ($Status -eq 'accepted') {
        $hostReceipt['accepted_at_utc'] = [DateTime]::UtcNow.ToString('o')
    }
    else {
        $hostReceipt['rejection'] = $Rejection
        $hostReceipt['rejected_at_utc'] = [DateTime]::UtcNow.ToString('o')
    }
    return $hostReceipt
}

$contentLock = $null
$child = $null
$mutationStarted = $false
$script:commandletStatus = 'none'
$script:commandletError = ''
$script:commandletExitCode = -1
$script:commandletManifest = ''
$script:commandletGenerated = 0
$script:commandletSkipped = 0
$script:commandletShaderCompiles = 0
$script:commandletRetainedOrphans = @()
$script:restoreSource = ''
try {
    $contentLock = Enter-ProjectGeneratedContentMutationLock `
        -ProjectRoot $projectRoot `
        -OwnerName 'project generated-content'
    Assert-ProjectGeneratedContentNoProjectUnrealProcess -ProjectFile $projectFile -Purpose "$Domain generation"
    Assert-MaterialOperationIdUnused `
        -OperationId $operationId `
        -OperationRoot $operationRoot `
        -JournalPath $journalPath `
        -RollbackRoot $rollbackRoot
    if ($CleanupOrphans) {
        $pendingRestore = @(Get-PendingRestoreSources)
        if ($pendingRestore.Count -gt 0) {
            throw "Orphan cleanup refused while an unrecovered transaction can still restore content; recover it first: $($pendingRestore[0])"
        }
    }

    $mutationStarted = $true
    $pendingOperationId = Restore-MaterialPendingJournal `
        -JournalPath $journalPath `
        -AllowedTargetRoot $allowedTargetRoot `
        -AllowedSnapshotRoot $transactionRoot

    if ($isRestore) {
        $bundle = $null
        if ($pendingOperationId -ceq $RestoreOperationId) {
            # That operation's own journal already put back the state it replaced.
            $script:restoreSource = 'journal'
        }
        else {
            $bundle = Read-MaterialRollbackBundle -RollbackRoot $rollbackRoot
            if ($null -eq $bundle -or [string]$bundle.replaced_by_operation_id -cne $RestoreOperationId) {
                $script:restoreSource = 'none'
                $bundle = $null
            }
        }
        if ($null -ne $bundle) {
            Assert-MaterialRollbackBundle -Bundle $bundle -RollbackRoot $rollbackRoot
            $null = New-MaterialHostTransaction `
                -OperationId $operationId -ModeName $modeName `
                -OutputRoot $outputRoot -ManifestRoot $manifestRoot `
                -SnapshotRoot $snapshotRoot -JournalPath $journalPath
            Restore-MaterialRoots `
                -SnapshotRoot (Join-Path $rollbackRoot 'snapshot') `
                -OutputRoot $outputRoot `
                -ManifestRoot $manifestRoot `
                -OutputWasPresent ([bool]$bundle.output_was_present) `
                -ManifestWasPresent ([bool]$bundle.manifest_was_present) `
                -OwnerRoot $allowedTargetRoot `
                -WorkRoot (Join-Path $operationRoot 'restore')
            if ((Get-MaterialTreeSha256 -Root $outputRoot) -cne [string]$bundle.output_tree_sha256 -or
                (Get-MaterialTreeSha256 -Root $manifestRoot) -cne [string]$bundle.manifest_tree_sha256) {
                throw "$Domain RestorePrevious left content that does not match the bundle digests."
            }
            # The bundle is kept: a rerun restores the same bytes and the next Regenerate replaces it.
            Remove-Item -LiteralPath $journalPath -Force
            $script:restoreSource = 'bundle'
        }
    }
    else {
        $transaction = New-MaterialHostTransaction `
            -OperationId $operationId -ModeName $modeName `
            -OutputRoot $outputRoot -ManifestRoot $manifestRoot `
            -SnapshotRoot $snapshotRoot -JournalPath $journalPath

        $arguments = @(
            "`"$projectFile`"",
            "-run=$commandletName",
            "-operation=$operationId",
            "-hosttransaction=$operationId",
            "-receipt=`"$receiptPath`"",
            "-mode=$modeName",
            '-unattended',
            '-nopause',
            '-nosplash',
            '-nosound',
            '-NullRHI',
            '-log',
            "-abslog=`"$commandletLogPath`""
        )
        if ($isTest) {
            $arguments += "-testroot=`"$testOwnerRoot`""
        }
        if (-not [string]::IsNullOrWhiteSpace($LayoutReceipt)) {
            $layoutReceiptPath = Assert-PathWithin `
                -Path $LayoutReceipt `
                -Root $projectRoot `
                -Label 'Layout receipt'
            $arguments += "-layoutreceipt=`"$layoutReceiptPath`""
        }
        if (-not [string]::IsNullOrWhiteSpace($PatternTestRoot)) {
            $patternTestRootPath = Assert-PathWithin `
                -Path $PatternTestRoot `
                -Root (Join-Path $projectRoot 'tmp\texture\generation') `
                -Label 'Pattern test root'
            $arguments += "-patterntestroot=`"$patternTestRootPath`""
        }
        if ($CleanupOrphans) {
            $arguments += '-cleanup'
        }
        if ($InjectFailure -eq 'post-save') {
            $arguments += "-injectfailure=$InjectFailure"
        }
        New-Item -ItemType Directory -Path $operationRoot -Force | Out-Null
        $child = Start-Process `
            -FilePath $editorCommand `
            -ArgumentList $arguments `
            -WindowStyle Hidden `
            -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while (-not $child.HasExited -and [DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 250
            $child.Refresh()
        }
        if (-not $child.HasExited) {
            Stop-Process -Id $child.Id -Force
            $child.WaitForExit()
            throw "Material commandlet timed out after $TimeoutSeconds seconds."
        }
        $child.WaitForExit()
        $script:commandletExitCode = $child.ExitCode
        if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
            throw "Material commandlet produced no receipt; exit=$($child.ExitCode)"
        }
        $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
        $expectedAuthentication = Get-StringSha256 -Value (
            "operation=$operationId|status=$([string]$receipt.status)|manifest=$([string]$receipt.manifest_sha256)|generated=$([int]$receipt.generated)|skipped=$([int]$receipt.skipped)")
        if ([string]$receipt.operation_id -cne $operationId -or
            [string]$receipt.authentication_sha256 -cne $expectedAuthentication) {
            $script:commandletStatus = 'unauthenticated'
            throw "Material commandlet rejected or receipt authentication failed; exit=$($child.ExitCode) status=$($receipt.status)"
        }
        $script:commandletStatus = [string]$receipt.status
        $errorProperty = $receipt.PSObject.Properties['error']
        $script:commandletError = if ($null -ne $errorProperty) { [string]$errorProperty.Value } else { '' }
        if ($child.ExitCode -ne 0 -or [string]$receipt.status -cne 'accepted') {
            throw "Material commandlet rejected or receipt authentication failed; exit=$($child.ExitCode) status=$($receipt.status)"
        }
        $script:commandletManifest = [string]$receipt.manifest_sha256
        $script:commandletGenerated = [int]$receipt.generated
        $script:commandletSkipped = [int]$receipt.skipped
        $script:commandletShaderCompiles = [int]$receipt.shader_compiles
        $script:commandletRetainedOrphans = @($receipt.retained_orphans)
        if ($Mode -eq 'Regenerate') {
            New-MaterialRollbackBundle `
                -RollbackRoot $rollbackRoot `
                -SnapshotRoot $snapshotRoot `
                -OperationId $operationId `
                -OutputWasPresent $transaction.OutputWasPresent `
                -ManifestWasPresent $transaction.ManifestWasPresent
            if ($InjectFailure -eq 'pre-commit') {
                throw "$Domain transaction rejected by an injected failure after retention and before commit."
            }
        }
        # Deleting the journal commits this run.
        Remove-Item -LiteralPath $journalPath -Force
    }

    $summary = New-HostReceipt -Status 'accepted'
    $currentEvidence = Join-Path $evidenceRoot 'Current'
    $previousEvidence = Join-Path $evidenceRoot 'Previous'
    $rejectedEvidence = Join-Path $evidenceRoot 'Rejected'
    if (Test-Path -LiteralPath $rejectedEvidence) {
        Remove-Item -LiteralPath $rejectedEvidence -Recurse -Force
    }
    if (Test-Path -LiteralPath $previousEvidence) {
        Remove-Item -LiteralPath $previousEvidence -Recurse -Force
    }
    if (Test-Path -LiteralPath $currentEvidence) {
        Move-Item -LiteralPath $currentEvidence -Destination $previousEvidence
    }
    New-Item -ItemType Directory -Path $currentEvidence -Force | Out-Null
    foreach ($artifact in @($receiptPath, $commandletLogPath)) {
        if (Test-Path -LiteralPath $artifact -PathType Leaf) {
            Copy-Item -LiteralPath $artifact -Destination (Join-Path $currentEvidence (Split-Path -Leaf $artifact)) -Force
        }
    }
    Write-JsonAtomic -Document $summary -Path (Join-Path $currentEvidence 'host.receipt.json')
    Write-Output ($summary | ConvertTo-Json -Compress)
}
catch {
    $failure = $_
    $rejection = $_.Exception.Message
    $rejectedEvidence = $null
    # A run that never held the lock leaves evidence alone: the live owner may be writing it.
    if ($null -ne $contentLock) {
        $rejectedEvidence = Join-Path $evidenceRoot 'Rejected'
        if (Test-Path -LiteralPath $rejectedEvidence) {
            Remove-Item -LiteralPath $rejectedEvidence -Recurse -Force
        }
        New-Item -ItemType Directory -Path $rejectedEvidence -Force | Out-Null
        foreach ($artifact in @($receiptPath, $commandletLogPath)) {
            if (Test-Path -LiteralPath $artifact -PathType Leaf) {
                Copy-Item -LiteralPath $artifact -Destination (Join-Path $rejectedEvidence (Split-Path -Leaf $artifact)) -Force
            }
        }
        Set-Content -LiteralPath (Join-Path $rejectedEvidence 'host.error.txt') -Value $rejection -Encoding UTF8
    }
    $rollbackFailure = $null
    if ($mutationStarted -and (Test-Path -LiteralPath $journalPath -PathType Leaf)) {
        try {
            $null = Restore-MaterialPendingJournal `
                -JournalPath $journalPath `
                -AllowedTargetRoot $allowedTargetRoot `
                -AllowedSnapshotRoot $transactionRoot
        }
        catch {
            $rollbackFailure = $_.Exception.Message
        }
    }
    if ($null -ne $rejectedEvidence) {
        Write-JsonAtomic `
            -Document (New-HostReceipt -Status 'rejected' -Rejection $rejection) `
            -Path (Join-Path $rejectedEvidence 'host.receipt.json')
    }
    if ($null -ne $rollbackFailure) {
        throw "$Domain transaction was rejected and its rollback failed; the journal and its snapshot are kept, and the next run restores them first. Rejection: $rejection Rollback: $rollbackFailure"
    }
    throw $failure
}
finally {
    if ($null -ne $child -and $child -is [System.IDisposable]) {
        $child.Dispose()
    }
    # A journal that survives a failed rollback still names the snapshot in this operation folder.
    if ((Test-Path -LiteralPath $operationRoot) -and -not (Test-Path -LiteralPath $journalPath -PathType Leaf)) {
        Remove-Item -LiteralPath $operationRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $transactionRoot) {
        $remaining = @(Get-ChildItem -LiteralPath $transactionRoot -Force -ErrorAction SilentlyContinue)
        if ($remaining.Count -eq 0) {
            Remove-Item -LiteralPath $transactionRoot -Force
        }
    }
    if (-not $isTest) {
        foreach ($emptyRoot in @(
                (Join-Path $projectRoot "tmp\$scratchDomain\generation"),
                (Join-Path $projectRoot "tmp\$scratchDomain"))) {
            if ((Test-Path -LiteralPath $emptyRoot) -and
                @(Get-ChildItem -LiteralPath $emptyRoot -Force).Count -eq 0) {
                Remove-Item -LiteralPath $emptyRoot -Force
            }
        }
    }
    # Released last: nothing this run does happens after its handle is gone, which outer
    # recovery's quiescence proof relies on.
    if ($null -ne $contentLock) {
        $contentLock.Dispose()
    }
}
