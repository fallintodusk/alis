# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_content_transaction.ps1')
}

Describe 'ProjectWorld generated-content transaction' {
    BeforeEach {
        $contentRoot = Join-Path $TestDrive 'Content'
        $mapPackage = '/ProjectWorldTestData/Generated/Representative/L_TestWorld'
        $generatedPackageRoot = '/ProjectWorldTestData/Generated/'
        $mapRoot = Join-Path $contentRoot 'Generated\Representative'
        $mapFile = Join-Path $mapRoot 'L_TestWorld.umap'
        $externalRoot = Join-Path $contentRoot '__ExternalActors__\Generated\Representative\L_TestWorld'
        $transactionParent = Join-Path $TestDrive 'transactions'
        $resultPath = Join-Path $TestDrive 'result.json'
        $journalPath = Join-Path $TestDrive 'journal.json'
        New-Item -ItemType Directory -Path $mapRoot, $externalRoot -Force | Out-Null
    }

    It 'refuses an obsolete presentation artifact root before snapshot mutation' {
        $legacyPresentation = Join-Path $contentRoot 'Generated\Presentation'
        New-Item -ItemType Directory -Path $legacyPresentation -Force | Out-Null
        $snapshotRoot = Join-Path $transactionParent 'snapshot-legacy'
        try {
            { New-ProjectWorldGeneratedSnapshot `
                -ContentRoot $contentRoot -MapPackage $mapPackage `
                -GeneratedPackageRoot $generatedPackageRoot -SnapshotRoot $snapshotRoot } |
                Should -Throw '*Obsolete World presentation artifact root*'
            Test-Path -LiteralPath $snapshotRoot | Should -BeFalse
        }
        finally {
            Remove-Item -LiteralPath $legacyPresentation -Recurse -Force
        }
    }

    It 'restores exact existing map content after rejection' {
        Set-Content -LiteralPath $mapFile -Value 'accepted-map' -NoNewline
        Set-Content -LiteralPath (Join-Path $externalRoot 'actor.uasset') -Value 'accepted-actor' -NoNewline
        $snapshotRoot = Join-Path $transactionParent 'snapshot-existing'
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -SnapshotRoot $snapshotRoot)

        Set-Content -LiteralPath $mapFile -Value 'rejected-map' -NoNewline
        Set-Content -LiteralPath (Join-Path $externalRoot 'actor.uasset') -Value 'rejected-actor' -NoNewline
        Set-Content -LiteralPath (Join-Path $mapRoot 'L_TestWorld_HLODLayer_Rejected.uasset') -Value 'rejected-extra' -NoNewline

        Set-Content -LiteralPath $resultPath -Value '{"status":"rejected"}' -NoNewline
        $completion = Complete-ProjectWorldGeneratedTransaction `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -Records $records `
            -TransactionParent $transactionParent `
            -TransactionRoot $snapshotRoot `
            -ResultPath $resultPath `
            -EngineExitCode 4 `
            -ChildStatus 'rejected' `
            -JournalPath $journalPath

        $completion.State | Should -Be 'rolled_back'
        Get-Content -LiteralPath $mapFile -Raw | Should -Be 'accepted-map'
        Get-Content -LiteralPath (Join-Path $externalRoot 'actor.uasset') -Raw | Should -Be 'accepted-actor'
        Test-Path -LiteralPath (Join-Path $mapRoot 'L_TestWorld_HLODLayer_Rejected.uasset') | Should -BeFalse
        Test-Path -LiteralPath $resultPath | Should -BeTrue
        Test-Path -LiteralPath $snapshotRoot | Should -BeFalse
    }

    It 'removes every generated artifact when the target was initially absent' {
        Remove-Item -LiteralPath $mapRoot, $externalRoot -Recurse -Force
        $snapshotRoot = Join-Path $transactionParent 'snapshot-absent'
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -SnapshotRoot $snapshotRoot)

        New-Item -ItemType Directory -Path $mapRoot, $externalRoot -Force | Out-Null
        Set-Content -LiteralPath $mapFile -Value 'rejected-map' -NoNewline
        Set-Content -LiteralPath (Join-Path $externalRoot 'actor.uasset') -Value 'rejected-actor' -NoNewline

        Restore-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -Records $records

        @(Get-ProjectWorldGeneratedPaths -ContentRoot $contentRoot `
            -MapPackage $mapPackage -GeneratedPackageRoot $generatedPackageRoot) | Should -BeNullOrEmpty
    }

    It 'restores existing and initially absent layer roots in the same rollback' {
        $existingLayer = Join-Path $contentRoot 'Generated\Twin\Terrain'
        $absentLayer = Join-Path $contentRoot 'Generated\Twin\Water'
        New-Item -ItemType Directory -Path $existingLayer -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $existingLayer 'terrain.uasset') -Value 'accepted' -NoNewline
        $snapshotRoot = Join-Path $transactionParent 'snapshot-layers'
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -SnapshotRoot $snapshotRoot `
            -AdditionalPaths @($existingLayer, $absentLayer))

        Set-Content -LiteralPath (Join-Path $existingLayer 'terrain.uasset') -Value 'rejected' -NoNewline
        New-Item -ItemType Directory -Path $absentLayer -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $absentLayer 'water.uasset') -Value 'rejected' -NoNewline
        Restore-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -Records $records

        Get-Content -LiteralPath (Join-Path $existingLayer 'terrain.uasset') -Raw | Should -Be 'accepted'
        Test-Path -LiteralPath $absentLayer | Should -BeFalse
    }

    It 'removes a retired map scope without deleting a sibling map' {
        Set-Content -LiteralPath $mapFile -Value 'map' -NoNewline
        Set-Content -LiteralPath (Join-Path $externalRoot 'actor.uasset') -Value 'actor' -NoNewline
        $hlod = Join-Path $mapRoot 'L_TestWorld_HLODLayer_Merged.uasset'
        $sibling = Join-Path $mapRoot 'L_TestWorldNight.umap'
        Set-Content -LiteralPath $hlod -Value 'hlod' -NoNewline
        Set-Content -LiteralPath $sibling -Value 'sibling' -NoNewline
        Remove-ProjectWorldGeneratedPaths -ContentRoot $contentRoot `
            -MapPackage $mapPackage -GeneratedPackageRoot $generatedPackageRoot
        @(Get-ProjectWorldGeneratedPaths -ContentRoot $contentRoot `
            -MapPackage $mapPackage -GeneratedPackageRoot $generatedPackageRoot) | Should -BeNullOrEmpty
        Test-Path -LiteralPath $hlod | Should -BeFalse
        Get-Content -LiteralPath $sibling -Raw | Should -Be 'sibling'
    }

    It 'uses the declared data-plugin mount for snapshot and rollback' {
        $productionMap = '/ProjectWorldData/Generated/Representative/L_TestWorld'
        Set-Content -LiteralPath $mapFile -Value 'accepted-production-map' -NoNewline
        $snapshotRoot = Join-Path $transactionParent 'snapshot-production-owner'
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot `
            -MapPackage $productionMap `
            -GeneratedPackageRoot '/ProjectWorldData/Generated/' `
            -SnapshotRoot $snapshotRoot)
        Set-Content -LiteralPath $mapFile -Value 'rejected-production-map' -NoNewline
        Restore-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot `
            -MapPackage $productionMap `
            -GeneratedPackageRoot '/ProjectWorldData/Generated/' `
            -Records $records
        Get-Content -LiteralPath $mapFile -Raw | Should -Be 'accepted-production-map'
    }

    It 'rolls back a child accepted result when the engine exits nonzero' {
        Set-Content -LiteralPath $mapFile -Value 'accepted-map' -NoNewline
        $snapshotRoot = Join-Path $transactionParent 'snapshot-engine-failure'
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -SnapshotRoot $snapshotRoot)
        Set-Content -LiteralPath $mapFile -Value 'uncommitted-map' -NoNewline
        Set-Content -LiteralPath $resultPath -Value '{"status":"accepted"}' -NoNewline

        Complete-ProjectWorldGeneratedTransaction `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -Records $records `
            -TransactionParent $transactionParent `
            -TransactionRoot $snapshotRoot `
            -ResultPath $resultPath `
            -EngineExitCode 5 `
            -ChildStatus 'accepted' `
            -JournalPath $journalPath | Out-Null

        Get-Content -LiteralPath $mapFile -Raw | Should -Be 'accepted-map'
        Test-Path -LiteralPath $resultPath | Should -BeFalse
        Test-Path -LiteralPath $snapshotRoot | Should -BeFalse
    }

    It 'rolls back when the child emits no receipt' {
        Set-Content -LiteralPath $mapFile -Value 'accepted-map' -NoNewline
        $snapshotRoot = Join-Path $transactionParent 'snapshot-missing-receipt'
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -SnapshotRoot $snapshotRoot)
        Set-Content -LiteralPath $mapFile -Value 'uncommitted-map' -NoNewline

        Complete-ProjectWorldGeneratedTransaction `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -Records $records `
            -TransactionParent $transactionParent `
            -TransactionRoot $snapshotRoot `
            -ResultPath $resultPath `
            -EngineExitCode 5 `
            -ChildStatus 'missing' `
            -JournalPath $journalPath | Out-Null

        Get-Content -LiteralPath $mapFile -Raw | Should -Be 'accepted-map'
        Test-Path -LiteralPath $resultPath | Should -BeFalse
        Test-Path -LiteralPath $snapshotRoot | Should -BeFalse
    }

    It 'preserves the recovery snapshot when restoration fails' {
        Set-Content -LiteralPath $mapFile -Value 'accepted-map' -NoNewline
        $snapshotRoot = Join-Path $transactionParent 'snapshot-restore-failure'
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot `
            -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot `
            -SnapshotRoot $snapshotRoot)
        Set-Content -LiteralPath $mapFile -Value 'uncommitted-map' -NoNewline
        Mock Copy-Item { throw 'forced restore copy failure' } -ParameterFilter {
            $LiteralPath.StartsWith($snapshotRoot, [System.StringComparison]::OrdinalIgnoreCase)
        }

        $caught = $null
        try {
            Complete-ProjectWorldGeneratedTransaction `
                -ContentRoot $contentRoot `
                -MapPackage $mapPackage `
                -GeneratedPackageRoot $generatedPackageRoot `
                -Records $records `
                -TransactionParent $transactionParent `
                -TransactionRoot $snapshotRoot `
                -ResultPath $resultPath `
                -EngineExitCode 5 `
                -ChildStatus 'missing' `
                -JournalPath $journalPath | Out-Null
        }
        catch {
            $caught = $_
        }

        $caught | Should -Not -BeNullOrEmpty
        $caught.Exception.Message | Should -Match ([regex]::Escape([System.IO.Path]::GetFullPath($snapshotRoot)))
        $caught.Exception.Message | Should -Match 'forced restore copy failure'
        Test-Path -LiteralPath $mapFile | Should -BeFalse
        Test-Path -LiteralPath $snapshotRoot | Should -BeTrue
        Test-Path -LiteralPath $records[0].Backup | Should -BeTrue
    }

}
