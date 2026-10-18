# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
Set-StrictMode -Version Latest

function Invoke-ProjectWorldRefinementOwner {
    param([string]$ProjectRoot,[ValidateSet('CanonicalCompilation','EndToEndValidation')][string]$Owner,
        [string[]]$Arguments,[string]$EvidenceDirectory)
    New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
    $errorPath=Join-Path $EvidenceDirectory ([guid]::NewGuid().ToString('N')+'.stderr.log')
    $python=(Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $entry=Join-Path $ProjectRoot ('tools/World/'+$Owner+'/bootstrap.py')
    $ErrorActionPreference='Continue'
    $output=@(& $python -S $entry @Arguments 2> $errorPath)
    $exitCode=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    if ($exitCode -ne 0 -or $output.Count -eq 0) {throw "Refinement $Owner refused; evidence retained at $errorPath"}
    return ($output -join "`n") | ConvertFrom-Json
}

function Invoke-ProjectWorldRefinementNativeInputs {
    param([string]$ProjectRoot,[string]$CompileResult,[hashtable]$ValidationArguments,
        [string]$EvidenceDirectory,[ValidateSet('base','effective')][string]$Label)
    $arguments=@{};foreach ($key in $ValidationArguments.Keys) {$arguments[$key]=$ValidationArguments[$key]}
    $path=Join-Path $EvidenceDirectory ($Label+'.json')
    $arguments.Mode='Validate';$arguments.CompileResult=$CompileResult;$arguments.EvidencePath=$path
    & (Join-Path $ProjectRoot 'scripts/ue/world/realize_canonical_world.ps1') @arguments | Out-Host
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $path -PathType Leaf)) {throw 'Native refinement input census refused.'}
    return Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
}

