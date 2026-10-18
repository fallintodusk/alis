BeforeAll {
    . (Join-Path $PSScriptRoot '..\layer_execution_selection.ps1')
    . (Join-Path $PSScriptRoot '..\layer_input_compatibility.ps1')
}

Describe 'Native census against accepted layer authority' {
    BeforeEach {
        $before = [pscustomobject]@{layer_inventories=@()}
        $active = [pscustomobject]@{Sha256=('1'*64);Manifests=[ordered]@{};Record=@{scopes=@()}}
        $fingerprints = [ordered]@{}
        foreach ($id in @('terrain','roads')) {
            $scope = 'layer_fixture_' + $id
            $contract = [pscustomobject]@{scope_id=$scope;layer_id=$id;generator_id=$id;generator_version=1;
                realization_profile_sha256=('2'*64);normalized_layer_contract_sha256=('3'*64);artifact_root='/Test/'+$id;
                canonical_inputs=@(@{unit_id='cell';sha256=('4'*64)});dependency_inputs=@()}
            $before.layer_inventories += $contract
            $active.Manifests[$scope] = [pscustomobject]@{generator_fingerprint=('5'*64);
                input_identity=@{compile_result_sha256=('6'*64)};
                layer_contract=($contract | ConvertTo-Json -Depth 10 | ConvertFrom-Json)}
            $active.Record.scopes += @{scope_id=$scope;manifest_path='scopes/'+$scope+'.1.json';manifest_sha256=('7'*64)}
            $fingerprints[$scope] = '5'*64
        }
        $arguments = @{Before=$before;ActiveSet=$active;GeneratorFingerprints=$fingerprints;
            BaseResultSha256=('6'*64);RealizationProfileHash=('2'*64);SelectedScopeIds=@('layer_fixture_roads')}
    }
    It 'binds the exact accepted manifests and all native baseline inputs' {
        $result = Compare-ProjectWorldAcceptedLayerInputs @arguments
        $result.layer_fixture_terrain.accepted_manifest_sha256 | Should -Be ('7'*64)
        $result.layer_fixture_roads.accepted_generator_fingerprint | Should -Be ('5'*64)
    }
    It 'refuses a native baseline differing from accepted canonical or dependency inputs' {
        $before.layer_inventories[1].canonical_inputs[0].sha256 = '8'*64
        { Compare-ProjectWorldAcceptedLayerInputs @arguments } | Should -Throw '*accepted canonical_inputs*'
        $before.layer_inventories[1].canonical_inputs[0].sha256 = '4'*64
        $before.layer_inventories[1].dependency_inputs = @(@{unit_id='other';sha256=('8'*64)})
        { Compare-ProjectWorldAcceptedLayerInputs @arguments } | Should -Throw '*accepted dependency_inputs*'
    }
    It 'refuses missing or changed accepted contracts and prior result lineage' {
        $active.Manifests.layer_fixture_roads.layer_contract.artifact_root = '/Other/'
        { Compare-ProjectWorldAcceptedLayerInputs @arguments } | Should -Throw '*accepted layer contract*'
        $active.Manifests.layer_fixture_roads.layer_contract.artifact_root = '/Test/roads'
        $active.Manifests.layer_fixture_roads.input_identity.compile_result_sha256 = '8'*64
        { Compare-ProjectWorldAcceptedLayerInputs @arguments } | Should -Throw '*base lineage*'
        $active.Manifests.Remove('layer_fixture_roads')
        { Compare-ProjectWorldAcceptedLayerInputs @arguments } | Should -Throw '*accepted owner*'
    }
    It 'refuses unselected producer changes but records selected producer changes' {
        $fingerprints.layer_fixture_roads = '8'*64
        $result = Compare-ProjectWorldAcceptedLayerInputs @arguments
        $result.layer_fixture_roads.current_generator_fingerprint | Should -Be ('8'*64)
        $fingerprints.layer_fixture_terrain = '8'*64
        { Compare-ProjectWorldAcceptedLayerInputs @arguments } | Should -Throw '*Unselected producer*'
    }
    It 'refuses missing, duplicate or invalid manifest and producer pins' {
        $active.Record.scopes += $active.Record.scopes[0]
        { Compare-ProjectWorldAcceptedLayerInputs @arguments } | Should -Throw '*manifest pin*'
        $active.Record.scopes = @($active.Record.scopes[0], $active.Record.scopes[1])
        $fingerprints.Remove('layer_fixture_roads')
        { Compare-ProjectWorldAcceptedLayerInputs @arguments } | Should -Throw '*producer fingerprint*'
    }
}

