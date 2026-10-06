# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# World's part of a generated-content outer operation: one snapshot of the
# whole generated content roots and the durable manifest root, taken from a
# settled authority, and its restore. The shared lock owner
# (scripts/ue/generated_content) records the snapshot path and the hash of
# records.json in its outer-recovery marker; World owns the records.

Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'generated_content_transaction.ps1')
. (Join-Path $PSScriptRoot 'generated_manifest.ps1')

function Get-ProjectWorldOuterManifestEntries {
    # The authority entries of a manifest root. authority.lock is a live
    # handle, and a journal or staging file is never part of a settled root.
    return @('active_set.json', 'scopes', 'archive')
}

function Get-ProjectWorldOuterTextSha256 {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Value)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}

function Get-ProjectWorldManifestRootSha256 {
    # Tree digest of the durable authority: relative path, length, and SHA-256
    # of every active-set, scope, and archive file, ordinal-sorted.
    param([Parameter(Mandatory = $true)][string]$ManifestRoot)
    $root = [System.IO.Path]::GetFullPath($ManifestRoot).TrimEnd('\', '/')
    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($entry in Get-ProjectWorldOuterManifestEntries) {
        $path = Join-Path $root $entry
        $files = @()
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $files = @(Get-Item -LiteralPath $path -Force)
        }
        elseif (Test-Path -LiteralPath $path -PathType Container) {
            $files = @(Get-ChildItem -LiteralPath $path -Recurse -File -Force)
        }
        foreach ($file in $files) {
            $relative = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
            $lines.Add(('{0}|{1}|{2}' -f $relative, $file.Length, (Get-ProjectWorldFileSha256 -Path $file.FullName)))
        }
    }
    $sorted = $lines.ToArray()
    [Array]::Sort($sorted, [System.StringComparer]::Ordinal)
    return Get-ProjectWorldOuterTextSha256 -Value ($sorted -join "`n")
}

function ConvertTo-ProjectWorldOuterRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$Path
    )
    $root = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $full = [System.IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Outer snapshot path escapes the project: $full"
    }
    return $full.Substring($root.Length).Replace('\', '/')
}

