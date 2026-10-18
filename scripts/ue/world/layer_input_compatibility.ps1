# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Get-ProjectWorldNativeSharedInputFields {
    return @('world_data_plugin', 'grid_id', 'canonical_crs', 'coordinate_transform', 'map_package',
        'presentation_profile_sha256', 'runtime_profile_sha256', 'authored_overlay_set_sha256',
        'realization_profile_sha256', 'vertical_origin_m', 'lattice_origin_m',
        'engine_georeference_origin_m', 'sample_spacing_m', 'canonical_cell_count',
        'presentation_material_bindings', 'terrain_material_semantic_identity', 'authored_anchor_resolutions')
}

function Get-ProjectWorldSharedNativeInputsDigest {
    param([object]$Inputs,[switch]$GeometryOnly)
    $shared=[ordered]@{}
    foreach ($field in Get-ProjectWorldNativeSharedInputFields) {
        if ($null -eq $Inputs.PSObject.Properties[$field]) {throw "Native shared input is missing: $field"}
        if ($GeometryOnly -and $field -cin @('presentation_profile_sha256','presentation_material_bindings','terrain_material_semantic_identity')) {continue}
        $shared[$field]=$Inputs.$field
    }
    return Get-ProjectWorldBytesHash -Bytes ([Text.Encoding]::UTF8.GetBytes(
        ((ConvertTo-ProjectWorldSortedValue $shared) | ConvertTo-Json -Depth 32 -Compress)))
}

function Get-ProjectWorldUniqueInputHashes {
    param([AllowEmptyCollection()][object[]]$Inputs = @())
    $hashes = @{}
    foreach ($unitInput in $Inputs) {
        $id = [string]$unitInput.unit_id
        $hash = [string]$unitInput.sha256
        if ([string]::IsNullOrWhiteSpace($id) -or $hash -cnotmatch '^[a-f0-9]{64}$' -or $hashes.ContainsKey($id)) {
            throw 'Native input census has a missing, duplicated or invalid unit identity.'
        }
        $hashes[$id] = $hash
    }
    return ,$hashes
}

function Get-ProjectWorldInputSetDigest {
    param([AllowEmptyCollection()][object[]]$Inputs = @())
    $hashes = Get-ProjectWorldUniqueInputHashes -Inputs $Inputs
    $rows = @($hashes.Keys | Sort-Object | ForEach-Object { [ordered]@{unit_id=$_;sha256=$hashes[$_]} })
    return Get-ProjectWorldBytesHash -Bytes ([Text.Encoding]::UTF8.GetBytes((ConvertTo-Json -InputObject $rows -Compress)))
}

