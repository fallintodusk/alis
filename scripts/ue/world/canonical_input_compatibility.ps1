# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'layer_input_compatibility.ps1')

function Test-ProjectWorldCanonicalCompatibilityDocument {
    param([object]$Compatibility)
    $record = $Compatibility | ConvertTo-Json -Depth 24 | ConvertFrom-Json
    Assert-ProjectWorldDescriptorFields $record @('kind', 'prior_active_set_sha256', 'base_canonical_authority_sha256',
        'map_scope_id', 'accepted_map', 'map_generator_fingerprint', 'base_map_input_identity',
        'effective_map_input_identity', 'realization_profile_sha256', 'input_selection_sha256',
        'selected_scope_ids', 'candidate', 'shared_native_inputs_sha256', 'shared_geometry_inputs_sha256', 'layers') @() 'canonical input compatibility'
    if ([string]$record.kind -cne 'feature_refinement_inputs_v1' -or [string]$record.map_scope_id -cnotmatch '^map_[a-z0-9_]+$') {
        throw 'Unsupported canonical input compatibility kind or map owner.'
    }
    foreach ($field in @('prior_active_set_sha256', 'base_canonical_authority_sha256', 'map_generator_fingerprint',
            'realization_profile_sha256', 'input_selection_sha256', 'shared_native_inputs_sha256', 'shared_geometry_inputs_sha256')) {
        if ([string]$record.$field -cnotmatch '^[a-f0-9]{64}$') { throw "Invalid canonical compatibility pin: $field" }
    }
    Assert-ProjectWorldDescriptorFields $record.candidate @('base_authority_id', 'base_result_sha256', 'result_sha256',
        'active_sha256', 'effective_identity', 'bundle_sha256', 'inputs_hash', 'implementation_sha256',
        'source_context_identity', 'execution_identity') @() 'canonical compatibility candidate'
    if ([string]::IsNullOrWhiteSpace([string]$record.candidate.base_authority_id)) { throw 'Canonical compatibility has no base authority.' }
    foreach ($property in $record.candidate.PSObject.Properties | Where-Object Name -ne base_authority_id) {
        if ([string]$property.Value -cnotmatch '^[a-f0-9]{64}$') { throw 'Invalid canonical candidate identity.' }
    }
    Assert-ProjectWorldDescriptorFields $record.accepted_map @('scope_id','manifest_path','manifest_sha256') @() 'canonical compatibility map'
    if ([string]$record.accepted_map.scope_id -cne [string]$record.map_scope_id -or
        [string]$record.accepted_map.manifest_path -cnotmatch ('^scopes/' + $record.map_scope_id + '\.[0-9]+\.json$') -or
        [string]$record.accepted_map.manifest_sha256 -cnotmatch '^[a-f0-9]{64}$') { throw 'Invalid canonical compatibility map pin.' }
    foreach ($identity in @($record.base_map_input_identity, $record.effective_map_input_identity)) {
        Assert-ProjectWorldDescriptorFields $identity @('compile_result_sha256', 'presentation_profile_sha256',
            'runtime_profile_sha256', 'authored_overlay_profile_sha256', 'map_package') @() 'canonical compatibility map inputs'
        foreach ($field in @('compile_result_sha256', 'presentation_profile_sha256', 'authored_overlay_profile_sha256')) {
            if ([string]$identity.$field -cnotmatch '^[a-f0-9]{64}$') { throw 'Invalid canonical compatibility map input pin.' }
        }
        if ([string]$identity.runtime_profile_sha256 -cnotmatch '^([a-f0-9]{64}|none)$') { throw 'Invalid canonical compatibility runtime pin.' }
    }
    if ([string]$record.base_map_input_identity.compile_result_sha256 -cne [string]$record.candidate.base_result_sha256 -or
        [string]$record.effective_map_input_identity.compile_result_sha256 -cne [string]$record.candidate.result_sha256) {
        throw 'Canonical compatibility lost base/effective lineage.'
    }
    foreach ($field in @('presentation_profile_sha256','runtime_profile_sha256','authored_overlay_profile_sha256','map_package')) {
        if ([string]$record.base_map_input_identity.$field -cne [string]$record.effective_map_input_identity.$field) {
            throw 'Canonical compatibility changes shared map inputs.'
        }
    }
    $layers = @($record.layers.PSObject.Properties.Name)
    $selected = @($record.selected_scope_ids)
    if ($layers.Count -eq 0 -or $selected.Count -eq 0 -or @($selected | Sort-Object -Unique).Count -ne $selected.Count -or
        @($selected | Where-Object { $_ -cnotin $layers }).Count -gt 0) { throw 'Invalid canonical compatibility owner selection.' }
    foreach ($scope in $layers) {
        if ([string]$scope -cnotmatch '^layer_[a-z0-9_]+$') { throw 'Invalid canonical compatibility layer owner.' }
        $layer = $record.layers.$scope
        Assert-ProjectWorldDescriptorFields $layer @('accepted','normalized_layer_contract_sha256','generator_id','generator_version',
            'artifact_root','before_canonical_inputs_sha256','after_canonical_inputs_sha256','before_dependency_inputs_sha256',
            'after_dependency_inputs_sha256','changes') @() 'canonical compatibility layer'
        Assert-ProjectWorldDescriptorFields $layer.accepted @('accepted_manifest_path','accepted_manifest_sha256',
            'accepted_generator_fingerprint','current_generator_fingerprint') @() 'canonical compatibility accepted owner'
        if ([string]$layer.accepted.accepted_manifest_path -cnotmatch ('^scopes/' + $scope + '\.[0-9]+\.json$')) {
            throw 'Invalid canonical compatibility prior manifest path.'
        }
        foreach ($field in @('accepted_manifest_sha256','accepted_generator_fingerprint','current_generator_fingerprint')) {
            if ([string]$layer.accepted.$field -cnotmatch '^[a-f0-9]{64}$') { throw 'Invalid canonical compatibility owner pin.' }
        }
        foreach ($field in @('normalized_layer_contract_sha256','before_canonical_inputs_sha256','after_canonical_inputs_sha256',
                'before_dependency_inputs_sha256','after_dependency_inputs_sha256')) {
            if ([string]$layer.$field -cnotmatch '^[a-f0-9]{64}$') { throw 'Invalid canonical compatibility input pin.' }
        }
        Assert-ProjectWorldDescriptorFields $layer.changes @('canonical_inputs','dependency_inputs') @() 'canonical compatibility changes'
        foreach ($field in @('canonical_inputs','dependency_inputs')) {
            Assert-ProjectWorldDescriptorFields $layer.changes.$field @('before_count','after_count','changed_units') @() 'canonical compatibility changed units'
        }
    }
}

