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
        throw "Generated content path escapes its world-data plugin: $candidate"
    }
    return $candidate
}

function Get-ProjectWorldPresentationRoot {
    # SINGLE authority for legacy World-owned generated presentation artifacts.
    # Universal materials are now owned by ProjectMaterial, but remaining World
    # presentation outputs still need one transaction-root authority. Invariant
    # 19 pins this path so rollback cannot omit a shared World-owned artifact.
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContentRoot
    )

    return Join-Path $ContentRoot 'Generated\Presentation'
}

function Get-ProjectWorldGeneratedPaths {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContentRoot,

        [Parameter(Mandatory = $true)]
        [string]$MapPackage,

        [Parameter(Mandatory = $true)]
        [string]$GeneratedPackageRoot,

        [bool]$IncludePresentation = $true
    )

    if ($GeneratedPackageRoot -notmatch '^(/[A-Za-z][A-Za-z0-9_]*/)Generated/$') {
        throw "Generated package root is invalid: $GeneratedPackageRoot"
    }
    $mountRoot = $Matches[1]
    if (-not $MapPackage.StartsWith($GeneratedPackageRoot, [System.StringComparison]::Ordinal) -or
        $MapPackage.Length -le $GeneratedPackageRoot.Length -or
        $MapPackage -notmatch '^/[A-Za-z][A-Za-z0-9_]*/Generated/[A-Za-z0-9_/]+$') {
        throw "Generated map package is outside the supported mount: $MapPackage"
    }
    $relative = $MapPackage.Substring($mountRoot.Length).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
    $mapBase = Join-Path $ContentRoot $relative
    $paths = [System.Collections.Generic.List[string]]::new()
    $parent = Split-Path -Parent $mapBase
    $leaf = Split-Path -Leaf $mapBase
    foreach ($candidate in @("$mapBase.umap", "${mapBase}_BuiltData.uasset")) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $paths.Add((Assert-ProjectWorldOwnedPath -ContentRoot $ContentRoot -Path $candidate))
        }
    }
    if (Test-Path -LiteralPath $parent -PathType Container) {
        # The generator reserves this explicit companion namespace. A plain
        # map-name prefix is never ownership: L_City must not claim L_CityNight.
        Get-ChildItem -LiteralPath $parent -Filter "${leaf}_HLODLayer_*.uasset" -File |
            ForEach-Object {
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
        $presentation = Get-ProjectWorldPresentationRoot -ContentRoot $ContentRoot
        if (Test-Path -LiteralPath $presentation) {
            $paths.Add((Assert-ProjectWorldOwnedPath -ContentRoot $ContentRoot -Path $presentation))
        }
    }
    return @($paths | Sort-Object -Unique)
}

function Assert-ProjectWorldSnapshotCoverage {
    # Invariant 19 fail-closed guard: a transaction may not begin unless its
    # snapshot covers every generated root the operation can mutate. The map
    # scope is derived from the map package, and layer roots arrive as declared
    # AdditionalPaths, but the shared presentation root belongs to no scope and
    # is therefore the one root a caller can silently omit. Refuse rather than
    # discover the omission during a failed rollback.
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContentRoot,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [object[]]$Records
    )

    $presentation = Get-ProjectWorldPresentationRoot -ContentRoot $ContentRoot
    if (-not (Test-Path -LiteralPath $presentation)) {
        return
    }
    $target = [System.IO.Path]::GetFullPath($presentation).TrimEnd('\', '/')
    $covered = @($Records | ForEach-Object {
        [System.IO.Path]::GetFullPath([string]$_.Source).TrimEnd('\', '/')
    })
    if ($covered -notcontains $target) {
        throw "Generated content transaction does not cover the shared presentation root: $target"
    }
}

function New-ProjectWorldGeneratedSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContentRoot,

        [Parameter(Mandatory = $true)]
        [string]$MapPackage,

        [Parameter(Mandatory = $true)]
        [string]$GeneratedPackageRoot,

        [Parameter(Mandatory = $true)]
        [string]$SnapshotRoot,

        [AllowEmptyCollection()]
        [string[]]$AdditionalPaths = @()
    )

    New-Item -ItemType Directory -Path $SnapshotRoot -Force | Out-Null
    $records = [System.Collections.Generic.List[object]]::new()
    $index = 0
    $standardPaths = @(Get-ProjectWorldGeneratedPaths `
        -ContentRoot $ContentRoot -MapPackage $MapPackage `
        -GeneratedPackageRoot $GeneratedPackageRoot)
    $allPaths = @($standardPaths)
    foreach ($path in $AdditionalPaths) {
        $allPaths += Assert-ProjectWorldOwnedPath -ContentRoot $ContentRoot -Path $path
    }
    foreach ($source in @($allPaths | Sort-Object -Unique)) {
        $backup = Join-Path $SnapshotRoot $index
        $existed = Test-Path -LiteralPath $source
        if ($existed) {
            Copy-Item -LiteralPath $source -Destination $backup -Recurse -Force
        }
        $records.Add([pscustomobject]@{ Source = $source; Backup = $backup; Existed = $existed })
        ++$index
    }
    Assert-ProjectWorldSnapshotCoverage -ContentRoot $ContentRoot -Records @($records)
    return @($records)
}

function Remove-ProjectWorldGeneratedPaths {
    param(
        [Parameter(Mandatory = $true)][string]$ContentRoot,
        [Parameter(Mandatory = $true)][string]$MapPackage,
        [Parameter(Mandatory = $true)]
        [string]$GeneratedPackageRoot,
        [bool]$IncludePresentation = $true
    )
    foreach ($path in Get-ProjectWorldGeneratedPaths `
        -ContentRoot $ContentRoot -MapPackage $MapPackage `
        -GeneratedPackageRoot $GeneratedPackageRoot `
        -IncludePresentation $IncludePresentation) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}

function Remove-ProjectWorldGeneratedHLODArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$ContentRoot,
        [Parameter(Mandatory = $true)][string]$MapPackage,
        [Parameter(Mandatory = $true)][string]$GeneratedPackageRoot
    )
    foreach ($path in Get-ProjectWorldGeneratedPaths `
        -ContentRoot $ContentRoot -MapPackage $MapPackage `
        -GeneratedPackageRoot $GeneratedPackageRoot `
        -IncludePresentation $false) {
        if ((Split-Path -Leaf $path) -like '*_HLODLayer_*.uasset') {
            Remove-Item -LiteralPath $path -Force
        }
    }
}

function Restore-ProjectWorldGeneratedSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContentRoot,

        [Parameter(Mandatory = $true)]
        [string]$MapPackage,

        [Parameter(Mandatory = $true)]
        [string]$GeneratedPackageRoot,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [object[]]$Records
    )

    Remove-ProjectWorldGeneratedPaths -ContentRoot $ContentRoot `
        -MapPackage $MapPackage -GeneratedPackageRoot $GeneratedPackageRoot
    foreach ($record in $Records) {
        $destination = Assert-ProjectWorldOwnedPath -ContentRoot $ContentRoot -Path $record.Source
        if (Test-Path -LiteralPath $destination) {
            Remove-Item -LiteralPath $destination -Recurse -Force
        }
        $existedProperty = $record.PSObject.Properties['Existed']
        if ($null -ne $existedProperty -and -not [bool]$existedProperty.Value) {
            continue
        }
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
        [string]$GeneratedPackageRoot,

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
            -GeneratedPackageRoot $GeneratedPackageRoot `
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
