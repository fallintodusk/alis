# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
# Canonical selection participates in the existing generated-content outer marker.

function New-ProjectGeneratedContentCanonicalParticipant {
    param([string]$ProjectRoot, [object]$Marker, [object]$Transition)
    $snapshotRoot = Join-Path $ProjectRoot $Marker.world.snapshot_root
    if ([string]$Marker.world.pre.audit_status -cne 'accepted' -or
        (Get-ProjectWorldManifestRootSha256 (Join-Path $snapshotRoot 'manifest_root')) -cne [string]$Marker.world.pre.manifest_root_sha256) {
        throw 'Canonical outer snapshot does not match accepted pre-operation World authority.'
    }
    $state = Invoke-ProjectWorldCanonicalSelection -ProjectRoot $ProjectRoot -Transition $Transition -Action validate
    if ([string]$state.state -cne 'prior') { throw 'Canonical outer operation requires the exact prior selection.' }
    $inventory = Get-ProjectWorldOuterSnapshotInventorySha256 -ProjectRoot $ProjectRoot `
        -WorldDataPlugin $Marker.world.world_data_plugin -SnapshotRoot $snapshotRoot -RecordsSha256 $Marker.world.snapshot_records_sha256
    $current = Get-ProjectWorldOuterSnapshotInventorySha256 -ProjectRoot $ProjectRoot `
        -WorldDataPlugin $Marker.world.world_data_plugin -SnapshotRoot $snapshotRoot -RecordsSha256 $Marker.world.snapshot_records_sha256 -Current
    if ($inventory -cne $current) { throw 'Initial outer snapshot inventory does not match current source bytes.' }
    return [pscustomobject]@{
        transition = $Transition
        snapshot_inventory_sha256 = $inventory
    }
}

function Assert-ProjectGeneratedContentCanonicalParticipant {
    param([object]$Participant)
    Assert-ProjectGeneratedContentOuterKeys -Value $Participant -Label 'canonical_selection' `
        -Keys @('transition', 'snapshot_inventory_sha256')
    if ($Participant.transition -isnot [System.Management.Automation.PSCustomObject] -or
        -not (Test-ProjectGeneratedContentOuterHash $Participant.snapshot_inventory_sha256)) {
        throw 'Outer canonical selection participant is malformed.'
    }
}

function Test-ProjectGeneratedContentCanonicalParticipant {
    param([object]$Marker)
    return $null -ne $Marker.PSObject.Properties['canonical_selection']
}

function Assert-ProjectGeneratedContentCanonicalRestore {
    # Authenticate ALL backups before inner recovery or reverse canonical CAS can mutate state.
    param([string]$ProjectRoot, [object]$Marker, [switch]$Current)
    if (-not (Test-ProjectGeneratedContentCanonicalParticipant $Marker)) { return }
    Assert-ProjectGeneratedContentCanonicalParticipant $Marker.canonical_selection
    $actual = Get-ProjectWorldOuterSnapshotInventorySha256 -ProjectRoot $ProjectRoot `
        -WorldDataPlugin $Marker.world.world_data_plugin -SnapshotRoot (Join-Path $ProjectRoot $Marker.world.snapshot_root) `
        -RecordsSha256 $Marker.world.snapshot_records_sha256 -Current:$Current
    if ($actual -cne [string]$Marker.canonical_selection.snapshot_inventory_sha256) {
        throw 'Outer snapshot inventory differs from its pre-operation bytes; recovery evidence retained.'
    }
    $state = Invoke-ProjectWorldCanonicalSelection -ProjectRoot $ProjectRoot -Transition $Marker.canonical_selection.transition -Action validate
    if ($Current -and [string]$state.state -cne 'prior') { throw 'Outer restore did not recover the prior canonical selection.' }
}

function Restore-ProjectGeneratedContentCanonicalParticipant {
    param([string]$ProjectRoot, [object]$Marker)
    if (-not (Test-ProjectGeneratedContentCanonicalParticipant $Marker)) { return }
    $state = Invoke-ProjectWorldCanonicalSelection -ProjectRoot $ProjectRoot -Transition $Marker.canonical_selection.transition -Action restore
    if ([string]$state.state -cne 'prior') { throw 'Canonical reverse CAS did not restore the prior selection.' }
}

function Enter-ProjectGeneratedContentCanonicalChild {
    # A private child cannot become an independent durable writer outside its live outer owner.
    param([string]$ProjectRoot, [string]$OperationId, [string]$CandidateSha256, [string]$EffectiveResultSha256, [string]$WorldDataPlugin)
    $variable = Get-ProjectGeneratedContentLockTokenVariable
    if ([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($variable))) {
        throw 'Canonical target child requires live outer lock delegation.'
    }
    $handle = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $ProjectRoot
    try {
        $marker = Read-ProjectGeneratedContentOuterMarker -ProjectRoot $ProjectRoot
        if ($handle.CanWrite -or $null -eq $marker -or [string]$marker.operation_id -cne $OperationId -or
            [string]$marker.world.world_data_plugin -cne $WorldDataPlugin -or
            -not (Test-ProjectGeneratedContentCanonicalParticipant $marker) -or
            [string]$marker.canonical_selection.transition.new_active_sha256 -cne $CandidateSha256 -or
            [string]$marker.canonical_selection.transition.effective_result_sha256 -cne $EffectiveResultSha256) {
            throw 'Canonical target child has no matching live outer operation.'
        }
        return $handle
    }
    catch { $handle.Dispose(); throw }
}