function New-ProjectWorldCanonicalRefinementOperation {
    param([string]$ProjectRoot,[string]$SelectionPath,[string]$CompileResult,[object]$ActiveSet,
        [System.Collections.IDictionary]$InputIdentity,[System.Collections.IDictionary]$LayerDefinitions,
        [System.Collections.IDictionary]$GeneratorFingerprints,[string]$MapScopeId,[string]$MapGeneratorFingerprint,
        [string]$RealizationProfileHash,[string]$CurrentPresentationProfileHash,[string[]]$ExecuteLayer,
        [hashtable]$ValidationArguments,[string]$EvidenceDirectory)
    $root=[IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\','/')
    $path=[IO.Path]::GetFullPath($SelectionPath)
    if (-not $path.StartsWith($root+'\tmp\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Refinement input selection must belong to repository tmp.'}
    $selectionHash=Get-ProjectWorldFileSha256 $path
    $selection=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    $profilePath=Join-Path $root $selection.base_selection.validation_profile_path
    $verified=Invoke-ProjectWorldRefinementOwner -ProjectRoot $root -Owner EndToEndValidation `
        -Arguments @('verify-refinement-inputs','--profile',$profilePath,'--selection',$path) -EvidenceDirectory $EvidenceDirectory
    if ([string]$verified.status -cne 'accepted' -or [string]$verified.selection_sha256 -cne $selectionHash -or
        (Get-ProjectWorldFileSha256 $path) -cne $selectionHash) {throw 'Refinement input selection changed or failed authentication.'}
    $compilePath=[IO.Path]::GetFullPath($CompileResult)
    $compileHash=Get-ProjectWorldFileSha256 $compilePath
    $matching=@($verified.candidates.PSObject.Properties | Where-Object {
        [string]$_.Value.result_sha256 -ceq $compileHash -and
        [IO.Path]::GetFullPath((Join-Path $root $_.Value.materialized_result_path)) -ceq $compilePath
    })
    if ($matching.Count -ne 1) {throw 'Refinement operation requires the exact admitted effective materialization.'}
    $name=[string]$matching[0].Name;$candidate=$matching[0].Value
    $profile=Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
    $compilerPath=Join-Path $root $profile.profiles.$name.compiler_profile_path
    $base=$selection.base_selection.profiles.$name
    $materialized=Invoke-ProjectWorldRefinementOwner -ProjectRoot $root -Owner CanonicalCompilation `
        -Arguments @('materialize','--profile',$compilerPath) -EvidenceDirectory $EvidenceDirectory
    if ([string]$materialized.status -cne 'accepted') {throw 'Refinement base materialization refused.'}
    $basePath=Join-Path $root $materialized.result_path
    if ((Get-ProjectWorldFileSha256 $basePath) -cne [string]$candidate.base_result_sha256) {throw 'Refinement base selection no longer matches.'}
    $before=Invoke-ProjectWorldRefinementNativeInputs -ProjectRoot $root -CompileResult $basePath `
        -ValidationArguments $ValidationArguments -EvidenceDirectory $EvidenceDirectory -Label base
    $after=Invoke-ProjectWorldRefinementNativeInputs -ProjectRoot $root -CompileResult $compilePath `
        -ValidationArguments $ValidationArguments -EvidenceDirectory $EvidenceDirectory -Label effective
    $proof=New-ProjectWorldNativeRefinementTransition -Before $before -After $after -ActiveSet $ActiveSet `
        -Candidate $candidate -InputSelectionSha256 $selectionHash -BaseCanonicalAuthoritySha256 $base.active_sha256 `
        -LayerDefinitions $LayerDefinitions -GeneratorFingerprints $GeneratorFingerprints -InputIdentity $InputIdentity `
        -MapScopeId $MapScopeId -MapGeneratorFingerprint $MapGeneratorFingerprint -RealizationProfileHash $RealizationProfileHash `
        -CurrentPresentationProfileHash $CurrentPresentationProfileHash -ExecuteLayer $ExecuteLayer
    if ((Get-ProjectWorldFileSha256 $path) -cne $selectionHash) {throw 'Refinement selection changed during native preflight.'}
    $ticket=Invoke-ProjectWorldRefinementOwner -ProjectRoot $root -Owner CanonicalCompilation `
        -Arguments @('prepare-selection-transition','--profile',$compilerPath,'--candidate',(Join-Path $root $candidate.active_path),
            '--expected-active-sha256',$base.active_sha256) -EvidenceDirectory $EvidenceDirectory
    if ([string]$ticket.old_active_sha256 -cne [string]$base.active_sha256 -or
        [string]$ticket.new_active_sha256 -cne [string]$candidate.active_sha256 -or
        [string]$ticket.base_result_sha256 -cne [string]$candidate.base_result_sha256 -or
        [string]$ticket.effective_result_sha256 -cne $compileHash -or
        [string]$ticket.effective_identity -cne [string]$candidate.effective_identity) {throw 'Canonical preparation returned a different transition.'}
    return [pscustomobject]@{Compatibility=($proof | ConvertTo-Json -Depth 32 | ConvertFrom-Json);Transition=$ticket;
        EffectiveInputs=$after;SelectionPath=$path;SelectionSha256=$selectionHash}
}

function Assert-ProjectWorldRefinementResult {
    param([object]$Operation,[object]$Result,[System.Collections.IDictionary]$LayerDefinitions,[string[]]$ExecuteLayer)
    if ($null -eq $Operation) {return}
    $comparison=Compare-ProjectWorldNativeLayerInputs -Before $Operation.EffectiveInputs -After $Result `
        -BaseResultSha256 $Operation.Compatibility.candidate.result_sha256 `
        -EffectiveResultSha256 $Operation.Compatibility.candidate.result_sha256 `
        -LayerDefinitions $LayerDefinitions -ExecuteLayer $ExecuteLayer
    foreach ($layer in $comparison.Layers.Values) {
        foreach ($field in @('canonical_inputs','dependency_inputs')) {
            if (@($layer.$field.changed_units).Count -gt 0) {throw 'Realized refinement inputs differ from their native preflight.'}
        }
    }
    if ($Operation.SelectionPath -and (Get-ProjectWorldFileSha256 $Operation.SelectionPath) -cne $Operation.SelectionSha256) {
        throw 'Refinement admission changed during realization.'
    }
}

function Get-ProjectWorldAcceptedRefinementOperation {
    param([string]$ProjectRoot,[object]$ActiveSet,[string]$MapScopeId,[string]$CompileResult,
        [string]$ProfileId,[string]$DataRoot,[System.Collections.IDictionary]$InputIdentity,
        [System.Collections.IDictionary]$GeneratorFingerprints,[string]$MapGeneratorFingerprint,
        [hashtable]$ValidationArguments,[string]$EvidenceDirectory,[switch]$MaterialReferencesOnly,
        [string]$PresentationProfilePath='', [string]$MaterialManifestPath='')
    if ($null -eq $ActiveSet -or $null -eq $ActiveSet.Record.PSObject.Properties['canonical_input_compatibility']) {return $null}
    $bindings=@($ActiveSet.Record.canonical_input_compatibility | Where-Object map_scope_id -CEQ $MapScopeId)
    if ($bindings.Count -eq 0) {return $null}
    if ($bindings.Count -ne 1) {throw 'Accepted map has ambiguous canonical compatibility.'}
    $proof=$bindings[0]
    Get-ProjectWorldRefinementMapIdentity -InputIdentity $InputIdentity -Compatibility $proof -MapScopeId $MapScopeId | Out-Null
    if ($ProfileId -cnotmatch '^[a-z][a-z0-9_]*$' -or
        (Get-ProjectWorldFileSha256 $CompileResult) -cne [string]$proof.candidate.result_sha256 -or
        $MapGeneratorFingerprint -cne [string]$proof.map_generator_fingerprint) {throw 'Accepted canonical/map producer selection changed; explicit reconciliation is required.'}
    foreach ($scope in $GeneratorFingerprints.Keys) {
        if ($null -eq $proof.layers.PSObject.Properties[[string]$scope] -or
            [string]$GeneratorFingerprints[$scope] -cne [string]$proof.layers.$scope.accepted.current_generator_fingerprint) {
            throw 'Accepted refinement producer changed; explicit reconciliation is required.'
        }
    }
    $compiler=Join-Path $DataRoot ('Profiles/CanonicalCompilation/'+$ProfileId+'.compile.json')
    $authority=Invoke-ProjectWorldRefinementOwner -ProjectRoot $ProjectRoot -Owner CanonicalCompilation `
        -Arguments @('authority','--profile',$compiler) -EvidenceDirectory $EvidenceDirectory
    $active=Join-Path $DataRoot ('Canonical/'+$ProfileId+'/active.json')
    if ((Get-ProjectWorldFileSha256 $active) -cne [string]$proof.candidate.active_sha256 -or
        [string]$authority.authority_id -cne [string]$proof.candidate.base_authority_id -or
        [string]$authority.compile_result_sha256 -cne [string]$proof.candidate.base_result_sha256 -or
        $null -eq $authority.PSObject.Properties['selected_refinement']) {throw 'Accepted effective canonical authority changed.'}
    $selected=$authority.selected_refinement
    foreach ($field in @('effective_identity','result_sha256','inputs_hash','implementation_sha256','source_context_identity','execution_identity')) {
        if ([string]$selected.$field -cne [string]$proof.candidate.$field) {throw 'Accepted canonical child provenance changed.'}
    }
    if ([string]$selected.bundle.sha256 -cne [string]$proof.candidate.bundle_sha256) {throw 'Accepted canonical child payload changed.'}
    if ($MaterialReferencesOnly) {
        return [pscustomobject]@{Compatibility=$proof;Transition=$null;EffectiveInputs=$null;SelectionPath='';SelectionSha256=''}
    }
    $native=Invoke-ProjectWorldRefinementNativeInputs -ProjectRoot $ProjectRoot -CompileResult $CompileResult `
        -ValidationArguments $ValidationArguments -EvidenceDirectory $EvidenceDirectory -Label effective
    if ([string]$native.status -cne 'accepted' -or [string]$native.input_sha256 -cne [string]$proof.candidate.result_sha256) {
        throw 'Accepted effective native census does not bind its selected receipt.'
    }
    if ((Get-ProjectWorldSharedNativeInputsDigest $native -GeometryOnly) -cne [string]$proof.shared_geometry_inputs_sha256) {
        throw 'Accepted refinement shared geometry contract changed.'
    }
    if ((Get-ProjectWorldSharedNativeInputsDigest $native) -cne [string]$proof.shared_native_inputs_sha256) {
        $appearance=Resolve-ProjectWorldSelectedGeometryProfile -ActiveSet $ActiveSet -MapScopeId $MapScopeId `
            -SelectedScopes @($GeneratorFingerprints.Keys) -CandidatePath $PresentationProfilePath -MaterialManifestPath $MaterialManifestPath
        if ($null -eq $appearance -or [string]$appearance.GeometryHash -cne [string]$proof.base_map_input_identity.presentation_profile_sha256) {
            throw 'Accepted refinement appearance changed without authenticated material adoption.'
        }
    }
    $seen=@{}
    foreach ($inventory in @($native.layer_inventories)) {
        $scope=[string]$inventory.scope_id
        if ($seen.ContainsKey($scope) -or $null -eq $proof.layers.PSObject.Properties[$scope]) {throw 'Accepted refinement census has duplicate or unknown owners.'}
        $seen[$scope]=$true;$row=$proof.layers.$scope
        foreach ($field in @('normalized_layer_contract_sha256','generator_id','generator_version','artifact_root')) {
            if ([string]$inventory.$field -cne [string]$row.$field) {throw 'Accepted refinement native layer contract changed.'}
        }
        foreach ($field in @('canonical_inputs','dependency_inputs')) {
            if ((Get-ProjectWorldInputSetDigest $inventory.$field) -cne [string]$row.('after_'+$field+'_sha256')) {
                throw 'Accepted effective native inputs changed; explicit reconciliation is required.'
            }
        }
    }
    if ($seen.Count -ne @($proof.layers.PSObject.Properties.Name).Count) {throw 'Accepted refinement census omits an owner.'}
    return [pscustomobject]@{Compatibility=$proof;Transition=$null;EffectiveInputs=$native;SelectionPath='';SelectionSha256=''}
}

function Get-ProjectWorldRefinementMapIdentity {
    param([System.Collections.IDictionary]$InputIdentity,[object]$Compatibility,[string]$MapScopeId)
    if ($null -eq $Compatibility) {return $InputIdentity}
    Test-ProjectWorldCanonicalCompatibilityDocument $Compatibility
    if ([string]$Compatibility.map_scope_id -cne $MapScopeId) {throw 'Canonical compatibility targets another map.'}
    foreach ($field in $InputIdentity.Keys) {
        if ([string]$InputIdentity[$field] -cne [string]$Compatibility.effective_map_input_identity.$field) {
            throw 'Canonical compatibility targets another map input.'
        }
    }
    $identity=[ordered]@{}
    foreach ($property in $Compatibility.base_map_input_identity.PSObject.Properties) {$identity[$property.Name]=$property.Value}
    return $identity
}

function Get-ProjectWorldRefinementLayerCandidate {
    param([object]$Candidate,[object]$Prior,[object]$Compatibility,[switch]$AcceptedReplay)
    if ($null -eq $Compatibility -or $null -eq $Prior) {return $Candidate}
    $scope=[string]$Candidate.scope_id
    if (-not $AcceptedReplay -and $scope -cnotin @($Compatibility.selected_scope_ids)) {throw 'Canonical refinement emitted an unselected layer candidate.'}
    $row=$Compatibility.layers.$scope
    foreach ($field in @('canonical_inputs','dependency_inputs')) {
        if ((Get-ProjectWorldInputSetDigest $Candidate.layer_contract.$field) -cne [string]$row.('after_'+$field+'_sha256')) {
            throw 'Canonical refinement candidate has unexpected consumed inputs.'
        }
    }
    $copy=$Candidate | ConvertTo-Json -Depth 32 | ConvertFrom-Json
    $copy.input_identity=$Prior.input_identity
    if (Test-ProjectWorldManifestUnchanged -PriorManifest $Prior -CandidateManifest $copy `
        -GeneratorFingerprint $Candidate.generator_fingerprint -CompareLayerContract) {return $copy}
    if ($AcceptedReplay -and $scope -cnotin @($Compatibility.selected_scope_ids)) {
        throw 'Reconstructed carried refinement owner differs from its accepted output.'
    }
    return $Candidate
}
