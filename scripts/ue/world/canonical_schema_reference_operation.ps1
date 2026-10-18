# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '../generated_content/generated_content_outer_recovery.ps1')
. (Join-Path $PSScriptRoot 'canonical_refinement_operation.ps1')
. (Join-Path $PSScriptRoot 'canonical_selection_transition.ps1')

function Get-ProjectWorldSchemaRepairInventory {
    param([string]$ProjectRoot, [object]$Roots)
    $result = [ordered]@{}
    foreach ($file in Get-ChildItem -LiteralPath $Roots.PluginRoot -Recurse -File | Sort-Object FullName) {
        if ($file.Name -ceq 'authority.lock') { continue }
        $relative = ConvertTo-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -Path $file.FullName
        $result[$relative] = Get-ProjectWorldFileSha256 $file.FullName
    }
    return $result
}

function Assert-ProjectWorldSchemaRepairLink {
    param([object]$Prior, [object]$Current, [object]$Ticket, [string]$OperationId)
    $expected = $Prior.Record | ConvertTo-Json -Depth 32 | ConvertFrom-Json
    $matches = @($expected.canonical_input_compatibility | Where-Object {
        [string]$_.candidate.active_sha256 -ceq [string]$Ticket.old_active_sha256
    })
    if ($matches.Count -ne 1) { throw 'Schema repair requires one exact accepted Canonical compatibility.' }
    $matches[0].candidate.active_sha256 = $Ticket.new_active_sha256
    $expected.transaction_id = $OperationId
    $expected.accepted_operation_id = $OperationId
    $expected.prior_active_set_sha256 = $Prior.Sha256
    $a = ConvertTo-Json -InputObject (ConvertTo-ProjectWorldSortedValue $expected) -Depth 32 -Compress
    $b = ConvertTo-Json -InputObject (ConvertTo-ProjectWorldSortedValue $Current.Record) -Depth 32 -Compress
    if ($a -cne $b) { throw 'Schema repair changed World beyond its exact pointer and transaction metadata.' }
}

