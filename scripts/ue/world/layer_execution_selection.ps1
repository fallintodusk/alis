# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Resolve-ProjectWorldExecutionSelection {
    param(
        [AllowEmptyCollection()][string[]]$ExecuteLayer = @(),
        [Parameter(Mandatory = $true)][string]$Mode,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$LayerDefinitions,
        [switch]$EnrollManifests,
        [switch]$Reconstruct,
        [switch]$MaterialReferencesOnly
    )
    if ($ExecuteLayer.Count -eq 0) { return @($LayerDefinitions.Keys) }
    if ($Mode -cne 'apply' -or $EnrollManifests -or $Reconstruct) {
        throw 'Owner selection requires incremental Apply with existing authority.'
    }
    if (@($ExecuteLayer | Sort-Object -Unique).Count -ne $ExecuteLayer.Count) {
        throw 'Execution selection contains duplicate owners.'
    }
    $scopes = @()
    foreach ($id in $ExecuteLayer) {
        $matchingScopes = @($LayerDefinitions.Keys | Where-Object {
            [string]$LayerDefinitions[$_].layer_id -ceq $id
        })
        if ($id -cnotmatch '^[a-z0-9_]+$' -or $matchingScopes.Count -ne 1) {
            throw "Unknown execution owner: $id"
        }
        foreach ($layer in $LayerDefinitions.Values) {
            if ($MaterialReferencesOnly) { continue }
            if ($id -cin @($layer.depends_on) -and [string]$layer.layer_id -cnotin $ExecuteLayer) {
                throw "Execution selection requires consumer closure: $id -> $($layer.layer_id)"
            }
        }
        $scopes += [string]$matchingScopes[0]
    }
    return @($scopes | Sort-Object)
}

function Assert-ProjectWorldSelectedAuthority {
    param(
        [Parameter(Mandatory = $true)][object]$ActiveSet,
        [Parameter(Mandatory = $true)][string]$MapScopeId,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$InputIdentity,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$LayerDefinitions,
        [Parameter(Mandatory = $true)][string]$RealizationProfileHash
    )
    if ($null -eq $ActiveSet -or -not $ActiveSet.Manifests.Contains($MapScopeId)) {
        throw 'Execution selection requires an accepted existing map.'
    }
    $prior = $ActiveSet.Manifests[$MapScopeId]
    foreach ($field in $InputIdentity.Keys) {
        if ([string]$prior.input_identity.$field -cne [string]$InputIdentity[$field]) {
            throw "Execution selection cannot change shared map input: $field"
        }
    }
    $priorScopes = @($ActiveSet.Manifests.Keys | Where-Object {
        $_ -like 'layer_*' -and $MapScopeId -cin @($ActiveSet.Manifests[$_].consumer_references)
    })
    if ($priorScopes.Count -ne $LayerDefinitions.Count) {
        throw 'Execution selection cannot add or retire layer scopes.'
    }
    foreach ($scope in $LayerDefinitions.Keys) {
        if ($scope -cnotin $priorScopes -or
            [string]$ActiveSet.Manifests[$scope].layer_contract.realization_profile_sha256 -cne $RealizationProfileHash) {
            throw "Execution selection requires the accepted full realization profile: $scope"
        }
    }
}

function Get-ProjectWorldLayerOwnedPaths {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Candidates,
        [object]$ActiveSet,
        [Parameter(Mandatory = $true)][string]$MapScopeId
    )
    $paths = @{}
    if ($null -ne $ActiveSet) {
        foreach ($scope in $ActiveSet.Manifests.Keys) {
            if ($scope -like 'layer_*' -and -not $Candidates.ContainsKey($scope) -and
                $MapScopeId -cin @($ActiveSet.Manifests[$scope].consumer_references)) {
                foreach ($record in @($ActiveSet.Manifests[$scope].artifacts)) {
                    $paths[[string]$record.path] = $true
                }
            }
        }
    }
    foreach ($scope in $Candidates.Keys) {
        foreach ($record in @($Candidates[$scope].Records)) {
            $paths[[string]$record.path] = $true
        }
    }
    return $paths
}

function Assert-ProjectWorldCarriedArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][object]$ActiveSet,
        [Parameter(Mandatory = $true)][string[]]$SelectedScopeIds,
        [hashtable]$LayerScopePaths = @{}
    )
    foreach ($scope in $ActiveSet.Manifests.Keys) {
        if ($scope -cin $SelectedScopeIds) { continue }
        foreach ($record in @($ActiveSet.Manifests[$scope].artifacts)) {
            $path = Join-Path $ProjectRoot ([string]$record.path)
            if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
                (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne [string]$record.digest) {
                throw "Selected execution changed a carried artifact: $scope/$($record.path)"
            }
        }
    }
    $carriedRoots = @{}
    foreach ($scope in $LayerScopePaths.Keys) {
        if ($scope -cnotin $SelectedScopeIds) { $carriedRoots[$scope] = $LayerScopePaths[$scope] }
    }
    if ($carriedRoots.Count -gt 0) {
        Test-ProjectWorldScopeDrift -ProjectRoot $ProjectRoot -ActiveSet $ActiveSet -ScopePathsById $carriedRoots
    }
}
