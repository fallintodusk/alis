# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
# Shared accepted base/child lineage fixture for compatibility and recovery.
function New-CompatibilityFixture {
    param([string]$ProjectRoot,[string]$ManifestRoot,[string]$Prefix='fixture',[object]$PriorActiveSet=$null,
        [string]$MapScope='', [hashtable]$LayerArtifactPaths=@{},[string]$PresentationProfileHash=('b'*64))
    if (-not $MapScope) {$MapScope='map_'+$Prefix}
    $map='/ProjectWorldTestData/Generated/Representative/L_'+$Prefix
    $baseIdentity=@{compile_result_sha256=('a'*64);presentation_profile_sha256=$PresentationProfileHash;runtime_profile_sha256=('c'*64);
        authored_overlay_profile_sha256=('d'*64);map_package=$map}
    $mapArgs=@{ProjectRoot=$ProjectRoot;ScopeId=$mapScope;Generation=1;OwningLayer='map';OperationId='base';
        InputIdentity=$baseIdentity;ScopePaths=@();ArtifactRecords=@();GeneratorFingerprint=('f'*64)}
    $manifests=@((New-ProjectWorldCandidateManifest @mapArgs)); $definitions=[ordered]@{}; $fingerprints=[ordered]@{}
    $before=[pscustomobject]@{status='accepted';operation='unreal_world_realization';input_sha256=('a'*64);layer_inventories=@()}
    foreach ($field in Get-ProjectWorldNativeSharedInputFields) { $before | Add-Member -NotePropertyName $field -NotePropertyValue 'same' }
    $before.map_package=$map; $before.presentation_profile_sha256=$PresentationProfileHash; $before.runtime_profile_sha256='c'*64
    $before.authored_overlay_set_sha256='d'*64; $before.realization_profile_sha256='2'*64
    foreach ($id in @('terrain','roads','water','vegetation','buildings')) {
        $scope='layer_'+$Prefix+'_'+$id
        $depends=if ($id -eq 'vegetation') {@('roads','water')} elseif ($id -eq 'terrain') {@()} else {@('terrain')}
        $definitions[$scope]=@{layer_id=$id;generator_id=$id;generator_version=1;depends_on=$depends}
        $contract=@{realization_profile_id=$Prefix;realization_profile_sha256=('2'*64);normalized_layer_contract_sha256=('3'*64);
            generator_id=$id;generator_version=1;artifact_root='/ProjectWorldTestData/Generated/'+$Prefix+'/'+$id+'/';
            canonical_inputs=@(@{unit_id='cell';sha256=('4'*64)});dependency_inputs=@();final_dirty_units=@();semantic_outputs=@()}
        $artifacts=@(if ($LayerArtifactPaths.ContainsKey($id)) {
            Get-ProjectWorldScopeArtifactRecords -ProjectRoot $ProjectRoot -ScopePaths $LayerArtifactPaths[$id]})
        $contract.semantic_outputs=@($artifacts | ForEach-Object {@{artifact_path=$_.path;semantic_sha256=('6'*64)}})
        $layerArgs=@{ProjectRoot=$ProjectRoot;ScopeId=$scope;Generation=1;OwningLayer=$id;OperationId='base';ScopePaths=@();
            ArtifactRecords=$artifacts;GeneratorFingerprint=('5'*64);ConsumerReferences=@($mapScope);LayerContract=$contract;
            InputIdentity=(New-ProjectWorldLayerInputIdentity -OperationIdentity $baseIdentity)}
        $manifests+=New-ProjectWorldCandidateManifest @layerArgs
        $inventory=$contract | ConvertTo-Json -Depth 12 | ConvertFrom-Json
        $inventory | Add-Member -NotePropertyName scope_id -NotePropertyValue $scope
        $inventory | Add-Member -NotePropertyName layer_id -NotePropertyValue $id
        $before.layer_inventories+=$inventory; $fingerprints[$scope]='5'*64
    }
    $publish=@{ProjectRoot=$ProjectRoot;ManifestRoot=$ManifestRoot;TransactionId=('0'*32);OperationId='base';CandidateManifests=$manifests}
    if ($null -ne $PriorActiveSet) {$publish.PriorActiveSet=$PriorActiveSet}
    Publish-ProjectWorldActiveSet @publish | Out-Null
    $active=Read-ProjectWorldActiveSet -ManifestRoot $ManifestRoot -ProjectRoot $ProjectRoot
    $after=$before | ConvertTo-Json -Depth 16 | ConvertFrom-Json; $after.input_sha256='7'*64
    ($after.layer_inventories | Where-Object layer_id -eq roads).canonical_inputs[0].sha256='8'*64
    $identity=@{}; foreach ($key in $baseIdentity.Keys) {$identity[$key]=$baseIdentity[$key]}; $identity.compile_result_sha256='7'*64
    $candidate=@{base_authority_id=$Prefix+':base';base_result_sha256=('a'*64);result_sha256=('7'*64);active_sha256=('9'*64);
        effective_identity=('a'*64);bundle_sha256=('b'*64);inputs_hash=('c'*64);implementation_sha256=('d'*64);
        source_context_identity=('e'*64);execution_identity=('f'*64)}
    $proof=New-ProjectWorldNativeRefinementTransition -Before $before -After $after -ActiveSet $active -Candidate $candidate `
        -InputSelectionSha256 ('1'*64) -BaseCanonicalAuthoritySha256 ('2'*64) -LayerDefinitions $definitions `
        -GeneratorFingerprints $fingerprints -InputIdentity $identity -MapScopeId $mapScope -MapGeneratorFingerprint ('f'*64) `
        -RealizationProfileHash ('2'*64) -CurrentPresentationProfileHash $PresentationProfileHash -ExecuteLayer @('roads','water','vegetation')
    $road=$active.Manifests['layer_'+$Prefix+'_roads'].layer_contract | ConvertTo-Json -Depth 12 | ConvertFrom-Json
    $road.canonical_inputs=($after.layer_inventories | Where-Object layer_id -eq roads).canonical_inputs
    $next=New-ProjectWorldCandidateManifest -ProjectRoot $ProjectRoot -ScopeId ('layer_'+$Prefix+'_roads') -Generation 2 `
        -OwningLayer roads -OperationId child -InputIdentity (New-ProjectWorldLayerInputIdentity $identity) -ScopePaths @() `
        -ArtifactRecords @() -GeneratorFingerprint ('5'*64) -LayerContract $road -ConsumerReferences @($mapScope)
    return @{Active=$active;Proof=$proof;Next=$next;Definitions=$definitions;MapScope=$mapScope;
        Before=$before;After=$after;Fingerprints=$fingerprints}
}

function Save-CompatibilityFixture {
    param([object]$Fixture,[string]$ProjectRoot,[string]$ManifestRoot)
    Publish-ProjectWorldActiveSet -ProjectRoot $ProjectRoot -ManifestRoot $ManifestRoot -TransactionId ('1'*32) `
        -OperationId child -CandidateManifests @($Fixture.Next) -PriorActiveSet $Fixture.Active `
        -CanonicalInputCompatibility $Fixture.Proof | Out-Null
    return Read-ProjectWorldActiveSet -ManifestRoot $ManifestRoot -ProjectRoot $ProjectRoot
}
