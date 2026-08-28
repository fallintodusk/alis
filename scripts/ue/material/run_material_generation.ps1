# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [ValidateSet('Validate', 'Regenerate')]
    [string]$Mode = 'Validate',

    [string]$TestRoot = '',

    [string]$EvidencePath = '',

    [switch]$CleanupOrphans,

    [ValidateSet('', 'post-save')]
    [string]$InjectFailure = '',

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
$config = Resolve-UEConfig -ConfigDir $configDirectory
$editorCommand = Join-Path $config.UE_PATH 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'

function Get-NormalizedFullPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return [System.IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
}

function Assert-PathWithin {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Label
    )
    $fullPath = Get-NormalizedFullPath -Path $Path
    $fullRoot = Get-NormalizedFullPath -Path $Root
    if ($fullPath -ne $fullRoot -and -not $fullPath.StartsWith(
        $fullRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label escapes its admitted owner root: $fullPath"
    }
    return $fullPath
}

function Get-FileSha256OrNone {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return 'none'
    }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-StringSha256 {
    param([Parameter(Mandatory = $true)][string]$Value)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Assert-NoSameProjectEditor {
    $projectNeedle = (Get-NormalizedFullPath -Path $projectFile).Replace('\', '/').ToLowerInvariant()
    $matches = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" |
        Where-Object {
            $commandLine = ([string]$_.CommandLine).Replace('\', '/').ToLowerInvariant()
            $commandLine.Contains($projectNeedle)
        })
    if ($matches.Count -gt 0) {
        $ids = ($matches | ForEach-Object { [string]$_.ProcessId }) -join ','
        throw "ProjectMaterial mutation refused while this project's Editor is running: pid=$ids"
    }
}

function Copy-DirectorySnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )
    if (Test-Path -LiteralPath $Source -PathType Container) {
        New-Item -ItemType Directory -Path $Destination -Force | Out-Null
        Get-ChildItem -LiteralPath $Source -Force |
            Copy-Item -Destination $Destination -Recurse -Force
        return $true
    }
    return $false
}

function Remove-ScopedDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$OwnerRoot
    )
    $target = Assert-PathWithin -Path $Path -Root $OwnerRoot -Label 'Rollback target'
    if ($target -eq (Get-NormalizedFullPath -Path $OwnerRoot)) {
        throw "Rollback refuses to remove an owner root directly: $target"
    }
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

function Restore-MaterialSnapshot {
    param(
        [Parameter(Mandatory = $true)][object]$Journal,
        [Parameter(Mandatory = $true)][string]$JournalPath,
        [Parameter(Mandatory = $true)][string]$AllowedTargetRoot,
        [Parameter(Mandatory = $true)][string]$AllowedSnapshotRoot
    )
    if ([string]$Journal.schema_version -ne '1' -or
        [string]$Journal.operation_id -notmatch '^[a-f0-9]{32}$') {
        throw 'Material recovery journal is malformed; recovery fails closed.'
    }
    $snapshotRoot = Assert-PathWithin `
        -Path ([string]$Journal.snapshot_root) `
        -Root $AllowedSnapshotRoot `
        -Label 'Snapshot root'
    $outputRoot = Assert-PathWithin `
        -Path ([string]$Journal.output_root) `
        -Root $AllowedTargetRoot `
        -Label 'Output root'
    $manifestRoot = Assert-PathWithin `
        -Path ([string]$Journal.manifest_root) `
        -Root $AllowedTargetRoot `
        -Label 'Manifest root'
    Remove-ScopedDirectory -Path $outputRoot -OwnerRoot $AllowedTargetRoot
    Remove-ScopedDirectory -Path $manifestRoot -OwnerRoot $AllowedTargetRoot
    if ([bool]$Journal.output_was_present) {
        Copy-Item -LiteralPath (Join-Path $snapshotRoot 'output') -Destination $outputRoot -Recurse -Force
    }
    if ([bool]$Journal.manifest_was_present) {
        Copy-Item -LiteralPath (Join-Path $snapshotRoot 'manifests') -Destination $manifestRoot -Recurse -Force
    }
    Remove-Item -LiteralPath $JournalPath -Force
    if (Test-Path -LiteralPath $snapshotRoot) {
        Remove-Item -LiteralPath $snapshotRoot -Recurse -Force
    }
}

function Write-JsonAtomic {
    param(
        [Parameter(Mandatory = $true)][object]$Document,
        [Parameter(Mandatory = $true)][string]$Path
    )
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    $staging = "$Path.staging"
    $Document | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $staging -Encoding UTF8
    Move-Item -LiteralPath $staging -Destination $Path -Force
}

if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
    throw "Launcher UnrealEditor-Cmd does not exist: $editorCommand"
}
if ($Mode -eq 'Validate' -and (-not [string]::IsNullOrWhiteSpace($InjectFailure) -or $CleanupOrphans)) {
    throw 'Cleanup and failure injection are valid only for Regenerate.'
}

$isTest = -not [string]::IsNullOrWhiteSpace($TestRoot)
if ($isTest) {
    $testOwnerRoot = Assert-PathWithin `
        -Path $TestRoot `
        -Root (Join-Path $projectRoot 'tmp\material\generation') `
        -Label 'Test root'
    $outputRoot = Join-Path $testOwnerRoot 'content\Generated'
    $manifestRoot = Join-Path $testOwnerRoot 'manifests'
    $transactionRoot = Join-Path $testOwnerRoot 'transaction'
    $allowedTargetRoot = $testOwnerRoot
}
else {
    $pluginRoot = Join-Path $projectRoot 'Plugins\Resources\ProjectMaterial'
    $outputRoot = Join-Path $pluginRoot 'Content\Generated'
    $manifestRoot = Join-Path $pluginRoot 'Data\Manifests\Materials'
    $transactionRoot = Join-Path $projectRoot 'tmp\material\generation\transactions'
    $allowedTargetRoot = $pluginRoot
}
$operationId = [System.Guid]::NewGuid().ToString('N')
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
        Join-Path $projectRoot 'Saved\Validation\MaterialGeneration'
    }
}
else {
    Assert-PathWithin -Path $EvidencePath -Root $projectRoot -Label 'Evidence path'
}

