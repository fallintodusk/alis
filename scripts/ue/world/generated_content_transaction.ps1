# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Assert-ProjectWorldOwnedPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContentRoot,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $root = [System.IO.Path]::GetFullPath($ContentRoot).TrimEnd('\', '/')
    $candidate = [System.IO.Path]::GetFullPath($Path)
    $prefix = $root + [System.IO.Path]::DirectorySeparatorChar
    if (-not $candidate.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Generated content path escapes ProjectWorld: $candidate"
    }
    return $candidate
}

function Get-ProjectWorldGeneratedPaths {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContentRoot,

        [Parameter(Mandatory = $true)]
        [string]$MapPackage,

        [bool]$IncludePresentation = $true
    )

    if ($MapPackage -notmatch '^/ProjectWorld/Generated/[A-Za-z0-9_/]+$') {
        throw "Generated map package is outside the supported mount: $MapPackage"
    }
    $relative = $MapPackage.Substring('/ProjectWorld/'.Length).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
    $mapBase = Join-Path $ContentRoot $relative
    $paths = [System.Collections.Generic.List[string]]::new()
    $parent = Split-Path -Parent $mapBase
    $leaf = Split-Path -Leaf $mapBase
    if (Test-Path -LiteralPath $parent -PathType Container) {
        Get-ChildItem -LiteralPath $parent -Filter "$leaf*" | ForEach-Object {
            $paths.Add((Assert-ProjectWorldOwnedPath -ContentRoot $ContentRoot -Path $_.FullName))
        }
    }
    foreach ($externalRoot in @('__ExternalActors__', '__ExternalObjects__')) {
        $candidate = Join-Path (Join-Path $ContentRoot $externalRoot) $relative
        if (Test-Path -LiteralPath $candidate) {
            $paths.Add((Assert-ProjectWorldOwnedPath -ContentRoot $ContentRoot -Path $candidate))
        }
    }
    if ($IncludePresentation) {
        $presentation = Join-Path $ContentRoot 'Generated\Presentation'
        if (Test-Path -LiteralPath $presentation) {
            $paths.Add((Assert-ProjectWorldOwnedPath -ContentRoot $ContentRoot -Path $presentation))
        }
    }
    return @($paths | Sort-Object -Unique)
}

function New-ProjectWorldGeneratedSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContentRoot,

        [Parameter(Mandatory = $true)]
        [string]$MapPackage,

        [Parameter(Mandatory = $true)]
        [string]$SnapshotRoot
    )

    New-Item -ItemType Directory -Path $SnapshotRoot -Force | Out-Null
    $records = [System.Collections.Generic.List[object]]::new()
    $index = 0
    foreach ($source in Get-ProjectWorldGeneratedPaths -ContentRoot $ContentRoot -MapPackage $MapPackage) {
        $backup = Join-Path $SnapshotRoot $index
        Copy-Item -LiteralPath $source -Destination $backup -Recurse -Force
        $records.Add([pscustomobject]@{ Source = $source; Backup = $backup })
        ++$index
    }
    return @($records)
}

function Restore-ProjectWorldGeneratedSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContentRoot,

        [Parameter(Mandatory = $true)]
        [string]$MapPackage,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [object[]]$Records
    )

    foreach ($path in Get-ProjectWorldGeneratedPaths -ContentRoot $ContentRoot -MapPackage $MapPackage) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
    foreach ($record in $Records) {
        $destination = Assert-ProjectWorldOwnedPath -ContentRoot $ContentRoot -Path $record.Source
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $record.Backup -Destination $destination -Recurse -Force
    }
}

function Resolve-ProjectWorldTransactionRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$TransactionParent,

        [Parameter(Mandatory = $true)]
        [string]$TransactionRoot
    )

    $parent = [System.IO.Path]::GetFullPath($TransactionParent).TrimEnd('\', '/')
    $root = [System.IO.Path]::GetFullPath($TransactionRoot)
    $prefix = $parent + [System.IO.Path]::DirectorySeparatorChar
    if (-not $root.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Transaction root escapes its parent: $root"
    }
    return $root
}

function Remove-ProjectWorldGeneratedSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$TransactionParent,

        [Parameter(Mandatory = $true)]
        [string]$TransactionRoot
    )

    $root = Resolve-ProjectWorldTransactionRoot `
        -TransactionParent $TransactionParent `
        -TransactionRoot $TransactionRoot
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}

function Complete-ProjectWorldGeneratedTransaction {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContentRoot,

        [Parameter(Mandatory = $true)]
        [string]$MapPackage,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [object[]]$Records,

        [Parameter(Mandatory = $true)]
        [string]$TransactionParent,

        [Parameter(Mandatory = $true)]
        [string]$TransactionRoot,

        [Parameter(Mandatory = $true)]
        [string]$ResultPath,

        [Parameter(Mandatory = $true)]
        [int]$EngineExitCode,

        [Parameter(Mandatory = $true)]
        [string]$ChildStatus
    )

    $root = Resolve-ProjectWorldTransactionRoot `
        -TransactionParent $TransactionParent `
        -TransactionRoot $TransactionRoot
    if ($EngineExitCode -eq 0 -and $ChildStatus -eq 'accepted') {
        Remove-ProjectWorldGeneratedSnapshot `
            -TransactionParent $TransactionParent `
            -TransactionRoot $root
        return [pscustomobject]@{ State = 'committed'; RecoverySnapshot = $null }
    }

    try {
        Restore-ProjectWorldGeneratedSnapshot `
            -ContentRoot $ContentRoot `
            -MapPackage $MapPackage `
            -Records $Records
    }
    catch {
        throw "Generated content rollback failed. Recovery snapshot preserved at: $root. $($_.Exception.Message)"
    }

    if ($EngineExitCode -ne 0 -and $ChildStatus -eq 'accepted' -and
        (Test-Path -LiteralPath $ResultPath -PathType Leaf)) {
        try {
            Remove-Item -LiteralPath $ResultPath -Force
        }
        catch {
            throw "Generated content was restored, but the stale accepted receipt could not be removed. Recovery snapshot preserved at: $root. $($_.Exception.Message)"
        }
    }

    Remove-ProjectWorldGeneratedSnapshot `
        -TransactionParent $TransactionParent `
        -TransactionRoot $root
    return [pscustomobject]@{ State = 'rolled_back'; RecoverySnapshot = $null }
}
