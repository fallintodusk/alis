# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
BeforeAll {
    . (Join-Path $PSScriptRoot '../generated_content_outer_recovery.ps1')
    . (Join-Path $PSScriptRoot '../../world/test/outer_snapshot_test_helpers.ps1')
    . (Join-Path $PSScriptRoot '../../world/operator_controls.ps1')
    $script:FixtureParent = Join-Path (Resolve-Path "$PSScriptRoot/../../../..").Path 'tmp/world/canonical_outer_tests'
    $script:RealSnapshot = (Get-Command New-ProjectWorldOuterSnapshot).ScriptBlock
}
Describe 'Canonical participant in the existing outer recovery owner' {
    BeforeEach {
        $ErrorActionPreference = 'Stop'
        $project = New-ProjectWorldOuterTestProject -Root (Join-Path $script:FixtureParent ([guid]::NewGuid().ToString('N')))
        $canonical = Join-Path $project.Root 'canonical.txt'
        Set-Content -LiteralPath $canonical -Value 'prior' -NoNewline
        $before = Get-ProjectWorldOuterTestState -Project $project
        $script:CanonicalCalls = @()
        Mock Invoke-ProjectWorldCanonicalSelection {
            param($ProjectRoot, $Transition, $Action)
            $script:CanonicalCalls += $Action
            $path = Join-Path $ProjectRoot 'canonical.txt'
            $state = Get-Content -LiteralPath $path -Raw
            if ($state -cnotin @('prior', 'selected')) { throw 'Unknown canonical selection' }
            if ($Action -eq 'restore') { Set-Content -LiteralPath $path -Value 'prior' -NoNewline; $state = 'prior' }
            return @{status = 'accepted'; state = $state}
        }
        Mock Get-ProjectGeneratedContentWorldIdentity {
            param($ProjectRoot, $WorldDataPlugin, $EvidenceRoot)
            $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $WorldDataPlugin
            return @{active_set_sha256 = Get-ProjectWorldFileSha256 (Join-Path $roots.ManifestRoot 'active_set.json');
                manifest_root_sha256 = Get-ProjectWorldManifestRootSha256 $roots.ManifestRoot;
                audit_status = 'accepted'; audit_verdict_sha256 = ('1' * 64)}
        }
        Mock Get-ProjectGeneratedContentMaterialIdentity { throw 'Canonical-only work must not invoke Material' }
        $lock = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $project.Root -RequireOwnership
        $prior = Enable-ProjectGeneratedContentLockDelegation -Lock $lock
        $open = @{Lock = $lock; ProjectRoot = $project.Root; WorldDataPlugin = 'ProjectWorldTestData';
            Label = 'refinement_target'; CanonicalTransition = ([pscustomobject]@{fixture = 'canonical-owner-ticket'})}
    }
    AfterEach {
        Disable-ProjectGeneratedContentLockDelegation -Prior $prior
        $lock.Dispose()
        $path = [IO.Path]::GetFullPath($project.Root)
        if (-not $path.StartsWith(([IO.Path]::GetFullPath($script:FixtureParent) + '\'), [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Fixture cleanup escapes its scratch owner.'
        }
        Remove-Item -LiteralPath $path -Recurse -Force
    }
    Context 'Restoring base enrollment capability' {
        BeforeEach {
            $open.PrepareCanonicalBase=$true
            $open.CanonicalTransition=[pscustomobject]@{new_active_sha256=('b'*64);effective_result_sha256=('c'*64)}
            $identity=[ordered]@{map_package=$project.MapPackage;compile_result_sha256=('a'*64);
                presentation_profile_sha256=('b'*64);runtime_profile_sha256='none';authored_overlay_profile_sha256=('d'*64)}
            $open.CanonicalBaseInputs=@([pscustomobject]@{input_identity=[pscustomobject]$identity;realization_profile_sha256=('e'*64)})
            $marker=New-ProjectGeneratedContentOuterOperation @open
            $enroll=@{ProjectRoot=$project.Root;RestoringCanonicalBaseOperationId=$marker.operation_id;WorldDataPlugin='ProjectWorldTestData';
                InputIdentity=$identity;RealizationProfileHash=('e'*64);EnrollManifests=$true;NonInteractive=$true;
                TargetsDurableAuthority=$true;FullApply=$true}
        }
        It 'allows only the admitted existing map and restores a temporarily added scope' {
            Assert-ProjectWorldProductionEnrollmentAllowed @enroll
            $active=Read-ProjectWorldActiveSet -ProjectRoot $project.Root -ManifestRoot $project.ManifestRoot
            $contract=@{realization_profile_id='fixture';realization_profile_sha256=('e'*64);normalized_layer_contract_sha256=('3'*64);
                generator_id='project_constructed_surface';generator_version=1;artifact_root='/ProjectWorldTestData/Generated/Representative/Structures/';
                canonical_inputs=@();dependency_inputs=@();final_dirty_units=@();semantic_outputs=@()}
            $candidate=New-ProjectWorldCandidateManifest -ProjectRoot $project.Root -ScopeId layer_fixture_structures `
                -Generation 1 -OwningLayer structures -OperationId preparing -InputIdentity $identity -ScopePaths @() `
                -ArtifactRecords @() -GeneratorFingerprint ('f'*64) -ConsumerReferences @($project.ScopeId) -LayerContract $contract
            Publish-ProjectWorldActiveSet -ProjectRoot $project.Root -ManifestRoot $project.ManifestRoot -PriorActiveSet $active `
                -TransactionId ('6'*32) -OperationId preparing -CandidateManifests @($candidate) | Out-Null
            (Get-ProjectWorldOuterTestState $project) | Should -Not -Be $before
            $null=Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $project.Root -Marker $marker -EvidenceRoot (Join-Path $project.Root 'tmp/proof')
            (Get-ProjectWorldOuterTestState $project) | Should -Be $before
        }
        It 'refuses invalid context without any scope or authority writes' -ForEach @(
            @{Fault='outer';ExpectedFailure='*matching live outer*'},@{Fault='token';ExpectedFailure='*live outer lock delegation*'},
            @{Fault='plugin';ExpectedFailure='*matching live outer*'},@{Fault='prepared';ExpectedFailure='*prepared World phase*'},
            @{Fault='backup';ExpectedFailure='*snapshot*inventory*'},@{Fault='compile';ExpectedFailure='*exact admitted base inputs*'},
            @{Fault='profile';ExpectedFailure='*exact admitted base inputs*'},@{Fault='operation';ExpectedFailure='*full restoring base Apply only*'},
            @{Fault='new_map';ExpectedFailure='*new map authority*'},@{Fault='selected';ExpectedFailure='*prior canonical selection*'},
            @{Fault='absent';ExpectedFailure='*matching canonical preparation owner*'},
            @{Fault='empty_inputs';ExpectedFailure='*exact admitted base inputs*'},
            @{Fault='permanent';ExpectedFailure='*full restoring base Apply only*'}
        ) {
            switch($Fault) {
                outer {$enroll.RestoringCanonicalBaseOperationId='0'*32}
                token {[Environment]::SetEnvironmentVariable((Get-ProjectGeneratedContentLockTokenVariable),[NullString]::Value,'Process')}
                plugin {$enroll.WorldDataPlugin='OtherWorldData'}
                prepared {$null=Set-ProjectGeneratedContentCanonicalPrepared -Lock $lock -ProjectRoot $project.Root -Marker $marker}
                backup {
                    $backup=Get-ChildItem (Join-Path $project.Root $marker.world.snapshot_root) -Recurse -File |
                        Where-Object Extension -eq '.uasset' | Select-Object -First 1
                    Set-Content -LiteralPath $backup.FullName -Value corrupt -NoNewline
                }
                compile {$identity.compile_result_sha256='0'*64}
                profile {$enroll.RealizationProfileHash='0'*64}
                operation {$enroll.FullApply=$false}
                new_map {
                    $identity.map_package='/ProjectWorldTestData/Generated/Unaccepted/L_New'
                    $marker.canonical_selection.base_inputs[0].input_identity.map_package=$identity.map_package
                    Write-ProjectGeneratedContentOuterMarker -Lock $lock -ProjectRoot $project.Root -Marker $marker
                }
                selected {Set-Content -LiteralPath $canonical -Value selected -NoNewline}
                absent {Mock Read-ProjectGeneratedContentOuterMarker {$null}}
                empty_inputs {
                    $marker.canonical_selection.base_inputs=@()
                    Write-ProjectGeneratedContentOuterMarker -Lock $lock -ProjectRoot $project.Root -Marker $marker
                }
                permanent {$enroll.DurableEnrollmentAuthorized=$true}
            }
            {Assert-ProjectWorldProductionEnrollmentAllowed @enroll} | Should -Throw $ExpectedFailure
            (Get-ProjectWorldOuterTestState $project) | Should -Be $before
            if($Fault -eq 'selected'){(Get-Content $canonical -Raw) | Should -Be selected}
        }
    }
    It 'opens a canonical-only operation without a fake material participant' {
        $marker = New-ProjectGeneratedContentOuterOperation @open
        $marker.material | Should -BeNullOrEmpty
        $marker.canonical_selection.transition.fixture | Should -Be 'canonical-owner-ticket'
        $marker.canonical_selection.snapshot_inventory_sha256 | Should -Match '^[a-f0-9]{64}$'
        Should -Invoke Get-ProjectGeneratedContentMaterialIdentity -Times 0
    }
    It 'restores an advanced canonical pointer and the exact saved World inventory' {
        $marker = New-ProjectGeneratedContentOuterOperation @open
        Set-Content -LiteralPath $canonical -Value 'selected' -NoNewline
        $artifact = Get-ChildItem -LiteralPath $project.ContentRoot -Recurse -File | Select-Object -First 1
        Set-Content -LiteralPath $artifact.FullName -Value 'changed' -NoNewline
        $null = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $project.Root -Marker $marker -EvidenceRoot (Join-Path $project.Root 'tmp/proof')
        (Get-Content -LiteralPath $canonical -Raw) | Should -Be 'prior'
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
        $script:CanonicalCalls | Should -Contain 'restore'
        $null = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $project.Root -Marker $marker -EvidenceRoot (Join-Path $project.Root 'tmp/repeat')
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }
    It 'refuses corrupted backup bytes before either owner can be restored' {
        $marker = New-ProjectGeneratedContentOuterOperation @open
        Set-Content -LiteralPath $canonical -Value 'selected' -NoNewline
        $snapshot = Join-Path $project.Root $marker.world.snapshot_root
        $backup = Get-ChildItem -LiteralPath $snapshot -Recurse -File | Where-Object Name -ne 'records.json' | Select-Object -First 1
        Set-Content -LiteralPath $backup.FullName -Value 'corrupt-backup' -NoNewline
        { Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $project.Root -Marker $marker -EvidenceRoot (Join-Path $project.Root 'tmp/proof') } |
            Should -Throw '*snapshot*inventory*'
        (Get-Content -LiteralPath $canonical -Raw) | Should -Be 'selected'
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
        Should -Invoke Invoke-ProjectWorldCanonicalSelection -Times 0 -ParameterFilter {$Action -eq 'restore'}
        Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeTrue
    }
    It 'refuses an unknown canonical state without changing World' {
        $marker = New-ProjectGeneratedContentOuterOperation @open
        Set-Content -LiteralPath $canonical -Value 'unknown' -NoNewline
        { Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $project.Root -Marker $marker -EvidenceRoot (Join-Path $project.Root 'tmp/proof') } |
            Should -Throw '*Unknown canonical*'
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }
    It 'rejects a corrupted snapshot copy before recording it as the restoration baseline' {
        Mock New-ProjectWorldOuterSnapshot {
            param($ProjectRoot, $WorldDataPlugin, $SnapshotRoot)
            $sha = & $script:RealSnapshot @PSBoundParameters
            $backup = Get-ChildItem -LiteralPath $SnapshotRoot -Recurse -File |
                Where-Object { $_.Name -ne 'records.json' -and $_.Extension -eq '.uasset' } | Select-Object -First 1
            Set-Content -LiteralPath $backup.FullName -Value 'bad-initial-copy' -NoNewline
            return $sha
        }
        { New-ProjectGeneratedContentOuterOperation @open } | Should -Throw '*snapshot*inventory*'
        Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeFalse
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }
    It 'refuses a private target child without live outer delegation' {
        $variable = Get-ProjectGeneratedContentLockTokenVariable
        [Environment]::SetEnvironmentVariable($variable, [NullString]::Value, 'Process')
        try {
            { Enter-ProjectGeneratedContentCanonicalChild -ProjectRoot $project.Root -OperationId ('a' * 32) `
                -CandidateSha256 ('b' * 64) -EffectiveResultSha256 ('c' * 64) } | Should -Throw '*live outer lock delegation*'
        }
        finally { $null = Enable-ProjectGeneratedContentLockDelegation -Lock $lock }
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }
    It 'joins only the matching canonical outer operation and releases refused child handles' {
        $open.CanonicalTransition = [pscustomobject]@{new_active_sha256 = ('b' * 64); effective_result_sha256 = ('c' * 64)}
        $marker = New-ProjectGeneratedContentOuterOperation @open
        $childArgs = @{ProjectRoot = $project.Root; OperationId = $marker.operation_id;
            CandidateSha256 = ('b' * 64); EffectiveResultSha256 = ('c' * 64); WorldDataPlugin = 'ProjectWorldTestData'}
        $child = Enter-ProjectGeneratedContentCanonicalChild @childArgs
        try { $child.CanWrite | Should -BeFalse } finally { $child.Dispose() }
        $childArgs.CandidateSha256 = ('d' * 64)
        { Enter-ProjectGeneratedContentCanonicalChild @childArgs } | Should -Throw '*matching live outer*'
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }
    It 'refuses a matching canonical ticket under a different World snapshot owner' {
        $open.CanonicalTransition = [pscustomobject]@{new_active_sha256 = ('b' * 64); effective_result_sha256 = ('c' * 64)}
        $marker = New-ProjectGeneratedContentOuterOperation @open
        $marker.world.world_data_plugin = 'OtherWorldData'
        Mock Read-ProjectGeneratedContentOuterMarker { return $marker }
        { $child = $null
            try { $child = Enter-ProjectGeneratedContentCanonicalChild -ProjectRoot $project.Root -OperationId $marker.operation_id `
                -CandidateSha256 ('b' * 64) -EffectiveResultSha256 ('c' * 64) -WorldDataPlugin 'ProjectWorldTestData' }
            finally { if ($null -ne $child) { $child.Dispose() } } } |
            Should -Throw '*matching live outer*'
    }
    It 'journals incomplete preparation and refuses selected children until the checkpoint' {
        $open.PrepareCanonicalBase = $true
        $open.CanonicalTransition = [pscustomobject]@{new_active_sha256 = ('b' * 64); effective_result_sha256 = ('c' * 64)}
        $marker = New-ProjectGeneratedContentOuterOperation @open
        $childArgs = @{ProjectRoot = $project.Root; OperationId = $marker.operation_id;
            CandidateSha256 = ('b' * 64); EffectiveResultSha256 = ('c' * 64); WorldDataPlugin = 'ProjectWorldTestData'}
        { Enter-ProjectGeneratedContentCanonicalChild @childArgs } | Should -Throw '*prepared World phase*'
        $child = Enter-ProjectGeneratedContentCanonicalChild @childArgs -PreparingBase
        try { $child.CanWrite | Should -BeFalse } finally { $child.Dispose() }
        $artifact = Get-ChildItem -LiteralPath $project.ContentRoot -Recurse -File | Select-Object -First 1
        Set-Content -LiteralPath $artifact.FullName -Value 'incomplete base' -NoNewline
        $disk = Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root
        $disk.canonical_selection.prepared_world | Should -BeNullOrEmpty
        $null = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $project.Root -Marker $disk -EvidenceRoot (Join-Path $project.Root 'tmp/proof')
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }
    It 'keeps the accepted prepared recovery target distinct from final original restoration' {
        $open.PrepareCanonicalBase = $true
        $marker = New-ProjectGeneratedContentOuterOperation @open
        $artifact = Get-ChildItem -LiteralPath $project.ContentRoot -Recurse -File | Select-Object -First 1
        Set-Content -LiteralPath $artifact.FullName -Value 'accepted prepared base' -NoNewline
        $prepared = Get-ProjectWorldOuterTestState -Project $project
        $marker = Set-ProjectGeneratedContentCanonicalPrepared -Lock $lock -ProjectRoot $project.Root -Marker $marker
        $marker.canonical_selection.prepared_world.inventory_sha256 | Should -Not -Be $marker.canonical_selection.snapshot_inventory_sha256
        Set-Content -LiteralPath $artifact.FullName -Value 'failed child' -NoNewline
        $saved = $marker.canonical_selection.prepared_world
        Restore-ProjectWorldOuterSnapshot -ProjectRoot $project.Root -WorldDataPlugin 'ProjectWorldTestData' `
            -SnapshotRoot (Join-Path $project.Root $saved.snapshot_root) -RecordsSha256 $saved.records_sha256
        Assert-ProjectGeneratedContentCanonicalRestore -ProjectRoot $project.Root -Marker $marker -Prepared -Current
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $prepared
        { Assert-ProjectGeneratedContentCanonicalRestore -ProjectRoot $project.Root -Marker $marker -Current } | Should -Throw '*snapshot inventory*'
        $null = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $project.Root -Marker $marker -EvidenceRoot (Join-Path $project.Root 'tmp/proof')
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }
    It 'does not commit a prepared checkpoint after a rejected cold audit' {
        $open.PrepareCanonicalBase = $true
        $marker = New-ProjectGeneratedContentOuterOperation @open
        Mock Get-ProjectGeneratedContentWorldIdentity { return @{audit_status = 'rejected'} }
        { Set-ProjectGeneratedContentCanonicalPrepared -Lock $lock -ProjectRoot $project.Root -Marker $marker } | Should -Throw '*strict accepted cold audit*'
        (Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root).canonical_selection.prepared_world | Should -BeNullOrEmpty
        (Get-Content -LiteralPath $canonical -Raw) | Should -Be 'prior'
    }
    It 'recovers the original snapshot when checkpoint publication is interrupted' {
        $open.PrepareCanonicalBase = $true
        $marker = New-ProjectGeneratedContentOuterOperation @open
        $artifact = Get-ChildItem -LiteralPath $project.ContentRoot -Recurse -File | Select-Object -First 1
        Set-Content -LiteralPath $artifact.FullName -Value 'prepared before interruption' -NoNewline
        Mock Write-ProjectGeneratedContentOuterMarker { throw 'checkpoint interrupted' }
        { Set-ProjectGeneratedContentCanonicalPrepared -Lock $lock -ProjectRoot $project.Root -Marker $marker } | Should -Throw '*checkpoint interrupted*'
        $disk = Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root
        $disk.canonical_selection.prepared_world | Should -BeNullOrEmpty
        $null = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $project.Root -Marker $disk -EvidenceRoot (Join-Path $project.Root 'tmp/proof')
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }
    It 'authenticates the prepared backup before restoring either authority' {
        $open.PrepareCanonicalBase = $true
        $marker = New-ProjectGeneratedContentOuterOperation @open
        $marker = Set-ProjectGeneratedContentCanonicalPrepared -Lock $lock -ProjectRoot $project.Root -Marker $marker
        $snapshot = Join-Path $project.Root $marker.canonical_selection.prepared_world.snapshot_root
        $backup = Get-ChildItem -LiteralPath $snapshot -Recurse -File | Where-Object Name -ne 'records.json' | Select-Object -First 1
        Set-Content -LiteralPath $backup.FullName -Value 'corrupted prepared backup' -NoNewline
        Set-Content -LiteralPath $canonical -Value 'selected' -NoNewline
        { Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $project.Root -Marker $marker -EvidenceRoot (Join-Path $project.Root 'tmp/proof') } | Should -Throw '*snapshot inventory*'
        (Get-Content -LiteralPath $canonical -Raw) | Should -Be 'selected'
        Should -Invoke Invoke-ProjectWorldCanonicalSelection -Times 0 -ParameterFilter {$Action -eq 'restore'}
    }
    It 'permits measured fingerprint drift only in the explicit preparation phase' {
        Mock Get-ProjectGeneratedContentWorldIdentity {
            param($ProjectRoot, $WorldDataPlugin, $EvidenceRoot)
            $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $WorldDataPlugin
            $failures = @('generator_fingerprint_current: map')
            New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
            @{status='rejected'; checks=@(@{name='generator_fingerprint_current';passed=$false});failures=$failures} |
                ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'world_audit.json')
            return @{active_set_sha256=Get-ProjectWorldFileSha256 (Join-Path $roots.ManifestRoot 'active_set.json');
                manifest_root_sha256=Get-ProjectWorldManifestRootSha256 $roots.ManifestRoot;
                audit_status='rejected';audit_verdict_sha256=Get-ProjectGeneratedContentTextSha256 ($failures -join "`n")}
        }
        { New-ProjectGeneratedContentOuterOperation @open } | Should -Throw '*accepted pre-operation*'
        $open.PrepareCanonicalBase = $true
        $marker = New-ProjectGeneratedContentOuterOperation @open
        $marker.world.pre.audit_status | Should -Be 'rejected'
        $marker.canonical_selection.prepared_world | Should -BeNullOrEmpty
        { Set-ProjectGeneratedContentCanonicalPrepared -Lock $lock -ProjectRoot $project.Root -Marker $marker } | Should -Throw '*strict accepted cold audit*'
        $null = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $project.Root -Marker $marker -EvidenceRoot (Join-Path $project.Root 'tmp/proof')
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }
    It 'refuses damaged authority as a preparation starting point' {
        Mock Get-ProjectGeneratedContentWorldIdentity {
            param($EvidenceRoot)
            New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
            @{status='rejected';checks=@(@{name='artifacts_intact';passed=$false});failures=@('artifacts_intact: missing')} |
                ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'world_audit.json')
            return @{active_set_sha256=('1'*64);manifest_root_sha256=('1'*64);audit_status='rejected';audit_verdict_sha256=('1'*64)}
        }
        $open.PrepareCanonicalBase = $true
        { New-ProjectGeneratedContentOuterOperation @open } | Should -Throw '*only measured producer fingerprint drift*'
        Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeFalse
        (Get-ProjectWorldOuterTestState -Project $project) | Should -Be $before
    }
}
