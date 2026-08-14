# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Confirm-ProjectWorldMutation {
    param(
        [Parameter(Mandatory = $true)][string]$Operation,
        [Parameter(Mandatory = $true)][string]$MapPackage,
        [Parameter(Mandatory = $true)][hashtable]$ParticipatingScopes,
        [AllowNull()][object]$ActiveSet,
        [Parameter(Mandatory = $true)][string]$AuthoredContentRoot,
        [switch]$NonInteractive
    )

    Write-Host '[WorldRealization] MUTATION PREVIEW'
    Write-Host "  operation: $Operation"
    Write-Host "  map: $MapPackage"
    Write-Host '  replaced scopes:'
    foreach ($scopeId in @($ParticipatingScopes.Keys | Sort-Object)) {
        Write-Host "    $scopeId"
        foreach ($path in @($ParticipatingScopes[$scopeId] | Sort-Object -Unique)) {
            Write-Host "      $path"
        }
    }
    Write-Host '  protected:'
    Write-Host "    $AuthoredContentRoot"
    Write-Host '    Landscape edit layer: Authored Corrections'
    Write-Host '  untouched accepted scopes:'
    $untouched = @(if ($null -eq $ActiveSet) {
        @()
    }
    else {
        @($ActiveSet.Manifests.Keys | Where-Object { -not $ParticipatingScopes.Contains($_) } | Sort-Object)
    })
    if ($untouched.Count -eq 0) {
        Write-Host '    none'
    }
    else {
        foreach ($scopeId in $untouched) {
            Write-Host "    $scopeId"
        }
    }

    if ($NonInteractive) {
        Write-Host '  confirmation: automation opt-in (-NonInteractive)'
        return
    }
    $answer = Read-Host 'Type yes to execute this mutation'
    if ($answer -ine 'yes') {
        throw 'World realization cancelled: operator did not type yes.'
    }
}

function Get-ProjectWorldGenerationHistory {
    param([Parameter(Mandatory = $true)][string]$ManifestRoot)

    $activePaths = @{}
    $activeSetPath = Join-Path $ManifestRoot 'active_set.json'
    if (Test-Path -LiteralPath $activeSetPath -PathType Leaf) {
        $activeSet = Get-Content -LiteralPath $activeSetPath -Raw | ConvertFrom-Json
        foreach ($scope in @($activeSet.scopes)) {
            $activePaths[[string]$scope.manifest_path] = $true
        }
    }

    $records = @()
    foreach ($directoryName in @('scopes', 'archive')) {
        $directory = Join-Path $ManifestRoot $directoryName
        if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
            continue
        }
        foreach ($file in @(Get-ChildItem -LiteralPath $directory -Filter '*.json' -File)) {
            $manifest = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
            $relativePath = "$directoryName/$($file.Name)"
            $acceptedAt = $manifest.PSObject.Properties['accepted_at_utc']
            $records += [pscustomobject]@{
                scope_id = [string]$manifest.scope_id
                generation = [int]$manifest.generation
                state = if ($activePaths.ContainsKey($relativePath)) { 'active' } else { 'retired' }
                accepted_at_utc = if ($null -eq $acceptedAt) { 'not_recorded' } else { [string]$acceptedAt.Value }
                originating_run = [string]$manifest.accepted_operation_id
                manifest = $relativePath
            }
        }
    }
    return @($records | Sort-Object scope_id, generation)
}