function Compare-ProjectWorldNativeLayerInputs {
    param(
        [Parameter(Mandatory = $true)][object]$Before,
        [Parameter(Mandatory = $true)][object]$After,
        [Parameter(Mandatory = $true)][string]$BaseResultSha256,
        [Parameter(Mandatory = $true)][string]$EffectiveResultSha256,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$LayerDefinitions,
        [Parameter(Mandatory = $true)][string[]]$ExecuteLayer
    )
    if ($BaseResultSha256 -cnotmatch '^[a-f0-9]{64}$' -or $EffectiveResultSha256 -cnotmatch '^[a-f0-9]{64}$' -or
        [string]$Before.input_sha256 -cne $BaseResultSha256 -or [string]$After.input_sha256 -cne $EffectiveResultSha256) {
        throw 'Native input census does not bind the requested base and effective receipts.'
    }
    foreach ($document in @($Before, $After)) {
        if ([string]$document.status -cne 'accepted' -or [string]$document.operation -cne 'unreal_world_realization') {
            throw 'Native input census requires accepted realization evidence.'
        }
    }
    foreach ($field in Get-ProjectWorldNativeSharedInputFields) {
        if ($null -eq $Before.PSObject.Properties[$field] -or $null -eq $After.PSObject.Properties[$field] -or
            ($Before.$field | ConvertTo-Json -Depth 32 -Compress) -cne ($After.$field | ConvertTo-Json -Depth 32 -Compress)) {
            throw "Native input census changed a shared contract: $field"
        }
    }
    $scopes = @(Resolve-ProjectWorldExecutionSelection -ExecuteLayer $ExecuteLayer -Mode apply -LayerDefinitions $LayerDefinitions)
    $expected = @($LayerDefinitions.Values | ForEach-Object { [string]$_.layer_id } | Sort-Object)
    $snapshots = @()
    foreach ($document in @($Before, $After)) {
        $layers = @{}
        foreach ($layer in @($document.layer_inventories)) {
            $id = [string]$layer.layer_id
            if ($id -cnotin $expected -or $layers.ContainsKey($id)) { throw 'Native layer census contains unknown or duplicate owners.' }
            $layers[$id] = $layer
        }
        if ($layers.Count -ne $expected.Count) { throw 'Native layer census omits an owner.' }
        $snapshots += ,$layers
    }
    $comparison = [ordered]@{}
    foreach ($id in $expected) {
        $prior, $next = $snapshots[0][$id], $snapshots[1][$id]
        foreach ($field in @('scope_id', 'generator_id', 'generator_version', 'normalized_layer_contract_sha256', 'artifact_root')) {
            if ([string]$prior.$field -cne [string]$next.$field) { throw "Native layer contract changed: $id/$field" }
        }
        $scope = @($LayerDefinitions.Keys | Where-Object { [string]$LayerDefinitions[$_].layer_id -ceq $id })[0]
        $definition = $LayerDefinitions[$scope]
        if ([string]$prior.scope_id -cne [string]$scope -or [string]$prior.generator_id -cne [string]$definition.generator_id -or
            [int]$prior.generator_version -ne [int]$definition.generator_version) { throw "Native census differs from the selected layer owner: $id" }
        $changes = [ordered]@{}
        foreach ($field in @('canonical_inputs', 'dependency_inputs')) {
            $old = Get-ProjectWorldUniqueInputHashes -Inputs @($prior.$field)
            $new = Get-ProjectWorldUniqueInputHashes -Inputs @($next.$field)
            $changed = @(@($old.Keys) + @($new.Keys) | Sort-Object -Unique | Where-Object { [string]$old[$_] -cne [string]$new[$_] })
            if ($scope -cnotin $scopes -and $changed.Count -gt 0) { throw "Unselected layer inputs changed: $id/$field" }
            $changes[$field] = [ordered]@{ before_count = $old.Count; after_count = $new.Count; changed_units = $changed }
        }
        $comparison[$id] = $changes
    }
    return [pscustomobject]@{ Layers = $comparison; SelectedScopeIds = $scopes }
}