function ConvertFrom-ProjectWorldOuterRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$RelativePath
    )
    if ($RelativePath -notmatch '^[A-Za-z0-9_./ -]+$' -or $RelativePath -match '(^|/)\.\.?(/|$)' -or
        $RelativePath.StartsWith('/')) {
        throw "Outer snapshot record path is not a confined project-relative path: $RelativePath"
    }
    return [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $RelativePath.Replace('/', '\')))
}

function Test-ProjectWorldOuterPathUnder {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root
    )
    $prefix = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return ([System.IO.Path]::GetFullPath($Path) + '\').StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function New-ProjectWorldOuterSnapshot {
    # Snapshots a settled authority for an outer operation and returns the
    # SHA-256 of records.json. The caller holds the shared content lock; the
    # authority lock is taken only for the copy, never across a child process.
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$WorldDataPlugin,
        [Parameter(Mandatory = $true)][string]$SnapshotRoot
    )
    $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $WorldDataPlugin
    $snapshotFull = [System.IO.Path]::GetFullPath($SnapshotRoot)
    if (Test-Path -LiteralPath $snapshotFull) {
        throw "Outer snapshot root already exists: $snapshotFull"
    }
    $authorityLock = Enter-ProjectWorldAuthorityLock -ManifestRoot $roots.ManifestRoot
    try {
        # Settled means no journal and no staging debris; the layout check
        # refuses staging, and an active set must name the map to snapshot.
        Assert-ProjectWorldManifestRootLayout -ManifestRoot $roots.ManifestRoot
        if (Test-Path -LiteralPath (Join-Path $roots.ManifestRoot 'journal.json')) {
            throw 'Outer snapshot refused: a World transaction journal is pending; run recover_generated_transaction.ps1 first.'
        }
        $active = Read-ProjectWorldActiveSet -ManifestRoot $roots.ManifestRoot -ProjectRoot $ProjectRoot
        if ($null -eq $active) {
            throw 'Outer snapshot refused: the World authority has no active set.'
        }
        $mapScopes = @($active.Manifests.Keys | Where-Object { $_ -like 'map_*' } | Sort-Object)
        if ($mapScopes.Count -lt 1) {
            throw 'Outer snapshot refused: the active set has no map scope.'
        }
        $mapPackage = [string]$active.Manifests[$mapScopes[0]].input_identity.map_package
        $contentRecords = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $roots.ContentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $roots.GeneratedPackageRoot `
            -SnapshotRoot $snapshotFull `
            -AdditionalPaths (Get-ProjectWorldGeneratedRoots -ContentRoot $roots.ContentRoot))
        $manifestBackupRoot = Join-Path $snapshotFull 'manifest_root'
        New-Item -ItemType Directory -Path $manifestBackupRoot -Force | Out-Null
        $manifestRecords = foreach ($entry in Get-ProjectWorldOuterManifestEntries) {
            $source = Join-Path $roots.ManifestRoot $entry
            $backup = Join-Path $manifestBackupRoot $entry
            $existed = Test-Path -LiteralPath $source
            if ($existed) {
                Copy-Item -LiteralPath $source -Destination $backup -Recurse -Force
            }
            [pscustomobject]@{ Source = $source; Backup = $backup; Existed = $existed }
        }
        $records = [ordered]@{
            schema_version = 1
            world_data_plugin = $WorldDataPlugin
            map_package = $mapPackage
            content_records = @($contentRecords | ForEach-Object {
                [ordered]@{
                    source = ConvertTo-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -Path $_.Source
                    backup = ConvertTo-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -Path $_.Backup
                    existed = [bool]$_.Existed
                }
            })
            manifest_records = @($manifestRecords | ForEach-Object {
                [ordered]@{
                    source = ConvertTo-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -Path $_.Source
                    backup = ConvertTo-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -Path $_.Backup
                    existed = [bool]$_.Existed
                }
            })
        }
        $recordsPath = Join-Path $snapshotFull 'records.json'
        $staging = "$recordsPath.tmp"
        $json = ($records | ConvertTo-Json -Depth 8) -replace "`r`n", "`n"
        [System.IO.File]::WriteAllText($staging, $json + "`n", [System.Text.UTF8Encoding]::new($false))
        [System.IO.File]::Move($staging, $recordsPath)
        return Get-ProjectWorldFileSha256 -Path $recordsPath
    }
    finally {
        $authorityLock.Dispose()
    }
}

