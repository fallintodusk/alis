# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Outer generated-content operation: one envelope over World generated
# authority and ProjectMaterial surfaces under one live content lock. The
# marker beside the lock is its durable journal and recover_generated_content.ps1
# its only resolver. Restores stay with their owners: World's outer snapshot
# and transaction recovery, ProjectMaterial's RestorePrevious mode.
#
# Marker protocol: create once with File.Move onto an absent name; update with
# File.Replace; delete stale staging only while the committed marker exists,
# otherwise promote the staging copy first; any marker write failure is
# terminal for the caller. Marker or staging present means pending.

. (Join-Path $PSScriptRoot 'generated_content_mutation_lock.ps1')
. (Join-Path $PSScriptRoot '..\world\generated_outer_snapshot.ps1')

function Get-ProjectGeneratedContentOuterPhases {
    # Logged, never branched on: recovery decides from what is on disk.
    return @(
        'snapshot_taken', 'o1_running', 'o4_running', 'o2_running', 'layout_receipt_running',
        'o3_running', 'o3_completed', 'checks_running', 'restoring', 'restore_failed')
}

function Write-ProjectGeneratedContentOuterLog {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host "[GeneratedContentOuterRecovery] $Message"
}

function Get-ProjectGeneratedContentTextSha256 {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Value)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}

function Get-ProjectGeneratedContentOuterRoot {
    # The confined root of one outer operation: World snapshot and input copies.
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$OperationId
    )
    $lockDirectory = Split-Path -Parent (Get-ProjectGeneratedContentLockPath -ProjectRoot $ProjectRoot)
    return Join-Path $lockDirectory "outer_recovery\$OperationId"
}

function Get-ProjectGeneratedContentOuterEvidenceRoot {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$OperationId
    )
    return Join-Path $ProjectRoot "Saved\Validation\GeneratedContentRecovery\$OperationId"
}

function Assert-ProjectGeneratedContentLockOwner {
    # Only the live owner (an outer coordinator or recovery) writes the marker.
    param(
        [Parameter(Mandatory = $true)][System.IO.FileStream]$Lock,
        [Parameter(Mandatory = $true)][string]$ProjectRoot
    )
    $expected = [System.IO.Path]::GetFullPath((Get-ProjectGeneratedContentLockPath -ProjectRoot $ProjectRoot))
    if (-not $Lock.CanWrite -or [System.IO.Path]::GetFullPath($Lock.Name) -ine $expected) {
        throw "Outer-recovery state is written only by the live owner of $expected."
    }
}

function Invoke-ProjectGeneratedContentChildScript {
    # Runs one owner entry point synchronously in a child Windows PowerShell; a
    # delegated token in this process is inherited. Output goes to LogPath.
    param(
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
    )
    $ErrorActionPreference = 'Continue'
    New-Item -ItemType Directory -Path (Split-Path -Parent $LogPath) -Force | Out-Null
    $powershell = (Get-Process -Id $PID).Path
    $output = & $powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $ScriptPath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    @($output | ForEach-Object { [string]$_ }) | Set-Content -LiteralPath $LogPath -Encoding UTF8
    return $exitCode
}

function Get-ProjectGeneratedContentWorldIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$WorldDataPlugin,
        [Parameter(Mandatory = $true)][string]$EvidenceRoot
    )
    $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $WorldDataPlugin
    $activePath = Join-Path $roots.ManifestRoot 'active_set.json'
    $activeSha = if (Test-Path -LiteralPath $activePath -PathType Leaf) {
        Get-ProjectWorldFileSha256 -Path $activePath
    }
    else { 'none' }
    $receiptPath = Join-Path $EvidenceRoot 'world_audit.json'
    if (Test-Path -LiteralPath $receiptPath) {
        Remove-Item -LiteralPath $receiptPath -Force
    }
    $audit = Join-Path $PSScriptRoot '..\world\audit_generated_authority.ps1'
    $exitCode = Invoke-ProjectGeneratedContentChildScript -ScriptPath $audit -Arguments @(
        '-ProjectRoot', $ProjectRoot, '-WorldDataPlugin', $WorldDataPlugin, '-EvidencePath', $receiptPath) `
        -LogPath (Join-Path $EvidenceRoot 'world_audit.log')
    if ($exitCode -notin @(0, 1) -or -not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
        throw "World authority audit wrote no receipt (exit $exitCode); the World identity is unknown."
    }
    $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    $status = [string]$receipt.status
    # An audit that never entered the lock audited nothing: its rejection is
    # not an identity, so it must not be compared as one.
    if ($status -notin @('accepted', 'rejected') -or
        @($receipt.checks | Where-Object { [string]$_.name -ceq 'transaction_settled' }).Count -ne 1) {
        throw "World authority audit did not run under the content lock; the World identity is unknown: $receiptPath"
    }
    $failures = @($receipt.failures | ForEach-Object { [string]$_ })
    return [ordered]@{
        active_set_sha256 = $activeSha
        manifest_root_sha256 = Get-ProjectWorldManifestRootSha256 -ManifestRoot $roots.ManifestRoot
        audit_status = $status
        audit_verdict_sha256 = Get-ProjectGeneratedContentTextSha256 -Value ($failures -join "`n")
    }
}

function Read-ProjectGeneratedContentMaterialHostReceipt {
    # The host receipt that one Material host run wrote, bound by operation id.
    param(
        [Parameter(Mandatory = $true)][string]$EvidenceRoot,
        [Parameter(Mandatory = $true)][string]$OperationId
    )
    foreach ($folder in @('Current', 'Rejected')) {
        $path = Join-Path $EvidenceRoot "$folder\host.receipt.json"
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        $receipt = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        if ([string]$receipt.operation_id -ceq $OperationId) {
            return $receipt
        }
    }
    return $null
}

function Get-ProjectGeneratedContentMaterialIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$LayoutReceipt,
        [Parameter(Mandatory = $true)][string]$EvidenceRoot
    )
    $operationId = [System.Guid]::NewGuid().ToString('N')
    if (Test-Path -LiteralPath $EvidenceRoot) {
        Remove-Item -LiteralPath $EvidenceRoot -Recurse -Force
    }
    $materialHost = Join-Path $ProjectRoot 'scripts\ue\material\run_material_generation.ps1'
    $exitCode = Invoke-ProjectGeneratedContentChildScript -ScriptPath $materialHost -Arguments @(
        '-Domain', 'Surface', '-Mode', 'Validate', '-OperationId', $operationId,
        '-LayoutReceipt', $LayoutReceipt, '-EvidencePath', $EvidenceRoot) `
        -LogPath "$EvidenceRoot.log"
    $receipt = Read-ProjectGeneratedContentMaterialHostReceipt -EvidenceRoot $EvidenceRoot -OperationId $operationId
    if ($null -eq $receipt) {
        throw "ProjectMaterial Validate wrote no host receipt bound to operation $operationId (exit $exitCode); the Material identity is unknown."
    }
    foreach ($field in @('status', 'commandlet_status', 'commandlet_error', 'manifest_tree_sha256', 'output_tree_sha256')) {
        if ($null -eq $receipt.PSObject.Properties[$field]) {
            throw "ProjectMaterial host receipt has no $field; the Material identity is unknown."
        }
    }
    $status = [string]$receipt.status
    return [ordered]@{
        manifest_tree_sha256 = [string]$receipt.manifest_tree_sha256
        output_tree_sha256 = [string]$receipt.output_tree_sha256
        validate_status = $status
        validate_verdict_sha256 = Get-ProjectGeneratedContentTextSha256 -Value (
            "status=$status|commandlet_status=$([string]$receipt.commandlet_status)|commandlet_error=$([string]$receipt.commandlet_error)")
    }
}

function Get-ProjectGeneratedContentOwnerIdentity {
    # The same identity before the operation and after a restore; both owner
    # checks run as delegated children of the live lock owner.
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$WorldDataPlugin,
        [Parameter(Mandatory = $true)][string]$LayoutReceipt,
        [Parameter(Mandatory = $true)][string]$EvidenceRoot
    )
    return [ordered]@{
        world = Get-ProjectGeneratedContentWorldIdentity `
            -ProjectRoot $ProjectRoot -WorldDataPlugin $WorldDataPlugin -EvidenceRoot $EvidenceRoot
        material = Get-ProjectGeneratedContentMaterialIdentity `
            -ProjectRoot $ProjectRoot -LayoutReceipt $LayoutReceipt -EvidenceRoot (Join-Path $EvidenceRoot 'material')
    }
}

function Invoke-ProjectGeneratedContentWorldTransactionRecovery {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$WorldDataPlugin,
        [Parameter(Mandatory = $true)][string]$EvidenceRoot
    )
    $recover = Join-Path $PSScriptRoot '..\world\recover_generated_transaction.ps1'
    $log = Join-Path $EvidenceRoot 'world_transaction_recovery.log'
    $exitCode = Invoke-ProjectGeneratedContentChildScript -ScriptPath $recover -Arguments @(
        '-WorldDataPlugin', $WorldDataPlugin, '-ProjectRoot', $ProjectRoot) -LogPath $log
    $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $WorldDataPlugin
    if ($exitCode -ne 0 -or (Test-Path -LiteralPath (Join-Path $roots.ManifestRoot 'journal.json'))) {
        throw "World transaction recovery did not settle the journal (exit $exitCode); see $log"
    }
}

function Invoke-ProjectGeneratedContentMaterialRestore {
    # Returns the source the host restored from: journal, bundle, or none.
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$RestoreOperationId,
        [Parameter(Mandatory = $true)][string]$EvidenceRoot
    )
    $operationId = [System.Guid]::NewGuid().ToString('N')
    if (Test-Path -LiteralPath $EvidenceRoot) {
        Remove-Item -LiteralPath $EvidenceRoot -Recurse -Force
    }
    $materialHost = Join-Path $ProjectRoot 'scripts\ue\material\run_material_generation.ps1'
    $exitCode = Invoke-ProjectGeneratedContentChildScript -ScriptPath $materialHost -Arguments @(
        '-Domain', 'Surface', '-Mode', 'RestorePrevious', '-RestoreOperationId', $RestoreOperationId,
        '-OperationId', $operationId, '-EvidencePath', $EvidenceRoot) `
        -LogPath "$EvidenceRoot.log"
    $receipt = Read-ProjectGeneratedContentMaterialHostReceipt -EvidenceRoot $EvidenceRoot -OperationId $operationId
    if ($exitCode -ne 0 -or $null -eq $receipt -or [string]$receipt.status -cne 'accepted') {
        throw "ProjectMaterial RestorePrevious for operation $RestoreOperationId failed (exit $exitCode); see $EvidenceRoot.log"
    }
    return [string]$receipt.source
}

function ConvertTo-ProjectGeneratedContentMarkerDocument {
    # The canonical marker shape, in its fixed key order.
    param([Parameter(Mandatory = $true)][object]$Marker)
    $materialOperation = $Marker.material.operation_id
    return [ordered]@{
        schema_version = 1
        operation_id = [string]$Marker.operation_id
        label = [string]$Marker.label
        phase = [string]$Marker.phase
        created_at_utc = [string]$Marker.created_at_utc
        updated_at_utc = [string]$Marker.updated_at_utc
        world = [ordered]@{
            world_data_plugin = [string]$Marker.world.world_data_plugin
            snapshot_root = [string]$Marker.world.snapshot_root
            snapshot_records_sha256 = [string]$Marker.world.snapshot_records_sha256
            pre = [ordered]@{
                active_set_sha256 = [string]$Marker.world.pre.active_set_sha256
                manifest_root_sha256 = [string]$Marker.world.pre.manifest_root_sha256
                audit_status = [string]$Marker.world.pre.audit_status
                audit_verdict_sha256 = [string]$Marker.world.pre.audit_verdict_sha256
            }
        }
        material = [ordered]@{
            operation_id = $(if ($null -eq $materialOperation) { $null } else { [string]$materialOperation })
            state = [string]$Marker.material.state
            layout_receipt_sha256 = [string]$Marker.material.layout_receipt_sha256
            pre = [ordered]@{
                manifest_tree_sha256 = [string]$Marker.material.pre.manifest_tree_sha256
                output_tree_sha256 = [string]$Marker.material.pre.output_tree_sha256
                validate_status = [string]$Marker.material.pre.validate_status
                validate_verdict_sha256 = [string]$Marker.material.pre.validate_verdict_sha256
            }
        }
    }
}