Describe 'Native base-to-refinement layer input comparison' {
    BeforeEach {
        $definitions = [ordered]@{}
        $layers = @()
        foreach ($id in @('terrain','roads','water','vegetation','buildings')) {
            $depends = if ($id -eq 'terrain') { @() } elseif ($id -eq 'vegetation') { @('terrain','roads','water') } else { @('terrain') }
            $scope = 'layer_fixture_' + $id
            $definitions[$scope] = @{ layer_id=$id; depends_on=$depends; generator_id=$id; generator_version=1 }
            $layers += @{ layer_id=$id; scope_id=$scope; generator_id=$id; generator_version=1;
                normalized_layer_contract_sha256=('a'*64); artifact_root='/Test/'+$id;
                canonical_inputs=@(@{unit_id='cell';sha256=('a'*64)}); dependency_inputs=@() }
        }
        $before = [pscustomobject]@{ status='accepted';operation='unreal_world_realization';input_sha256=('a'*64);layer_inventories=$layers }
        foreach ($field in @('world_data_plugin','grid_id','canonical_crs','coordinate_transform','map_package',
                'presentation_profile_sha256','runtime_profile_sha256','authored_overlay_set_sha256','realization_profile_sha256',
                'vertical_origin_m','lattice_origin_m','engine_georeference_origin_m','sample_spacing_m','canonical_cell_count',
                'presentation_material_bindings','terrain_material_semantic_identity','authored_anchor_resolutions')) {
            $before | Add-Member -NotePropertyName $field -NotePropertyValue 'unchanged'
        }
        $after = $before | ConvertTo-Json -Depth 24 | ConvertFrom-Json
        $after.input_sha256 = 'b'*64
        ($after.layer_inventories | Where-Object layer_id -eq roads).canonical_inputs[0].sha256 = 'b'*64
        ($after.layer_inventories | Where-Object layer_id -eq water).canonical_inputs[0].sha256 = 'c'*64
        $arguments = @{Before=$before;After=$after;BaseResultSha256=('a'*64);EffectiveResultSha256=('b'*64);
            LayerDefinitions=$definitions;ExecuteLayer=@('roads','water','vegetation')}
    }
    It 'retains every unselected owner while identifying only consumed changed units' {
        $result = Compare-ProjectWorldNativeLayerInputs @arguments
        @($result.Layers.roads.canonical_inputs.changed_units) | Should -Be @('cell')
        @($result.Layers.terrain.canonical_inputs.changed_units).Count | Should -Be 0
        @($result.Layers.vegetation.canonical_inputs.changed_units).Count | Should -Be 0
    }
    It 'refuses unselected canonical or dependency changes' {
        ($after.layer_inventories | Where-Object layer_id -eq buildings).canonical_inputs[0].sha256 = 'd'*64
        { Compare-ProjectWorldNativeLayerInputs @arguments } | Should -Throw '*Unselected*'
        ($after.layer_inventories | Where-Object layer_id -eq buildings).canonical_inputs[0].sha256 = 'a'*64
        ($after.layer_inventories | Where-Object layer_id -eq buildings).dependency_inputs = @(@{unit_id='dep';sha256=('d'*64)})
        { Compare-ProjectWorldNativeLayerInputs @arguments } | Should -Throw '*Unselected*'
    }
    It 'refuses omitted dependent execution owners' {
        $arguments.ExecuteLayer = @('roads','water')
        { Compare-ProjectWorldNativeLayerInputs @arguments } | Should -Throw '*consumer closure*'
    }
    It 'refuses duplicate units and missing layer inventories' {
        $layer = $after.layer_inventories | Where-Object layer_id -eq roads
        $layer.canonical_inputs += $layer.canonical_inputs[0]
        { Compare-ProjectWorldNativeLayerInputs @arguments } | Should -Throw '*duplicated*'
        $layer.canonical_inputs = @($layer.canonical_inputs[0])
        $after.layer_inventories = @($after.layer_inventories | Where-Object layer_id -ne buildings)
        { Compare-ProjectWorldNativeLayerInputs @arguments } | Should -Throw '*omits*'
    }
    It 'refuses unrelated shared map or profile changes' {
        $after.map_package = '/Other/Map'
        { Compare-ProjectWorldNativeLayerInputs @arguments } | Should -Throw '*shared contract*'
    }
    It 'refuses another receipt and rejected native execution' {
        $after.input_sha256 = 'd'*64
        { Compare-ProjectWorldNativeLayerInputs @arguments } | Should -Throw '*requested base*'
        $after.input_sha256 = 'b'*64
        $after.status = 'rejected'
        { Compare-ProjectWorldNativeLayerInputs @arguments } | Should -Throw '*accepted*'
    }
}
