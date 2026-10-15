# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\layer_execution_selection.ps1')
}

Describe 'ProjectWorld owner execution selection' {
    BeforeEach {
        $definitions = [ordered]@{
            layer_terrain = @{ layer_id = 'terrain'; depends_on = @() }
            layer_roads = @{ layer_id = 'roads'; depends_on = @('terrain') }
            layer_buildings = @{ layer_id = 'buildings'; depends_on = @('terrain') }
        }
        $identity = [ordered]@{ compile_result_sha256 = 'canonical'; map_package = '/Test/Map' }
        $active = @{ Manifests = [ordered]@{
            map = @{ input_identity = @{ compile_result_sha256 = 'canonical'; map_package = '/Test/Map' } }
            layer_terrain = @{ consumer_references = @('map'); layer_contract = @{ realization_profile_sha256 = 'profile' }; artifacts = @(@{ path = 'terrain' }) }
            layer_roads = @{ consumer_references = @('map'); layer_contract = @{ realization_profile_sha256 = 'profile' }; artifacts = @(@{ path = 'road_external' }) }
            layer_buildings = @{ consumer_references = @('map'); layer_contract = @{ realization_profile_sha256 = 'profile' }; artifacts = @(@{ path = 'old_building' }) }
        } }
    }
    It 'selects buildings while retaining the full profile and pending roads' {
        $selected = @(Resolve-ProjectWorldExecutionSelection -ExecuteLayer buildings -Mode apply -LayerDefinitions $definitions)
        $selected | Should -Be @('layer_buildings')
        $definitions.Count | Should -Be 3
        Assert-ProjectWorldSelectedAuthority -ActiveSet $active -MapScopeId map -InputIdentity $identity -LayerDefinitions $definitions -RealizationProfileHash profile
    }
    It 'admits nonleaf presentation owners only in material-reference mode' {
        $definitions['layer_vegetation'] = @{ layer_id = 'vegetation'; depends_on = @('roads') }
        { Resolve-ProjectWorldExecutionSelection -ExecuteLayer roads -Mode apply -LayerDefinitions $definitions } | Should -Throw '*consumer closure*'
        @(Resolve-ProjectWorldExecutionSelection -ExecuteLayer roads -Mode apply -LayerDefinitions $definitions -MaterialReferencesOnly) | Should -Be @('layer_roads')
    }
    It 'admits a complete generated consumer closure in geometric mode' {
        $definitions['layer_water'] = @{ layer_id = 'water'; depends_on = @('terrain') }
        $definitions['layer_vegetation'] = @{ layer_id = 'vegetation'; depends_on = @('roads','water') }
        @(Resolve-ProjectWorldExecutionSelection -ExecuteLayer @('vegetation','water','roads') -Mode apply -LayerDefinitions $definitions) |
            Should -Be @('layer_roads','layer_vegetation','layer_water')
        $definitions.Count | Should -Be 5
    }
    It 'refuses omitted direct or transitive consumers' {
        $definitions['layer_vegetation'] = @{ layer_id = 'vegetation'; depends_on = @('roads') }
        $definitions['layer_details'] = @{ layer_id = 'details'; depends_on = @('vegetation') }
        { Resolve-ProjectWorldExecutionSelection -ExecuteLayer roads -Mode apply -LayerDefinitions $definitions } | Should -Throw
        { Resolve-ProjectWorldExecutionSelection -ExecuteLayer @('roads','vegetation') -Mode apply -LayerDefinitions $definitions } | Should -Throw
    }
    It 'refuses unknown, duplicate, nonleaf and unsupported operations' {
        { Resolve-ProjectWorldExecutionSelection -ExecuteLayer missing -Mode apply -LayerDefinitions $definitions } | Should -Throw '*Unknown*'
        { Resolve-ProjectWorldExecutionSelection -ExecuteLayer @('buildings','buildings') -Mode apply -LayerDefinitions $definitions } | Should -Throw '*duplicate*'
        { Resolve-ProjectWorldExecutionSelection -ExecuteLayer terrain -Mode apply -LayerDefinitions $definitions } | Should -Throw '*consumer closure*'
        { Resolve-ProjectWorldExecutionSelection -ExecuteLayer buildings -Mode delete -LayerDefinitions $definitions } | Should -Throw '*incremental*'
        { Resolve-ProjectWorldExecutionSelection -ExecuteLayer buildings -Mode apply -LayerDefinitions $definitions -Reconstruct } | Should -Throw '*incremental*'
        { Resolve-ProjectWorldExecutionSelection -ExecuteLayer buildings -Mode apply -LayerDefinitions $definitions -EnrollManifests } | Should -Throw '*incremental*'
    }
    It 'refuses changed shared inputs and profile retirement before mutation' {
        $identity.compile_result_sha256 = 'changed'
        { Assert-ProjectWorldSelectedAuthority -ActiveSet $active -MapScopeId map -InputIdentity $identity -LayerDefinitions $definitions -RealizationProfileHash profile } | Should -Throw '*shared map input*'
        $identity.compile_result_sha256 = 'canonical'
        { Assert-ProjectWorldSelectedAuthority -ActiveSet $active -MapScopeId map -InputIdentity $identity -LayerDefinitions $definitions -RealizationProfileHash changed } | Should -Throw '*full realization profile*'
        $definitions.Remove('layer_roads')
        { Assert-ProjectWorldSelectedAuthority -ActiveSet $active -MapScopeId map -InputIdentity $identity -LayerDefinitions $definitions -RealizationProfileHash profile } | Should -Throw '*retire*'
    }
    It 'excludes carried road actors as well as new building artifacts from map ownership' {
        $candidates = @{ layer_buildings = @{ Records = @(@{ path = 'new_building' }) } }
        $paths = Get-ProjectWorldLayerOwnedPaths -Candidates $candidates -ActiveSet $active -MapScopeId map
        $paths.Keys | Sort-Object | Should -Be @('new_building','road_external','terrain')
        $paths.ContainsKey('old_building') | Should -BeFalse
    }
    It 'rejects a changed carried file while permitting selected owner replacement' {
        $roadFile = Join-Path $TestDrive 'road.uasset'
        Set-Content -LiteralPath $roadFile -Value 'accepted' -NoNewline
        $fileActive = @{ Manifests = @{
            layer_roads = @{ artifacts = @(@{ path = 'road.uasset'; digest = (Get-FileHash $roadFile).Hash.ToLowerInvariant() }) }
            layer_buildings = @{ artifacts = @(@{ path = 'not_yet_saved.uasset'; digest = 'selected' }) }
        } }
        Assert-ProjectWorldCarriedArtifacts -ProjectRoot $TestDrive -ActiveSet $fileActive -SelectedScopeIds layer_buildings
        Set-Content -LiteralPath $roadFile -Value 'unexpected' -NoNewline
        { Assert-ProjectWorldCarriedArtifacts -ProjectRoot $TestDrive -ActiveSet $fileActive -SelectedScopeIds layer_buildings } | Should -Throw '*carried artifact*'
    }
}