function Write-ProjectGeneratedContentOuterMarker {
    param(
        [Parameter(Mandatory = $true)][System.IO.FileStream]$Lock,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][object]$Marker,
        [switch]$Create
    )
    Assert-ProjectGeneratedContentLockOwner -Lock $Lock -ProjectRoot $ProjectRoot
    $markerPath = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $ProjectRoot
    $staging = "$markerPath.tmp"
    $json = ((ConvertTo-ProjectGeneratedContentMarkerDocument -Marker $Marker) | ConvertTo-Json -Depth 8) -replace "`r`n", "`n"
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($json + "`n")
    try {
        if ($Create) {
            if ((Test-Path -LiteralPath $markerPath) -or (Test-Path -LiteralPath $staging)) {
                throw "an outer-recovery marker already exists: $markerPath"
            }
        }
        elseif (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
            # A lost replacement leaves only the staging copy: promote it, never delete it.
            if (-not (Test-Path -LiteralPath $staging -PathType Leaf)) {
                throw "no outer-recovery marker exists to update: $markerPath"
            }
            [System.IO.File]::Move($staging, $markerPath)
        }
        if (Test-Path -LiteralPath $staging) {
            [System.IO.File]::Delete($staging)
        }
        $stream = New-Object System.IO.FileStream(
            $staging, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write,
            [System.IO.FileShare]::None, 4096, [System.IO.FileOptions]::WriteThrough)
        try {
            $stream.Write($bytes, 0, $bytes.Length)
            $stream.Flush($true)
        }
        finally { $stream.Dispose() }
        if ($Create) {
            [System.IO.File]::Move($staging, $markerPath)
        }
        else {
            [System.IO.File]::Replace($staging, $markerPath, [NullString]::Value)
        }
    }
    catch {
        throw "Outer-recovery marker write failed; this is terminal for the run: keep the lock and snapshots and stop. $($_.Exception.Message)"
    }
}

function Test-ProjectGeneratedContentOuterHash {
    param([AllowNull()][object]$Value, [switch]$AllowNone)
    $text = [string]$Value
    if ($AllowNone) { return $text -cmatch '^([a-f0-9]{64}|none)$' }
    return $text -cmatch '^[a-f0-9]{64}$'
}

function Assert-ProjectGeneratedContentOuterKeys {
    param([AllowNull()][object]$Value, [Parameter(Mandatory = $true)][string[]]$Keys, [Parameter(Mandatory = $true)][string]$Label)
    if ($null -eq $Value -or $Value -isnot [System.Management.Automation.PSCustomObject]) {
        throw "Outer-recovery marker $Label is not an object."
    }
    $actual = @($Value.PSObject.Properties.Name | Sort-Object)
    if (($actual -join ',') -cne (@($Keys | Sort-Object) -join ',')) {
        throw "Outer-recovery marker $Label has keys [$($actual -join ',')]; expected [$(@($Keys | Sort-Object) -join ',')]."
    }
}

