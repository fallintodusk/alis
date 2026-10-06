# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Outer recovery across processes: delegated owner children, a failed restore
# followed by a dead coordinator, cleanup refusals, malformed markers, an
# orphaned delegated child, the marker write protocol, and identity
# verification. Every case runs on its own fake project root in TestDrive with
# its own lock and marker; no case takes the repository's lock or launches
# Unreal. ProjectMaterial is a stand-in host script at the fake root.

BeforeAll {
    $script:Library = (Resolve-Path (Join-Path $PSScriptRoot '..\generated_content_outer_recovery.ps1')).Path
    . $script:Library
    . (Join-Path $PSScriptRoot 'outer_recovery_test_helpers.ps1')
    $script:LockFile = (Resolve-Path (Join-Path $PSScriptRoot '..\generated_content_mutation_lock.ps1')).Path
    $script:Entry = (Resolve-Path (Join-Path $PSScriptRoot '..\recover_generated_content.ps1')).Path
    $script:Audit = (Resolve-Path (Join-Path $PSScriptRoot '..\..\world\audit_generated_authority.ps1')).Path
    $script:Cleanup = (Resolve-Path (Join-Path $PSScriptRoot '..\..\world\cleanup_workspace.ps1')).Path
    $script:Variable = Get-ProjectGeneratedContentLockTokenVariable

    function script:New-TestProject {
        return New-OuterRecoveryTestProject -Root (Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N')))
    }

    function script:Get-MarkerState {
        # Marker, staging, and the confined operation root, as bytes on disk.
        param([object]$Project)
        $marker = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $Project.Root
        $outer = Join-Path (Split-Path -Parent $marker) 'outer_recovery'
        return Get-ProjectWorldOuterTestDigest -Paths @($marker, "$marker.tmp", $outer)
    }

    function script:Invoke-Entry {
        param([string]$Root)
        & $script:Entry -ProjectRoot $Root | Out-Null
        return $LASTEXITCODE
    }

    $script:DelegateBody = @'
param([string]$LockFile, [string]$Root, [string]$Ready, [string]$Release)
$ErrorActionPreference = 'Stop'
. $LockFile
$handle = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $Root
if ($handle.CanWrite) { [Environment]::Exit(11) }
Set-Content -LiteralPath "$Ready.tmp" -Value $PID -NoNewline
Move-Item -LiteralPath "$Ready.tmp" -Destination $Ready
$deadline = [DateTime]::UtcNow.AddSeconds(180)
while (-not (Test-Path -LiteralPath $Release) -and [DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 100
}
$handle.Dispose()
'@

    $script:FailedRestoreCoordinatorBody = @'
param([string]$Library, [string]$Root, [string]$LayoutReceipt, [string]$ScopeId, [string]$EditFile)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
. $Library
$lock = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $Root -OwnerName 'test coordinator' -RequireOwnership
$null = Enable-ProjectGeneratedContentLockDelegation -Lock $lock
$marker = New-ProjectGeneratedContentOuterOperation -Lock $lock -ProjectRoot $Root `
    -WorldDataPlugin 'ProjectWorldTestData' -Label 'test_failed_restore' -LayoutReceipt $LayoutReceipt
Set-ProjectGeneratedContentOuterPhase -Lock $lock -ProjectRoot $Root -Phase 'o4_running'
Set-Content -LiteralPath $EditFile -Value 'changed-by-operation' -NoNewline
$roots = Resolve-ProjectWorldDataRoots -ProjectRoot $Root -PluginName 'ProjectWorldTestData'
Set-Content -LiteralPath (Join-Path $roots.ManifestRoot "scopes\$ScopeId.2.json") -Value '{}' -NoNewline
Set-ProjectGeneratedContentOuterPhase -Lock $lock -ProjectRoot $Root -Phase 'restoring'
try {
    $null = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $Root `
        -Marker (Read-ProjectGeneratedContentOuterMarker -ProjectRoot $Root) `
        -EvidenceRoot (Get-ProjectGeneratedContentOuterEvidenceRoot -ProjectRoot $Root -OperationId $marker.operation_id)
}
catch {
    Set-ProjectGeneratedContentOuterPhase -Lock $lock -ProjectRoot $Root -Phase 'restore_failed'
    # The coordinator keeps the lock while it lives and never releases it after a failed
    # restore; its exit is what drops the handle.
    [Environment]::Exit(3)
}
[Environment]::Exit(0)
'@

    $script:OrphaningCoordinatorBody = @'
param([string]$Library, [string]$LockFile, [string]$Root, [string]$LayoutReceipt, [string]$ScopeId,
    [string]$MaterialOperation, [string]$DelegateScript, [string]$Ready, [string]$Release)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
. $Library
$lock = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $Root -OwnerName 'test coordinator' -RequireOwnership
$null = Enable-ProjectGeneratedContentLockDelegation -Lock $lock
$null = New-ProjectGeneratedContentOuterOperation -Lock $lock -ProjectRoot $Root `
    -WorldDataPlugin 'ProjectWorldTestData' -Label 'test_orphaned_child' -LayoutReceipt $LayoutReceipt
# A Material operation is bound before it starts and retains the state it replaces.
Set-ProjectGeneratedContentOuterPhase -Lock $lock -ProjectRoot $Root -Phase 'o3_running' -MaterialOperationId $MaterialOperation
Copy-Item -LiteralPath (Join-Path $Root 'material_state.txt') -Destination (Join-Path $Root "material_bundle_$MaterialOperation.txt")
Set-Content -LiteralPath (Join-Path $Root 'material_state.txt') -Value 'material-after-operation' -NoNewline
# A delegated child inherits the token and outlives this coordinator.
$null = Start-Process -FilePath (Get-Process -Id $PID).Path -PassThru -WindowStyle Hidden -ArgumentList @(
    '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', "`"$DelegateScript`"",
    '-LockFile', "`"$LockFile`"", '-Root', "`"$Root`"", '-Ready', "`"$Ready`"", '-Release', "`"$Release`"")
$deadline = [DateTime]::UtcNow.AddSeconds(120)
while (-not (Test-Path -LiteralPath $Ready)) {
    if ([DateTime]::UtcNow -gt $deadline) { [Environment]::Exit(10) }
    Start-Sleep -Milliseconds 100
}
# A World transaction interrupted mid-mutation: its journal, its inner snapshot, a candidate
# manifest, and changed content.
$roots = Resolve-ProjectWorldDataRoots -ProjectRoot $Root -PluginName 'ProjectWorldTestData'
$active = Read-ProjectWorldActiveSet -ManifestRoot $roots.ManifestRoot -ProjectRoot $Root
$transactionId = [System.Guid]::NewGuid().ToString('N')
$snapshotRoot = Join-Path $Root "tmp\world\world_realization\transactions\$transactionId"
$mapPackage = [string]$active.Manifests[$ScopeId].input_identity.map_package
$records = @(New-ProjectWorldGeneratedSnapshot -ContentRoot $roots.ContentRoot -MapPackage $mapPackage `
    -GeneratedPackageRoot $roots.GeneratedPackageRoot -SnapshotRoot $snapshotRoot)
Write-ProjectWorldTransactionJournal -ManifestRoot $roots.ManifestRoot -Journal ([ordered]@{
    transaction_id = $transactionId
    phase = 'mutating'
    operation = 'apply'
    map_package = $mapPackage
    snapshot_root = $snapshotRoot
    snapshot_records = @($records | ForEach-Object {
        [ordered]@{ source = $_.Source; backup = $_.Backup; existed = [bool]$_.Existed }
    })
    candidate_manifest_paths = @("scopes/$ScopeId.2.json")
    expected_active_set_sha256 = ''
    prior_active_set_sha256 = $active.Sha256
    mutation_scope_ids = @($ScopeId)
    retired_scopes = @()
})
Set-Content -LiteralPath (Join-Path $roots.ManifestRoot "scopes\$ScopeId.2.json") -Value '{}' -NoNewline
Get-ChildItem -LiteralPath (Join-Path $roots.ContentRoot '__ExternalActors__\Generated') -Recurse -File |
    ForEach-Object { Set-Content -LiteralPath $_.FullName -Value 'changed-by-interrupted-transaction' -NoNewline }
# No finally block runs: the OS closes this coordinator's handle and leaves the delegated child.
[Environment]::Exit(9)
'@
}

Describe 'Generated-content outer recovery' {
    BeforeEach {
        $ErrorActionPreference = 'Stop'
        $script:Orphan = $null
    }

    AfterEach {
        [Environment]::SetEnvironmentVariable($script:Variable, [NullString]::Value, 'Process')
        if ($null -ne $script:Orphan -and -not $script:Orphan.HasExited) {
            Stop-Process -Id $script:Orphan.Id -Force -ErrorAction SilentlyContinue
        }
    }

    Context 'delegated owner children' {
        It 'admits a World child carrying the live token and refuses one without it' {
            $project = New-TestProject
            $owner = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $project.Root -OwnerName 'test coordinator' -RequireOwnership
            try {
                $prior = Enable-ProjectGeneratedContentLockDelegation -Lock $owner
                $entered = Join-Path $project.Root 'audit_entered.json'
                $null = Invoke-OuterRecoveryChild -Script $script:Audit -Arguments @(
                    '-ProjectRoot', $project.Root, '-WorldDataPlugin', 'ProjectWorldTestData', '-EvidencePath', $entered)
                $receipt = Get-Content -LiteralPath $entered -Raw | ConvertFrom-Json
                @($receipt.checks | Where-Object { $_.name -eq 'transaction_settled' -and $_.passed }).Count | Should -Be 1
                (@($receipt.failures) -join ';') | Should -Not -BeLike '*Another operation holds*'

                Disable-ProjectGeneratedContentLockDelegation -Prior $prior
                $refused = Join-Path $project.Root 'audit_refused.json'
                $child = Invoke-OuterRecoveryChild -Script $script:Audit -Arguments @(
                    '-ProjectRoot', $project.Root, '-WorldDataPlugin', 'ProjectWorldTestData', '-EvidencePath', $refused)
                $child.ExitCode | Should -Be 1
                $receipt = Get-Content -LiteralPath $refused -Raw | ConvertFrom-Json
                (@($receipt.failures) -join ';') |
                    Should -BeLike '*authority_audit_execution: Another operation holds the ProjectWorld content mutation lock*'
                { Enter-ProjectWorldContentLock -ProjectRoot $project.Root } |
                    Should -Throw '*Another operation holds the ProjectWorld content mutation lock*'
            }
            finally { $owner.Dispose() }
        }

        It 'refuses a child that presents a token from before recovery' {
            $project = New-TestProject
            $owner = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $project.Root -RequireOwnership
            $null = Enable-ProjectGeneratedContentLockDelegation -Lock $owner
            $stale = [Environment]::GetEnvironmentVariable($script:Variable)
            [Environment]::SetEnvironmentVariable($script:Variable, [NullString]::Value, 'Process')
            Set-Content -LiteralPath (Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $project.Root) -Value '{}'
            $owner.Dispose()
            $recovery = Enter-ProjectGeneratedContentRecoveryLock -ProjectRoot $project.Root `
                -ProjectFile (Join-Path $project.Root 'Alis.uproject')
            try {
                $delegateScript = Join-Path $project.Root 'delegate_once.ps1'
                Set-Content -LiteralPath $delegateScript -Value @'
param([string]$LockFile, [string]$Root)
$ErrorActionPreference = 'Stop'
. $LockFile
(Enter-ProjectGeneratedContentMutationLock -ProjectRoot $Root).Dispose()
'@
                [Environment]::SetEnvironmentVariable($script:Variable, $stale, 'Process')
                $child = Invoke-OuterRecoveryChild -Script $delegateScript -Arguments @('-LockFile', $script:LockFile, '-Root', $project.Root)
                $child.ExitCode | Should -Not -Be 0
                $child.Output | Should -BeLike '*does not match the live lock owner*'
            }
            finally {
                [Environment]::SetEnvironmentVariable($script:Variable, [NullString]::Value, 'Process')
                $recovery.Dispose()
            }
        }
    }

    Context 'a failed restore and a dead coordinator' {
        It 'keeps the marker and snapshots, refuses later mutations and cleanup, and recovers once quiet' {
            $project = New-TestProject
            $before = Get-OuterRecoveryTestState -Project $project
            $coordinator = Join-Path $project.Root 'coordinator.ps1'
            Set-Content -LiteralPath $coordinator -Value $script:FailedRestoreCoordinatorBody
            # A reader without delete sharing makes the outer restore fail.
            $handle = [System.IO.File]::Open($project.MapFile, 'Open', 'Read', 'ReadWrite')
            try {
                $child = Invoke-OuterRecoveryChild -Script $coordinator -Arguments @(
                    '-Library', $script:Library, '-Root', $project.Root, '-LayoutReceipt', $project.LayoutReceipt,
                    '-ScopeId', $project.ScopeId, '-EditFile', $project.ActorFile)
            }
            finally { $handle.Dispose() }
            $child.ExitCode | Should -Be 3

            $marker = Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root
            $marker.phase | Should -Be 'restore_failed'
            $records = Join-Path (Get-ProjectGeneratedContentOuterRoot -ProjectRoot $project.Root -OperationId $marker.operation_id) 'outer-snapshot\records.json'
            Get-ProjectWorldFileSha256 -Path $records | Should -Be $marker.world.snapshot_records_sha256
            # Nobody holds the lock, and still nothing may mutate.
            $lockPath = Get-ProjectGeneratedContentLockPath -ProjectRoot $project.Root
            ([System.IO.File]::Open($lockPath, 'Open', 'Read', 'None')).Dispose()
            { Enter-ProjectGeneratedContentMutationLock -ProjectRoot $project.Root } | Should -Throw '*outer recovery is pending*'
            $auditReceipt = Join-Path $project.Root 'audit_refused.json'
            $null = Invoke-OuterRecoveryChild -Script $script:Audit -Arguments @(
                '-ProjectRoot', $project.Root, '-WorldDataPlugin', 'ProjectWorldTestData', '-EvidencePath', $auditReceipt)
            (@((Get-Content -LiteralPath $auditReceipt -Raw | ConvertFrom-Json).failures) -join ';') |
                Should -BeLike '*outer recovery is pending*'

            # Cleanup refuses too and changes nothing it could otherwise delete.
            $keep = Join-Path $project.Root 'tmp\world\runtime_profile_locality\keep'
            New-Item -ItemType Directory -Path (Split-Path -Parent $keep) -Force | Out-Null
            Set-Content -LiteralPath $keep -Value 'keep' -NoNewline
            $markerState = Get-MarkerState -Project $project
            $lockBytes = [System.Convert]::ToBase64String([System.IO.File]::ReadAllBytes($lockPath))
            $cleanup = Invoke-OuterRecoveryChild -Script $script:Cleanup -Arguments @('-ProjectRoot', $project.Root, '-Apply')
            $cleanup.ExitCode | Should -Not -Be 0
            $cleanup.Output | Should -BeLike '*outer recovery is pending*'
            Test-Path -LiteralPath $keep | Should -BeTrue
            Get-MarkerState -Project $project | Should -Be $markerState
            [System.Convert]::ToBase64String([System.IO.File]::ReadAllBytes($lockPath)) | Should -Be $lockBytes
            Test-Path -LiteralPath (Join-Path $project.Root 'Saved\Validation\WorldCleanup') | Should -BeFalse

            # Once the blocker is gone, the recovery entry restores the state before the operation.
            Invoke-Entry -Root $project.Root | Should -Be 0
            Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeFalse
            Get-OuterRecoveryTestState -Project $project | Should -Be $before
        }
    }

    Context 'cleanup refusals' {
        It 'refuses cleanup while another operation holds the lock' {
            $project = New-TestProject
            $keep = Join-Path $project.Root 'tmp\world\runtime_profile_locality\keep'
            New-Item -ItemType Directory -Path (Split-Path -Parent $keep) -Force | Out-Null
            Set-Content -LiteralPath $keep -Value 'keep' -NoNewline
            $owner = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $project.Root -RequireOwnership
            try {
                $cleanup = Invoke-OuterRecoveryChild -Script $script:Cleanup -Arguments @('-ProjectRoot', $project.Root, '-Apply')
                $cleanup.ExitCode | Should -Not -Be 0
                $cleanup.Output | Should -BeLike '*Another operation holds*'

                $null = Enable-ProjectGeneratedContentLockDelegation -Lock $owner
                $cleanup = Invoke-OuterRecoveryChild -Script $script:Cleanup -Arguments @('-ProjectRoot', $project.Root, '-Apply')
                $cleanup.ExitCode | Should -Not -Be 0
                $cleanup.Output | Should -BeLike '*must run as the content-lock owner*'
            }
            finally { $owner.Dispose() }
            Test-Path -LiteralPath $keep | Should -BeTrue
            Test-Path -LiteralPath (Get-ProjectGeneratedContentLockPath -ProjectRoot $project.Root) | Should -BeTrue
        }
    }

    Context 'malformed markers' {
        BeforeEach {
            Mock Get-ProjectGeneratedContentOwnerIdentity {
                [ordered]@{
                    world = [ordered]@{ active_set_sha256 = ('1' * 64); manifest_root_sha256 = ('2' * 64); audit_status = 'rejected'; audit_verdict_sha256 = ('3' * 64) }
                    material = [ordered]@{ manifest_tree_sha256 = ('4' * 64); output_tree_sha256 = 'none'; validate_status = 'rejected'; validate_verdict_sha256 = ('5' * 64) }
                }
            }
        }

        It 'fails closed on a marker <Name> and changes nothing' -ForEach @(
            @{ Name = 'that is not JSON'; Corrupt = { param($p) Set-Content -LiteralPath $p.Marker -Value 'not json' } },
            @{ Name = 'with an unknown key'; Corrupt = { param($p) & $p.Update { param($m) $m | Add-Member -NotePropertyName extra -NotePropertyValue 1 } } },
            @{ Name = 'in an unknown phase'; Corrupt = { param($p) & $p.Update { param($m) $m.phase = 'o9_running' } } },
            @{ Name = 'naming a snapshot outside its confined root'; Corrupt = { param($p) & $p.Update { param($m) $m.world.snapshot_root = 'tmp/world/other/outer-snapshot' } } },
            @{ Name = 'naming its snapshot through a parent segment'; Corrupt = { param($p) & $p.Update { param($m) $m.world.snapshot_root = "tmp/world/world_realization/outer_recovery/$($m.operation_id)/../outer-snapshot" } } },
            @{ Name = 'whose records changed'; Corrupt = { param($p) Add-Content -LiteralPath $p.Records -Value ' ' -NoNewline } },
            @{ Name = 'whose records name a source outside the content root'; Corrupt = {
                    param($p)
                    $document = Get-Content -LiteralPath $p.Records -Raw | ConvertFrom-Json
                    $document.content_records[0].source = 'tmp/world/elsewhere'
                    Set-Content -LiteralPath $p.Records -Value ($document | ConvertTo-Json -Depth 8) -NoNewline
                    $hash = Get-ProjectWorldFileSha256 -Path $p.Records
                    & $p.Update { param($m) $m.world.snapshot_records_sha256 = $hash }.GetNewClosure()
                } },
            @{ Name = 'that is only a malformed staging file'; Corrupt = { param($p) Remove-Item -LiteralPath $p.Marker; Set-Content -LiteralPath "$($p.Marker).tmp" -Value 'garbage' } }
        ) {
            $project = New-TestProject
            $operation = Start-OuterRecoveryTestOperation -Project $project
            Stop-OuterRecoveryTestOperation -Operation $operation
            $markerPath = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $project.Root
            $paths = [pscustomobject]@{
                Marker = $markerPath
                Records = Join-Path (Get-ProjectGeneratedContentOuterRoot -ProjectRoot $project.Root -OperationId $operation.Marker.operation_id) 'outer-snapshot\records.json'
                Update = {
                    param([scriptblock]$Change)
                    $document = Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json
                    & $Change $document
                    Set-Content -LiteralPath $markerPath -Value ($document | ConvertTo-Json -Depth 8) -NoNewline
                }.GetNewClosure()
            }
            & $Corrupt $paths
            Set-Content -LiteralPath $project.ActorFile -Value 'changed-by-operation' -NoNewline
            $content = Get-OuterRecoveryTestState -Project $project
            $markerState = Get-MarkerState -Project $project

            { Enter-ProjectGeneratedContentMutationLock -ProjectRoot $project.Root } | Should -Throw '*outer recovery is pending*'
            Invoke-Entry -Root $project.Root | Should -Be 6
            Get-MarkerState -Project $project | Should -Be $markerState
            Get-OuterRecoveryTestState -Project $project | Should -Be $content
        }
    }

    Context 'an orphaned delegated child' {
        It 'refuses recovery while the child runs and restores both owners after it exits' {
            $project = New-TestProject
            $before = Get-OuterRecoveryTestState -Project $project
            $activeSet = Join-Path $project.ManifestRoot 'active_set.json'
            $activeBefore = Get-ProjectWorldFileSha256 -Path $activeSet
            $children = Join-Path $project.Root 'children'
            New-Item -ItemType Directory -Path $children -Force | Out-Null
            $coordinator = Join-Path $children 'coordinator.ps1'
            $delegate = Join-Path $children 'delegate.ps1'
            Set-Content -LiteralPath $coordinator -Value $script:OrphaningCoordinatorBody
            Set-Content -LiteralPath $delegate -Value $script:DelegateBody
            $ready = Join-Path $children 'ready'
            $release = Join-Path $children 'release'
            $materialOperation = [System.Guid]::NewGuid().ToString('N')

            $parent = Start-OuterRecoveryChild -Script $coordinator -Arguments @(
                '-Library', $script:Library, '-LockFile', $script:LockFile, '-Root', $project.Root,
                '-LayoutReceipt', $project.LayoutReceipt, '-ScopeId', $project.ScopeId,
                '-MaterialOperation', $materialOperation, '-DelegateScript', $delegate,
                '-Ready', $ready, '-Release', $release)
            $parent.WaitForExit(240000) | Should -BeTrue
            $parent.ExitCode | Should -Be 9
            Wait-OuterRecoveryFile -Path $ready
            $script:Orphan = Get-Process -Id ([int](Get-Content -LiteralPath $ready -Raw))
            $script:Orphan.HasExited | Should -BeFalse
            $operationId = (Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root).operation_id
            $journal = Join-Path $project.ManifestRoot 'journal.json'
            Test-Path -LiteralPath $journal | Should -BeTrue
            $interrupted = Get-OuterRecoveryTestState -Project $project

            { Enter-ProjectGeneratedContentRecoveryLock -ProjectRoot $project.Root -ProjectFile (Join-Path $project.Root 'Alis.uproject') } |
                Should -Throw '*participant still holds the content lock*'
            $refused = Invoke-OuterRecoveryChild -Script $script:Entry -Arguments @('-ProjectRoot', $project.Root)
            $refused.ExitCode | Should -Be 5
            Get-OuterRecoveryTestState -Project $project | Should -Be $interrupted
            Test-Path -LiteralPath $journal | Should -BeTrue

            Set-Content -LiteralPath $release -Value 'release' -NoNewline
            $script:Orphan.WaitForExit(60000) | Should -BeTrue
            Invoke-Entry -Root $project.Root | Should -Be 0

            Test-Path -LiteralPath $journal | Should -BeFalse
            @(Get-ChildItem -LiteralPath (Join-Path $project.Root 'tmp\world\world_realization\transactions') -Force -ErrorAction SilentlyContinue).Count |
                Should -Be 0
            Get-ProjectWorldFileSha256 -Path $activeSet | Should -Be $activeBefore
            Get-OuterRecoveryTestState -Project $project | Should -Be $before
            @(Get-Content -LiteralPath $project.MaterialCalls | Where-Object { $_ -like 'RestorePrevious|*' }) |
                Should -Be @("RestorePrevious|$materialOperation|delegated=True")

            # After recovery the marker and its snapshots are gone, and a mutation and cleanup proceed.
            Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeFalse
            Test-Path -LiteralPath (Get-ProjectGeneratedContentOuterRoot -ProjectRoot $project.Root -OperationId $operationId) |
                Should -BeFalse
            (Enter-ProjectGeneratedContentMutationLock -ProjectRoot $project.Root).Dispose()
            # The cleanup's machine-wide Unreal and build process check is outside this case.
            Mock Get-Process { @() } -ParameterFilter { $null -ne $Name -and $Name -contains 'UnrealEditor' }
            & $script:Cleanup -ProjectRoot $project.Root -Apply | Out-Null
            @(Get-ChildItem -LiteralPath (Join-Path $project.Root 'Saved\Validation\WorldCleanup') -Filter '*.json').Count |
                Should -Be 1
        }
    }

    Context 'coordinator protocol' {
        BeforeEach {
            Mock Get-ProjectGeneratedContentOwnerIdentity {
                [ordered]@{
                    world = [ordered]@{ active_set_sha256 = ('1' * 64); manifest_root_sha256 = ('2' * 64); audit_status = 'rejected'; audit_verdict_sha256 = ('3' * 64) }
                    material = [ordered]@{ manifest_tree_sha256 = ('4' * 64); output_tree_sha256 = 'none'; validate_status = 'rejected'; validate_verdict_sha256 = ('5' * 64) }
                }
            }
        }

        It 'writes the marker after the World snapshot and removes it before the one release' {
            $project = New-TestProject
            $script:Log = [System.Collections.Generic.List[string]]::new()
            Mock Write-ProjectGeneratedContentOuterLog { $script:Log.Add(($Message -split ' ')[0]) }
            $operation = Start-OuterRecoveryTestOperation -Project $project
            try {
                $marker = $operation.Marker
                $records = Join-Path (Get-ProjectGeneratedContentOuterRoot -ProjectRoot $project.Root -OperationId $marker.operation_id) 'outer-snapshot\records.json'
                Get-ProjectWorldFileSha256 -Path $records | Should -Be $marker.world.snapshot_records_sha256
                $marker.phase | Should -Be 'snapshot_taken'
                $marker.material.state | Should -Be 'not_started'
                Complete-ProjectGeneratedContentOuterOperation -Lock $operation.Lock -ProjectRoot $project.Root `
                    -Marker $marker -Result 'succeeded'
                Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeFalse
                # The lock is still held: the one release comes after the marker removal.
                { [System.IO.File]::Open((Get-ProjectGeneratedContentLockPath -ProjectRoot $project.Root), 'Open', 'Read', 'None') } |
                    Should -Throw
                $evidence = Join-Path (Get-ProjectGeneratedContentOuterEvidenceRoot -ProjectRoot $project.Root -OperationId $marker.operation_id) 'outer_operation.json'
                (Get-Content -LiteralPath $evidence -Raw | ConvertFrom-Json).result | Should -Be 'succeeded'
            }
            finally { Stop-OuterRecoveryTestOperation -Operation $operation }
            @($script:Log) | Should -Be @('snapshot_taken', 'marker_written', 'marker_removed')
        }

        It 'treats a committed marker as authoritative over stale staging and resolves a staging-only marker' {
            $project = New-TestProject
            $markerPath = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $project.Root
            Stop-OuterRecoveryTestOperation -Operation (Start-OuterRecoveryTestOperation -Project $project)
            Set-Content -LiteralPath "$markerPath.tmp" -Value 'stale staging' -NoNewline
            { Enter-ProjectGeneratedContentMutationLock -ProjectRoot $project.Root } | Should -Throw '*outer recovery is pending*'
            Invoke-Entry -Root $project.Root | Should -Be 0
            Test-Path -LiteralPath $markerPath | Should -BeFalse
            Test-Path -LiteralPath "$markerPath.tmp" | Should -BeFalse

            # An interrupted create leaves only its complete staging file.
            Stop-OuterRecoveryTestOperation -Operation (Start-OuterRecoveryTestOperation -Project $project)
            [System.IO.File]::Move($markerPath, "$markerPath.tmp")
            Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeTrue
            Invoke-Entry -Root $project.Root | Should -Be 0
            Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeFalse
        }

        It 'promotes the staging copy a lost replacement left, so a later failed write still leaves it pending' {
            $project = New-TestProject
            $operation = Start-OuterRecoveryTestOperation -Project $project
            try {
                $markerPath = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $project.Root
                $materialOperation = [System.Guid]::NewGuid().ToString('N')
                # ReplaceFileW can remove the target and then fail, leaving only the replacement.
                $lost = Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json
                $lost.phase = 'o3_running'
                $lost.material.operation_id = $materialOperation
                $lost.material.state = 'running'
                Set-Content -LiteralPath "$markerPath.tmp" -Value ($lost | ConvertTo-Json -Depth 8) -NoNewline
                [System.IO.File]::Delete($markerPath)
                Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeTrue
                (Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root).material.operation_id |
                    Should -Be $materialOperation

                # The next write fails after the step that handles the leftover copy.
                Mock New-Object { throw 'injected staging write failure' } -ParameterFilter {
                    $TypeName -eq 'System.IO.FileStream'
                }
                { Set-ProjectGeneratedContentOuterPhase -Lock $operation.Lock -ProjectRoot $project.Root `
                    -Phase 'o3_completed' -MaterialRetained } | Should -Throw '*terminal*'
                Test-Path -LiteralPath $markerPath | Should -BeTrue
                $marker = Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root
                $marker.phase | Should -Be 'o3_running'
                $marker.material.operation_id | Should -Be $materialOperation
            }
            finally { Stop-OuterRecoveryTestOperation -Operation $operation }
        }

        It 'completes an update after a lost replacement without losing the Material binding' {
            $project = New-TestProject
            $operation = Start-OuterRecoveryTestOperation -Project $project
            try {
                $markerPath = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $project.Root
                $materialOperation = [System.Guid]::NewGuid().ToString('N')
                $lost = Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json
                $lost.phase = 'o3_running'
                $lost.material.operation_id = $materialOperation
                $lost.material.state = 'running'
                Set-Content -LiteralPath "$markerPath.tmp" -Value ($lost | ConvertTo-Json -Depth 8) -NoNewline
                [System.IO.File]::Delete($markerPath)

                Set-ProjectGeneratedContentOuterPhase -Lock $operation.Lock -ProjectRoot $project.Root `
                    -Phase 'o3_completed' -MaterialRetained
                Test-Path -LiteralPath "$markerPath.tmp" | Should -BeFalse
                $marker = Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root
                $marker.phase | Should -Be 'o3_completed'
                $marker.material.operation_id | Should -Be $materialOperation
                $marker.material.state | Should -Be 'retained'
            }
            finally { Stop-OuterRecoveryTestOperation -Operation $operation }
        }

        It 'deletes stale staging only while the committed marker exists' {
            $project = New-TestProject
            $operation = Start-OuterRecoveryTestOperation -Project $project
            try {
                $markerPath = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $project.Root
                Set-Content -LiteralPath "$markerPath.tmp" -Value 'interrupted update' -NoNewline
                Set-ProjectGeneratedContentOuterPhase -Lock $operation.Lock -ProjectRoot $project.Root -Phase 'o4_running'
                Test-Path -LiteralPath "$markerPath.tmp" | Should -BeFalse
                (Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root).phase | Should -Be 'o4_running'
            }
            finally { Stop-OuterRecoveryTestOperation -Operation $operation }
        }

        It 'treats a failed marker write as terminal and keeps a pending copy' {
            $project = New-TestProject
            $operation = Start-OuterRecoveryTestOperation -Project $project
            try {
                $markerPath = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $project.Root
                # Without delete sharing the replacement cannot complete.
                $handle = [System.IO.File]::Open($markerPath, 'Open', 'Read', 'ReadWrite')
                try {
                    { Set-ProjectGeneratedContentOuterPhase -Lock $operation.Lock -ProjectRoot $project.Root -Phase 'o2_running' } |
                        Should -Throw '*terminal*'
                    Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeTrue
                    (Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root).phase | Should -Be 'snapshot_taken'
                }
                finally { $handle.Dispose() }
                Set-ProjectGeneratedContentOuterPhase -Lock $operation.Lock -ProjectRoot $project.Root -Phase 'o2_running'
                (Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root).phase | Should -Be 'o2_running'
            }
            finally { Stop-OuterRecoveryTestOperation -Operation $operation }
        }

        It 'refuses marker writes from anything but the live owner' {
            $project = New-TestProject
            $operation = Start-OuterRecoveryTestOperation -Project $project
            try {
                $delegate = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $project.Root
                try {
                    { Set-ProjectGeneratedContentOuterPhase -Lock $delegate -ProjectRoot $project.Root -Phase 'o2_running' } |
                        Should -Throw '*written only by the live owner*'
                }
                finally { $delegate.Dispose() }
                (Read-ProjectGeneratedContentOuterMarker -ProjectRoot $project.Root).phase | Should -Be 'snapshot_taken'
            }
            finally { Stop-OuterRecoveryTestOperation -Operation $operation }
        }
    }

    Context 'identity verification' {
        It 'keeps the marker while a restored owner differs from its recorded identity, and a rerun resolves it' {
            $project = New-TestProject
            $operation = Start-OuterRecoveryTestOperation -Project $project
            $materialOperation = [System.Guid]::NewGuid().ToString('N')
            Set-ProjectGeneratedContentOuterPhase -Lock $operation.Lock -ProjectRoot $project.Root `
                -Phase 'o3_running' -MaterialOperationId $materialOperation
            # The Material operation changed its state but retained nothing to restore.
            Set-Content -LiteralPath $project.MaterialState -Value 'material-after-operation' -NoNewline
            Stop-OuterRecoveryTestOperation -Operation $operation
            $markerState = Get-MarkerState -Project $project

            Invoke-Entry -Root $project.Root | Should -Be 7
            Get-MarkerState -Project $project | Should -Be $markerState

            Set-Content -LiteralPath (Join-Path $project.Root "material_bundle_$materialOperation.txt") -Value 'material-before' -NoNewline
            Invoke-Entry -Root $project.Root | Should -Be 0
            Get-Content -LiteralPath $project.MaterialState -Raw | Should -Be 'material-before'
            Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $project.Root | Should -BeFalse
        }

        It 'reports nothing to resolve when no outer operation is pending' {
            $project = New-TestProject
            $child = Invoke-OuterRecoveryChild -Script $script:Entry -Arguments @('-ProjectRoot', $project.Root)
            $child.ExitCode | Should -Be 0
            $child.Output | Should -BeLike '*state=no_outer_operation*'
        }
    }
}