function Invoke-ProjectWorldSchemaReferenceRepair {
    param([string]$ProjectRoot, [string]$CompilerProfile, [string]$ExpectedCanonicalSha256,
        [string]$ExpectedWorldSha256, [string]$EvidenceRoot, [string]$RetainedRoot,
        [switch]$RestoreAfterProof, [scriptblock]$BeforeWorldCommit = $null)
    $root = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\','/')
    $evidence = [IO.Path]::GetFullPath($EvidenceRoot)
    $retained = [IO.Path]::GetFullPath($RetainedRoot)
    if (-not $evidence.StartsWith($root+'\tmp\',[StringComparison]::OrdinalIgnoreCase) -or
        -not $retained.StartsWith($root+'\Saved\Validation\WorldAuthority\',[StringComparison]::OrdinalIgnoreCase) -or
        (Test-Path -LiteralPath $evidence) -or (Test-Path -LiteralPath $retained)) {
        throw 'Schema repair requires new confined scratch and retained evidence directories.'
    }
    Assert-ProjectGeneratedContentNoProjectUnrealProcess -ProjectFile (Join-Path $root 'Alis.uproject') -Purpose 'Canonical schema reference repair'
    New-Item -ItemType Directory -Path $evidence | Out-Null
    $lock = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root -RequireOwnership -OwnerName 'Canonical schema reference repair'
    $delegation = $null
    $marker = $null
    try {
        $delegation = Enable-ProjectGeneratedContentLockDelegation -Lock $lock
        if (Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $root) { throw 'Resolve pending outer recovery first.' }
        $ticket = Invoke-ProjectWorldRefinementOwner -ProjectRoot $root -Owner CanonicalCompilation -Arguments @(
            'prepare-schema-reference-transition','--profile',$CompilerProfile,'--expected-active-sha256',$ExpectedCanonicalSha256
        ) -EvidenceDirectory (Join-Path $evidence 'preparation')
        if ([string]$ticket.transition_kind -cne 'schema_reference_v1') { throw 'Unexpected Canonical repair operation.' }
        $authority = Get-Content -LiteralPath (Join-Path $root $ticket.active_path) -Raw | ConvertFrom-Json
        $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $root -PluginName $authority.world_data_plugin
        $authorityLock = Enter-ProjectWorldAuthorityLock -ManifestRoot $roots.ManifestRoot
        try {
            $prior = Read-ProjectWorldActiveSet -ManifestRoot $roots.ManifestRoot -ProjectRoot $root
            if ($null -eq $prior -or $prior.Sha256 -cne $ExpectedWorldSha256) { throw 'World changed before schema repair.' }
            $matches = @($prior.Record.canonical_input_compatibility | Where-Object {
                [string]$_.candidate.active_sha256 -ceq [string]$ticket.old_active_sha256
            })
            if ($matches.Count -ne 1) { throw 'Schema repair does not match one accepted World selection.' }
            $replacement = $matches[0] | ConvertTo-Json -Depth 32 | ConvertFrom-Json
            $replacement.candidate.active_sha256 = $ticket.new_active_sha256
            $before = Get-ProjectWorldSchemaRepairInventory -ProjectRoot $root -Roots $roots
        }
        finally { $authorityLock.Dispose() }
        $marker = New-ProjectGeneratedContentOuterOperation -Lock $lock -ProjectRoot $root `
            -WorldDataPlugin $roots.PluginName -Label 'canonical_schema_reference' -CanonicalTransition $ticket
        try {
            $authorityLock = Enter-ProjectWorldAuthorityLock -ManifestRoot $roots.ManifestRoot
            try {
                $current = Read-ProjectWorldActiveSet -ManifestRoot $roots.ManifestRoot -ProjectRoot $root
                if ($current.Sha256 -cne $prior.Sha256) { throw 'World changed after schema repair snapshot.' }
                $null = Invoke-ProjectWorldCanonicalSelection -ProjectRoot $root -Transition $ticket -Action publish
                $published = Publish-ProjectWorldActiveSet -ManifestRoot $roots.ManifestRoot -ProjectRoot $root `
                    -TransactionId $marker.operation_id -OperationId $marker.operation_id -CandidateManifests @() `
                    -PriorActiveSet $prior -CanonicalInputCompatibility $replacement -BeforeCommit $BeforeWorldCommit
                $saved = Read-ProjectWorldActiveSet -ManifestRoot $roots.ManifestRoot -ProjectRoot $root
                Assert-ProjectWorldSchemaRepairLink -Prior $prior -Current $saved -Ticket $ticket -OperationId $marker.operation_id
            }
            finally { $authorityLock.Dispose() }
            $audit = Get-ProjectGeneratedContentWorldIdentity -ProjectRoot $root -WorldDataPlugin $roots.PluginName `
                -EvidenceRoot (Join-Path $evidence 'saved-audit')
            if ($audit.audit_status -cne 'accepted') { throw 'Repaired authority audit failed.' }
            $authorityLock = Enter-ProjectWorldAuthorityLock -ManifestRoot $roots.ManifestRoot
            try {
                $cold = Read-ProjectWorldActiveSet -ManifestRoot $roots.ManifestRoot -ProjectRoot $root
                Assert-ProjectWorldSchemaRepairLink -Prior $prior -Current $cold -Ticket $ticket -OperationId $marker.operation_id
                $repeat = Invoke-ProjectWorldCanonicalSelection -ProjectRoot $root -Transition $ticket -Action publish
                $noop = Publish-ProjectWorldActiveSet -ManifestRoot $roots.ManifestRoot -ProjectRoot $root `
                    -TransactionId $marker.operation_id -OperationId $marker.operation_id -CandidateManifests @() `
                    -PriorActiveSet $cold -CanonicalInputCompatibility $replacement
                if ($repeat.changed -or $noop.Sha256 -cne $saved.Sha256) { throw 'Schema repair replay was not a no-op.' }
                $after = Get-ProjectWorldSchemaRepairInventory -ProjectRoot $root -Roots $roots
            }
            finally { $authorityLock.Dispose() }
            $changed = @(@($before.Keys)+@($after.Keys) | Sort-Object -Unique | Where-Object { $before[$_] -cne $after[$_] })
            $worldRelative = ConvertTo-ProjectWorldOuterRelativePath -ProjectRoot $root -Path $saved.Path
            if (($changed -join ',') -cne ((@($worldRelative,$ticket.active_path) | Sort-Object) -join ',')) {
                throw 'Schema repair changed more than the two existing authority selectors.'
            }
            New-Item -ItemType Directory -Path $retained | Out-Null
            $pins = [ordered]@{}
            $snapshot = Join-Path $root $marker.world.snapshot_root
            $payloads = [ordered]@{
                'prior-world.json' = [IO.File]::ReadAllBytes((Join-Path $snapshot 'manifest_root/active_set.json'))
                'current-world.json' = [IO.File]::ReadAllBytes($saved.Path)
                'prior-canonical.json' = [Convert]::FromBase64String($ticket.old_active_bytes_base64)
                'current-canonical.json' = [Convert]::FromBase64String($ticket.new_active_bytes_base64)
            }
            foreach ($name in $payloads.Keys) {
                $path = Join-Path $retained $name
                [IO.File]::WriteAllBytes($path,$payloads[$name])
                $pins[$name] = Get-ProjectWorldFileSha256 $path
            }
            Write-ProjectWorldJson -Path (Join-Path $retained 'transition.json') -Document $ticket
            $pins['transition.json'] = Get-ProjectWorldFileSha256 (Join-Path $retained 'transition.json')
            Copy-Item -LiteralPath (Join-Path $evidence 'saved-audit/world_audit.json') -Destination (Join-Path $retained 'world-audit.json')
            $pins['world-audit.json'] = Get-ProjectWorldFileSha256 (Join-Path $retained 'world-audit.json')
            $sourceFiles = [ordered]@{}
            $sources = @(Get-ChildItem -LiteralPath (Join-Path $root 'tools/World/CanonicalCompilation') -Recurse -File |
                Where-Object { $_.Extension -cin @('.py','.json') })
            foreach ($relative in @('scripts/ue/world/canonical_schema_reference_operation.ps1',
                    'scripts/ue/world/repair_canonical_schema_reference.ps1','scripts/ue/world/test/canonical_schema_reference.Tests.ps1')) {
                $sources += Get-Item -LiteralPath (Join-Path $root $relative)
            }
            foreach ($file in $sources | Sort-Object FullName) {
                $relative = ConvertTo-ProjectWorldOuterRelativePath -ProjectRoot $root -Path $file.FullName
                $sourceFiles[$relative] = Get-ProjectWorldFileSha256 $file.FullName
            }
            $proof = [ordered]@{schema_version=1;status='prepared';operation_id=$marker.operation_id;
                kind='canonical_schema_reference_v1';world_path=$worldRelative;canonical_path=$ticket.active_path;
                changed_paths=$changed;payloads=$pins;current_world_sha256=$saved.Sha256;
                prior_world_sha256=$prior.Sha256;current_canonical_sha256=$ticket.new_active_sha256;
                prior_canonical_sha256=$ticket.old_active_sha256;protected_files=$before.Count;
                cold_noop=$true;world_audit_status=$audit.audit_status;restored_fixture=[bool]$RestoreAfterProof;
                source_files=$sourceFiles}
            Write-ProjectWorldJson -Path (Join-Path $retained 'repair.json') -Document $proof
            if ($RestoreAfterProof) {
                $null = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $root -Marker $marker -EvidenceRoot (Join-Path $evidence 'restoration')
                $restored = Get-ProjectWorldSchemaRepairInventory -ProjectRoot $root -Roots $roots
                if ((ConvertTo-Json $restored -Compress) -cne (ConvertTo-Json $before -Compress)) { throw 'Repair fixture did not restore exact bytes.' }
            }
        }
        catch {
            $failure = $_
            $null = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $root -Marker $marker -EvidenceRoot (Join-Path $evidence 'failed-restoration')
            Complete-ProjectGeneratedContentOuterOperation -Lock $lock -ProjectRoot $root -Marker $marker -Result restored
            throw $failure
        }
        $result = if ($RestoreAfterProof) { 'restored' } else { 'succeeded' }
        Complete-ProjectGeneratedContentOuterOperation -Lock $lock -ProjectRoot $root -Marker $marker -Result $result
        $proof.status = 'accepted'
        Write-ProjectWorldJson -Path (Join-Path $retained 'repair.json') -Document $proof
        return $proof
    }
    finally {
        Disable-ProjectGeneratedContentLockDelegation -Prior $delegation
        $lock.Dispose()
    }
}