function Read-ProjectGeneratedContentOuterMarker {
    # Returns the validated pending marker (the committed copy wins over
    # staging), or $null when none is pending. Any violation throws before
    # anything relies on the marker.
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)
    $markerPath = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $ProjectRoot
    $source = if (Test-Path -LiteralPath $markerPath) { $markerPath }
        elseif (Test-Path -LiteralPath "$markerPath.tmp") { "$markerPath.tmp" }
        else { $null }
    if ($null -eq $source) { return $null }
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Outer-recovery marker is not a file: $source"
    }
    try {
        $marker = [System.IO.File]::ReadAllText($source, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    }
    catch {
        throw "Outer-recovery marker is not valid JSON: $source"
    }
    Assert-ProjectGeneratedContentOuterKeys -Value $marker -Label 'document' -Keys @(
        'schema_version', 'operation_id', 'label', 'phase', 'created_at_utc', 'updated_at_utc', 'world', 'material')
    Assert-ProjectGeneratedContentOuterKeys -Value $marker.world -Label 'world' -Keys @(
        'world_data_plugin', 'snapshot_root', 'snapshot_records_sha256', 'pre')
    Assert-ProjectGeneratedContentOuterKeys -Value $marker.world.pre -Label 'world.pre' -Keys @(
        'active_set_sha256', 'manifest_root_sha256', 'audit_status', 'audit_verdict_sha256')
    Assert-ProjectGeneratedContentOuterKeys -Value $marker.material -Label 'material' -Keys @(
        'operation_id', 'state', 'layout_receipt_sha256', 'pre')
    Assert-ProjectGeneratedContentOuterKeys -Value $marker.material.pre -Label 'material.pre' -Keys @(
        'manifest_tree_sha256', 'output_tree_sha256', 'validate_status', 'validate_verdict_sha256')
    if ($marker.schema_version -isnot [int] -or $marker.schema_version -ne 1) {
        throw 'Outer-recovery marker schema_version is unsupported.'
    }
    $operationId = [string]$marker.operation_id
    if ($operationId -cnotmatch '^[a-f0-9]{32}$') { throw 'Outer-recovery marker operation_id is invalid.' }
    if ([string]$marker.label -cnotmatch '^[a-z0-9_]{1,64}$') { throw 'Outer-recovery marker label is invalid.' }
    if ([string]$marker.phase -cnotin (Get-ProjectGeneratedContentOuterPhases)) {
        throw "Outer-recovery marker phase is unknown: $($marker.phase)"
    }
    foreach ($field in @('created_at_utc', 'updated_at_utc')) {
        $text = [string]$marker.$field
        $parsed = [DateTime]::MinValue
        if ($text -cnotmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z$' -or
            -not [DateTime]::TryParseExact($text, 'o', [System.Globalization.CultureInfo]::InvariantCulture,
                [System.Globalization.DateTimeStyles]::RoundtripKind, [ref]$parsed)) {
            throw "Outer-recovery marker $field is not a round-trip UTC timestamp."
        }
    }
    $plugin = [string]$marker.world.world_data_plugin
    $null = Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $plugin
    $outerRoot = Get-ProjectGeneratedContentOuterRoot -ProjectRoot $ProjectRoot -OperationId $operationId
    $expectedSnapshot = "tmp/world/world_realization/outer_recovery/$operationId/outer-snapshot"
    if ([string]$marker.world.snapshot_root -cne $expectedSnapshot) {
        throw "Outer-recovery marker snapshot_root must be $expectedSnapshot."
    }
    $snapshotRoot = Join-Path $outerRoot 'outer-snapshot'
    if (-not (Test-Path -LiteralPath $snapshotRoot -PathType Container)) {
        throw "Outer-recovery snapshot is missing: $snapshotRoot"
    }
    foreach ($hash in @($marker.world.snapshot_records_sha256, $marker.world.pre.active_set_sha256,
            $marker.world.pre.manifest_root_sha256, $marker.world.pre.audit_verdict_sha256,
            $marker.material.layout_receipt_sha256, $marker.material.pre.validate_verdict_sha256)) {
        if (-not (Test-ProjectGeneratedContentOuterHash -Value $hash)) { throw 'Outer-recovery marker holds an invalid hash.' }
    }
    foreach ($hash in @($marker.material.pre.manifest_tree_sha256, $marker.material.pre.output_tree_sha256)) {
        if (-not (Test-ProjectGeneratedContentOuterHash -Value $hash -AllowNone)) { throw 'Outer-recovery marker holds an invalid tree hash.' }
    }
    foreach ($status in @($marker.world.pre.audit_status, $marker.material.pre.validate_status)) {
        if ([string]$status -cnotin @('accepted', 'rejected')) { throw 'Outer-recovery marker holds an invalid status.' }
    }
    $materialOperation = $marker.material.operation_id
    $state = [string]$marker.material.state
    if ($null -eq $materialOperation) {
        if ($state -cne 'not_started') { throw 'Outer-recovery marker binds no Material operation but its state is not not_started.' }
    }
    elseif ([string]$materialOperation -cnotmatch '^[a-f0-9]{32}$' -or [string]$materialOperation -ceq $operationId -or
        $state -cnotin @('running', 'retained')) {
        throw 'Outer-recovery marker Material operation binding is invalid.'
    }
    $layoutCopy = Join-Path $outerRoot 'inputs\material-layout-receipt.json'
    if (-not (Test-Path -LiteralPath $layoutCopy -PathType Leaf) -or
        (Get-ProjectWorldFileSha256 -Path $layoutCopy) -cne [string]$marker.material.layout_receipt_sha256) {
        throw "Outer-recovery layout receipt copy is missing or changed: $layoutCopy"
    }
    $null = Read-ProjectWorldOuterSnapshotRecords -ProjectRoot $ProjectRoot -WorldDataPlugin $plugin `
        -SnapshotRoot $snapshotRoot -RecordsSha256 ([string]$marker.world.snapshot_records_sha256)
    return $marker
}

function New-ProjectGeneratedContentOuterOperation {
    # Opens an outer operation under the live owner lock with delegation
    # enabled: input copy, pre-operation identities, World outer snapshot, and
    # the marker last. A failure before the marker leaves debris only.
    param(
        [Parameter(Mandatory = $true)][System.IO.FileStream]$Lock,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$WorldDataPlugin,
        [Parameter(Mandatory = $true)][ValidatePattern('^[a-z0-9_]{1,64}$')][string]$Label,
        [Parameter(Mandatory = $true)][string]$LayoutReceipt
    )
    Assert-ProjectGeneratedContentLockOwner -Lock $Lock -ProjectRoot $ProjectRoot
    if (Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $ProjectRoot) {
        throw 'An outer operation is already pending; run recover_generated_content.ps1.'
    }
    $variable = Get-ProjectGeneratedContentLockTokenVariable
    $null = $Lock.Seek(0, [System.IO.SeekOrigin]::Begin)
    $buffer = New-Object byte[] 256
    $count = $Lock.Read($buffer, 0, $buffer.Length)
    if ([Environment]::GetEnvironmentVariable($variable) -cne [System.Text.Encoding]::ASCII.GetString($buffer, 0, $count).Trim()) {
        throw 'An outer operation requires Enable-ProjectGeneratedContentLockDelegation for its owner checks.'
    }
    $operationId = [System.Guid]::NewGuid().ToString('N')
    $outerRoot = Get-ProjectGeneratedContentOuterRoot -ProjectRoot $ProjectRoot -OperationId $operationId
    $evidenceRoot = Get-ProjectGeneratedContentOuterEvidenceRoot -ProjectRoot $ProjectRoot -OperationId $operationId
    $layoutCopy = Join-Path $outerRoot 'inputs\material-layout-receipt.json'
    New-Item -ItemType Directory -Path (Split-Path -Parent $layoutCopy) -Force | Out-Null
    Copy-Item -LiteralPath $LayoutReceipt -Destination $layoutCopy
    $pre = Get-ProjectGeneratedContentOwnerIdentity -ProjectRoot $ProjectRoot -WorldDataPlugin $WorldDataPlugin `
        -LayoutReceipt $layoutCopy -EvidenceRoot (Join-Path $evidenceRoot 'pre')
    $recordsSha = New-ProjectWorldOuterSnapshot -ProjectRoot $ProjectRoot -WorldDataPlugin $WorldDataPlugin `
        -SnapshotRoot (Join-Path $outerRoot 'outer-snapshot')
    Write-ProjectGeneratedContentOuterLog -Message "snapshot_taken operation=$operationId"
    $now = [DateTime]::UtcNow.ToString('o', [System.Globalization.CultureInfo]::InvariantCulture)
    $marker = [pscustomobject]@{
        operation_id = $operationId
        label = $Label
        phase = 'snapshot_taken'
        created_at_utc = $now
        updated_at_utc = $now
        world = [pscustomobject]@{
            world_data_plugin = $WorldDataPlugin
            snapshot_root = "tmp/world/world_realization/outer_recovery/$operationId/outer-snapshot"
            snapshot_records_sha256 = $recordsSha
            pre = $pre.world
        }
        material = [pscustomobject]@{
            operation_id = $null
            state = 'not_started'
            layout_receipt_sha256 = Get-ProjectWorldFileSha256 -Path $layoutCopy
            pre = $pre.material
        }
    }
    Write-ProjectGeneratedContentOuterMarker -Lock $Lock -ProjectRoot $ProjectRoot -Marker $marker -Create
    Write-ProjectGeneratedContentOuterLog -Message "marker_written operation=$operationId"
    return Read-ProjectGeneratedContentOuterMarker -ProjectRoot $ProjectRoot
}

function Set-ProjectGeneratedContentOuterPhase {
    # Advances the phase; binds the Material operation id once, before that
    # operation starts, and marks its rollback bundle retained after it commits.
    param(
        [Parameter(Mandatory = $true)][System.IO.FileStream]$Lock,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$Phase,
        [ValidatePattern('^([a-f0-9]{32})?$')][string]$MaterialOperationId = '',
        [switch]$MaterialRetained
    )
    if ($Phase -cnotin (Get-ProjectGeneratedContentOuterPhases)) { throw "Unknown outer phase: $Phase" }
    $marker = Read-ProjectGeneratedContentOuterMarker -ProjectRoot $ProjectRoot
    if ($null -eq $marker) { throw 'No outer operation is pending.' }
    if ($MaterialOperationId) {
        if ($null -ne $marker.material.operation_id) { throw 'The Material operation of this outer operation is already bound.' }
        if ($MaterialOperationId -ceq [string]$marker.operation_id) { throw 'The Material operation id must differ from the outer operation id.' }
        $marker.material.operation_id = $MaterialOperationId
        $marker.material.state = 'running'
    }
    if ($MaterialRetained) {
        if ($null -eq $marker.material.operation_id) { throw 'No Material operation is bound.' }
        $marker.material.state = 'retained'
    }
    $marker.phase = $Phase
    $marker.updated_at_utc = [DateTime]::UtcNow.ToString('o', [System.Globalization.CultureInfo]::InvariantCulture)
    Write-ProjectGeneratedContentOuterMarker -Lock $Lock -ProjectRoot $ProjectRoot -Marker $marker
    Write-ProjectGeneratedContentOuterLog -Message "phase=$Phase operation=$($marker.operation_id)"
}

function Invoke-ProjectGeneratedContentOuterRestore {
    # Restores both owners to their pre-operation state and proves it: World
    # transaction recovery when a journal is pending, the World outer
    # snapshot, ProjectMaterial's own RestorePrevious for a bound operation,
    # then both identities against the marker. Every step is idempotent.
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][object]$Marker,
        [Parameter(Mandatory = $true)][string]$EvidenceRoot
    )
    $plugin = [string]$Marker.world.world_data_plugin
    $outerRoot = Get-ProjectGeneratedContentOuterRoot -ProjectRoot $ProjectRoot -OperationId ([string]$Marker.operation_id)
    $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $plugin
    if (Test-Path -LiteralPath (Join-Path $roots.ManifestRoot 'journal.json')) {
        Write-ProjectGeneratedContentOuterLog -Message 'world_transaction_recovery'
        Invoke-ProjectGeneratedContentWorldTransactionRecovery -ProjectRoot $ProjectRoot -WorldDataPlugin $plugin `
            -EvidenceRoot $EvidenceRoot
    }
    Write-ProjectGeneratedContentOuterLog -Message 'world_outer_restore'
    Restore-ProjectWorldOuterSnapshot -ProjectRoot $ProjectRoot -WorldDataPlugin $plugin `
        -SnapshotRoot (Join-Path $outerRoot 'outer-snapshot') `
        -RecordsSha256 ([string]$Marker.world.snapshot_records_sha256)
    $materialSource = 'unbound'
    if ($null -ne $Marker.material.operation_id) {
        $materialSource = Invoke-ProjectGeneratedContentMaterialRestore -ProjectRoot $ProjectRoot `
            -RestoreOperationId ([string]$Marker.material.operation_id) `
            -EvidenceRoot (Join-Path $EvidenceRoot 'material_restore')
        Write-ProjectGeneratedContentOuterLog -Message "material_restore source=$materialSource"
    }
    $post = Get-ProjectGeneratedContentOwnerIdentity -ProjectRoot $ProjectRoot -WorldDataPlugin $plugin `
        -LayoutReceipt (Join-Path $outerRoot 'inputs\material-layout-receipt.json') `
        -EvidenceRoot (Join-Path $EvidenceRoot 'post')
    $mismatches = [System.Collections.Generic.List[string]]::new()
    foreach ($owner in @('world', 'material')) {
        foreach ($property in $Marker.$owner.pre.PSObject.Properties) {
            if ([string]$post[$owner][$property.Name] -cne [string]$property.Value) {
                $mismatches.Add("$owner.$($property.Name)")
            }
        }
    }
    if ($mismatches.Count -gt 0) {
        throw "Restored owners do not match their pre-operation identity: $($mismatches -join ', ')"
    }
    Write-ProjectGeneratedContentOuterLog -Message 'identities_match'
    return [pscustomobject]@{ MaterialSource = $materialSource; Post = $post }
}