function Test-ProjectWorldCanonicalCompatibilitySet {
    param([AllowEmptyCollection()][object[]]$Compatibilities = @(),
        [System.Collections.IDictionary]$Manifests, [string]$ManifestRoot)
    $seen = @{}
    foreach ($value in $Compatibilities) {
        Test-ProjectWorldCanonicalCompatibilityDocument -Compatibility $value
        $record = $value | ConvertTo-Json -Depth 24 | ConvertFrom-Json
        $mapId = [string]$record.map_scope_id
        if ($seen.ContainsKey($mapId) -or -not $Manifests.Contains($mapId)) { throw 'Duplicate or missing compatibility map.' }
        $seen[$mapId] = $true
        $map = $Manifests[$mapId]
        $mapPath = Join-Path $ManifestRoot ([string]$record.accepted_map.manifest_path)
        if (-not (Test-Path -LiteralPath $mapPath -PathType Leaf) -or
            (Get-ProjectWorldFileSha256 $mapPath) -cne [string]$record.accepted_map.manifest_sha256 -or
            (ConvertTo-ProjectWorldSortedValue $map | ConvertTo-Json -Depth 16 -Compress) -cne
            (ConvertTo-ProjectWorldSortedValue (Get-Content $mapPath -Raw | ConvertFrom-Json) | ConvertTo-Json -Depth 16 -Compress) -or
            [string]$map.generator_fingerprint -cne [string]$record.map_generator_fingerprint) {
            throw 'Canonical compatibility changed or lost its carried map authority.'
        }
        foreach ($field in $record.base_map_input_identity.PSObject.Properties.Name) {
            if ([string]$map.input_identity.$field -cne [string]$record.base_map_input_identity.$field) { throw 'Canonical compatibility map lineage differs.' }
        }
        $scopes = @($Manifests.Keys | Where-Object { $_ -clike 'layer_*' -and $mapId -cin @($Manifests[$_].consumer_references) } | Sort-Object)
        if (($scopes -join ',') -cne ((@($record.layers.PSObject.Properties.Name) | Sort-Object) -join ',')) {
            throw 'Canonical compatibility does not cover the actual map owners.'
        }
        foreach ($scope in $scopes) {
            $row = $record.layers.$scope
            if ($scope -cnotin @($record.selected_scope_ids) -and
                [string]$row.accepted.accepted_generator_fingerprint -cne [string]$row.accepted.current_generator_fingerprint) {
                throw 'Unselected producer fingerprint changed in canonical compatibility.'
            }
            $priorPath = Join-Path $ManifestRoot ([string]$row.accepted.accepted_manifest_path)
            if (-not (Test-Path -LiteralPath $priorPath -PathType Leaf) -or
                (Get-ProjectWorldFileSha256 $priorPath) -cne [string]$row.accepted.accepted_manifest_sha256) {
                throw 'Canonical compatibility prior owner evidence changed or is missing.'
            }
            $prior = Get-Content $priorPath -Raw | ConvertFrom-Json
            $current = $Manifests[$scope]
            if ([string]$prior.scope_id -cne $scope -or [string]$prior.input_identity.compile_result_sha256 -cne [string]$record.candidate.base_result_sha256 -or
                [string]$prior.generator_fingerprint -cne [string]$row.accepted.accepted_generator_fingerprint -or
                [string]$current.generator_fingerprint -cne [string]$row.accepted.current_generator_fingerprint -or
                [string]$current.input_identity.compile_result_sha256 -cnotin @([string]$record.candidate.base_result_sha256, [string]$record.candidate.result_sha256)) {
                throw 'Canonical compatibility owner lineage or producer differs.'
            }
            foreach ($field in @('normalized_layer_contract_sha256','generator_id','generator_version','artifact_root')) {
                if ([string]$prior.layer_contract.$field -cne [string]$row.$field -or
                    [string]$current.layer_contract.$field -cne [string]$row.$field) { throw 'Canonical compatibility layer contract differs.' }
            }
            if ([string]$prior.layer_contract.realization_profile_sha256 -cne [string]$record.realization_profile_sha256 -or
                [string]$current.layer_contract.realization_profile_sha256 -cne [string]$record.realization_profile_sha256) { throw 'Canonical compatibility realization profile differs.' }
            foreach ($owner in @($prior,$current)) {
                foreach ($field in @('map_package','presentation_profile_sha256','authored_overlay_profile_sha256')) {
                    if ([string]$owner.input_identity.$field -cne [string]$record.base_map_input_identity.$field) { throw 'Canonical compatibility owner shared inputs differ.' }
                }
                if ([string]$owner.input_identity.runtime_profile_sha256 -cne 'none') { throw 'Canonical compatibility layer cannot own runtime policy.' }
            }
            foreach ($field in @('canonical_inputs','dependency_inputs')) {
                $before = @($prior.layer_contract.$field); $after = @($current.layer_contract.$field)
                if ((Get-ProjectWorldInputSetDigest $before) -cne [string]$row.('before_'+$field+'_sha256') -or
                    (Get-ProjectWorldInputSetDigest $after) -cne [string]$row.('after_'+$field+'_sha256')) {
                    throw "Canonical compatibility consumed $field differ."
                }
                $old = Get-ProjectWorldUniqueInputHashes $before; $new = Get-ProjectWorldUniqueInputHashes $after
                $changed = @(@($old.Keys) + @($new.Keys) | Sort-Object -Unique | Where-Object { [string]$old[$_] -cne [string]$new[$_] })
                if ([int]$row.changes.$field.before_count -ne $old.Count -or [int]$row.changes.$field.after_count -ne $new.Count -or
                    (($changed | Sort-Object) -join ',') -cne ((@($row.changes.$field.changed_units) | Sort-Object) -join ',') -or
                    ($scope -cnotin @($record.selected_scope_ids) -and $changed.Count -gt 0) -or
                    ([string]$current.input_identity.compile_result_sha256 -ceq [string]$record.candidate.base_result_sha256 -and $changed.Count -gt 0)) {
                    throw 'Canonical compatibility changed-unit evidence or carried lineage differs.'
                }
            }
        }
    }
}

function Get-ProjectWorldCanonicalCompatibilities {
    param([object]$PriorActiveSet, [object]$Replacement = $null, [string[]]$RetiredScopeIds = @())
    $records = [ordered]@{}
    if ($null -ne $PriorActiveSet -and $null -ne $PriorActiveSet.Record.PSObject.Properties['canonical_input_compatibility']) {
        foreach ($record in @($PriorActiveSet.Record.canonical_input_compatibility)) { $records[[string]$record.map_scope_id] = $record }
    }
    if ($null -ne $Replacement) {
        Test-ProjectWorldCanonicalCompatibilityDocument $Replacement
        $records[[string]$Replacement.map_scope_id] = $Replacement
    }
    return @($records.Keys | Sort-Object | Where-Object { $_ -cnotin $RetiredScopeIds } | ForEach-Object { $records[$_] })
}