function New-ProjectWorldNativeRefinementTransition {
    param(
        [Parameter(Mandatory = $true)][object]$Before,
        [Parameter(Mandatory = $true)][object]$After,
        [Parameter(Mandatory = $true)][object]$ActiveSet,
        [Parameter(Mandatory = $true)][object]$Candidate,
        [Parameter(Mandatory = $true)][string]$InputSelectionSha256,
        [Parameter(Mandatory = $true)][string]$BaseCanonicalAuthoritySha256,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$LayerDefinitions,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$GeneratorFingerprints,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$InputIdentity,
        [Parameter(Mandatory = $true)][string]$MapScopeId,
        [Parameter(Mandatory = $true)][string]$MapGeneratorFingerprint,
        [Parameter(Mandatory = $true)][string]$RealizationProfileHash,
        [Parameter(Mandatory = $true)][string]$CurrentPresentationProfileHash,
        [Parameter(Mandatory = $true)][string[]]$ExecuteLayer
    )
    # This is native input compatibility, not producing acceptance or activation.
    if ($InputSelectionSha256 -cnotmatch '^[a-f0-9]{64}$' -or $BaseCanonicalAuthoritySha256 -cnotmatch '^[a-f0-9]{64}$') {
        throw 'Native transition requires authenticated input-selection and base-authority pins.'
    }
    foreach ($field in @('base_result_sha256', 'result_sha256', 'active_sha256', 'effective_identity',
            'bundle_sha256', 'inputs_hash', 'implementation_sha256', 'source_context_identity', 'execution_identity')) {
        if ([string]$Candidate.$field -cnotmatch '^[a-f0-9]{64}$') { throw "Refinement candidate lacks a pinned $field" }
    }
    if ([string]$InputIdentity.compile_result_sha256 -cne [string]$Candidate.result_sha256) {
        throw 'Native transition targets another effective receipt.'
    }
    $baseIdentity = [ordered]@{}
    foreach ($field in $InputIdentity.Keys) { $baseIdentity[$field] = $InputIdentity[$field] }
    $baseIdentity.compile_result_sha256 = [string]$Candidate.base_result_sha256
    Assert-ProjectWorldSelectedAuthority -ActiveSet $ActiveSet -MapScopeId $MapScopeId `
        -InputIdentity $baseIdentity -LayerDefinitions $LayerDefinitions -RealizationProfileHash $RealizationProfileHash
    if ($MapGeneratorFingerprint -cnotmatch '^[a-f0-9]{64}$' -or
        $MapGeneratorFingerprint -cne [string]$ActiveSet.Manifests[$MapScopeId].generator_fingerprint) {
        throw 'Native transition cannot carry a changed map producer.'
    }
    foreach ($binding in @(
            @('map_package', [string]$InputIdentity.map_package),
            @('runtime_profile_sha256', [string]$InputIdentity.runtime_profile_sha256),
            @('authored_overlay_set_sha256', [string]$InputIdentity.authored_overlay_profile_sha256),
            @('presentation_profile_sha256', $CurrentPresentationProfileHash),
            @('realization_profile_sha256', $RealizationProfileHash))) {
        if ([string]$Before.($binding[0]) -cne [string]$binding[1]) {
            throw "Native transition does not consume the selected map/profile: $($binding[0])"
        }
    }
    $comparison = Compare-ProjectWorldNativeLayerInputs -Before $Before -After $After `
        -BaseResultSha256 $Candidate.base_result_sha256 -EffectiveResultSha256 $Candidate.result_sha256 `
        -LayerDefinitions $LayerDefinitions -ExecuteLayer $ExecuteLayer
    $accepted = Compare-ProjectWorldAcceptedLayerInputs -Before $Before -ActiveSet $ActiveSet `
        -GeneratorFingerprints $GeneratorFingerprints -BaseResultSha256 $Candidate.base_result_sha256 `
        -RealizationProfileHash $RealizationProfileHash -SelectedScopeIds $comparison.SelectedScopeIds
    $mapEntry = @($ActiveSet.Record.scopes | Where-Object { [string]$_.scope_id -ceq $MapScopeId })
    if ($mapEntry.Count -ne 1 -or [string]$mapEntry[0].manifest_sha256 -cnotmatch '^[a-f0-9]{64}$') {
        throw 'Native transition lacks the accepted map manifest pin.'
    }
    $layers = [ordered]@{}
    foreach ($scope in $LayerDefinitions.Keys) {
        $prior = @($Before.layer_inventories | Where-Object { [string]$_.scope_id -ceq $scope })[0]
        $next = @($After.layer_inventories | Where-Object { [string]$_.scope_id -ceq $scope })[0]
        $layers[$scope] = [ordered]@{
            accepted = $accepted[$scope]
            normalized_layer_contract_sha256 = [string]$prior.normalized_layer_contract_sha256
            generator_id = [string]$prior.generator_id; generator_version = [int]$prior.generator_version
            artifact_root = [string]$prior.artifact_root
            before_canonical_inputs_sha256 = Get-ProjectWorldInputSetDigest -Inputs @($prior.canonical_inputs)
            after_canonical_inputs_sha256 = Get-ProjectWorldInputSetDigest -Inputs @($next.canonical_inputs)
            before_dependency_inputs_sha256 = Get-ProjectWorldInputSetDigest -Inputs @($prior.dependency_inputs)
            after_dependency_inputs_sha256 = Get-ProjectWorldInputSetDigest -Inputs @($next.dependency_inputs)
            changes = $comparison.Layers[[string]$prior.layer_id]
        }
    }
    $child = [ordered]@{base_authority_id = [string]$Candidate.base_authority_id}
    if ([string]::IsNullOrWhiteSpace($child.base_authority_id)) { throw 'Native transition lacks base authority identity.' }
    foreach ($field in @('base_result_sha256', 'result_sha256', 'active_sha256', 'effective_identity',
            'bundle_sha256', 'inputs_hash', 'implementation_sha256', 'source_context_identity', 'execution_identity')) {
        $child[$field] = [string]$Candidate.$field
    }
    return [ordered]@{
        kind = 'feature_refinement_inputs_v1'; prior_active_set_sha256 = [string]$ActiveSet.Sha256
        base_canonical_authority_sha256 = $BaseCanonicalAuthoritySha256
        map_scope_id = $MapScopeId; accepted_map = $mapEntry[0]; map_generator_fingerprint = $MapGeneratorFingerprint
        base_map_input_identity = $baseIdentity; effective_map_input_identity = $InputIdentity
        realization_profile_sha256 = $RealizationProfileHash; input_selection_sha256 = $InputSelectionSha256
        selected_scope_ids = @($comparison.SelectedScopeIds); candidate = $child
        shared_native_inputs_sha256 = Get-ProjectWorldSharedNativeInputsDigest $Before
        shared_geometry_inputs_sha256 = Get-ProjectWorldSharedNativeInputsDigest $Before -GeometryOnly
        layers = $layers
    }
}

function Compare-ProjectWorldAcceptedLayerInputs {
    param(
        [Parameter(Mandatory = $true)][object]$Before,
        [Parameter(Mandatory = $true)][object]$ActiveSet,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$GeneratorFingerprints,
        [Parameter(Mandatory = $true)][string]$BaseResultSha256,
        [Parameter(Mandatory = $true)][string]$RealizationProfileHash,
        [Parameter(Mandatory = $true)][string[]]$SelectedScopeIds
    )
    if ([string]$ActiveSet.Sha256 -cnotmatch '^[a-f0-9]{64}$' -or
        $BaseResultSha256 -cnotmatch '^[a-f0-9]{64}$' -or $RealizationProfileHash -cnotmatch '^[a-f0-9]{64}$') {
        throw 'Accepted native comparison requires pinned authority, base and profile identities.'
    }
    $records = [ordered]@{}
    foreach ($layer in @($Before.layer_inventories)) {
        $scope = [string]$layer.scope_id
        if ($records.Contains($scope) -or -not $ActiveSet.Manifests.Contains($scope)) {
            throw "Native baseline has a duplicate or missing accepted owner: $scope"
        }
        $prior = $ActiveSet.Manifests[$scope]
        if ([string]$prior.input_identity.compile_result_sha256 -cne $BaseResultSha256) {
            throw "Accepted owner does not have the requested base lineage: $scope"
        }
        foreach ($field in @('normalized_layer_contract_sha256', 'generator_id', 'generator_version', 'artifact_root')) {
            if ([string]$prior.layer_contract.$field -cne [string]$layer.$field) {
                throw "Native baseline differs from accepted layer contract: $scope/$field"
            }
        }
        if ([string]$prior.layer_contract.realization_profile_sha256 -cne $RealizationProfileHash) {
            throw "Native baseline differs from accepted layer contract: $scope/realization_profile_sha256"
        }
        foreach ($field in @('canonical_inputs', 'dependency_inputs')) {
            $accepted = Get-ProjectWorldUniqueInputHashes -Inputs @($prior.layer_contract.$field)
            $native = Get-ProjectWorldUniqueInputHashes -Inputs @($layer.$field)
            $different = @(@($accepted.Keys) + @($native.Keys) | Sort-Object -Unique |
                Where-Object { [string]$accepted[$_] -cne [string]$native[$_] })
            if ($different.Count -gt 0) { throw "Native baseline differs from accepted ${field}: $scope" }
        }
        $fingerprint = [string]$GeneratorFingerprints[$scope]
        if ($fingerprint -cnotmatch '^[a-f0-9]{64}$' -or [string]$prior.generator_fingerprint -cnotmatch '^[a-f0-9]{64}$') {
            throw "Missing or invalid producer fingerprint: $scope"
        }
        if ($scope -cnotin $SelectedScopeIds -and $fingerprint -cne [string]$prior.generator_fingerprint) {
            throw "Unselected producer fingerprint changed: $scope"
        }
        $entry = @($ActiveSet.Record.scopes | Where-Object { [string]$_.scope_id -ceq $scope })
        if ($entry.Count -ne 1 -or [string]$entry[0].manifest_sha256 -cnotmatch '^[a-f0-9]{64}$') {
            throw "Missing, duplicated or invalid accepted manifest pin: $scope"
        }
        $records[$scope] = [ordered]@{
            accepted_manifest_path = [string]$entry[0].manifest_path
            accepted_manifest_sha256 = [string]$entry[0].manifest_sha256
            accepted_generator_fingerprint = [string]$prior.generator_fingerprint
            current_generator_fingerprint = $fingerprint
        }
    }
    return ,$records
}
