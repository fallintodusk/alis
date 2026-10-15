# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
Set-StrictMode -Version Latest

function Get-ProjectWorldBytesHash {
    param([byte[]]$Bytes)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $hash.Dispose() }
}

function ConvertTo-ProjectWorldSortedValue {
    param([object]$Value)
    if ($null -eq $Value) { return $null }
    if ($Value -is [System.Collections.IDictionary]) {
        $copy = [ordered]@{}
        foreach ($key in @($Value.Keys | Sort-Object)) { $copy[$key] = ConvertTo-ProjectWorldSortedValue $Value[$key] }
        return $copy
    }
    if ($Value -is [PSCustomObject]) {
        $copy = [ordered]@{}
        foreach ($key in @($Value.PSObject.Properties.Name | Sort-Object)) { $copy[$key] = ConvertTo-ProjectWorldSortedValue $Value.$key }
        return $copy
    }
    if ($Value -is [array]) { return , @($Value | ForEach-Object { ConvertTo-ProjectWorldSortedValue $_ }) }
    return $Value
}

function Get-ProjectWorldProtectedPresentation {
    param([object]$Document)
    $copy = ConvertTo-ProjectWorldSortedValue $Document
    foreach ($role in @('road', 'building', 'water')) { $copy.materials.Remove($role) }
    return ($copy | ConvertTo-Json -Depth 24 -Compress)
}

function Resolve-ProjectWorldMaterialProfile {
    param([object]$ActiveSet, [string]$MapScopeId, [string[]]$SelectedScopes,
        [string]$CandidatePath, [string]$PriorPath)
    if ($null -eq $ActiveSet -or -not $ActiveSet.Manifests.Contains($MapScopeId)) { throw 'Material-only application requires an accepted map.' }
    $expected = [string]$ActiveSet.Manifests[$MapScopeId].input_identity.presentation_profile_sha256
    $bytes = $null
    if ($PriorPath) { $bytes = [IO.File]::ReadAllBytes([IO.Path]::GetFullPath($PriorPath)) }
    foreach ($scope in $SelectedScopes) {
        if (-not $ActiveSet.Manifests.Contains($scope) -or
            [string]$ActiveSet.Manifests[$scope].input_identity.presentation_profile_sha256 -cne $expected) {
            throw 'Material-only application requires the accepted geometry profile for every selected owner.'
        }
        $property = $ActiveSet.Manifests[$scope].PSObject.Properties['material_adoption']
        if ($null -eq $bytes -and $null -ne $property) {
            $bytes = [Convert]::FromBase64String([string]$property.Value.geometry_profile_source_base64)
        }
    }
    if ($null -eq $bytes) {
        foreach ($scope in $ActiveSet.Manifests.Keys) {
            if ($scope -notlike 'layer_*') { continue }
            $manifest = $ActiveSet.Manifests[$scope]
            if ($MapScopeId -cnotin @($manifest.consumer_references) -or
                [string]$manifest.input_identity.presentation_profile_sha256 -cne $expected) { continue }
            $adopted = $manifest.PSObject.Properties['material_adoption']
            if ($null -ne $adopted) {
                $bytes = [Convert]::FromBase64String([string]$adopted.Value.geometry_profile_source_base64)
                break
            }
        }
    }
    if ($null -eq $bytes) { $bytes = [IO.File]::ReadAllBytes($CandidatePath) }
    if ((Get-ProjectWorldBytesHash $bytes) -cne $expected) { throw 'Prior presentation source fails accepted geometry-profile authentication.' }
    $old = [Text.Encoding]::UTF8.GetString($bytes).TrimStart([char]0xFEFF) | ConvertFrom-Json
    $candidate = Get-Content -LiteralPath $CandidatePath -Raw | ConvertFrom-Json
    if ((Get-ProjectWorldProtectedPresentation $old) -cne (Get-ProjectWorldProtectedPresentation $candidate)) {
        throw 'Material-only application cannot change profile identity, schema, cloud, environment or cameras.'
    }
    $changedRoles = @()
    $acceptedDeclarations = @{}
    foreach ($scope in $ActiveSet.Manifests.Keys) {
        if ($scope -notlike 'layer_*') { continue }
        $manifest = $ActiveSet.Manifests[$scope]
        if ($MapScopeId -cnotin @($manifest.consumer_references)) { continue }
        $adopted = $manifest.PSObject.Properties['material_adoption']
        if ($null -ne $adopted) {
            $role = [string]$adopted.Value.role
            $declaration = [string]$adopted.Value.declaration
            if ($acceptedDeclarations.ContainsKey($role) -and $acceptedDeclarations[$role] -cne $declaration) {
                throw 'Accepted material declarations disagree for the same map role.'
            }
            $acceptedDeclarations[$role] = $declaration
        }
    }
    foreach ($role in @('road', 'building', 'water')) {
        $a = $old.materials.PSObject.Properties[$role]
        $b = $candidate.materials.PSObject.Properties[$role]
        $before = if ($null -eq $a) { '' } else { [string]$a.Value }
        if ($acceptedDeclarations.ContainsKey($role)) { $before = $acceptedDeclarations[$role] }
        $after = if ($null -eq $b) { '' } else { [string]$b.Value }
        if ($before -cne $after) { $changedRoles += $role }
    }
    return [PSCustomObject]@{ GeometryHash = $expected; SourceBase64 = [Convert]::ToBase64String($bytes); ChangedRoles = $changedRoles }
}

