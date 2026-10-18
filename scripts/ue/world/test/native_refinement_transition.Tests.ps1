BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')
    . (Join-Path $PSScriptRoot '..\layer_execution_selection.ps1')
    . (Join-Path $PSScriptRoot '..\layer_input_compatibility.ps1')
}

Describe 'Native refinement transition preparation' {
    BeforeEach {
        $definitions = [ordered]@{}
        $fingerprints = [ordered]@{}
        $identity = [ordered]@{compile_result_sha256=('a'*64);map_package='/Test/Map';
            presentation_profile_sha256=('b'*64);runtime_profile_sha256=('c'*64);authored_overlay_profile_sha256=('d'*64)}
        $active = [pscustomobject]@{Sha256=('e'*64);Record=@{scopes=@()};Manifests=[ordered]@{
            map_fixture=[pscustomobject]@{input_identity=($identity | ConvertTo-Json | ConvertFrom-Json);generator_fingerprint=('f'*64)}}}
        $active.Record.scopes += @{scope_id='map_fixture';manifest_path='scopes/map_fixture.1.json';manifest_sha256=('1'*64)}
        $before = [pscustomobject]@{status='accepted';operation='unreal_world_realization';input_sha256=('a'*64);layer_inventories=@()}
        foreach ($field in Get-ProjectWorldNativeSharedInputFields) { $before | Add-Member -NotePropertyName $field -NotePropertyValue 'same' }
        $before.map_package='/Test/Map'; $before.presentation_profile_sha256='b'*64
        $before.runtime_profile_sha256='c'*64; $before.authored_overlay_set_sha256='d'*64
        $before.realization_profile_sha256='2'*64
        foreach ($id in @('terrain','roads','water','vegetation','buildings')) {
            $scope = 'layer_fixture_' + $id
            $depends = if ($id -eq 'vegetation') { @('roads','water') } elseif ($id -eq 'terrain') { @() } else { @('terrain') }
            $definitions[$scope] = @{layer_id=$id;generator_id=$id;generator_version=1;depends_on=$depends}
            $inventory = [pscustomobject]@{scope_id=$scope;layer_id=$id;generator_id=$id;generator_version=1;
                normalized_layer_contract_sha256=('3'*64);artifact_root='/Test/'+$id;
                canonical_inputs=@(@{unit_id='cell';sha256=('4'*64)});dependency_inputs=@()}
            $before.layer_inventories += $inventory
            $contract = $inventory | ConvertTo-Json -Depth 10 | ConvertFrom-Json
            $contract | Add-Member -NotePropertyName realization_profile_sha256 -NotePropertyValue ('2'*64)
            $active.Manifests[$scope] = [pscustomobject]@{input_identity=($identity | ConvertTo-Json | ConvertFrom-Json);
                consumer_references=@('map_fixture');layer_contract=$contract;generator_fingerprint=('5'*64)}
            $active.Record.scopes += @{scope_id=$scope;manifest_path='scopes/'+$scope+'.1.json';manifest_sha256=('6'*64)}
            $fingerprints[$scope] = '5'*64
        }
        $after = $before | ConvertTo-Json -Depth 16 | ConvertFrom-Json
        $after.input_sha256='7'*64
        ($after.layer_inventories | Where-Object layer_id -eq roads).canonical_inputs[0].sha256='8'*64
        $identity.compile_result_sha256='7'*64
        $candidate = @{base_authority_id='fixture:base';base_result_sha256=('a'*64);result_sha256=('7'*64);active_sha256=('9'*64);
            effective_identity=('a'*64);bundle_sha256=('b'*64);inputs_hash=('c'*64);implementation_sha256=('d'*64);
            source_context_identity=('e'*64);execution_identity=('f'*64)}
        $arguments = @{Before=$before;After=$after;ActiveSet=$active;Candidate=$candidate;
            InputSelectionSha256=('1'*64);BaseCanonicalAuthoritySha256=('2'*64);
            LayerDefinitions=$definitions;GeneratorFingerprints=$fingerprints;InputIdentity=$identity;
            MapScopeId='map_fixture';MapGeneratorFingerprint=('f'*64);RealizationProfileHash=('2'*64);
            CurrentPresentationProfileHash=('b'*64);ExecuteLayer=@('roads','water','vegetation')}
    }
    It 'prepares compact exact lineage without changing prior authority or retaining temporary paths' {
        $original = $active | ConvertTo-Json -Depth 24 -Compress
        $proof = New-ProjectWorldNativeRefinementTransition @arguments
        $proof.kind | Should -Be 'feature_refinement_inputs_v1'
        $proof.base_map_input_identity.compile_result_sha256 | Should -Be ('a'*64)
        $proof.effective_map_input_identity.compile_result_sha256 | Should -Be ('7'*64)
        $proof.layers.layer_fixture_roads.before_canonical_inputs_sha256 |
            Should -Not -Be $proof.layers.layer_fixture_roads.after_canonical_inputs_sha256
        $proof.layers.layer_fixture_vegetation.before_canonical_inputs_sha256 |
            Should -Be $proof.layers.layer_fixture_vegetation.after_canonical_inputs_sha256
        ($proof | ConvertTo-Json -Depth 24 -Compress) | Should -Not -Match 'tmp[/\\]|materialized_result_path|active_path'
        ($active | ConvertTo-Json -Depth 24 -Compress) | Should -BeExactly $original
    }
    It 'refuses another child, map producer or shared profile' {
        $candidate.result_sha256='0'*64
        { New-ProjectWorldNativeRefinementTransition @arguments } | Should -Throw '*another effective*'
        $candidate.result_sha256='7'*64; $arguments.MapGeneratorFingerprint='0'*64
        { New-ProjectWorldNativeRefinementTransition @arguments } | Should -Throw '*map producer*'
        $arguments.MapGeneratorFingerprint='f'*64; $identity.runtime_profile_sha256='0'*64
        { New-ProjectWorldNativeRefinementTransition @arguments } | Should -Throw '*shared map input*'
    }
    It 'refuses a native envelope differing from the selected profile even when both arms match' {
        $before.runtime_profile_sha256='0'*64; $after.runtime_profile_sha256='0'*64
        { New-ProjectWorldNativeRefinementTransition @arguments } | Should -Throw '*selected map/profile*'
    }
    It 'refuses a different accepted baseline and omitted execution consumers' {
        $active.Manifests.layer_fixture_roads.layer_contract.canonical_inputs[0].sha256='0'*64
        { New-ProjectWorldNativeRefinementTransition @arguments } | Should -Throw '*accepted canonical_inputs*'
        $active.Manifests.layer_fixture_roads.layer_contract.canonical_inputs[0].sha256='4'*64
        $arguments.ExecuteLayer=@('roads','water')
        { New-ProjectWorldNativeRefinementTransition @arguments } | Should -Throw '*consumer closure*'
    }
    It 'uses order-independent consumed-unit digests and refuses duplicated units' {
        $rows=@(@{unit_id='b';sha256=('b'*64)},@{unit_id='a';sha256=('a'*64)})
        (Get-ProjectWorldInputSetDigest -Inputs $rows) | Should -Be (Get-ProjectWorldInputSetDigest -Inputs @($rows[1],$rows[0]))
        { Get-ProjectWorldInputSetDigest -Inputs @($rows[0],$rows[0]) } | Should -Throw '*duplicated*'
    }
    It 'authenticates unchanged empty native objects without treating them as unknown fields' {
        $before.coordinate_transform = [pscustomobject]@{}
        $after.coordinate_transform = [pscustomobject]@{}
        { New-ProjectWorldNativeRefinementTransition @arguments } | Should -Not -Throw
    }
}