$contentLock = $null
$child = $null
$accepted = $false
try {
    $contentLock = Enter-ProjectGeneratedContentMutationLock `
        -ProjectRoot $projectRoot `
        -OwnerName 'project generated-content'
    Assert-NoSameProjectEditor

    if (Test-Path -LiteralPath $journalPath -PathType Leaf) {
        $pending = Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
        Restore-MaterialSnapshot `
            -Journal $pending `
            -JournalPath $journalPath `
            -AllowedTargetRoot $allowedTargetRoot `
            -AllowedSnapshotRoot $transactionRoot
    }

    New-Item -ItemType Directory -Path $snapshotRoot -Force | Out-Null
    $outputWasPresent = Copy-DirectorySnapshot -Source $outputRoot -Destination (Join-Path $snapshotRoot 'output')
    $manifestWasPresent = Copy-DirectorySnapshot -Source $manifestRoot -Destination (Join-Path $snapshotRoot 'manifests')
    $journal = [ordered]@{
        schema_version = '1'
        operation_id = $operationId
        mode = $Mode.ToLowerInvariant()
        output_root = Get-NormalizedFullPath -Path $outputRoot
        manifest_root = Get-NormalizedFullPath -Path $manifestRoot
        snapshot_root = Get-NormalizedFullPath -Path $snapshotRoot
        output_was_present = $outputWasPresent
        manifest_was_present = $manifestWasPresent
    }
    Write-JsonAtomic -Document $journal -Path $journalPath

    $arguments = @(
        "`"$projectFile`"",
        '-run=ProjectMaterialGenerate',
        "-operation=$operationId",
        "-hosttransaction=$operationId",
        "-receipt=`"$receiptPath`"",
        "-mode=$($Mode.ToLowerInvariant())",
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
    if ($CleanupOrphans) {
        $arguments += '-cleanup'
    }
    if (-not [string]::IsNullOrWhiteSpace($InjectFailure)) {
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
    if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
        throw "Material commandlet produced no receipt; exit=$($child.ExitCode)"
    }
    $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    $expectedAuthentication = Get-StringSha256 -Value (
        "operation=$operationId|status=$([string]$receipt.status)|manifest=$([string]$receipt.manifest_sha256)|generated=$([int]$receipt.generated)|skipped=$([int]$receipt.skipped)")
    if ([string]$receipt.operation_id -cne $operationId -or
        [string]$receipt.authentication_sha256 -cne $expectedAuthentication -or
        $child.ExitCode -ne 0 -or
        [string]$receipt.status -cne 'accepted') {
        throw "Material commandlet rejected or receipt authentication failed; exit=$($child.ExitCode) status=$($receipt.status)"
    }

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
    Copy-Item -LiteralPath $receiptPath -Destination (Join-Path $currentEvidence 'commandlet.receipt.json') -Force
    Copy-Item -LiteralPath $commandletLogPath -Destination (Join-Path $currentEvidence 'commandlet.log') -Force
    $summary = [ordered]@{
        schema_version = '1'
        operation_id = $operationId
        mode = $Mode.ToLowerInvariant()
        commandlet_exit_code = $child.ExitCode
        receipt_sha256 = Get-FileSha256OrNone -Path $receiptPath
        manifest_sha256 = [string]$receipt.manifest_sha256
        generated = [int]$receipt.generated
        skipped = [int]$receipt.skipped
        shader_compiles = [int]$receipt.shader_compiles
        accepted_at_utc = [DateTime]::UtcNow.ToString('o')
    }
    Write-JsonAtomic -Document $summary -Path (Join-Path $currentEvidence 'host.receipt.json')
    if (-not $isTest -and $Mode -eq 'Regenerate') {
        $rollbackRoot = Join-Path $projectRoot 'Saved\Validation\MaterialGeneration\RollbackPrevious'
        if (Test-Path -LiteralPath $rollbackRoot) {
            Remove-Item -LiteralPath $rollbackRoot -Recurse -Force
        }
        New-Item -ItemType Directory -Path $rollbackRoot -Force | Out-Null
        if (Test-Path -LiteralPath $snapshotRoot) {
            Move-Item -LiteralPath $snapshotRoot -Destination (Join-Path $rollbackRoot 'snapshot')
        }
        $rollbackReceipt = [ordered]@{
            schema_version = '1'
            replaced_by_operation_id = $operationId
            output_was_present = $outputWasPresent
            manifest_was_present = $manifestWasPresent
            retained_at_utc = [DateTime]::UtcNow.ToString('o')
        }
        Write-JsonAtomic -Document $rollbackReceipt -Path (Join-Path $rollbackRoot 'rollback.receipt.json')
    }
    Remove-Item -LiteralPath $journalPath -Force
    $accepted = $true
    Write-Output ($summary | ConvertTo-Json -Compress)
}
catch {
    $rejectedEvidence = Join-Path $evidenceRoot 'Rejected'
    if (Test-Path -LiteralPath $rejectedEvidence) {
        Remove-Item -LiteralPath $rejectedEvidence -Recurse -Force
    }
    New-Item -ItemType Directory -Path $rejectedEvidence -Force | Out-Null
    if (Test-Path -LiteralPath $receiptPath -PathType Leaf) {
        Copy-Item -LiteralPath $receiptPath -Destination (Join-Path $rejectedEvidence 'commandlet.receipt.json') -Force
    }
    if (Test-Path -LiteralPath $commandletLogPath -PathType Leaf) {
        Copy-Item -LiteralPath $commandletLogPath -Destination (Join-Path $rejectedEvidence 'commandlet.log') -Force
    }
    Set-Content -LiteralPath (Join-Path $rejectedEvidence 'host.error.txt') -Value $_.Exception.Message -Encoding UTF8
    if (Test-Path -LiteralPath $journalPath -PathType Leaf) {
        $pending = Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
        Restore-MaterialSnapshot `
            -Journal $pending `
            -JournalPath $journalPath `
            -AllowedTargetRoot $allowedTargetRoot `
            -AllowedSnapshotRoot $transactionRoot
    }
    throw
}
finally {
    if ($null -ne $child) {
        $child.Dispose()
    }
    if ($null -ne $contentLock) {
        $contentLock.Dispose()
    }
    if (Test-Path -LiteralPath $operationRoot) {
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
                (Join-Path $projectRoot 'tmp\material\generation'),
                (Join-Path $projectRoot 'tmp\material'))) {
            if ((Test-Path -LiteralPath $emptyRoot) -and
                @(Get-ChildItem -LiteralPath $emptyRoot -Force).Count -eq 0) {
                Remove-Item -LiteralPath $emptyRoot -Force
            }
        }
    }
}
