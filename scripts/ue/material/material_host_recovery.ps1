# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Recovery state of the ProjectMaterial/ProjectTexture host transaction: the
# journal snapshot, the retained RollbackPrevious bundle, content digests, and
# restores. A restore never deletes live content first: it copies the snapshot
# into staging, checks it, moves the live root aside, moves the staged copy in,
# checks again, and only then deletes the displaced copy.

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

function Get-StringSha256 {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Value)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Get-MaterialTreeSha256 {
    # Relative path, length, and SHA-256 of every file, ordinal-sorted and
    # LF-joined; an absent root is 'none'.
    param([Parameter(Mandatory = $true)][string]$Root)
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        return 'none'
    }
    $fullRoot = Get-NormalizedFullPath -Path $Root
    $lines = [string[]]@(Get-ChildItem -LiteralPath $fullRoot -Recurse -File -Force | ForEach-Object {
        '{0}|{1}|{2}' -f $_.FullName.Substring($fullRoot.Length + 1).Replace('\', '/'), $_.Length,
            (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    })
    [Array]::Sort($lines, [System.StringComparer]::Ordinal)
    return Get-StringSha256 -Value ($lines -join "`n")
}

function Write-JsonAtomic {
    # File.Replace swaps an existing document in one call and File.Move claims
    # an absent name; neither leaves the target missing.
    param(
        [Parameter(Mandatory = $true)][object]$Document,
        [Parameter(Mandatory = $true)][string]$Path
    )
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    $target = [System.IO.Path]::GetFullPath($Path)
    $staging = "$target.staging"
    $Document | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $staging -Encoding UTF8
    if (Test-Path -LiteralPath $target -PathType Leaf) {
        [System.IO.File]::Replace($staging, $target, [NullString]::Value)
    }
    else {
        [System.IO.File]::Move($staging, $target)
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

function Restore-MaterialRoots {
    param(
        [Parameter(Mandatory = $true)][string]$SnapshotRoot,
        [Parameter(Mandatory = $true)][string]$OutputRoot,
        [Parameter(Mandatory = $true)][string]$ManifestRoot,
        [Parameter(Mandatory = $true)][bool]$OutputWasPresent,
        [Parameter(Mandatory = $true)][bool]$ManifestWasPresent,
        [Parameter(Mandatory = $true)][string]$OwnerRoot,
        [Parameter(Mandatory = $true)][string]$WorkRoot
    )
    $owner = Get-NormalizedFullPath -Path $OwnerRoot
    $roots = @(
        [pscustomobject]@{ Name = 'output'; Live = $OutputRoot; Present = $OutputWasPresent; Staged = $null; Expected = 'none' },
        [pscustomobject]@{ Name = 'manifests'; Live = $ManifestRoot; Present = $ManifestWasPresent; Staged = $null; Expected = 'none' }
    )
    foreach ($root in $roots) {
        $root.Live = Assert-PathWithin -Path $root.Live -Root $owner -Label 'Restore target'
        if ($root.Live -eq $owner) {
            throw "Restore refuses to replace an owner root directly: $($root.Live)"
        }
        $source = Join-Path $SnapshotRoot $root.Name
        if ($root.Present -ne (Test-Path -LiteralPath $source -PathType Container)) {
            throw "Material snapshot does not match its recorded $($root.Name) presence; restore refused before any change: $source"
        }
    }
    if (Test-Path -LiteralPath $WorkRoot) {
        Remove-Item -LiteralPath $WorkRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $WorkRoot -Force | Out-Null
    foreach ($root in $roots) {
        if (-not $root.Present) { continue }
        $source = Join-Path $SnapshotRoot $root.Name
        $root.Staged = Join-Path $WorkRoot "staged-$($root.Name)"
        Copy-Item -LiteralPath $source -Destination $root.Staged -Recurse
        $root.Expected = Get-MaterialTreeSha256 -Root $source
        if ((Get-MaterialTreeSha256 -Root $root.Staged) -cne $root.Expected) {
            throw "Staged $($root.Name) copy differs from its snapshot; restore refused before any change."
        }
    }
    foreach ($root in $roots) {
        if (Test-Path -LiteralPath $root.Live) {
            [System.IO.Directory]::Move($root.Live, (Join-Path $WorkRoot "displaced-$($root.Name)"))
        }
        if ($root.Present) {
            New-Item -ItemType Directory -Path (Split-Path -Parent $root.Live) -Force | Out-Null
            [System.IO.Directory]::Move($root.Staged, $root.Live)
        }
    }
    foreach ($root in $roots) {
        if ((Get-MaterialTreeSha256 -Root $root.Live) -cne $root.Expected) {
            throw "Restored $($root.Name) does not match its snapshot: $($root.Live)"
        }
    }
    Remove-Item -LiteralPath $WorkRoot -Recurse -Force
}

function Restore-MaterialSnapshot {
    # Restores one journal's snapshot, then removes the journal before the
    # snapshot, so an interruption leaves debris and never a journal without
    # its snapshot.
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
    $operationRoot = Split-Path -Parent $snapshotRoot
    if ((Split-Path -Leaf $snapshotRoot) -ne 'snapshot' -or
        (Split-Path -Leaf $operationRoot) -ne [string]$Journal.operation_id) {
        throw 'Material recovery journal snapshot is not bound to its operation; recovery fails closed.'
    }
    $outputRoot = Assert-PathWithin `
        -Path ([string]$Journal.output_root) `
        -Root $AllowedTargetRoot `
        -Label 'Output root'
    $manifestRoot = Assert-PathWithin `
        -Path ([string]$Journal.manifest_root) `
        -Root $AllowedTargetRoot `
        -Label 'Manifest root'
    if (-not (Test-Path -LiteralPath $snapshotRoot -PathType Container)) {
        throw "Material recovery snapshot is missing; recovery fails closed before any change: $snapshotRoot"
    }
    Restore-MaterialRoots `
        -SnapshotRoot $snapshotRoot `
        -OutputRoot $outputRoot `
        -ManifestRoot $manifestRoot `
        -OutputWasPresent ([bool]$Journal.output_was_present) `
        -ManifestWasPresent ([bool]$Journal.manifest_was_present) `
        -OwnerRoot $AllowedTargetRoot `
        -WorkRoot (Join-Path $operationRoot 'restore')
    Remove-Item -LiteralPath $JournalPath -Force
    Remove-Item -LiteralPath $snapshotRoot -Recurse -Force
}

function Restore-MaterialPendingJournal {
    # Restores the transaction an earlier run left; returns its operation id,
    # or '' when nothing is pending.
    param(
        [Parameter(Mandatory = $true)][string]$JournalPath,
        [Parameter(Mandatory = $true)][string]$AllowedTargetRoot,
        [Parameter(Mandatory = $true)][string]$AllowedSnapshotRoot
    )
    if (-not (Test-Path -LiteralPath $JournalPath -PathType Leaf)) {
        return ''
    }
    $journal = Get-Content -LiteralPath $JournalPath -Raw | ConvertFrom-Json
    Restore-MaterialSnapshot `
        -Journal $journal `
        -JournalPath $JournalPath `
        -AllowedTargetRoot $AllowedTargetRoot `
        -AllowedSnapshotRoot $AllowedSnapshotRoot
    return [string]$journal.operation_id
}

function New-MaterialHostTransaction {
    # Snapshots the live roots and journals them; returns what was present.
    param(
        [Parameter(Mandatory = $true)][string]$OperationId,
        [Parameter(Mandatory = $true)][string]$ModeName,
        [Parameter(Mandatory = $true)][string]$OutputRoot,
        [Parameter(Mandatory = $true)][string]$ManifestRoot,
        [Parameter(Mandatory = $true)][string]$SnapshotRoot,
        [Parameter(Mandatory = $true)][string]$JournalPath
    )
    New-Item -ItemType Directory -Path $SnapshotRoot -Force | Out-Null
    $outputWasPresent = Copy-DirectorySnapshot -Source $OutputRoot -Destination (Join-Path $SnapshotRoot 'output')
    $manifestWasPresent = Copy-DirectorySnapshot -Source $ManifestRoot -Destination (Join-Path $SnapshotRoot 'manifests')
    $journal = [ordered]@{
        schema_version = '1'
        operation_id = $OperationId
        mode = $ModeName
        output_root = Get-NormalizedFullPath -Path $OutputRoot
        manifest_root = Get-NormalizedFullPath -Path $ManifestRoot
        snapshot_root = Get-NormalizedFullPath -Path $SnapshotRoot
        output_was_present = $outputWasPresent
        manifest_was_present = $manifestWasPresent
    }
    Write-JsonAtomic -Document $journal -Path $JournalPath
    return [pscustomobject]@{ OutputWasPresent = $outputWasPresent; ManifestWasPresent = $manifestWasPresent }
}

function Read-MaterialRollbackBundle {
    # Returns the retained bundle's receipt, or $null when no bundle is retained.
    param([Parameter(Mandatory = $true)][string]$RollbackRoot)
    $receiptPath = Join-Path $RollbackRoot 'rollback.receipt.json'
    if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
        return $null
    }
    try {
        return Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    }
    catch {
        throw "RollbackPrevious receipt is unreadable: $receiptPath"
    }
}

function Assert-MaterialRollbackBundle {
    # Authenticates a bundle before it may replace live content: schema 2,
    # snapshot children present exactly as recorded, and matching digests.
    param(
        [Parameter(Mandatory = $true)][object]$Bundle,
        [Parameter(Mandatory = $true)][string]$RollbackRoot
    )
    foreach ($field in @('schema_version', 'replaced_by_operation_id', 'output_was_present',
            'manifest_was_present', 'output_tree_sha256', 'manifest_tree_sha256')) {
        if ($null -eq $Bundle.PSObject.Properties[$field]) {
            throw "RollbackPrevious receipt has no $field; only a schema 2 bundle binds content digests."
        }
    }
    if ([string]$Bundle.schema_version -ne '2') {
        throw "RollbackPrevious bundle schema $($Bundle.schema_version) is not restorable; only schema 2 binds content digests."
    }
    $snapshot = Join-Path $RollbackRoot 'snapshot'
    foreach ($pair in @(
            @{ Name = 'output'; Present = $Bundle.output_was_present; Digest = $Bundle.output_tree_sha256 },
            @{ Name = 'manifests'; Present = $Bundle.manifest_was_present; Digest = $Bundle.manifest_tree_sha256 })) {
        $child = Join-Path $snapshot $pair.Name
        if ($pair.Present -isnot [bool] -or $pair.Present -ne (Test-Path -LiteralPath $child -PathType Container)) {
            throw "RollbackPrevious $($pair.Name) copy does not match its recorded presence: $child"
        }
        if ((Get-MaterialTreeSha256 -Root $child) -cne [string]$pair.Digest) {
            throw "RollbackPrevious $($pair.Name) copy does not match its recorded digest: $child"
        }
    }
}

function New-MaterialRollbackBundle {
    # Retains the snapshot an accepted Regenerate replaced, bound to its
    # operation id: copy, stage, digest, then rename into place. The journal
    # still exists until the caller commits, so a failure here loses nothing.
    param(
        [Parameter(Mandatory = $true)][string]$RollbackRoot,
        [Parameter(Mandatory = $true)][string]$SnapshotRoot,
        [Parameter(Mandatory = $true)][string]$OperationId,
        [Parameter(Mandatory = $true)][bool]$OutputWasPresent,
        [Parameter(Mandatory = $true)][bool]$ManifestWasPresent
    )
    $staging = "$RollbackRoot.staging"
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
    New-Item -ItemType Directory -Path $staging -Force | Out-Null
    $stagedSnapshot = Join-Path $staging 'snapshot'
    Copy-Item -LiteralPath $SnapshotRoot -Destination $stagedSnapshot -Recurse
    $receipt = [ordered]@{
        schema_version = '2'
        replaced_by_operation_id = $OperationId
        output_was_present = $OutputWasPresent
        manifest_was_present = $ManifestWasPresent
        output_tree_sha256 = Get-MaterialTreeSha256 -Root (Join-Path $stagedSnapshot 'output')
        manifest_tree_sha256 = Get-MaterialTreeSha256 -Root (Join-Path $stagedSnapshot 'manifests')
        retained_at_utc = [DateTime]::UtcNow.ToString('o')
    }
    if ($receipt.output_tree_sha256 -cne (Get-MaterialTreeSha256 -Root (Join-Path $SnapshotRoot 'output')) -or
        $receipt.manifest_tree_sha256 -cne (Get-MaterialTreeSha256 -Root (Join-Path $SnapshotRoot 'manifests'))) {
        throw 'Staged RollbackPrevious copy differs from the transaction snapshot.'
    }
    Write-JsonAtomic -Document $receipt -Path (Join-Path $staging 'rollback.receipt.json')
    Assert-MaterialRollbackBundle -Bundle ([pscustomobject]$receipt) -RollbackRoot $staging
    if (Test-Path -LiteralPath $RollbackRoot) {
        Remove-Item -LiteralPath $RollbackRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $RollbackRoot) -Force | Out-Null
    [System.IO.Directory]::Move($staging, $RollbackRoot)
}

function Assert-MaterialOperationIdUnused {
    # A caller-supplied id names exactly one host transaction; reuse is
    # refused before any snapshot or restore.
    param(
        [Parameter(Mandatory = $true)][string]$OperationId,
        [Parameter(Mandatory = $true)][string]$OperationRoot,
        [Parameter(Mandatory = $true)][string]$JournalPath,
        [Parameter(Mandatory = $true)][string]$RollbackRoot
    )
    if (Test-Path -LiteralPath $OperationRoot) {
        throw "Material operation $OperationId was already used: $OperationRoot"
    }
    if (Test-Path -LiteralPath $JournalPath -PathType Leaf) {
        $pending = $null
        try { $pending = Get-Content -LiteralPath $JournalPath -Raw | ConvertFrom-Json } catch { }
        if ($null -ne $pending -and [string]$pending.operation_id -ceq $OperationId) {
            throw "Material operation $OperationId is the pending journal's operation."
        }
    }
    $bundle = $null
    try { $bundle = Read-MaterialRollbackBundle -RollbackRoot $RollbackRoot } catch { }
    if ($null -ne $bundle -and [string]$bundle.replaced_by_operation_id -ceq $OperationId) {
        throw "Material operation $OperationId is already bound to the retained RollbackPrevious bundle."
    }
}
