# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
BeforeAll {
    . (Join-Path $PSScriptRoot '../generated_content_outer_recovery.ps1')
    . (Join-Path $PSScriptRoot '../../world/test/outer_snapshot_test_helpers.ps1')
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
}
