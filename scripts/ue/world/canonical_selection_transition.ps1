# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
Set-StrictMode -Version Latest

function Invoke-ProjectWorldCanonicalSelection {
    param([Parameter(Mandatory=$true)][string]$ProjectRoot,
        [Parameter(Mandatory=$true)][object]$Transition,
        [Parameter(Mandatory=$true)][ValidateSet('validate','publish','restore')][string]$Action)
    $root=[IO.Path]::GetFullPath($ProjectRoot)
    $scratch=Join-Path $root ('tmp/world/world_realization/canonical-transition/'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $scratch -Force | Out-Null
    $ticketPath=Join-Path $scratch 'transition.json'
    $errorPath=Join-Path $scratch 'stderr.log'
    Write-ProjectWorldJson -Document $Transition -Path $ticketPath
    $python=(Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $bootstrap=Join-Path $root 'tools/World/CanonicalCompilation/bootstrap.py'
    if (-not (Test-Path -LiteralPath $bootstrap -PathType Leaf)) {throw 'Canonical transition entrypoint is missing.'}
    $ErrorActionPreference='Continue'
    $output=@(& $python -S $bootstrap 'selection-transition' '--transition' $ticketPath '--action' $Action 2> $errorPath)
    $exitCode=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    if ($exitCode -ne 0 -or $output.Count -eq 0) {
        throw "Canonical transition $Action refused (exit=$exitCode); recovery evidence retained. See $errorPath"
    }
    $result=($output -join "`n") | ConvertFrom-Json
    if ([string]$result.status -cne 'accepted' -or [string]$result.state -cnotin @('prior','selected')) {
        throw "Canonical transition $Action returned unqualified evidence; retained at $scratch"
    }
    return $result
}

function Resolve-ProjectWorldCanonicalJournalState {
    param([Parameter(Mandatory=$true)][string]$ProjectRoot,
        [Parameter(Mandatory=$true)][string]$ManifestRoot,
        [Parameter(Mandatory=$true)][string]$ContentRoot,
        [Parameter(Mandatory=$true)][object]$Journal,
        [switch]$RestorePrior)
    if ($null -eq $Journal.PSObject.Properties['canonical_selection_transition']) {return $null}
    if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {throw 'Canonical recovery requires an explicit project root.'}
    $transition=$Journal.canonical_selection_transition
    $recoverySha=if ([string]$Journal.phase -ceq 'publishing') {[string]$Journal.expected_active_set_sha256} else {''}
    $active=Read-ProjectWorldActiveSet -ManifestRoot $ManifestRoot -ProjectRoot $ProjectRoot -RecoveryActiveSetSha256 $recoverySha
    if ($null -eq $active) {throw 'Canonical recovery requires accepted World authority.'}
    $prior=[string]$active.Sha256 -ceq [string]$Journal.prior_active_set_sha256
    $next=[string]$Journal.phase -ceq 'publishing' -and [string]$active.Sha256 -ceq [string]$Journal.expected_active_set_sha256
    if (-not $prior -and -not $next) {throw 'World authority matches neither paired state; recovery preserves evidence.'}
    $plugin=Split-Path -Leaf (Split-Path -Parent $ContentRoot)
    $scope=Get-ProjectWorldMapScopeId -MapPackage $Journal.map_package -GeneratedPackageRoot "/$plugin/Generated/"
    if (-not $active.Manifests.Contains($scope) -or
        [string]$active.Manifests[$scope].input_identity.map_package -cne [string]$Journal.map_package -or
        [string]$active.Manifests[$scope].input_identity.compile_result_sha256 -cne [string]$transition.base_result_sha256) {
        throw 'Canonical transition is not paired with this World map/base.'
    }
    $bindings=@()
    if ($null -ne $active.Record.PSObject.Properties['canonical_input_compatibility']) {
        $bindings=@($active.Record.canonical_input_compatibility | Where-Object map_scope_id -CEQ $scope)
    }
    if ($next) {
        if ($bindings.Count -ne 1 -or
            [string]$bindings[0].base_canonical_authority_sha256 -cne [string]$transition.old_active_sha256 -or
            [string]$bindings[0].candidate.active_sha256 -cne [string]$transition.new_active_sha256 -or
            [string]$bindings[0].candidate.base_result_sha256 -cne [string]$transition.base_result_sha256 -or
            [string]$bindings[0].candidate.result_sha256 -cne [string]$transition.effective_result_sha256 -or
            [string]$bindings[0].candidate.effective_identity -cne [string]$transition.effective_identity) {
            throw 'Committed World lacks its exact canonical transition binding.'
        }
    }
    elseif ($bindings.Count -gt 0) {throw 'A subsequent canonical transition requires explicit reconciliation.'}
    $state=Invoke-ProjectWorldCanonicalSelection -ProjectRoot $ProjectRoot -Transition $transition -Action validate
    if ($next) {
        if ([string]$state.state -cne 'selected') {throw 'World committed before canonical selection; recovery preserves evidence.'}
        return 'committed'
    }
    if ($RestorePrior) {
        Assert-ProjectWorldCanonicalRollback -ProjectRoot $ProjectRoot -ManifestRoot $ManifestRoot -Journal $Journal -Snapshot
    }
    if ([string]$state.state -ceq 'selected') {
        if ([string]$Journal.phase -cne 'publishing') {throw 'Canonical selection advanced before the publishing phase.'}
        if (-not $RestorePrior) {return 'canonical_selected'}
        $restored=Invoke-ProjectWorldCanonicalSelection -ProjectRoot $ProjectRoot -Transition $transition -Action restore
        if ([string]$restored.state -cne 'prior') {throw 'Canonical recovery did not restore the prior selection.'}
    }
    return 'prior'
}

function Assert-ProjectWorldCanonicalRollback {
    param([Parameter(Mandatory=$true)][string]$ProjectRoot,
        [Parameter(Mandatory=$true)][string]$ManifestRoot,
        [Parameter(Mandatory=$true)][object]$Journal,
        [switch]$Snapshot)
    $recoverySha=if ([string]$Journal.phase -ceq 'publishing') {[string]$Journal.expected_active_set_sha256} else {''}
    $active=Read-ProjectWorldActiveSet -ManifestRoot $ManifestRoot -ProjectRoot $ProjectRoot -RecoveryActiveSetSha256 $recoverySha
    if ($null -eq $active -or [string]$active.Sha256 -cne [string]$Journal.prior_active_set_sha256) {
        throw 'Canonical rollback no longer has its exact prior World authority.'
    }
    if ($Snapshot) {
        if (-not (Test-Path -LiteralPath $Journal.snapshot_root -PathType Container)) {throw 'Canonical rollback snapshot is missing.'}
        foreach ($record in @($Journal.snapshot_records)) {
            $existed=$null -eq $record.PSObject.Properties['existed'] -or [bool]$record.existed
            if ($existed -and -not (Test-Path -LiteralPath $record.backup)) {throw 'Canonical rollback required backup is missing.'}
        }
    }
    foreach ($scope in @($Journal.mutation_scope_ids)) {
        if (-not $active.Manifests.Contains($scope)) {throw 'Canonical rollback has an unknown prior scope.'}
    }
    # Snapshot restoration can include carried layers and external actors beyond the selected writers.
    foreach ($scope in @($active.Manifests.Keys)) {
        foreach ($artifact in @($active.Manifests[$scope].artifacts)) {
            $path=[IO.Path]::GetFullPath((Join-Path $ProjectRoot $artifact.path))
            $backups=@(foreach ($record in @($Journal.snapshot_records)) {
                    $source=[IO.Path]::GetFullPath([string]$record.source).TrimEnd('\','/')
                    if ($path.Equals($source,[StringComparison]::OrdinalIgnoreCase)) {[string]$record.backup}
                    elseif ($path.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase)) {
                        Join-Path $record.backup $path.Substring($source.Length+1)
                    }
            })
            if ($backups.Count -eq 0) {
                if ($scope -cin @($Journal.mutation_scope_ids)) {throw 'Canonical rollback backup does not cover a prior owned artifact.'}
                continue
            }
            $targets=if ($Snapshot) {$backups} else {@($path)}
            foreach ($target in $targets) {
                if (-not (Test-Path -LiteralPath $target -PathType Leaf) -or
                    (Get-ProjectWorldFileSha256 -Path $target) -cne [string]$artifact.digest) {
                    throw 'Canonical rollback artifact differs from the prior accepted manifest.'
                }
            }
        }
    }
}
