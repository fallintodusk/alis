# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_content_transaction.ps1')
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')
    . (Join-Path $PSScriptRoot '..\realization_layer_operation.ps1')
    . (Join-Path $PSScriptRoot '..\layer_execution_selection.ps1')
    . (Join-Path $PSScriptRoot '..\realization_authority_publication.ps1')
    . (Join-Path $PSScriptRoot 'canonical_compatibility_test_helpers.ps1')
    $script:RepositoryRoot=(Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
    $script:FixtureParent=Join-Path $script:RepositoryRoot 'tmp/world/canonical_pair_tests'
}

Describe 'Recoverable canonical and World publication' {
    BeforeEach {
        $ErrorActionPreference='Stop'
        $projectRoot=Join-Path $script:FixtureParent ([guid]::NewGuid().ToString('N'))
        $contentRoot=Join-Path $projectRoot 'Plugins/World/ProjectWorldTestData/Content'
        $manifestRoot=Join-Path $projectRoot 'Plugins/World/ProjectWorldTestData/Data/Manifests'
        $schemaRoot=Join-Path $projectRoot 'Plugins/World/ProjectWorld/Data/Schemas'
        New-Item -ItemType Directory -Path $contentRoot,$schemaRoot,$manifestRoot -Force | Out-Null
        foreach ($name in @('project_world_active_manifest_set.schema.json','project_world_generated_manifest.schema.json')) {
            Copy-Item -LiteralPath (Join-Path $script:RepositoryRoot ('Plugins/World/ProjectWorld/Data/Schemas/'+$name)) -Destination $schemaRoot
        }
        $map='/ProjectWorldTestData/Generated/Representative/L_fixture'
        $mount='/ProjectWorldTestData/Generated/'
        $scope=Get-ProjectWorldMapScopeId -MapPackage $map -GeneratedPackageRoot $mount
        $actor=Join-Path $contentRoot 'Generated/fixture/roads/road.uasset'
        New-Item -ItemType Directory -Path (Split-Path -Parent $actor) -Force | Out-Null
        Set-Content -LiteralPath $actor -Value 'accepted-old-road' -NoNewline
        $building=Join-Path $contentRoot 'Generated/fixture/buildings/building.uasset'
        New-Item -ItemType Directory -Path (Split-Path -Parent $building) -Force | Out-Null
        Set-Content -LiteralPath $building -Value 'accepted-building' -NoNewline
        $fixture=New-CompatibilityFixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot -MapScope $scope `
            -LayerArtifactPaths @{roads=@($actor);buildings=@($building)}
        $transactionId='a'*32
        $transactionParent=Join-Path $projectRoot 'tmp/world/world_realization/transactions'
        $snapshotRoot=Join-Path $transactionParent $transactionId
        $records=@(New-ProjectWorldGeneratedSnapshot -ContentRoot $contentRoot -MapPackage $map `
            -GeneratedPackageRoot $mount -SnapshotRoot $snapshotRoot -AdditionalPaths @((Split-Path -Parent $actor),(Split-Path -Parent $building)))
        $journalPath=Join-Path $manifestRoot 'journal.json'
        $transition=[pscustomobject]@{old_active_sha256=('2'*64);new_active_sha256=('9'*64);
            base_result_sha256=('a'*64);effective_result_sha256=('7'*64);effective_identity=('a'*64)}
        $journal=[pscustomobject]@{
            transaction_id=$transactionId;phase='publishing';operation='apply';map_package=$map;
            snapshot_root=$snapshotRoot;snapshot_records=@($records | ForEach-Object {
                @{source=$_.Source;backup=$_.Backup;existed=[bool]$_.Existed}});
            candidate_manifest_paths=@('scopes/layer_fixture_roads.2.json');expected_active_set_sha256=('e'*64);
            prior_active_set_sha256=$fixture.Active.Sha256;mutation_scope_ids=@('layer_fixture_roads');
            retired_scopes=@();execution_layers=@('roads','water','vegetation');canonical_selection_transition=$transition
        }
        $script:CanonicalState='prior'; $script:RestoreFailure=$false; $script:CanonicalActions=@()
        Mock Invoke-ProjectWorldCanonicalSelection {
            param($ProjectRoot,$Transition,$Action)
            $script:CanonicalActions+=,$Action
            if ($script:CanonicalState -eq 'unknown') {throw 'unknown canonical pointer'}
            if ($Action -eq 'publish') {
                $onDisk=Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
                $onDisk.phase | Should -Be publishing
                $onDisk.expected_active_set_sha256 | Should -Match '^[a-f0-9]{64}$'
                (Get-ProjectWorldFileSha256 (Join-Path $manifestRoot 'active_set.json')) | Should -Be $fixture.Active.Sha256
                $script:CanonicalState='selected'
            }
            if ($Action -eq 'restore') {
                if ($script:RestoreFailure) {throw 'canonical restore failed'}
                (Get-Content -LiteralPath $actor -Raw) | Should -Be 'candidate-road'
                $script:CanonicalState='prior'
            }
            [pscustomobject]@{status='accepted';state=$script:CanonicalState;
                active_sha256=$(if ($script:CanonicalState -eq 'prior') {$Transition.old_active_sha256} else {$Transition.new_active_sha256})}
        }
        Set-Content -LiteralPath $actor -Value 'candidate-road' -NoNewline
        $fixture.Next.artifacts=@(Get-ProjectWorldScopeArtifactRecords -ProjectRoot $projectRoot -ScopePaths @($actor))
        $fixture.Next.layer_contract.semantic_outputs=@(@{artifact_path=$fixture.Next.artifacts[0].path;semantic_sha256=('6'*64)})
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        $completion=@{ContentRoot=$contentRoot;MapPackage=$map;GeneratedPackageRoot=$mount;Records=$records;
            TransactionParent=$transactionParent;TransactionRoot=$snapshotRoot;ResultPath=(Join-Path $projectRoot 'tmp/result.json');
            JournalPath=$journalPath;ProjectRoot=$projectRoot;EngineExitCode=4;ChildStatus='rejected'}
    }
    AfterEach {
        $target=[IO.Path]::GetFullPath($projectRoot)
        if (-not $target.StartsWith(([IO.Path]::GetFullPath($script:FixtureParent)+'\'),[StringComparison]::OrdinalIgnoreCase)) {throw 'Cleanup escapes scratch.'}
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    It 'rolls back packages with both prior authorities and removes recovery only afterward' {
        (Complete-ProjectWorldGeneratedTransaction @completion).State | Should -Be rolled_back
        Get-Content -LiteralPath $actor -Raw | Should -Be 'accepted-old-road'
        $script:CanonicalActions -join ',' | Should -Be validate
        (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot).Sha256 | Should -Be $fixture.Active.Sha256
        Test-Path $journalPath | Should -BeFalse
        Test-Path $snapshotRoot | Should -BeFalse
    }
    It 'restores canonical selection before restoring packages after interruption before World commit' {
        $script:CanonicalState='selected'
        (Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot).State | Should -Be rolled_back
        $script:CanonicalActions -join ',' | Should -Be 'validate,restore'
        Get-Content -LiteralPath $actor -Raw | Should -Be 'accepted-old-road'
        $script:CanonicalState | Should -Be prior
        Test-Path $journalPath | Should -BeFalse
    }
    It 'retains packages and recovery evidence when canonical restoration fails' {
        $script:CanonicalState='selected';$script:RestoreFailure=$true
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*canonical restore failed*'
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $journalPath | Should -BeTrue
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'refuses unknown canonical bytes without World mutation or evidence cleanup' {
        $script:CanonicalState='unknown'
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } | Should -Throw '*unknown canonical pointer*'
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $journalPath | Should -BeTrue
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'refuses unknown World bytes before calling canonical mutation' {
        $journal.prior_active_set_sha256='1'*64
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*neither paired state*'
        $script:CanonicalActions | Should -BeNullOrEmpty
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'refuses incorrect base/map pairing and premature canonical selection' {
        $journal.canonical_selection_transition.base_result_sha256='0'*64
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*map/base*'
        $script:CanonicalActions | Should -BeNullOrEmpty
        $journal.canonical_selection_transition.base_result_sha256='a'*64
        $journal.phase='mutating';$script:CanonicalState='selected'
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*before the publishing phase*'
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'does not trust a successful child when the authority pair never committed' {
        $completion.EngineExitCode=0;$completion.ChildStatus='accepted'
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*unsettled*'
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $journalPath | Should -BeTrue
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'cold validates both committed authorities and changed artifacts before finalizing' {
        $new=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $journal.expected_active_set_sha256=$new.Sha256;$script:CanonicalState='selected'
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        (Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot).State | Should -Be completed
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $journalPath | Should -BeFalse
        Test-Path $snapshotRoot | Should -BeFalse
    }
    It 'preserves a committed pair after a late child failure instead of rolling packages backward' {
        $new=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $journal.expected_active_set_sha256=$new.Sha256;$script:CanonicalState='selected'
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*unsettled*'
        $script:CanonicalActions -join ',' | Should -Be validate
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'refuses the reverse publication order with intact recovery evidence' {
        $new=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $journal.expected_active_set_sha256=$new.Sha256
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } | Should -Throw '*World committed before canonical*'
        Test-Path $journalPath | Should -BeTrue
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'refuses mismatched canonical binding before canonical mutation' {
        $new=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $journal.expected_active_set_sha256=$new.Sha256;$script:CanonicalState='selected'
        $journal.canonical_selection_transition.new_active_sha256='0'*64
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*exact canonical transition binding*'
        $script:CanonicalActions | Should -BeNullOrEmpty
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'retains committed recovery if a changed package no longer matches the saved manifest' {
        $new=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $journal.expected_active_set_sha256=$new.Sha256;$script:CanonicalState='selected'
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        Set-Content -LiteralPath $actor -Value 'unrecognized-package' -NoNewline
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } | Should -Throw '*post-commit validation*'
        Test-Path $journalPath | Should -BeTrue
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'never deletes paired recovery on successful completion with damaged committed packages' {
        $new=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $journal.expected_active_set_sha256=$new.Sha256;$script:CanonicalState='selected'
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        Set-Content -LiteralPath $actor -Value 'unrecognized-package' -NoNewline
        $completion.EngineExitCode=0;$completion.ChildStatus='accepted'
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*post-commit validation*'
        Test-Path $journalPath | Should -BeTrue
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'completes a successful child only after authenticating the committed pair and packages' {
        $new=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $journal.expected_active_set_sha256=$new.Sha256;$script:CanonicalState='selected'
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        $completion.EngineExitCode=0;$completion.ChildStatus='accepted'
        (Complete-ProjectWorldGeneratedTransaction @completion).State | Should -Be committed
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $journalPath | Should -BeFalse
        Test-Path $snapshotRoot | Should -BeFalse
    }
    It 'refuses a missing rollback snapshot before changing canonical selection or packages' {
        $script:CanonicalState='selected'
        $target=Resolve-ProjectWorldTransactionRoot -TransactionParent $transactionParent -TransactionRoot $snapshotRoot
        Remove-Item -LiteralPath $target -Recurse -Force
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } | Should -Throw '*rollback snapshot is missing*'
        $script:CanonicalState | Should -Be selected
        $script:CanonicalActions -join ',' | Should -Be validate
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $journalPath | Should -BeTrue
    }
    It 'refuses a corrupt prior artifact backup before changing either accepted authority' {
        $script:CanonicalState='selected'
        $record=@($records | Where-Object Source -eq (Split-Path -Parent $actor))[0]
        Set-Content -LiteralPath (Join-Path $record.Backup 'road.uasset') -Value 'corrupt-old-road' -NoNewline
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*differs from the prior accepted manifest*'
        $script:CanonicalState | Should -Be selected
        $script:CanonicalActions -join ',' | Should -Be validate
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $journalPath | Should -BeTrue
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'refuses a missing required backup before changing either authority or packages' {
        $script:CanonicalState='selected'
        $record=@($records | Where-Object Source -eq (Split-Path -Parent $actor))[0]
        $target=Resolve-ProjectWorldTransactionRoot -TransactionParent $snapshotRoot -TransactionRoot $record.Backup
        Remove-Item -LiteralPath $target -Recurse -Force
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*required backup is missing*'
        $script:CanonicalState | Should -Be selected
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $journalPath | Should -BeTrue
    }
    It 'retains recovery when restored package bytes do not match prior accepted ownership' {
        Mock Restore-ProjectWorldGeneratedSnapshot {Set-Content -LiteralPath $actor -Value 'bad-restoration' -NoNewline}
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*differs from the prior accepted manifest*'
        $script:CanonicalState | Should -Be prior
        Test-Path $journalPath | Should -BeTrue
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'authenticates carried owner backups that generic snapshot restoration would also replace' {
        $script:CanonicalState='selected'
        $record=@($records | Where-Object Source -eq (Split-Path -Parent $building))[0]
        Set-Content -LiteralPath (Join-Path $record.Backup 'building.uasset') -Value 'corrupt-building' -NoNewline
        { Complete-ProjectWorldGeneratedTransaction @completion } | Should -Throw '*differs from the prior accepted manifest*'
        $script:CanonicalState | Should -Be selected
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Get-Content -LiteralPath $building -Raw | Should -Be 'accepted-building'
        Test-Path $journalPath | Should -BeTrue
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'journals the final World hash, activates canonical selection, then commits World last' {
        $operation=[pscustomobject]@{Compatibility=$fixture.Proof;Transition=$transition}
        $published=Publish-ProjectWorldRealizationAuthority -ProjectRoot $projectRoot -ManifestRoot $manifestRoot `
            -ActiveSet $fixture.Active -TransactionId $transactionId -Candidates @($fixture.Next) -Retired @() -CanonicalOperation $operation
        $script:CanonicalActions -join ',' | Should -Be publish
        $script:CanonicalState | Should -Be selected
        $onDisk=Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
        $onDisk.expected_active_set_sha256 | Should -Be $published.Sha256
        $completion.EngineExitCode=0;$completion.ChildStatus='accepted'
        (Complete-ProjectWorldGeneratedTransaction @completion).State | Should -Be committed
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
    }
    It 'recovers both authorities and package bytes after canonical CAS succeeds but World commit fails' {
        Mock Move-ProjectWorldStagedFile {throw 'World activation refused after canonical CAS'} -ParameterFilter {
            $Path -eq (Join-Path $manifestRoot 'active_set.json')
        }
        $operation=[pscustomobject]@{Compatibility=$fixture.Proof;Transition=$transition}
        { Publish-ProjectWorldRealizationAuthority -ProjectRoot $projectRoot -ManifestRoot $manifestRoot `
            -ActiveSet $fixture.Active -TransactionId $transactionId -Candidates @($fixture.Next) -Retired @() -CanonicalOperation $operation } |
            Should -Throw '*World activation refused*'
        $script:CanonicalState | Should -Be selected
        (Get-ProjectWorldFileSha256 (Join-Path $manifestRoot 'active_set.json')) | Should -Be $fixture.Active.Sha256
        (Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot).State | Should -Be rolled_back
        $script:CanonicalState | Should -Be prior
        Get-Content -LiteralPath $actor -Raw | Should -Be 'accepted-old-road'
        Get-Content -LiteralPath $building -Raw | Should -Be 'accepted-building'
    }
    It 'retains unknown staging bytes before canonical restore or generated rollback' {
        $script:CanonicalState='selected'
        $staging=Join-Path $manifestRoot 'active_set.json.tmp'
        Set-Content -LiteralPath $staging -Value 'foreign staging' -NoNewline
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } |
            Should -Throw '*Staging debris*'
        $script:CanonicalActions | Should -BeNullOrEmpty
        Get-Content -LiteralPath $staging -Raw | Should -Be 'foreign staging'
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $journalPath | Should -BeTrue
        Test-Path $snapshotRoot | Should -BeTrue
    }
    It 'finalizes an exact committed pair with only its journal-qualified staging debris' {
        $new=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $journal.expected_active_set_sha256=$new.Sha256;$script:CanonicalState='selected'
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        $staging=Join-Path $manifestRoot 'active_set.json.tmp'
        Copy-Item -LiteralPath (Join-Path $manifestRoot 'active_set.json') -Destination $staging
        { Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot } | Should -Throw '*Staging debris*'
        (Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot).State | Should -Be completed
        Get-Content -LiteralPath $actor -Raw | Should -Be 'candidate-road'
        Test-Path $staging | Should -BeFalse
        Test-Path $journalPath | Should -BeFalse
    }
}
