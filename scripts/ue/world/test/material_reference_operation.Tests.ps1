# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
BeforeAll {
    . (Join-Path $PSScriptRoot '../generated_manifest.ps1')
    . (Join-Path $PSScriptRoot '../realization_layer_operation.ps1')
    . (Join-Path $PSScriptRoot '../realization_candidate_manifests.ps1')
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
    $CaseRoot = Join-Path $RepoRoot ('tmp/world/testing/material-references/' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $CaseRoot -Force | Out-Null
}
AfterAll {
    $AllowedRoot = [IO.Path]::GetFullPath((Join-Path $RepoRoot 'tmp/world/testing/material-references')).TrimEnd('\') + '\'
    if (-not [IO.Path]::GetFullPath($CaseRoot).StartsWith($AllowedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Test cleanup escaped scratch ownership.' }
    Remove-Item -LiteralPath $CaseRoot -Recurse -Force
}
Describe 'ProjectWorld reference-only material adoption' {
    BeforeEach {
        $PriorFile = Join-Path $CaseRoot 'prior.json'
        $CandidateFile = Join-Path $CaseRoot 'candidate.json'
        $Profile = [ordered]@{ '$schema' = 'schema.json'; schema_version = 1; profile_id = 'test'; materials = [ordered]@{ road = '/Engine/Old'; building = '/Engine/Old'; cloud = '/Engine/Cloud' }; outdoor_environment = @{ sunlight = 50 }; capture_viewpoints = @(@{ name = 'fixed' }) }
        $Profile | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $PriorFile -Encoding UTF8
        $Bytes = [IO.File]::ReadAllBytes($PriorFile)
        $Hash = Get-ProjectWorldBytesHash $Bytes
        $Profile.materials.road = 'road.default'
        $Profile.materials['water'] = 'water.default'
        $Profile | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $CandidateFile -Encoding UTF8
        $Scope = 'layer_test_roads'
        $Identity = @{ presentation_profile_sha256 = $Hash; compile_result_sha256 = ('a' * 64); runtime_profile_sha256 = 'none'; authored_overlay_profile_sha256 = 'none'; map_package = '/ProjectTest/Generated/Map' }
        $Contract = @{ realization_profile_id = 'test'; realization_profile_sha256 = ('b' * 64); normalized_layer_contract_sha256 = ('c' * 64); generator_id = 'project_road_mesh'; generator_version = 1; artifact_root = '/ProjectTest/Generated/Roads/'; canonical_inputs = @(@{ unit_id = 'cell'; sha256 = ('d' * 64) }); dependency_inputs = @(); final_dirty_units = @(); semantic_outputs = @(@{ artifact_path = 'Plugins/Test/road.uasset'; semantic_sha256 = ('e' * 64) }) }
        $Prior = @{ scope_id = $Scope; owning_layer = 'roads'; input_identity = $Identity; layer_contract = $Contract; generator_fingerprint = ('f' * 64); artifacts = @(@{ path = 'Plugins/Test/road.uasset'; kind = 'asset'; digest_kind = 'sha256'; digest = ('a' * 64) }); consumer_references = @('map_test') } | ConvertTo-Json -Depth 24 | ConvertFrom-Json
        $Active = @{ Manifests = [ordered]@{ map_test = @{ input_identity = $Identity }; $Scope = $Prior } }
        $Resource = @{ object_path = '/ProjectMaterial/Surfaces/Roads/MI_Default.MI_Default'; package_sha256 = ('b' * 64); semantic_identity = ('c' * 64); manifest_sha256 = ('d' * 64) }
        $Result = @{ presentation_profile_sha256 = ('e' * 64); presentation_material_bindings = @{ road = $Resource } } | ConvertTo-Json -Depth 8 | ConvertFrom-Json
        $Inventory = @{ material_reference_role = 'road'; artifacts = @(@{ path = 'Plugins/Test/road.uasset'; kind = 'asset'; digest = ('b' * 64); semantic_sha256 = ('e' * 64) }) } | ConvertTo-Json -Depth 8 | ConvertFrom-Json
    }
    It 'authenticates original geometry inputs and admits only material declarations' {
        $Evidence = Resolve-ProjectWorldMaterialProfile -ActiveSet $Active -MapScopeId map_test -SelectedScopes $Scope -CandidatePath $CandidateFile -PriorPath $PriorFile
        $Evidence.GeometryHash | Should -Be $Hash
        [Convert]::FromBase64String($Evidence.SourceBase64) | Should -Be $Bytes
    }
    It 'refuses altered cloud, environment, camera or profile identity' -TestCases @(
        @{ Field = 'cloud' }, @{ Field = 'outdoor_environment' }, @{ Field = 'capture_viewpoints' }, @{ Field = 'profile_id' }
    ) {
        param($Field)
        if ($Field -eq 'cloud') { $Profile.materials.cloud = '/Engine/Other' } else { $Profile[$Field] = 'changed' }
        $Profile | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $CandidateFile -Encoding UTF8
        { Resolve-ProjectWorldMaterialProfile -ActiveSet $Active -MapScopeId map_test -SelectedScopes $Scope -CandidatePath $CandidateFile -PriorPath $PriorFile } | Should -Throw '*cannot change*'
    }
    It 'refuses unauthenticated original source before adoption' {
        Add-Content -LiteralPath $PriorFile -Value ' '
        { Resolve-ProjectWorldMaterialProfile -ActiveSet $Active -MapScopeId map_test -SelectedScopes $Scope -CandidatePath $CandidateFile -PriorPath $PriorFile } | Should -Throw '*authentication*'
    }
    It 'rejects missing, changed or re-attributed geometry membership' {
        $Evidence = @{ SourceBase64 = [Convert]::ToBase64String($Bytes) }
        $Inventory.artifacts[0].semantic_sha256 = 'f' * 64
        { New-ProjectWorldMaterialAdoption -Inventory $Inventory -Prior $Prior -Result $Result -ProfileEvidence $Evidence -CandidatePath $CandidateFile } | Should -Throw '*semantics*'
        $Inventory.artifacts = @()
        { New-ProjectWorldMaterialAdoption -Inventory $Inventory -Prior $Prior -Result $Result -ProfileEvidence $Evidence -CandidatePath $CandidateFile } | Should -Throw '*membership*'
    }
    It 'carries geometry contracts while adding separately authenticated material provenance' {
        $Prior.consumer_references = @('map_test', 'map_secondary')
        $Evidence = Resolve-ProjectWorldMaterialProfile -ActiveSet $Active -MapScopeId map_test -SelectedScopes $Scope -CandidatePath $CandidateFile -PriorPath $PriorFile
        $Adoption = New-ProjectWorldMaterialAdoption -Inventory $Inventory -Prior $Prior -Result $Result -ProfileEvidence $Evidence -CandidatePath $CandidateFile
        $Infos = @{ $Scope = @{ Records = @(@{ path = 'Plugins/Test/road.uasset'; kind = 'asset'; digest_kind = 'sha256'; digest = ('b' * 64) }); Contract = @{ generator_id = 'current_unbuilt'; canonical_inputs = @('different') } } }
        $Args = @{ ProjectRoot = $RepoRoot; ActiveSet = $Active; OperationId = 'material-operation'; Generations = @{ $Scope = 2 }; Definitions = @{ $Scope = @{ layer_id = 'roads' } }; CandidateInfos = $Infos; Fingerprints = @{ $Scope = ('a' * 64) }; InputIdentity = @{ presentation_profile_sha256 = ('b' * 64) }; MapScopeId = 'map_test'; MaterialReferencesOnly = $true; Adoptions = @{ $Scope = $Adoption } }
        $Candidate = @(New-ProjectWorldLayerManifests @Args)
        $Candidate.Count | Should -Be 1
        $Candidate[0].generator_fingerprint | Should -Be $Prior.generator_fingerprint
        ($Candidate[0].layer_contract | ConvertTo-Json -Depth 24 -Compress) | Should -Be ($Prior.layer_contract | ConvertTo-Json -Depth 24 -Compress)
        $Candidate[0].input_identity.presentation_profile_sha256 | Should -Be $Hash
        $Candidate[0].consumer_references | Should -Be $Prior.consumer_references
        $Candidate[0].material_adoption.presentation_profile_sha256 | Should -Be ('e' * 64)
        Test-ProjectWorldMaterialAdoption -Manifest $Candidate[0]
        $Active.Manifests[$Scope] = $Candidate[0] | ConvertTo-Json -Depth 32 | ConvertFrom-Json
        @(New-ProjectWorldLayerManifests @Args).Count | Should -Be 0
        $Recovered = Resolve-ProjectWorldMaterialProfile -ActiveSet $Active -MapScopeId map_test -SelectedScopes $Scope -CandidatePath $CandidateFile -PriorPath ''
        $Recovered.GeometryHash | Should -Be $Hash
        $Candidate[0].material_adoption.resource.package_sha256 = 'wrong'
        { Test-ProjectWorldMaterialAdoption -Manifest $Candidate[0] } | Should -Throw '*identity*'
    }
    It 'uses accepted road adoption as the baseline for a later water-only update' {
        $Prior | Add-Member -NotePropertyName material_adoption -NotePropertyValue ([PSCustomObject]@{
            role = 'road'; declaration = 'road.default'; geometry_profile_source_base64 = [Convert]::ToBase64String($Bytes)
        })
        $Evidence = Resolve-ProjectWorldMaterialProfile -ActiveSet $Active -MapScopeId map_test -SelectedScopes $Scope -CandidatePath $CandidateFile -PriorPath ''
        $Evidence.ChangedRoles | Should -Be @('water')
        $Profile.materials.road = 'road.changed'
        $Profile | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $CandidateFile -Encoding UTF8
        $Evidence = Resolve-ProjectWorldMaterialProfile -ActiveSet $Active -MapScopeId map_test -SelectedScopes $Scope -CandidatePath $CandidateFile -PriorPath ''
        $Evidence.ChangedRoles | Should -Be @('road','water')
    }
    It 'preserves accepted artifact ordering despite shuffled native inventory' {
        $Adoption = New-ProjectWorldMaterialAdoption -Inventory $Inventory -Prior $Prior -Result $Result -ProfileEvidence @{ SourceBase64 = [Convert]::ToBase64String($Bytes) } -CandidatePath $CandidateFile
        $Prior.artifacts += [PSCustomObject]@{ path = 'Plugins/Test/a_road.uasset'; kind = 'asset'; digest_kind = 'sha256'; digest = ('d' * 64) }
        $Prior.layer_contract.semantic_outputs += [PSCustomObject]@{ artifact_path = 'Plugins/Test/a_road.uasset'; semantic_sha256 = ('e' * 64) }
        $Prior | Add-Member -NotePropertyName material_adoption -NotePropertyValue $Adoption
        $Infos = @{ $Scope = @{ Records = @($Prior.artifacts[1], $Prior.artifacts[0]); Contract = $Prior.layer_contract } }
        $Args = @{ ProjectRoot = $RepoRoot; ActiveSet = $Active; OperationId = 'repeated-material-operation'; Generations = @{ $Scope = 2 }; Definitions = @{ $Scope = @{ layer_id = 'roads' } }; CandidateInfos = $Infos; Fingerprints = @{ $Scope = ('a' * 64) }; InputIdentity = @{}; MapScopeId = 'map_test'; MaterialReferencesOnly = $true; Adoptions = @{ $Scope = $Adoption } }
        @(New-ProjectWorldLayerManifests @Args).Count | Should -Be 0
    }
    It 'recovers original geometry evidence from a carried road for first water-only adoption' {
        $Prior | Add-Member -NotePropertyName material_adoption -NotePropertyValue ([PSCustomObject]@{
            role = 'road'; declaration = 'road.default'; geometry_profile_source_base64 = [Convert]::ToBase64String($Bytes)
        })
        $Water = $Prior | ConvertTo-Json -Depth 24 | ConvertFrom-Json
        $Water.PSObject.Properties.Remove('material_adoption')
        $Water.scope_id = 'layer_test_water'
        $Water.owning_layer = 'water'
        $Active.Manifests['layer_test_water'] = $Water
        $Evidence = Resolve-ProjectWorldMaterialProfile -ActiveSet $Active -MapScopeId map_test -SelectedScopes layer_test_water -CandidatePath $CandidateFile -PriorPath ''
        $Evidence.GeometryHash | Should -Be $Hash
        $Evidence.ChangedRoles | Should -Be @('water')
    }
    It 'allows selected geometry after adoption without blessing changed declarations or retargeted instances' {
        $Profile.materials.Remove('water')
        $Profile | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $CandidateFile -Encoding UTF8
        $Adoption = New-ProjectWorldMaterialAdoption -Inventory $Inventory -Prior $Prior -Result $Result -ProfileEvidence @{ SourceBase64 = [Convert]::ToBase64String($Bytes) } -CandidatePath $CandidateFile
        $Prior | Add-Member -NotePropertyName material_adoption -NotePropertyValue $Adoption
        $MaterialManifest = Join-Path $CaseRoot 'materials.json'
        $Record = @{ binding_id = 'road.default'; output_object_path = $Resource.object_path; package_sha256 = ('9' * 64); semantic_identity = ('8' * 64) }
        @{ records = @($Record) } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $MaterialManifest -Encoding UTF8
        $BridgeParams = @{ ActiveSet = $Active; MapScopeId = 'map_test'; SelectedScopes = @($Scope); CandidatePath = $CandidateFile; MaterialManifestPath = $MaterialManifest }
        $Evidence = Resolve-ProjectWorldSelectedGeometryProfile @BridgeParams
        $Evidence.GeometryHash | Should -Be $Hash
        $Evidence.ChangedRoles.Count | Should -Be 0
        $Record.output_object_path = '/ProjectMaterial/Surfaces/Roads/MI_Other.MI_Other'
        @{ records = @($Record) } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $MaterialManifest -Encoding UTF8
        { Resolve-ProjectWorldSelectedGeometryProfile @BridgeParams } | Should -Throw '*retargeted*'
        $Profile.materials.road = 'road.other'
        $Profile | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $CandidateFile -Encoding UTF8
        { Resolve-ProjectWorldSelectedGeometryProfile @BridgeParams } | Should -Throw '*unaccepted material declarations*'
    }
    It 'carries historical adoption only for authenticated selected geometry, preserving current geometry lineage' {
        $Adoption = New-ProjectWorldMaterialAdoption -Inventory $Inventory -Prior $Prior -Result $Result -ProfileEvidence @{ SourceBase64 = [Convert]::ToBase64String($Bytes) } -CandidatePath $CandidateFile
        $Prior | Add-Member -NotePropertyName material_adoption -NotePropertyValue $Adoption
        $Infos = @{ $Scope = @{ Records = @($Inventory.artifacts); Contract = $Prior.layer_contract } }
        $Args = @{ ProjectRoot = $RepoRoot; ActiveSet = $Active; OperationId = 'geometry-operation'; Generations = @{ $Scope = 2 }; Definitions = @{ $Scope = @{ layer_id = 'roads' } }; CandidateInfos = $Infos; Fingerprints = @{ $Scope = ('1' * 64) }; InputIdentity = $Identity; MapScopeId = 'map_test'; PreserveMaterialAdoption = $true }
        $Candidate = @(New-ProjectWorldLayerManifests @Args)[0]
        $Candidate.generator_fingerprint | Should -Be ('1' * 64)
        ($Candidate.material_adoption | ConvertTo-Json -Depth 24 -Compress) | Should -Be ($Adoption | ConvertTo-Json -Depth 24 -Compress)
        Test-ProjectWorldMaterialAdoption -Manifest $Candidate
        $Args.Remove('PreserveMaterialAdoption')
        $Candidate = @(New-ProjectWorldLayerManifests @Args)[0]
        $Candidate.Contains('material_adoption') | Should -BeFalse
    }
    It 'exports scoped acceptance compatibility using the existing adoption authority' {
        $Profile.materials.Remove('water')
        $Profile | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $CandidateFile -Encoding UTF8
        $Adoption = New-ProjectWorldMaterialAdoption -Inventory $Inventory -Prior $Prior -Result $Result -ProfileEvidence @{ SourceBase64 = [Convert]::ToBase64String($Bytes) } -CandidatePath $CandidateFile
        $Prior | Add-Member -NotePropertyName material_adoption -NotePropertyValue $Adoption
        $Active.Manifests.map_test['owning_layer'] = 'map'
        $MaterialManifest = Join-Path $CaseRoot 'materials.json'
        $Record = @{ binding_id = 'road.default'; output_object_path = $Resource.object_path }
        @{ records = @($Record) } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $MaterialManifest -Encoding UTF8
        $Params = @{ ActiveSet = $Active; MapScopeId = 'map_test'; CandidatePath = $CandidateFile; MaterialManifestPath = $MaterialManifest }
        $Before = $Active | ConvertTo-Json -Depth 32 -Compress
        $Bridge = Get-ProjectWorldPresentationCompatibility @Params
        $Bridge.geometry_profile_sha256 | Should -Be $Hash
        $Bridge.current_profile_sha256 | Should -Be (Get-FileHash -LiteralPath $CandidateFile -Algorithm SHA256).Hash.ToLowerInvariant()
        $Bridge.validated_scope_ids | Should -Contain 'map_test'
        $Bridge.validated_scope_ids | Should -Contain $Scope
        $Bridge.adoptions[0].object_path | Should -Be $Resource.object_path
        ($Active | ConvertTo-Json -Depth 32 -Compress) | Should -Be $Before
        $Record.output_object_path = '/ProjectMaterial/Surfaces/Roads/MI_Other.MI_Other'
        @{ records = @($Record) } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $MaterialManifest -Encoding UTF8
        { Get-ProjectWorldPresentationCompatibility @Params } | Should -Throw '*retargeted*'
    }
    It 'requires adoption and authenticated original bytes for a changed-profile audit bridge' {
        $Active.Manifests.map_test['owning_layer'] = 'map'
        $Params = @{ ActiveSet = $Active; MapScopeId = 'map_test'; CandidatePath = $CandidateFile; MaterialManifestPath = (Join-Path $CaseRoot 'unused.json') }
        { Get-ProjectWorldPresentationCompatibility @Params } | Should -Throw '*requires authenticated material adoption*'
        $Profile.materials.Remove('water')
        $Profile | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $CandidateFile -Encoding UTF8
        $Adoption = New-ProjectWorldMaterialAdoption -Inventory $Inventory -Prior $Prior -Result $Result -ProfileEvidence @{ SourceBase64 = [Convert]::ToBase64String($Bytes) } -CandidatePath $CandidateFile
        $Adoption.geometry_profile_source_base64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes('tampered'))
        $Prior | Add-Member -NotePropertyName material_adoption -NotePropertyValue $Adoption
        { Get-ProjectWorldPresentationCompatibility @Params } | Should -Throw '*authentication*'
    }
}
