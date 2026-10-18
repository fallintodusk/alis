# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
# EndToEndValidation owns admission and judgment; this adapter composes existing owner operations.
#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RequestPath,
    [Parameter(Mandatory = $true)][string]$EvidencePath,
    [ValidateSet('failure', 'saved', 'noop')][string]$HostLeg = '',
    [string]$ExpectedRequestSha256 = '',
    [string]$OuterOperationId = ''
)
$ErrorActionPreference = 'Stop'
$targetEntry = $PSCommandPath
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
. (Join-Path $PSScriptRoot '../generated_content/generated_content_outer_recovery.ps1')
. (Join-Path $PSScriptRoot 'canonical_refinement_operation.ps1')
$requestFull = [IO.Path]::GetFullPath($RequestPath)
$evidenceFull = [IO.Path]::GetFullPath($EvidencePath)
$scratch = $projectRoot + '\tmp\'
if (-not $requestFull.StartsWith($scratch, [StringComparison]::OrdinalIgnoreCase) -or
    -not $evidenceFull.StartsWith($scratch, [StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $evidenceFull)) { throw 'Target requires a tmp request and a new tmp evidence file.' }
$evidenceRoot = Split-Path -Parent $evidenceFull
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$request = Get-Content -LiteralPath $requestFull -Raw | ConvertFrom-Json
$profile = Get-Content -LiteralPath (Join-Path $projectRoot $request.validation_profile_path) -Raw | ConvertFrom-Json
$selectionPath = Join-Path $projectRoot $request.selection_path
$selection = Get-Content -LiteralPath $selectionPath -Raw | ConvertFrom-Json
$names = @($selection.candidates.PSObject.Properties.Name)
if ($names.Count -ne 1) { throw 'One selected child is required per target operation.' }
$name = [string]$names[0]
$settings = $profile.profiles.$name
$candidate = $selection.candidates.$name
if ($HostLeg) {
    if ((Get-ProjectWorldFileSha256 $requestFull) -cne $ExpectedRequestSha256 -or
        (Get-ProjectWorldFileSha256 $selectionPath) -cne [string]$request.selection_sha256) { throw 'Target host inputs changed.' }
    $arguments = @{CompileResult = (Join-Path $projectRoot $candidate.materialized_result_path); Mode = 'Apply';
        Map = [string]$settings.map_package; WorldDataPlugin = [string]$settings.world_data_plugin;
        PresentationProfile = (Join-Path $projectRoot $settings.presentation_profile);
        RuntimeProfile = (Join-Path $projectRoot $settings.runtime_profile);
        AuthoredOverlayProfile = (Join-Path $projectRoot $settings.authored_overlay_profile);
        RealizationProfile = (Join-Path $projectRoot $settings.realization_profile);
        ExecuteLayer = @($request.execute_layers); NonInteractive = $true; EvidencePath = $evidenceFull}
    if ($HostLeg -cne 'noop') { $arguments.CanonicalRefinementSelection = $selectionPath }
    $childLock = Enter-ProjectGeneratedContentCanonicalChild -ProjectRoot $projectRoot -OperationId $OuterOperationId `
        -CandidateSha256 $candidate.active_sha256 -EffectiveResultSha256 $candidate.result_sha256 -WorldDataPlugin $settings.world_data_plugin
    try { & (Join-Path $PSScriptRoot 'realize_canonical_world.ps1') @arguments } finally { $childLock.Dispose() }
    exit $LASTEXITCODE
}

function Invoke-TargetLeg {
    param([string]$Leg)
    $path = Join-Path $evidenceRoot ($Leg + '-native.json')
    return Invoke-ProjectGeneratedContentChildScript -ScriptPath $targetEntry -Arguments @(
        '-RequestPath', $requestFull, '-EvidencePath', $path, '-HostLeg', $Leg,
        '-ExpectedRequestSha256', $report.request_sha256, '-OuterOperationId', $marker.operation_id) `
        -LogPath (Join-Path $evidenceRoot ($Leg + '.log'))
}
function Get-TargetReceiptPin {
    param([string]$Path)
    return @{path = (ConvertTo-ProjectWorldOuterRelativePath -ProjectRoot $projectRoot -Path $Path);
        sha256 = Get-ProjectWorldFileSha256 $Path}
}

Assert-ProjectGeneratedContentNoProjectUnrealProcess -ProjectFile (Join-Path $projectRoot 'Alis.uproject') -Purpose 'Selected refinement target'
$lock = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $projectRoot -RequireOwnership -OwnerName 'selected refinement acceptance'
$prior = $null
$marker = $null
$report = [ordered]@{status = 'rejected'; request_sha256 = (Get-ProjectWorldFileSha256 $requestFull); receipts = [ordered]@{}}
try {
    $prior = Enable-ProjectGeneratedContentLockDelegation -Lock $lock
    $verified = Invoke-ProjectWorldRefinementOwner -ProjectRoot $projectRoot -Owner EndToEndValidation `
        -Arguments @('verify-refinement-target', '--request', $requestFull) -EvidenceDirectory (Join-Path $evidenceRoot 'admission')
    if ([string]$verified.status -cne 'accepted' -or [string]$verified.request_sha256 -cne $report.request_sha256) {
        throw 'Target request failed public admission.'
    }
    $request = $verified.request
    $settings = $verified.settings
    $candidate = $verified.candidate
    $name = [string]$verified.candidate_name
    $selectionPath = Join-Path $projectRoot $request.selection_path
    $selection = Get-Content -LiteralPath $selectionPath -Raw | ConvertFrom-Json
    $ticket = Invoke-ProjectWorldRefinementOwner -ProjectRoot $projectRoot -Owner CanonicalCompilation -Arguments @(
        'prepare-selection-transition', '--profile', (Join-Path $projectRoot $settings.compiler_profile_path),
        '--candidate', (Join-Path $projectRoot $candidate.active_path), '--expected-active-sha256',
        [string]$selection.base_selection.profiles.$name.active_sha256) -EvidenceDirectory (Join-Path $evidenceRoot 'preparation')
    $marker = New-ProjectGeneratedContentOuterOperation -Lock $lock -ProjectRoot $projectRoot `
        -WorldDataPlugin $settings.world_data_plugin -Label 'selected_refinement_target' -CanonicalTransition $ticket
    $report.transition = $ticket
    $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $projectRoot -PluginName $settings.world_data_plugin
    $worldPath = Join-Path $roots.ManifestRoot 'active_set.json'
    $canonicalPath = Join-Path $projectRoot $ticket.active_path
    $beforeWorld = Get-ProjectWorldFileSha256 $worldPath
    $beforeInventory = [string]$marker.canonical_selection.snapshot_inventory_sha256
    try {
        if ([string]$marker.world.pre.audit_status -cne 'accepted') { throw 'Selected target requires accepted pre-operation World authority.' }
        Write-ProjectGeneratedContentOuterLog 'target=failure_control'
        $held = [IO.File]::Open($worldPath, 'Open', 'Read', 'Read')
        try { $failureExit = Invoke-TargetLeg 'failure' } finally { $held.Dispose() }
        $journalPath = Join-Path $roots.ManifestRoot 'journal.json'
        $journal = Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
        Test-ProjectWorldJournal -Journal $journal -ProjectRoot $projectRoot -ManifestRoot $roots.ManifestRoot -ContentRoot $roots.ContentRoot
        $state = Invoke-ProjectWorldCanonicalSelection -ProjectRoot $projectRoot -Transition $ticket -Action validate
        if ($failureExit -eq 0 -or [string]$journal.phase -cne 'publishing' -or [string]$state.state -cne 'selected' -or
            (Get-ProjectWorldFileSha256 $worldPath) -cne $beforeWorld) { throw 'Failure control did not reach the intended saved post-CAS boundary.' }
        Copy-Item -LiteralPath $journalPath -Destination (Join-Path $evidenceRoot 'failure-journal.json')
        Copy-Item -LiteralPath (Join-Path $roots.ManifestRoot 'active_set.json.tmp') -Destination (Join-Path $evidenceRoot 'failure-staged-active-set.json')
        Assert-ProjectGeneratedContentCanonicalRestore -ProjectRoot $projectRoot -Marker $marker
        Invoke-ProjectGeneratedContentWorldTransactionRecovery -ProjectRoot $projectRoot -WorldDataPlugin $settings.world_data_plugin `
            -EvidenceRoot (Join-Path $evidenceRoot 'failure-recovery')
        Assert-ProjectGeneratedContentCanonicalRestore -ProjectRoot $projectRoot -Marker $marker -Current
        $report.failure_recovery = @{status = 'accepted'; host_exit = $failureExit; phase = [string]$journal.phase;
            canonical_state = [string]$state.state; world_sha256 = $beforeWorld}
        $report.receipts.failure_native = Get-TargetReceiptPin (Join-Path $evidenceRoot 'failure-native.json')
        $report.receipts.failure_journal = Get-TargetReceiptPin (Join-Path $evidenceRoot 'failure-journal.json')
        $report.receipts.failure_staging = Get-TargetReceiptPin (Join-Path $evidenceRoot 'failure-staged-active-set.json')

        Write-ProjectGeneratedContentOuterLog 'target=saved_publication'
        if ((Invoke-TargetLeg 'saved') -ne 0) { throw 'Selected saved publication failed.' }
        $state = Invoke-ProjectWorldCanonicalSelection -ProjectRoot $projectRoot -Transition $ticket -Action validate
        if ([string]$state.state -cne 'selected') { throw 'Successful World publication has no selected canonical authority.' }
        $savedWorld = Get-ProjectWorldFileSha256 $worldPath
        $savedCanonical = Get-ProjectWorldFileSha256 $canonicalPath
        Copy-Item -LiteralPath $worldPath -Destination (Join-Path $evidenceRoot 'saved-active-set.json')
        $audit = Get-ProjectGeneratedContentWorldIdentity -ProjectRoot $projectRoot -WorldDataPlugin $settings.world_data_plugin `
            -EvidenceRoot (Join-Path $evidenceRoot 'saved-audit')
        if ($audit.audit_status -cne 'accepted') { throw 'Cold selected authority audit rejected.' }
        $report.saved_publication = @{status = 'accepted'; world_sha256 = $savedWorld; canonical_sha256 = $savedCanonical}
        $report.receipts.saved_native = Get-TargetReceiptPin (Join-Path $evidenceRoot 'saved-native.json')
        $report.receipts.saved_audit = Get-TargetReceiptPin (Join-Path $evidenceRoot 'saved-audit/world_audit.json')
        $report.receipts.saved_active = Get-TargetReceiptPin (Join-Path $evidenceRoot 'saved-active-set.json')

        Write-ProjectGeneratedContentOuterLog 'target=cold_noop'
        if ((Invoke-TargetLeg 'noop') -ne 0) { throw 'Cold accepted selected replay failed.' }
        if ((Get-ProjectWorldFileSha256 $worldPath) -cne $savedWorld -or
            (Get-ProjectWorldFileSha256 $canonicalPath) -cne $savedCanonical) { throw 'Cold replay changed an accepted pointer.' }
        $report.cold_noop = @{status = 'accepted'; world_sha256 = $savedWorld; canonical_sha256 = $savedCanonical}
        $report.receipts.noop_native = Get-TargetReceiptPin (Join-Path $evidenceRoot 'noop-native.json')
    }
    finally {
        Write-ProjectGeneratedContentOuterLog 'target=restoring'
        $null = Invoke-ProjectGeneratedContentOuterRestore -ProjectRoot $projectRoot -Marker $marker -EvidenceRoot (Join-Path $evidenceRoot 'restoration')
        $afterInventory = Get-ProjectWorldOuterSnapshotInventorySha256 -ProjectRoot $projectRoot -WorldDataPlugin $settings.world_data_plugin `
            -SnapshotRoot (Join-Path $projectRoot $marker.world.snapshot_root) -RecordsSha256 $marker.world.snapshot_records_sha256 -Current
        $report.restoration = @{status = 'accepted'; before_inventory_sha256 = $beforeInventory; after_inventory_sha256 = $afterInventory;
            before_world_sha256 = $beforeWorld; after_world_sha256 = (Get-ProjectWorldFileSha256 $worldPath); canonical_state = 'prior'}
        Complete-ProjectGeneratedContentOuterOperation -Lock $lock -ProjectRoot $projectRoot -Marker $marker -Result restored
    }
    $report.status = 'accepted'
}
finally {
    Write-ProjectWorldJson -Path $evidenceFull -Document $report
    Disable-ProjectGeneratedContentLockDelegation -Prior $prior
    $lock.Dispose()
}
