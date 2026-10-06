# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Every World transaction removes its journal before its snapshot and promotes
# a document with File.Replace or File.Move, so an interruption leaves debris,
# never a journal without its snapshot or a missing published document.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_content_transaction.ps1')
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')
    . (Join-Path $PSScriptRoot 'outer_snapshot_test_helpers.ps1')
}

Describe 'ProjectWorld transaction commit order' {
    BeforeEach {
        # Production callers run with Stop; a failed removal must surface here too.
        $ErrorActionPreference = 'Stop'
        $project = New-ProjectWorldOuterTestProject -Root (Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N')))
        $journalPath = Join-Path $project.ManifestRoot 'journal.json'
        $transactionParent = Join-Path $project.Root 'tmp\world\world_realization\transactions'
        $resultPath = Join-Path $project.Root 'result.json'
    }

    It 'commits by removing the journal and then the snapshot' {
        $snapshotRoot = Join-Path $transactionParent ('a' * 32)
        $records = @(New-ProjectWorldGeneratedSnapshot -ContentRoot $project.ContentRoot `
            -MapPackage $project.MapPackage -GeneratedPackageRoot $project.GeneratedPackageRoot `
            -SnapshotRoot $snapshotRoot)
        Set-Content -LiteralPath $journalPath -Value '{}' -NoNewline
        Set-Content -LiteralPath $project.ActorFile -Value 'accepted-new' -NoNewline
        $completion = Complete-ProjectWorldGeneratedTransaction -ContentRoot $project.ContentRoot `
            -MapPackage $project.MapPackage -GeneratedPackageRoot $project.GeneratedPackageRoot `
            -Records $records -TransactionParent $transactionParent -TransactionRoot $snapshotRoot `
            -ResultPath $resultPath -EngineExitCode 0 -ChildStatus 'accepted' -JournalPath $journalPath
        $completion.State | Should -Be 'committed'
        Get-Content -LiteralPath $project.ActorFile -Raw | Should -Be 'accepted-new'
        Test-Path -LiteralPath $journalPath | Should -BeFalse
        Test-Path -LiteralPath $snapshotRoot | Should -BeFalse
    }

    It 'leaves only snapshot debris when a rolled-back snapshot cannot be removed' {
        $snapshotRoot = Join-Path $transactionParent ('b' * 32)
        $records = @(New-ProjectWorldGeneratedSnapshot -ContentRoot $project.ContentRoot `
            -MapPackage $project.MapPackage -GeneratedPackageRoot $project.GeneratedPackageRoot `
            -SnapshotRoot $snapshotRoot)
        Set-Content -LiteralPath $journalPath -Value '{}' -NoNewline
        Set-Content -LiteralPath $project.ActorFile -Value 'rejected' -NoNewline
        $held = @(Get-ChildItem -LiteralPath $snapshotRoot -Recurse -File)[0].FullName
        # A reader without delete sharing (a scanner, an indexer) blocks the snapshot removal.
        $handle = [System.IO.File]::Open($held, 'Open', 'Read', 'ReadWrite')
        try {
            { Complete-ProjectWorldGeneratedTransaction -ContentRoot $project.ContentRoot `
                -MapPackage $project.MapPackage -GeneratedPackageRoot $project.GeneratedPackageRoot `
                -Records $records -TransactionParent $transactionParent -TransactionRoot $snapshotRoot `
                -ResultPath $resultPath -EngineExitCode 4 -ChildStatus 'rejected' -JournalPath $journalPath } |
                Should -Throw
            Get-Content -LiteralPath $project.ActorFile -Raw | Should -Be 'actor-bytes'
            Test-Path -LiteralPath $journalPath | Should -BeFalse
            Test-Path -LiteralPath $snapshotRoot | Should -BeTrue
        }
        finally {
            $handle.Dispose()
        }
        (Invoke-ProjectWorldTransactionRecovery -ManifestRoot $project.ManifestRoot `
            -ContentRoot $project.ContentRoot -ProjectRoot $project.Root).State | Should -Be 'no_transaction'
    }

    It 'recovery removes the journal before the snapshot' {
        $active = Read-ProjectWorldActiveSet -ManifestRoot $project.ManifestRoot -ProjectRoot $project.Root
        $transactionId = 'c' * 32
        $snapshotRoot = Join-Path $transactionParent $transactionId
        $records = @(New-ProjectWorldGeneratedSnapshot -ContentRoot $project.ContentRoot `
            -MapPackage $project.MapPackage -GeneratedPackageRoot $project.GeneratedPackageRoot `
            -SnapshotRoot $snapshotRoot)
        Write-ProjectWorldTransactionJournal -ManifestRoot $project.ManifestRoot -Journal ([ordered]@{
            transaction_id = $transactionId
            phase = 'mutating'
            operation = 'apply'
            map_package = $project.MapPackage
            snapshot_root = $snapshotRoot
            snapshot_records = @($records | ForEach-Object {
                [ordered]@{ source = $_.Source; backup = $_.Backup; existed = [bool]$_.Existed }
            })
            candidate_manifest_paths = @()
            expected_active_set_sha256 = ''
            prior_active_set_sha256 = $active.Sha256
            mutation_scope_ids = @($project.ScopeId)
            retired_scopes = @()
        })
        Set-Content -LiteralPath $project.ActorFile -Value 'interrupted' -NoNewline
        $held = @(Get-ChildItem -LiteralPath $snapshotRoot -Recurse -File)[0].FullName
        $handle = [System.IO.File]::Open($held, 'Open', 'Read', 'ReadWrite')
        try {
            { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $project.ManifestRoot `
                -ContentRoot $project.ContentRoot -ProjectRoot $project.Root } | Should -Throw
            Get-Content -LiteralPath $project.ActorFile -Raw | Should -Be 'actor-bytes'
            Test-Path -LiteralPath $journalPath | Should -BeFalse
        }
        finally {
            $handle.Dispose()
        }
        (Invoke-ProjectWorldTransactionRecovery -ManifestRoot $project.ManifestRoot `
            -ContentRoot $project.ContentRoot -ProjectRoot $project.Root).State | Should -Be 'no_transaction'
    }

    It 'never loses the published document when its staging file cannot be promoted' {
        $path = Join-Path $project.Root 'document.json'
        Write-ProjectWorldJson -Document ([ordered]@{ value = 1 }) -Path $path
        $published = [System.Convert]::ToBase64String([System.IO.File]::ReadAllBytes($path))
        $staging = "$path.tmp"
        [System.IO.File]::WriteAllText($staging, 'held')
        # The staging file is held without delete sharing, so it cannot be renamed.
        $handle = [System.IO.File]::Open($staging, 'Open', 'Read', 'ReadWrite')
        try {
            { Write-ProjectWorldJson -Document ([ordered]@{ value = 2 }) -Path $path } | Should -Throw
            Test-Path -LiteralPath $path -PathType Leaf | Should -BeTrue
            [System.Convert]::ToBase64String([System.IO.File]::ReadAllBytes($path)) | Should -Be $published
        }
        finally {
            $handle.Dispose()
        }
        Write-ProjectWorldJson -Document ([ordered]@{ value = 3 }) -Path $path
        (Get-Content -LiteralPath $path -Raw | ConvertFrom-Json).value | Should -Be 3
        Test-Path -LiteralPath $staging | Should -BeFalse
    }
}
