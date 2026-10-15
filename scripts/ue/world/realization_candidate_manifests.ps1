# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
Set-StrictMode -Version Latest

function New-ProjectWorldLayerManifests {
    param([string]$ProjectRoot, [object]$ActiveSet, [string]$OperationId,
        [System.Collections.IDictionary]$Generations, [System.Collections.IDictionary]$Definitions,
        [hashtable]$CandidateInfos, [System.Collections.IDictionary]$Fingerprints,
        [System.Collections.IDictionary]$InputIdentity, [string]$MapScopeId,
        [switch]$MaterialReferencesOnly, [hashtable]$Adoptions = @{}, [switch]$PreserveMaterialAdoption)
    $candidates = @()
    foreach ($scope in $CandidateInfos.Keys) {
        $info = $CandidateInfos[$scope]
        $prior = if ($null -ne $ActiveSet -and $ActiveSet.Manifests.Contains($scope)) { $ActiveSet.Manifests[$scope] } else { $null }
        $fingerprint = [string]$Fingerprints[$scope]
        $identity = $InputIdentity
        $contract = $info.Contract
        $consumers = @($MapScopeId)
        if ($MaterialReferencesOnly) {
            if ($null -eq $prior -or -not $Adoptions.ContainsKey($scope)) { throw 'Material-only candidate lacks prior geometry authority.' }
            $fingerprint = [string]$prior.generator_fingerprint
            $identity = @{}
            foreach ($field in $prior.input_identity.PSObject.Properties) { $identity[$field.Name] = $field.Value }
            $contract = $prior.layer_contract
            $consumers = @($prior.consumer_references)
        }
        $candidate = New-ProjectWorldCandidateManifest -ProjectRoot $ProjectRoot -ScopeId $scope `
            -Generation $Generations[$scope] -OwningLayer ([string]$Definitions[$scope].layer_id) `
            -OperationId $OperationId -InputIdentity $identity -ScopePaths @() -ArtifactRecords $info.Records `
            -LayerContract $contract -ConsumerReferences $consumers -GeneratorFingerprint $fingerprint
        if ($MaterialReferencesOnly) {
            $recordsByPath = @{}
            foreach ($record in $info.Records) { $recordsByPath[[string]$record.path] = $record }
            $candidate['artifacts'] = @($prior.artifacts | ForEach-Object {
                $record = $recordsByPath[[string]$_.path]
                $copy = $_ | ConvertTo-Json -Compress | ConvertFrom-Json
                $copy.digest = [string]$record.digest
                $copy
            })
            $candidate['consumer_references'] = @($prior.consumer_references)
            $candidate['material_adoption'] = $Adoptions[$scope]
        }
        elseif ($PreserveMaterialAdoption -and $null -ne $prior.PSObject.Properties['material_adoption']) {
            $candidate['material_adoption'] = $prior.material_adoption
            Test-ProjectWorldMaterialAdoption -Manifest $candidate
        }
        $unchanged = Test-ProjectWorldManifestUnchanged -PriorManifest $prior -CandidateManifest $candidate `
            -GeneratorFingerprint $fingerprint -CompareLayerContract
        if ($MaterialReferencesOnly -and $unchanged) {
            $old = if ($prior.PSObject.Properties.Name -contains 'material_adoption') { ConvertTo-ProjectWorldSortedValue $prior.material_adoption } else { $null }
            $new = ConvertTo-ProjectWorldSortedValue $candidate.material_adoption
            $unchanged = ($old | ConvertTo-Json -Depth 24 -Compress) -ceq ($new | ConvertTo-Json -Depth 24 -Compress)
        }
        if (-not $unchanged) { $candidates += , $candidate }
    }
    return $candidates
}

function New-ProjectWorldMapManifestIfChanged {
    param([string]$ProjectRoot, [object]$ActiveSet, [string]$MapScopeId, [int]$Generation,
        [string]$OperationId, [hashtable]$LayerCandidates, [string[]]$ScopePaths,
        [System.Collections.IDictionary]$InputIdentity, [string]$GeneratorFingerprint, [switch]$SelectedApply)
    $prior = if ($null -ne $ActiveSet -and $ActiveSet.Manifests.Contains($MapScopeId)) { $ActiveSet.Manifests[$MapScopeId] } else { $null }
    $owned = Get-ProjectWorldLayerOwnedPaths -Candidates $LayerCandidates -ActiveSet $ActiveSet -MapScopeId $MapScopeId
    $records = @(Get-ProjectWorldScopeArtifactRecords -ProjectRoot $ProjectRoot -ScopePaths $ScopePaths | Where-Object { -not $owned.ContainsKey([string]$_.path) })
    $fingerprint = if ($SelectedApply) { [string]$prior.generator_fingerprint } else { $GeneratorFingerprint }
    $candidate = New-ProjectWorldCandidateManifest -ProjectRoot $ProjectRoot -ScopeId $MapScopeId `
        -Generation $Generation -OwningLayer map -OperationId $OperationId -InputIdentity $InputIdentity `
        -ScopePaths @() -ArtifactRecords $records -GeneratorFingerprint $fingerprint
    if (-not (Test-ProjectWorldManifestUnchanged -PriorManifest $prior -CandidateManifest $candidate -GeneratorFingerprint $fingerprint)) {
        if ($SelectedApply) { throw 'Selected execution changed the shared map owner.' }
        return $candidate
    }
}
