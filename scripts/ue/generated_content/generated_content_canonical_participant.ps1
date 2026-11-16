# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
# Canonical selection participates in the existing generated-content outer marker.

function New-ProjectGeneratedContentCanonicalParticipant {
    param([string]$ProjectRoot, [object]$Marker, [object]$Transition, [switch]$PreparingBase,
        [AllowEmptyCollection()][object[]]$BaseInputs = @())
    $snapshotRoot = Join-Path $ProjectRoot $Marker.world.snapshot_root
    if ([string]$Marker.world.pre.audit_status -cne 'accepted') {
        if (-not $PreparingBase) { throw 'Canonical selection requires accepted pre-operation World authority.' }
        $evidence = Get-ProjectGeneratedContentOuterEvidenceRoot -ProjectRoot $ProjectRoot -OperationId $Marker.operation_id
        $audit = Get-Content -LiteralPath (Join-Path $evidence 'pre/world_audit.json') -Raw | ConvertFrom-Json
        $failed = @($audit.checks | Where-Object { -not $_.passed })
        $failures = @($audit.failures | ForEach-Object { [string]$_ })
        if ([string]$audit.status -cne 'rejected' -or $failed.Count -ne 1 -or
            [string]$failed[0].name -cne 'generator_fingerprint_current' -or $failures.Count -eq 0 -or
            @($failures | Where-Object { -not $_.StartsWith('generator_fingerprint_current:') }).Count -ne 0 -or
            (Get-ProjectGeneratedContentTextSha256 ($failures -join "`n")) -cne [string]$Marker.world.pre.audit_verdict_sha256) {
            throw 'Base preparation may reconcile only measured producer fingerprint drift.'
        }
    }
    if ((Get-ProjectWorldManifestRootSha256 (Join-Path $snapshotRoot 'manifest_root')) -cne [string]$Marker.world.pre.manifest_root_sha256) {
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
        base_inputs = @($BaseInputs)
        prepared_world = $(if ($PreparingBase) { $null } else { [pscustomobject]@{
            snapshot_root = [string]$Marker.world.snapshot_root
            records_sha256 = [string]$Marker.world.snapshot_records_sha256
            inventory_sha256 = $inventory
        } })
    }
}