function Resolve-ProjectWorldSelectedGeometryProfile {
    param([object]$ActiveSet, [string]$MapScopeId, [string[]]$SelectedScopes,
        [string]$CandidatePath, [string]$MaterialManifestPath)
    $adopted = @($ActiveSet.Manifests.Values | Where-Object {
        $null -ne $_.PSObject.Properties['material_adoption'] -and $MapScopeId -cin @($_.consumer_references)
    })
    if ($adopted.Count -eq 0) { return $null }
    $evidence = Resolve-ProjectWorldMaterialProfile -ActiveSet $ActiveSet -MapScopeId $MapScopeId `
        -SelectedScopes $SelectedScopes -CandidatePath $CandidatePath -PriorPath ''
    if ($evidence.ChangedRoles.Count -gt 0) { throw 'Selected geometry cannot change unaccepted material declarations; use reference-only migration.' }
    # Compare binding destinations before invoking a geometric writer. Native
    # resolution authenticates CURRENT bytes and loaded parents before mutation;
    # historical adoption digests must not invalidate same-path appearance tuning.
    $manifest = Get-Content -LiteralPath $MaterialManifestPath -Raw | ConvertFrom-Json
    foreach ($prior in $adopted) {
        Test-ProjectWorldMaterialAdoption -Manifest $prior
        $record = $prior.material_adoption
        $matches = @($manifest.records | Where-Object {
            $null -ne $_.PSObject.Properties['binding_id'] -and [string]$_.binding_id -ceq [string]$record.declaration
        })
        if ($matches.Count -ne 1 -or [string]$matches[0].output_object_path -cne [string]$record.resource.object_path) {
            throw 'Selected geometry refuses a missing or retargeted adopted instance; use reference-only migration.'
        }
    }
    return $evidence
}

function Get-ProjectWorldPresentationCompatibility {
    param([object]$ActiveSet, [string]$MapScopeId, [string]$CandidatePath, [string]$MaterialManifestPath)
    $map = $ActiveSet.Manifests[$MapScopeId]
    if ([string]$map.owning_layer -cne 'map') { throw 'Presentation compatibility requires an accepted map scope.' }
    $original = [string]$map.input_identity.presentation_profile_sha256
    $current = (Get-FileHash -LiteralPath $CandidatePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($original -ceq $current) { return $null }
    $selected = @($ActiveSet.Manifests.Keys | Where-Object {
        $_ -clike 'layer_*' -and [string]$ActiveSet.Manifests[$_].input_identity.map_package -ceq [string]$map.input_identity.map_package
    } | Sort-Object)
    $evidence = Resolve-ProjectWorldSelectedGeometryProfile -ActiveSet $ActiveSet -MapScopeId $MapScopeId `
        -SelectedScopes $selected -CandidatePath $CandidatePath -MaterialManifestPath $MaterialManifestPath
    if ($null -eq $evidence -or [string]$evidence.GeometryHash -cne $original) {
        throw 'Changed presentation requires authenticated material adoption for this map.'
    }
    $scopes = @($MapScopeId) + $selected
    foreach ($scope in $ActiveSet.Manifests.Keys) {
        $manifest = $ActiveSet.Manifests[$scope]
        if ([string]$manifest.owning_layer -ceq 'presentation' -and $MapScopeId -cin @($manifest.consumer_references) -and
            [string]$manifest.input_identity.presentation_profile_sha256 -ceq $original) { $scopes += $scope }
    }
    $adoptions = @($selected | ForEach-Object {
        $property = $ActiveSet.Manifests[$_].PSObject.Properties['material_adoption']
        if ($null -ne $property) {
            [ordered]@{ scope_id = $_; role = [string]$property.Value.role; declaration = [string]$property.Value.declaration;
                object_path = [string]$property.Value.resource.object_path }
        }
    })
    return [ordered]@{
        map_scope_id = $MapScopeId
        geometry_profile_sha256 = $original
        current_profile_sha256 = $current
        validated_scope_ids = @($scopes | Sort-Object -Unique)
        material_manifest_sha256 = (Get-FileHash -LiteralPath $MaterialManifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
        adoptions = $adoptions
    }
}

function Add-ProjectWorldMaterialMembership {
    param([string]$Path, [object]$ActiveSet, [string[]]$SelectedScopes,
        [AllowEmptyCollection()][string[]]$ChangedRoles = @())
    $input = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $layers = @()
    foreach ($scope in $SelectedScopes) {
        $prior = $ActiveSet.Manifests[$scope]; $semantics = @{}
        foreach ($row in $prior.layer_contract.semantic_outputs) { $semantics[[string]$row.artifact_path] = [string]$row.semantic_sha256 }
        $records = @($prior.artifacts | ForEach-Object {
            if (-not $semantics.ContainsKey([string]$_.path)) { throw 'Prior material membership lacks an accepted semantic identity.' }
            [ordered]@{ path = [string]$_.path; kind = [string]$_.kind; digest = [string]$_.digest; semantic_sha256 = $semantics[[string]$_.path] }
        })
        $layers += , ([ordered]@{ layer_id = [string]$prior.owning_layer; artifacts = $records })
    }
    $input | Add-Member -NotePropertyName material_reference_artifacts -NotePropertyValue $layers
    $input | Add-Member -NotePropertyName material_reference_changed_roles -NotePropertyValue @($ChangedRoles)
    Write-ProjectWorldJson -Path $Path -Document $input
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function New-ProjectWorldMaterialAdoption {
    param([object]$Inventory, [object]$Prior, [object]$Result, [object]$ProfileEvidence, [string]$CandidatePath)
    if (@($Inventory.artifacts).Count -ne @($Prior.artifacts).Count) { throw 'Material-only adoption changed artifact membership.' }
    $before = @($Prior.artifacts | Sort-Object path); $after = @($Inventory.artifacts | Sort-Object path)
    $semantics = @{}
    foreach ($row in $Prior.layer_contract.semantic_outputs) { $semantics[[string]$row.artifact_path] = [string]$row.semantic_sha256 }
    for ($i = 0; $i -lt $before.Count; ++$i) {
        if ([string]$before[$i].path -cne [string]$after[$i].path -or [string]$before[$i].kind -cne [string]$after[$i].kind -or
            [string]$after[$i].semantic_sha256 -cne $semantics[[string]$before[$i].path]) { throw 'Material-only adoption changed geometry membership or semantics.' }
    }
    $role = [string]$Inventory.material_reference_role
    if ($role -cnotin @('road', 'building', 'water')) { throw 'Owner did not authenticate its material role.' }
    $binding = $Result.presentation_material_bindings.PSObject.Properties[$role]
    if ($null -eq $binding) { throw 'Material-only adoption requires an authenticated ProjectMaterial binding.' }
    $profile = Get-Content -LiteralPath $CandidatePath -Raw | ConvertFrom-Json
    $declaration = [string]$profile.materials.$role
    if (-not $declaration.StartsWith($role + '.', [StringComparison]::Ordinal)) { throw 'Material adoption requires the declared logical binding.' }
    return [ordered]@{
        presentation_profile_sha256 = [string]$Result.presentation_profile_sha256
        geometry_profile_source_base64 = [string]$ProfileEvidence.SourceBase64
        role = $role
        declaration = $declaration
        resource = $binding.Value
    }
}

function Test-ProjectWorldMaterialAdoption {
    param([object]$Manifest)
    if ($Manifest -is [System.Collections.IDictionary]) {
        if (-not $Manifest.Contains('material_adoption')) { return }
        $record = $Manifest['material_adoption']
    } else {
        $property = $Manifest.PSObject.Properties['material_adoption']
        if ($null -eq $property) { return }
        $record = $property.Value
    }
    if ([string]$Manifest.scope_id -notlike 'layer_*' -or [string]$record.role -cnotin @('road', 'building', 'water') -or
        [string]$record.presentation_profile_sha256 -cnotmatch '^[a-f0-9]{64}$') { throw 'Invalid material-adoption provenance.' }
    if (-not ([string]$record.declaration).StartsWith(([string]$record.role + '.'), [StringComparison]::Ordinal)) { throw 'Invalid adopted material declaration.' }
    $bytes = [Convert]::FromBase64String([string]$record.geometry_profile_source_base64)
    if ((Get-ProjectWorldBytesHash $bytes) -cne [string]$Manifest.input_identity.presentation_profile_sha256) { throw 'Material adoption lost authenticated geometry-profile bytes.' }
    foreach ($field in @('package_sha256', 'semantic_identity', 'manifest_sha256')) {
        if ([string]$record.resource.$field -cnotmatch '^[a-f0-9]{64}$') { throw 'Invalid material resource adoption identity.' }
    }
    if ([string]$record.resource.object_path -cnotmatch '^/ProjectMaterial/Surfaces/.+\..+$') { throw 'Material adoption escapes ProjectMaterial ownership.' }
}