function Complete-ProjectGeneratedContentOuterOperation {
    # Removing the marker is the commit point, of a successful operation or of
    # a verified restore. What follows it is debris cleanup and evidence.
    param(
        [Parameter(Mandatory = $true)][System.IO.FileStream]$Lock,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][object]$Marker,
        [Parameter(Mandatory = $true)][ValidateSet('succeeded', 'restored')][string]$Result
    )
    Assert-ProjectGeneratedContentLockOwner -Lock $Lock -ProjectRoot $ProjectRoot
    $markerPath = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $ProjectRoot
    $staging = "$markerPath.tmp"
    if (Test-Path -LiteralPath $markerPath) {
        if (Test-Path -LiteralPath $staging) {
            [System.IO.File]::Delete($staging)
        }
        [System.IO.File]::Delete($markerPath)
    }
    elseif (Test-Path -LiteralPath $staging) {
        [System.IO.File]::Delete($staging)
    }
    $operationId = [string]$Marker.operation_id
    Write-ProjectGeneratedContentOuterLog -Message "marker_removed operation=$operationId result=$Result"
    try {
        $outerRoot = Get-ProjectGeneratedContentOuterRoot -ProjectRoot $ProjectRoot -OperationId $operationId
        if (Test-Path -LiteralPath $outerRoot) {
            Remove-Item -LiteralPath $outerRoot -Recurse -Force
        }
        $evidenceRoot = Get-ProjectGeneratedContentOuterEvidenceRoot -ProjectRoot $ProjectRoot -OperationId $operationId
        New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
        $evidence = [ordered]@{
            schema_version = 1
            operation_id = $operationId
            label = [string]$Marker.label
            result = $Result
            completed_at_utc = [DateTime]::UtcNow.ToString('o', [System.Globalization.CultureInfo]::InvariantCulture)
        }
        [System.IO.File]::WriteAllText((Join-Path $evidenceRoot 'outer_operation.json'),
            (($evidence | ConvertTo-Json -Depth 4) -replace "`r`n", "`n") + "`n", [System.Text.UTF8Encoding]::new($false))
    }
    catch {
        # Committed already; leftover state is debris that workspace cleanup removes.
        Write-Warning "Outer operation $operationId committed; post-commit cleanup or evidence failed: $($_.Exception.Message)"
    }
}