function Read-ProjectWorldOuterSnapshotRecords {
    # Validates an outer snapshot before anything relies on it: the records
    # hash, every source confined to its owner, every backup inside the
    # snapshot and present when its source existed, and complete coverage.
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$WorldDataPlugin,
        [Parameter(Mandatory = $true)][string]$SnapshotRoot,
        [Parameter(Mandatory = $true)][string]$RecordsSha256
    )
    $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $WorldDataPlugin
    $snapshotFull = [System.IO.Path]::GetFullPath($SnapshotRoot)
    $recordsPath = Join-Path $snapshotFull 'records.json'
    if (-not (Test-Path -LiteralPath $recordsPath -PathType Leaf)) {
        throw "Outer snapshot records are missing: $recordsPath"
    }
    if ((Get-ProjectWorldFileSha256 -Path $recordsPath) -cne $RecordsSha256) {
        throw "Outer snapshot records do not match their recorded hash: $recordsPath"
    }
    $document = Get-Content -LiteralPath $recordsPath -Raw | ConvertFrom-Json
    $keys = @($document.PSObject.Properties.Name | Sort-Object)
    if (($keys -join ',') -cne 'content_records,manifest_records,map_package,schema_version,world_data_plugin' -or
        [int]$document.schema_version -ne 1 -or [string]$document.world_data_plugin -cne $WorldDataPlugin) {
        throw 'Outer snapshot records are malformed or belong to another world-data owner.'
    }
    $mapPackage = [string]$document.map_package
    if (-not $mapPackage.StartsWith($roots.GeneratedPackageRoot, [System.StringComparison]::Ordinal) -or
        $mapPackage -notmatch '^/[A-Za-z][A-Za-z0-9_]*/Generated/[A-Za-z0-9_/]+$') {
        throw "Outer snapshot map package is invalid for its owner: $mapPackage"
    }
    function ConvertFrom-OuterRecord {
        param([object]$Record)
        $names = @($Record.PSObject.Properties.Name | Sort-Object)
        if (($names -join ',') -cne 'backup,existed,source' -or $Record.existed -isnot [bool]) {
            throw 'Outer snapshot record is malformed.'
        }
        $backup = ConvertFrom-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -RelativePath ([string]$Record.backup)
        if (-not (Test-ProjectWorldOuterPathUnder -Path $backup -Root $snapshotFull)) {
            throw "Outer snapshot backup escapes the snapshot root: $backup"
        }
        if ($Record.existed -and -not (Test-Path -LiteralPath $backup)) {
            throw "Outer snapshot backup is missing: $backup"
        }
        return [pscustomobject]@{
            Source = ConvertFrom-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -RelativePath ([string]$Record.source)
            Backup = $backup
            Existed = [bool]$Record.existed
        }
    }
    $contentRecords = @($document.content_records | ForEach-Object { ConvertFrom-OuterRecord -Record $_ })
    foreach ($record in $contentRecords) {
        $null = Assert-ProjectWorldOwnedPath -ContentRoot $roots.ContentRoot -Path $record.Source
    }
    foreach ($generatedRoot in Get-ProjectWorldGeneratedRoots -ContentRoot $roots.ContentRoot) {
        $full = [System.IO.Path]::GetFullPath($generatedRoot)
        if (@($contentRecords | Where-Object { $_.Source -ieq $full }).Count -ne 1) {
            throw "Outer snapshot does not cover the generated root: $full"
        }
    }
    $manifestRecords = @($document.manifest_records | ForEach-Object { ConvertFrom-OuterRecord -Record $_ })
    $expected = @(Get-ProjectWorldOuterManifestEntries | ForEach-Object {
        [System.IO.Path]::GetFullPath((Join-Path $roots.ManifestRoot $_))
    } | Sort-Object)
    $actual = @($manifestRecords | ForEach-Object { $_.Source } | Sort-Object)
    if (($actual -join '|') -ine ($expected -join '|')) {
        throw 'Outer snapshot manifest records must be exactly the active set, scopes, and archive of the durable root.'
    }
    return [pscustomobject]@{
        MapPackage = $mapPackage
        ContentRecords = $contentRecords
        ManifestRecords = $manifestRecords
    }
}

function Restore-ProjectWorldOuterSnapshot {
    # Puts the generated roots and the durable manifest root back to the
    # snapshot. World transaction recovery must run first, because a pending
    # journal still owns its own snapshot. Idempotent: a rerun after an
    # interruption restores the same bytes.
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$WorldDataPlugin,
        [Parameter(Mandatory = $true)][string]$SnapshotRoot,
        [Parameter(Mandatory = $true)][string]$RecordsSha256
    )
    $records = Read-ProjectWorldOuterSnapshotRecords `
        -ProjectRoot $ProjectRoot `
        -WorldDataPlugin $WorldDataPlugin `
        -SnapshotRoot $SnapshotRoot `
        -RecordsSha256 $RecordsSha256
    $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $WorldDataPlugin
    $authorityLock = Enter-ProjectWorldAuthorityLock -ManifestRoot $roots.ManifestRoot
    try {
        if (Test-Path -LiteralPath (Join-Path $roots.ManifestRoot 'journal.json')) {
            throw 'Outer restore refused: a World transaction journal is pending; run recover_generated_transaction.ps1 first.'
        }
        Restore-ProjectWorldGeneratedSnapshot `
            -ContentRoot $roots.ContentRoot `
            -MapPackage $records.MapPackage `
            -GeneratedPackageRoot $roots.GeneratedPackageRoot `
            -Records $records.ContentRecords
        # The snapshot came from a settled root, so staging left by an
        # interrupted step of the outer operation is debris.
        foreach ($debris in @('active_set.json.tmp', 'journal.json.tmp')) {
            $path = Join-Path $roots.ManifestRoot $debris
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Force
            }
        }
        foreach ($record in $records.ManifestRecords) {
            if (Test-Path -LiteralPath $record.Source) {
                Remove-Item -LiteralPath $record.Source -Recurse -Force
            }
            if ($record.Existed) {
                Copy-Item -LiteralPath $record.Backup -Destination $record.Source -Recurse -Force
            }
        }
        Assert-ProjectWorldManifestRootLayout -ManifestRoot $roots.ManifestRoot
    }
    finally {
        $authorityLock.Dispose()
    }
}