function Assert-ProjectGeneratedContentCanonicalParticipant {
    param([object]$Participant)
    Assert-ProjectGeneratedContentOuterKeys -Value $Participant -Label 'canonical_selection' `
        -Keys @('transition', 'snapshot_inventory_sha256', 'prepared_world', 'base_inputs')
    if ($Participant.transition -isnot [System.Management.Automation.PSCustomObject] -or
        -not (Test-ProjectGeneratedContentOuterHash $Participant.snapshot_inventory_sha256)) {
        throw 'Outer canonical selection participant is malformed.'
    }
    if ($null -ne $Participant.prepared_world) {
        Assert-ProjectGeneratedContentOuterKeys -Value $Participant.prepared_world -Label 'prepared_world' `
            -Keys @('snapshot_root', 'records_sha256', 'inventory_sha256')
        foreach ($value in @($Participant.prepared_world.records_sha256, $Participant.prepared_world.inventory_sha256)) {
            if (-not (Test-ProjectGeneratedContentOuterHash $value)) { throw 'Prepared World snapshot is malformed.' }
        }
    }
    $seen=@{}
    foreach($row in @($Participant.base_inputs)) {
        Assert-ProjectGeneratedContentOuterKeys -Value $row -Label 'base input' -Keys @('input_identity','realization_profile_sha256')
        Assert-ProjectGeneratedContentOuterKeys -Value $row.input_identity -Label 'base map inputs' `
            -Keys @('compile_result_sha256','presentation_profile_sha256','runtime_profile_sha256','authored_overlay_profile_sha256','map_package')
        foreach($field in @('compile_result_sha256','presentation_profile_sha256','authored_overlay_profile_sha256')) {
            if(-not (Test-ProjectGeneratedContentOuterHash $row.input_identity.$field)){throw 'Invalid restoring base input pin.'}
        }
        if(-not (Test-ProjectGeneratedContentOuterHash $row.realization_profile_sha256) -or
            [string]$row.input_identity.runtime_profile_sha256 -cnotmatch '^([a-f0-9]{64}|none)$' -or
            [string]$row.input_identity.map_package -cnotmatch '^/[A-Za-z][A-Za-z0-9_]*/Generated/[A-Za-z0-9_/]+$' -or
            $seen.ContainsKey([string]$row.input_identity.map_package)){throw 'Invalid or duplicate restoring base map.'}
        $seen[[string]$row.input_identity.map_package]=$true
    }
}

function Assert-ProjectGeneratedContentCanonicalBaseEnrollment {
    param([string]$ProjectRoot,[string]$OperationId,[string]$WorldDataPlugin,
        [System.Collections.IDictionary]$InputIdentity,[string]$RealizationProfileHash)
    $marker=Read-ProjectGeneratedContentOuterMarker -ProjectRoot $ProjectRoot
    if($null -eq $marker -or -not (Test-ProjectGeneratedContentCanonicalParticipant $marker)) {
        throw 'Temporary enrollment requires a matching canonical preparation owner.'
    }
    $participant=$marker.canonical_selection
    $lease=Enter-ProjectGeneratedContentCanonicalChild -ProjectRoot $ProjectRoot -OperationId $OperationId `
        -WorldDataPlugin $WorldDataPlugin -CandidateSha256 $participant.transition.new_active_sha256 `
        -EffectiveResultSha256 $participant.transition.effective_result_sha256 -PreparingBase
    try {
        $expected=@($participant.base_inputs | Where-Object {
            $_.realization_profile_sha256 -ceq $RealizationProfileHash -and
            (ConvertTo-Json -InputObject (ConvertTo-ProjectWorldSortedValue $_.input_identity) -Compress) -ceq
            (ConvertTo-Json -InputObject (ConvertTo-ProjectWorldSortedValue $InputIdentity) -Compress)
        })
        if($expected.Count -ne 1){throw 'Temporary enrollment requires exact admitted base inputs.'}
        # A prior city's refresh may already exist; authenticate original backups, not current equality.
        Assert-ProjectGeneratedContentCanonicalRestore -ProjectRoot $ProjectRoot -Marker $marker -RequirePrior
        $roots=Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $WorldDataPlugin
        $scope=Get-ProjectWorldMapScopeId -MapPackage $InputIdentity.map_package -GeneratedPackageRoot $roots.GeneratedPackageRoot
        $backupRoot=Join-Path (Join-Path $ProjectRoot $marker.world.snapshot_root) 'manifest_root'
        $original=Get-Content (Join-Path $backupRoot 'active_set.json') -Raw | ConvertFrom-Json
        $entries=@($original.scopes | Where-Object scope_id -CEQ $scope)
        if($entries.Count -ne 1 -or [string]$entries[0].manifest_path -cnotmatch ('^scopes/'+$scope+'\.[0-9]+\.json$')) {
            throw 'Temporary enrollment cannot introduce a new map authority.'
        }
        $mapPath=Join-Path $backupRoot $entries[0].manifest_path
        $map=Get-Content $mapPath -Raw | ConvertFrom-Json
        if((Get-ProjectWorldFileSha256 $mapPath) -cne $entries[0].manifest_sha256 -or
            $map.scope_id -cne $scope -or $map.input_identity.map_package -cne $InputIdentity.map_package) {
            throw 'Temporary enrollment lost its accepted original map.'
        }
    } finally {$lease.Dispose()}
}

function Test-ProjectGeneratedContentCanonicalParticipant {
    param([object]$Marker)
    return $null -ne $Marker.PSObject.Properties['canonical_selection']
}

function Assert-ProjectGeneratedContentCanonicalRestore {
    # Authenticate ALL backups before inner recovery or reverse canonical CAS can mutate state.
    param([string]$ProjectRoot, [object]$Marker, [switch]$Current, [switch]$Prepared, [switch]$RequirePrior)
    if (-not (Test-ProjectGeneratedContentCanonicalParticipant $Marker)) { return }
    Assert-ProjectGeneratedContentCanonicalParticipant $Marker.canonical_selection
    $selected = $Marker.canonical_selection.prepared_world
    if ($Prepared -and $null -eq $selected) { throw 'No accepted prepared World snapshot exists.' }
    if ($null -ne $selected) {
        $expectedRoot = [string]$Marker.world.snapshot_root -replace '/outer-snapshot$', '/prepared-snapshot'
        if ([string]$selected.snapshot_root -cnotin @([string]$Marker.world.snapshot_root, $expectedRoot)) {
            throw 'Prepared World snapshot escapes its outer operation.'
        }
        $actualPrepared = Get-ProjectWorldOuterSnapshotInventorySha256 -ProjectRoot $ProjectRoot `
            -WorldDataPlugin $Marker.world.world_data_plugin -SnapshotRoot (Join-Path $ProjectRoot $selected.snapshot_root) `
            -RecordsSha256 $selected.records_sha256 -Current:($Current -and $Prepared)
        if ($actualPrepared -cne [string]$selected.inventory_sha256) { throw 'Prepared snapshot inventory differs from accepted bytes.' }
    }
    $actual = Get-ProjectWorldOuterSnapshotInventorySha256 -ProjectRoot $ProjectRoot `
        -WorldDataPlugin $Marker.world.world_data_plugin -SnapshotRoot (Join-Path $ProjectRoot $Marker.world.snapshot_root) `
        -RecordsSha256 $Marker.world.snapshot_records_sha256 -Current:($Current -and -not $Prepared)
    if ($actual -cne [string]$Marker.canonical_selection.snapshot_inventory_sha256) {
        throw 'Outer snapshot inventory differs from its pre-operation bytes; recovery evidence retained.'
    }
    $state = Invoke-ProjectWorldCanonicalSelection -ProjectRoot $ProjectRoot -Transition $Marker.canonical_selection.transition -Action validate
    if (($Current -or $RequirePrior) -and [string]$state.state -cne 'prior') { throw 'Outer restore did not recover the prior canonical selection.' }
}

function Set-ProjectGeneratedContentCanonicalPrepared {
    param([System.IO.FileStream]$Lock, [string]$ProjectRoot, [object]$Marker)
    Assert-ProjectGeneratedContentLockOwner -Lock $Lock -ProjectRoot $ProjectRoot
    if ($null -ne $Marker.canonical_selection.prepared_world) { throw 'Prepared World checkpoint is already committed.' }
    Assert-ProjectGeneratedContentCanonicalRestore -ProjectRoot $ProjectRoot -Marker $Marker
    $state = Invoke-ProjectWorldCanonicalSelection -ProjectRoot $ProjectRoot -Transition $Marker.canonical_selection.transition -Action validate
    if ([string]$state.state -cne 'prior') { throw 'Base preparation cannot advance canonical selection.' }
    $identity = Get-ProjectGeneratedContentWorldIdentity -ProjectRoot $ProjectRoot -WorldDataPlugin $Marker.world.world_data_plugin `
        -EvidenceRoot (Join-Path (Get-ProjectGeneratedContentOuterEvidenceRoot -ProjectRoot $ProjectRoot -OperationId $Marker.operation_id) 'prepared')
    if ($identity.audit_status -cne 'accepted') { throw 'Prepared World requires a strict accepted cold audit.' }
    $relative = [string]$Marker.world.snapshot_root -replace '/outer-snapshot$', '/prepared-snapshot'
    $root = Join-Path $ProjectRoot $relative
    $records = New-ProjectWorldOuterSnapshot -ProjectRoot $ProjectRoot -WorldDataPlugin $Marker.world.world_data_plugin -SnapshotRoot $root
    $inventory = Get-ProjectWorldOuterSnapshotInventorySha256 -ProjectRoot $ProjectRoot -WorldDataPlugin $Marker.world.world_data_plugin `
        -SnapshotRoot $root -RecordsSha256 $records
    $current = Get-ProjectWorldOuterSnapshotInventorySha256 -ProjectRoot $ProjectRoot -WorldDataPlugin $Marker.world.world_data_plugin `
        -SnapshotRoot $root -RecordsSha256 $records -Current
    if ($inventory -cne $current) { throw 'Prepared snapshot inventory does not match current bytes.' }
    $Marker.canonical_selection.prepared_world = [pscustomobject]@{snapshot_root = $relative; records_sha256 = $records; inventory_sha256 = $inventory}
    Write-ProjectGeneratedContentOuterMarker -Lock $Lock -ProjectRoot $ProjectRoot -Marker $Marker
    return Read-ProjectGeneratedContentOuterMarker -ProjectRoot $ProjectRoot
}

function Restore-ProjectGeneratedContentCanonicalParticipant {
    param([string]$ProjectRoot, [object]$Marker)
    if (-not (Test-ProjectGeneratedContentCanonicalParticipant $Marker)) { return }
    $state = Invoke-ProjectWorldCanonicalSelection -ProjectRoot $ProjectRoot -Transition $Marker.canonical_selection.transition -Action restore
    if ([string]$state.state -cne 'prior') { throw 'Canonical reverse CAS did not restore the prior selection.' }
}

function Enter-ProjectGeneratedContentCanonicalChild {
    # A private child cannot become an independent durable writer outside its live outer owner.
    param([string]$ProjectRoot, [string]$OperationId, [string]$CandidateSha256, [string]$EffectiveResultSha256, [string]$WorldDataPlugin,
        [switch]$PreparingBase)
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
        if (($null -eq $marker.canonical_selection.prepared_world) -ne [bool]$PreparingBase) {
            throw 'Canonical target child does not match the accepted prepared World phase.'
        }
        return $handle
    }
    catch { $handle.Dispose(); throw }
}
