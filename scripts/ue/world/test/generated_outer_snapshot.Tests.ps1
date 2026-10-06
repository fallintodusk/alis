# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# World's outer snapshot: whole generated roots plus the durable manifest root,
# taken only from a settled authority and restored exactly; a records file that
# was changed, or that names a path outside its owner, is refused before any
# change.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_outer_snapshot.ps1')
    . (Join-Path $PSScriptRoot 'outer_snapshot_test_helpers.ps1')

    function script:New-TestProject {
        $root = Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N'))
        return New-ProjectWorldOuterTestProject -Root $root
    }

    function script:Set-Records {
        # Rewrites records.json and returns its new hash, so a test reaches the
        # confinement checks behind the hash check.
        param([string]$SnapshotRoot, [scriptblock]$Change)
        $path = Join-Path $SnapshotRoot 'records.json'
        $document = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        & $Change $document
        $json = ($document | ConvertTo-Json -Depth 8) -replace "`r`n", "`n"
        [System.IO.File]::WriteAllText($path, $json + "`n", [System.Text.UTF8Encoding]::new($false))
        return Get-ProjectWorldFileSha256 -Path $path
    }
}

Describe 'ProjectWorld outer snapshot' {
    BeforeEach {
        $project = New-TestProject
        $snapshotRoot = Join-Path $project.Root 'tmp\world\world_realization\outer_recovery\op\outer-snapshot'
    }

    It 'restores the generated roots and the manifest root byte-exactly after every mutation shape' {
        $before = Get-ProjectWorldOuterTestState -Project $project
        $recordsSha = New-ProjectWorldOuterSnapshot -ProjectRoot $project.Root `
            -WorldDataPlugin 'ProjectWorldTestData' -SnapshotRoot $snapshotRoot
        (Get-ProjectWorldFileSha256 -Path (Join-Path $snapshotRoot 'records.json')) | Should -Be $recordsSha

        Set-Content -LiteralPath $project.ActorFile -Value 'edited' -NoNewline
        Remove-Item -LiteralPath $project.SecondActorFile -Force
        $strayRoot = Join-Path $project.ContentRoot '__ExternalObjects__\Generated\Representative'
        New-Item -ItemType Directory -Path $strayRoot -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $strayRoot 'stray.uasset') -Value 'stray' -NoNewline
        Set-Content -LiteralPath (Join-Path $project.ManifestRoot "scopes\$($project.ScopeId).2.json") -Value '{}' -NoNewline
        Set-Content -LiteralPath (Join-Path $project.ManifestRoot 'active_set.json') -Value '{"changed":true}' -NoNewline
        Set-Content -LiteralPath (Join-Path $project.ManifestRoot 'active_set.json.tmp') -Value 'debris' -NoNewline
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Not -Be $before

        Restore-ProjectWorldOuterSnapshot -ProjectRoot $project.Root -WorldDataPlugin 'ProjectWorldTestData' `
            -SnapshotRoot $snapshotRoot -RecordsSha256 $recordsSha

        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
        Test-Path -LiteralPath (Join-Path $project.ContentRoot '__ExternalObjects__\Generated') | Should -BeFalse
        { Assert-ProjectWorldManifestRootLayout -ManifestRoot $project.ManifestRoot } | Should -Not -Throw
        (Read-ProjectWorldActiveSet -ManifestRoot $project.ManifestRoot -ProjectRoot $project.Root).Manifests.Count |
            Should -Be 1

        # Idempotent: a rerun after an interrupted restore puts back the same bytes.
        Restore-ProjectWorldOuterSnapshot -ProjectRoot $project.Root -WorldDataPlugin 'ProjectWorldTestData' `
            -SnapshotRoot $snapshotRoot -RecordsSha256 $recordsSha
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }

    It 'refuses to snapshot an unsettled authority (<Name>) and copies nothing' -ForEach @(
        @{ Name = 'pending journal'; Entry = 'journal.json' },
        @{ Name = 'active-set staging'; Entry = 'active_set.json.tmp' }
    ) {
        Set-Content -LiteralPath (Join-Path $project.ManifestRoot $Entry) -Value '{}' -NoNewline
        { New-ProjectWorldOuterSnapshot -ProjectRoot $project.Root -WorldDataPlugin 'ProjectWorldTestData' `
            -SnapshotRoot $snapshotRoot } | Should -Throw
        Test-Path -LiteralPath $snapshotRoot | Should -BeFalse
    }

    It 'refuses a restore whose records are <Name> and changes nothing' -ForEach @(
        @{ Name = 'changed after the snapshot'; Change = $null; Expected = '*recorded hash*' },
        @{ Name = 'naming a source outside the content root'; Change = { param($d) $d.content_records[0].source = 'tmp/world/elsewhere' }; Expected = '*escapes its world-data plugin*' },
        @{ Name = 'naming a manifest entry outside the authority set'; Change = { param($d) $d.manifest_records[0].source = $d.manifest_records[0].source -replace 'active_set\.json$', 'authority.lock' }; Expected = '*exactly the active set*' },
        @{ Name = 'naming a backup outside the snapshot'; Change = { param($d) $d.content_records[0].backup = 'tmp/world/elsewhere/0' }; Expected = '*escapes the snapshot root*' },
        @{ Name = 'climbing out of the project'; Change = { param($d) $d.content_records[0].source = '../outside' }; Expected = '*confined project-relative path*' }
    ) {
        $recordsSha = New-ProjectWorldOuterSnapshot -ProjectRoot $project.Root `
            -WorldDataPlugin 'ProjectWorldTestData' -SnapshotRoot $snapshotRoot
        if ($null -eq $Change) {
            # The records changed but the recorded hash did not.
            Add-Content -LiteralPath (Join-Path $snapshotRoot 'records.json') -Value ' ' -NoNewline
        }
        else {
            # The hash is refreshed so the confinement checks behind it are reached.
            $recordsSha = Set-Records -SnapshotRoot $snapshotRoot -Change $Change
        }
        Set-Content -LiteralPath $project.ActorFile -Value 'edited' -NoNewline
        $before = Get-ProjectWorldOuterTestState -Project $project
        { Restore-ProjectWorldOuterSnapshot -ProjectRoot $project.Root -WorldDataPlugin 'ProjectWorldTestData' `
            -SnapshotRoot $snapshotRoot -RecordsSha256 $recordsSha } | Should -Throw $Expected
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }

    It 'refuses a restore while a World transaction journal is pending' {
        $recordsSha = New-ProjectWorldOuterSnapshot -ProjectRoot $project.Root `
            -WorldDataPlugin 'ProjectWorldTestData' -SnapshotRoot $snapshotRoot
        Set-Content -LiteralPath $project.ActorFile -Value 'edited' -NoNewline
        Set-Content -LiteralPath (Join-Path $project.ManifestRoot 'journal.json') -Value '{}' -NoNewline
        $before = Get-ProjectWorldOuterTestState -Project $project
        { Restore-ProjectWorldOuterSnapshot -ProjectRoot $project.Root -WorldDataPlugin 'ProjectWorldTestData' `
            -SnapshotRoot $snapshotRoot -RecordsSha256 $recordsSha } | Should -Throw '*journal is pending*'
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }

    It 'digests the manifest authority entries only' {
        $digest = Get-ProjectWorldManifestRootSha256 -ManifestRoot $project.ManifestRoot
        Set-Content -LiteralPath (Join-Path $project.ManifestRoot 'journal.json') -Value '{}' -NoNewline
        Get-ProjectWorldManifestRootSha256 -ManifestRoot $project.ManifestRoot | Should -Be $digest
        New-Item -ItemType Directory -Path (Join-Path $project.ManifestRoot 'archive') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $project.ManifestRoot 'archive\old.1.json') -Value '{}' -NoNewline
        Get-ProjectWorldManifestRootSha256 -ManifestRoot $project.ManifestRoot | Should -Not -Be $digest
    }
}
