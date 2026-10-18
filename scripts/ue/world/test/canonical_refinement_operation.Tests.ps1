# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')
    . (Join-Path $PSScriptRoot '..\realization_layer_operation.ps1')
    . (Join-Path $PSScriptRoot '..\layer_execution_selection.ps1')
    . (Join-Path $PSScriptRoot '..\canonical_refinement_operation.ps1')
    . (Join-Path $PSScriptRoot '..\realization_candidate_manifests.ps1')
    . (Join-Path $PSScriptRoot 'canonical_compatibility_test_helpers.ps1')
    $script:RepositoryRoot=(Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
    $script:FixtureParent=Join-Path $script:RepositoryRoot 'tmp/world/refinement_operation_tests'
}
Describe 'Selected canonical refinement preflight' {
    BeforeEach {
        $ErrorActionPreference='Stop'
        $projectRoot=Join-Path $script:FixtureParent ([guid]::NewGuid().ToString('N'))
        $manifestRoot=Join-Path $projectRoot 'Plugins/World/ProjectWorldTestData/Data/Manifests'
        $schemaRoot=Join-Path $projectRoot 'Plugins/World/ProjectWorld/Data/Schemas'
        New-Item -ItemType Directory -Path $schemaRoot,$manifestRoot,(Join-Path $projectRoot 'tmp') -Force | Out-Null
        foreach ($name in @('project_world_active_manifest_set.schema.json','project_world_generated_manifest.schema.json')) {
            Copy-Item -LiteralPath (Join-Path $script:RepositoryRoot ('Plugins/World/ProjectWorld/Data/Schemas/'+$name)) -Destination $schemaRoot
        }
        $presentationPath=Join-Path $projectRoot 'tmp/original.presentation.json'
        Write-ProjectWorldJson -Path $presentationPath -Document @{profile_id='fixture';materials=@{
            road='/Engine/Original';water='/Engine/Water';building='/Engine/Building';cloud='/Engine/Cloud'};outdoor_environment=@{sun=50}}
        $presentationBytes=[IO.File]::ReadAllBytes($presentationPath)
        $presentationHash=Get-ProjectWorldFileSha256 $presentationPath
        $fixture=New-CompatibilityFixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot -PresentationProfileHash $presentationHash
        $candidate=$fixture.Proof.candidate | ConvertTo-Json | ConvertFrom-Json
        $candidate | Add-Member -NotePropertyName materialized_result_path -NotePropertyValue 'tmp/effective.json'
        $candidate | Add-Member -NotePropertyName active_path -NotePropertyValue 'tmp/candidate/active.json'
        $selection=[pscustomobject]@{base_selection=@{validation_profile_path='tmp/validation.json';profiles=@{kazan=@{active_sha256=('2'*64)}}};
            candidates=@{kazan=$candidate}}
        $selectionPath=Join-Path $projectRoot 'tmp/selection.json'
        Write-ProjectWorldJson -Document $selection -Path $selectionPath
        Write-ProjectWorldJson -Document @{profiles=@{kazan=@{compiler_profile_path='tmp/compiler.json'}}} -Path (Join-Path $projectRoot 'tmp/validation.json')
        $compilePath=Join-Path $projectRoot 'tmp/effective.json';$basePath=Join-Path $projectRoot 'tmp/base.json'
        Set-Content -LiteralPath $compilePath,$basePath -Value '{}' -NoNewline
        # Canonical owner and native-census transports have separate real-route tests. These two
        # fixture receipt hashes match the existing native inventory controls; every other hash is real.
        Mock Get-ProjectWorldFileSha256 { '7'*64 } -ParameterFilter {$Path -eq $compilePath}
        Mock Get-ProjectWorldFileSha256 { 'a'*64 } -ParameterFilter {$Path -eq $basePath}
        $script:OwnerCalls=@();$script:BadTicket=$false;$script:StaleSelection=$false
        Mock Invoke-ProjectWorldRefinementOwner {
            param($ProjectRoot,$Owner,$Arguments,$EvidenceDirectory)
            $script:OwnerCalls+=,$Arguments[0]
            switch ($Arguments[0]) {
                'verify-refinement-inputs' {
                    @{status='accepted';selection_sha256=$(if ($script:StaleSelection) {'0'*64} else {Get-ProjectWorldFileSha256 $selectionPath});
                        candidates=([pscustomobject]@{kazan=$candidate})}
                }
                materialize {@{status='accepted';result_path='tmp/base.json'}}
                'prepare-selection-transition' {
                    @{old_active_sha256=('2'*64);new_active_sha256=$(if ($script:BadTicket) {'0'*64} else {'9'*64});
                        base_result_sha256=('a'*64);effective_result_sha256=('7'*64);effective_identity=('a'*64)}
                }
                authority {
                    [pscustomobject]@{authority_id=$candidate.base_authority_id;compile_result_sha256=$candidate.base_result_sha256;
                        selected_refinement=[pscustomobject]@{effective_identity=$candidate.effective_identity;result_sha256=$candidate.result_sha256;
                            inputs_hash=$candidate.inputs_hash;implementation_sha256=$candidate.implementation_sha256;
                            source_context_identity=$candidate.source_context_identity;execution_identity=$candidate.execution_identity;
                            bundle=@{sha256=$candidate.bundle_sha256}}}
                }
                default {throw 'Unexpected owner operation'}
            }
        }
        Mock Invoke-ProjectWorldRefinementNativeInputs {
            param($ProjectRoot,$CompileResult,$ValidationArguments,$EvidenceDirectory,$Label)
            if ($Label -eq 'base') {return $fixture.Before};return $fixture.After
        }
        $arguments=@{ProjectRoot=$projectRoot;SelectionPath=$selectionPath;CompileResult=$compilePath;ActiveSet=$fixture.Active;
            InputIdentity=([ordered]@{compile_result_sha256=('7'*64);presentation_profile_sha256=$presentationHash;runtime_profile_sha256=('c'*64);
                authored_overlay_profile_sha256=('d'*64);map_package=$fixture.Before.map_package});
            LayerDefinitions=$fixture.Definitions;GeneratorFingerprints=$fixture.Fingerprints;MapScopeId=$fixture.MapScope;
            MapGeneratorFingerprint=('f'*64);RealizationProfileHash=('2'*64);CurrentPresentationProfileHash=$presentationHash;
            ExecuteLayer=@('roads','water','vegetation');ValidationArguments=@{};EvidenceDirectory=(Join-Path $projectRoot 'tmp/evidence')}
    }
    AfterEach {
        $target=[IO.Path]::GetFullPath($projectRoot)
        if (-not $target.StartsWith(([IO.Path]::GetFullPath($script:FixtureParent)+'\'),[StringComparison]::OrdinalIgnoreCase)) {throw 'Cleanup escapes scratch.'}
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    It 'authenticates selection, actual native inputs and existing owners before preparing paired publication' {
        $operation=New-ProjectWorldCanonicalRefinementOperation @arguments
        $script:OwnerCalls -join ',' | Should -Be 'verify-refinement-inputs,materialize,prepare-selection-transition'
        $operation.Compatibility.accepted_map.manifest_sha256 | Should -Be $fixture.Proof.accepted_map.manifest_sha256
        $operation.Compatibility.layers.layer_fixture_buildings.accepted.accepted_generator_fingerprint | Should -Be ('5'*64)
        $operation.Transition.new_active_sha256 | Should -Be ('9'*64)
        $mapIdentity=Get-ProjectWorldRefinementMapIdentity -InputIdentity $arguments.InputIdentity `
            -Compatibility $operation.Compatibility -MapScopeId $fixture.MapScope
        $mapIdentity.compile_result_sha256 | Should -Be ('a'*64)
        Assert-ProjectWorldRefinementResult -Operation $operation -Result $fixture.After `
            -LayerDefinitions $fixture.Definitions -ExecuteLayer $arguments.ExecuteLayer
    }
    It 'refuses stale input admission before native or preparation work' {
        $script:StaleSelection=$true
        { New-ProjectWorldCanonicalRefinementOperation @arguments } | Should -Throw '*failed authentication*'
        $script:OwnerCalls -join ',' | Should -Be verify-refinement-inputs
        Should -Invoke Invoke-ProjectWorldRefinementNativeInputs -Times 0
    }
    It 'refuses a different materialization path despite matching receipt identity' {
        $candidate.materialized_result_path='tmp/other.json'
        { New-ProjectWorldCanonicalRefinementOperation @arguments } | Should -Throw '*exact admitted effective materialization*'
        $script:OwnerCalls -join ',' | Should -Be verify-refinement-inputs
    }
    It 'refuses changed unselected canonical inputs before preparing canonical publication' {
        ($fixture.After.layer_inventories | Where-Object layer_id -eq buildings).canonical_inputs[0].sha256='0'*64
        { New-ProjectWorldCanonicalRefinementOperation @arguments } | Should -Throw '*Unselected layer inputs*'
        $script:OwnerCalls -join ',' | Should -Be 'verify-refinement-inputs,materialize'
    }
    It 'refuses an omitted dependency consumer before preparing canonical publication' {
        $arguments.ExecuteLayer=@('roads','water')
        { New-ProjectWorldCanonicalRefinementOperation @arguments } | Should -Throw '*consumer closure*'
        $script:OwnerCalls -join ',' | Should -Be 'verify-refinement-inputs,materialize'
    }
    It 'refuses unexpected canonical preparation identity without granting World compatibility' {
        $script:BadTicket=$true
        { New-ProjectWorldCanonicalRefinementOperation @arguments } | Should -Throw '*different transition*'
    }
    It 'refuses actual emitted inputs that differ from native preflight even for selected writers' {
        $operation=New-ProjectWorldCanonicalRefinementOperation @arguments
        $actual=$fixture.After | ConvertTo-Json -Depth 24 | ConvertFrom-Json
        ($actual.layer_inventories | Where-Object layer_id -eq roads).canonical_inputs[0].sha256='0'*64
        { Assert-ProjectWorldRefinementResult -Operation $operation -Result $actual -LayerDefinitions $fixture.Definitions `
            -ExecuteLayer $arguments.ExecuteLayer } | Should -Throw '*differ from their native preflight*'
    }
    It 'refuses changed admission bytes while the child was running' {
        $operation=New-ProjectWorldCanonicalRefinementOperation @arguments
        Add-Content -LiteralPath $selectionPath -Value ' '
        { Assert-ProjectWorldRefinementResult -Operation $operation -Result $fixture.After -LayerDefinitions $fixture.Definitions `
            -ExecuteLayer $arguments.ExecuteLayer } | Should -Throw '*admission changed*'
    }
    It 'keeps ordinary strict map identity and refuses unrelated map or profile compatibility' {
        $identity=Get-ProjectWorldRefinementMapIdentity -InputIdentity $arguments.InputIdentity -Compatibility $null -MapScopeId $fixture.MapScope
        $identity.compile_result_sha256 | Should -Be ('7'*64)
        $proof=$fixture.Proof | ConvertTo-Json -Depth 24 | ConvertFrom-Json
        { Get-ProjectWorldRefinementMapIdentity -InputIdentity $arguments.InputIdentity -Compatibility $proof -MapScopeId map_other } | Should -Throw '*another map*'
        $arguments.InputIdentity.runtime_profile_sha256='0'*64
        { Get-ProjectWorldRefinementMapIdentity -InputIdentity $arguments.InputIdentity -Compatibility $proof -MapScopeId $fixture.MapScope } | Should -Throw '*another map input*'
    }
    It 'carries unchanged map and dependent layer manifests while recording the changed Road effective identity' {
        $proof=$fixture.Proof | ConvertTo-Json -Depth 24 | ConvertFrom-Json
        $infos=@{};$generations=@{}
        foreach ($id in @('roads','water','vegetation')) {
            $scope='layer_fixture_'+$id;$prior=$fixture.Active.Manifests[$scope]
            $contract=if ($id -eq 'roads') {$fixture.Next.layer_contract} else {$prior.layer_contract}
            $infos[$scope]=@{Contract=$contract;Records=@()};$generations[$scope]=2
        }
        $candidates=@(New-ProjectWorldLayerManifests -ProjectRoot $projectRoot -ActiveSet $fixture.Active -OperationId child `
            -Generations $generations -Definitions $fixture.Definitions -CandidateInfos $infos -Fingerprints $fixture.Fingerprints `
            -InputIdentity (New-ProjectWorldLayerInputIdentity $arguments.InputIdentity) -MapScopeId $fixture.MapScope -CanonicalCompatibility $proof)
        $candidates.Count | Should -Be 1
        $candidates[0].scope_id | Should -Be layer_fixture_roads
        $candidates[0].input_identity.compile_result_sha256 | Should -Be ('7'*64)
        $mapCandidate=New-ProjectWorldMapManifestIfChanged -ProjectRoot $projectRoot -ActiveSet $fixture.Active -MapScopeId $fixture.MapScope `
            -Generation 2 -OperationId child -LayerCandidates $infos -ScopePaths @() -InputIdentity $arguments.InputIdentity `
            -GeneratorFingerprint ('f'*64) -SelectedApply -CanonicalCompatibility $proof
        $mapCandidate | Should -BeNullOrEmpty
    }
    It 'authenticates an accepted child on ordinary replay without requiring the original temporary selection' {
        $accepted=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $dataRoot=Split-Path -Parent $manifestRoot;$canonicalActive=Join-Path $dataRoot 'Canonical/fixture/active.json'
        Mock Get-ProjectWorldFileSha256 { '9'*64 } -ParameterFilter {$Path -eq $canonicalActive}
        $operation=Get-ProjectWorldAcceptedRefinementOperation -ProjectRoot $projectRoot -ActiveSet $accepted -MapScopeId $fixture.MapScope `
            -CompileResult $compilePath -ProfileId fixture -DataRoot $dataRoot -InputIdentity $arguments.InputIdentity `
            -GeneratorFingerprints $fixture.Fingerprints -MapGeneratorFingerprint ('f'*64) -ValidationArguments @{} -EvidenceDirectory $arguments.EvidenceDirectory
        $operation.Transition | Should -BeNullOrEmpty
        $operation.SelectionPath | Should -Be ''
        $script:OwnerCalls -join ',' | Should -Be authority
        Assert-ProjectWorldRefinementResult -Operation $operation -Result $fixture.After `
            -LayerDefinitions $fixture.Definitions -ExecuteLayer $arguments.ExecuteLayer
    }
    It 'keeps accepted material-only updates free of native geometry census or canonical publication' {
        $accepted=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $dataRoot=Split-Path -Parent $manifestRoot;$canonicalActive=Join-Path $dataRoot 'Canonical/fixture/active.json'
        Mock Get-ProjectWorldFileSha256 { '9'*64 } -ParameterFilter {$Path -eq $canonicalActive}
        $operation=Get-ProjectWorldAcceptedRefinementOperation -ProjectRoot $projectRoot -ActiveSet $accepted -MapScopeId $fixture.MapScope `
            -CompileResult $compilePath -ProfileId fixture -DataRoot $dataRoot -InputIdentity $arguments.InputIdentity `
            -GeneratorFingerprints $fixture.Fingerprints -MapGeneratorFingerprint ('f'*64) -ValidationArguments @{} `
            -EvidenceDirectory $arguments.EvidenceDirectory -MaterialReferencesOnly
        $operation.Transition | Should -BeNullOrEmpty
        $operation.EffectiveInputs | Should -BeNullOrEmpty
        Should -Invoke Invoke-ProjectWorldRefinementNativeInputs -Times 0
        $script:OwnerCalls -join ',' | Should -Be authority
    }
    It 'refuses a changed accepted producer without invoking raw source replay or publication' {
        $accepted=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $fingerprints=$fixture.Fingerprints;$fingerprints.layer_fixture_buildings='0'*64
        { Get-ProjectWorldAcceptedRefinementOperation -ProjectRoot $projectRoot -ActiveSet $accepted -MapScopeId $fixture.MapScope `
            -CompileResult $compilePath -ProfileId fixture -DataRoot (Split-Path -Parent $manifestRoot) -InputIdentity $arguments.InputIdentity `
            -GeneratorFingerprints $fingerprints -MapGeneratorFingerprint ('f'*64) -ValidationArguments @{} `
            -EvidenceDirectory $arguments.EvidenceDirectory } | Should -Throw '*producer changed*'
        $script:OwnerCalls | Should -BeNullOrEmpty
    }
    It 'replays accepted geometry after a real material-only adoption without rewriting canonical compatibility' {
        $accepted=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $profile=Get-Content -LiteralPath $presentationPath -Raw | ConvertFrom-Json
        $profile.materials.road='road.next'
        $nextPath=Join-Path $projectRoot 'tmp/next.presentation.json'
        Write-ProjectWorldJson -Document $profile -Path $nextPath
        $resource=[pscustomobject]@{object_path='/ProjectMaterial/Surfaces/Road/MI_Next.MI_Next';package_sha256=('b'*64);
            semantic_identity=('c'*64);manifest_sha256=('d'*64)}
        $result=[pscustomobject]@{presentation_profile_sha256=(Get-ProjectWorldFileSha256 $nextPath);
            presentation_material_bindings=[pscustomobject]@{road=$resource}}
        $evidence=Resolve-ProjectWorldMaterialProfile -ActiveSet $accepted -MapScopeId $fixture.MapScope `
            -SelectedScopes layer_fixture_roads -CandidatePath $nextPath -PriorPath $presentationPath
        $adoption=New-ProjectWorldMaterialAdoption -Inventory ([pscustomobject]@{material_reference_role='road';artifacts=@()}) `
            -Prior $accepted.Manifests.layer_fixture_roads -Result $result -ProfileEvidence $evidence -CandidatePath $nextPath
        $infos=@{layer_fixture_roads=@{Records=@();Contract=$accepted.Manifests.layer_fixture_roads.layer_contract}}
        $material=@(New-ProjectWorldLayerManifests -ProjectRoot $projectRoot -ActiveSet $accepted -OperationId material `
            -Generations @{layer_fixture_roads=3} -Definitions $fixture.Definitions -CandidateInfos $infos -Fingerprints $fixture.Fingerprints `
            -InputIdentity @{} -MapScopeId $fixture.MapScope -MaterialReferencesOnly -Adoptions @{layer_fixture_roads=$adoption})
        Publish-ProjectWorldActiveSet -ProjectRoot $projectRoot -ManifestRoot $manifestRoot -PriorActiveSet $accepted `
            -TransactionId ('4'*32) -OperationId material -CandidateManifests $material | Out-Null
        $current=Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
        $current.Record.canonical_input_compatibility[0].input_selection_sha256 | Should -Be $fixture.Proof.input_selection_sha256
        $fixture.After.presentation_profile_sha256=$result.presentation_profile_sha256
        $fixture.After.presentation_material_bindings=$result.presentation_material_bindings
        $materialManifest=Join-Path $projectRoot 'tmp/materials.json'
        Write-ProjectWorldJson -Document @{records=@(@{binding_id='road.next';output_object_path=$resource.object_path})} -Path $materialManifest
        $dataRoot=Split-Path -Parent $manifestRoot;$canonicalActive=Join-Path $dataRoot 'Canonical/fixture/active.json'
        Mock Get-ProjectWorldFileSha256 { '9'*64 } -ParameterFilter {$Path -eq $canonicalActive}
        $operation=Get-ProjectWorldAcceptedRefinementOperation -ProjectRoot $projectRoot -ActiveSet $current -MapScopeId $fixture.MapScope `
            -CompileResult $compilePath -ProfileId fixture -DataRoot $dataRoot -InputIdentity $arguments.InputIdentity `
            -GeneratorFingerprints $fixture.Fingerprints -MapGeneratorFingerprint ('f'*64) -ValidationArguments @{} `
            -EvidenceDirectory $arguments.EvidenceDirectory -PresentationProfilePath $nextPath -MaterialManifestPath $materialManifest
        Assert-ProjectWorldRefinementResult -Operation $operation -Result $fixture.After -LayerDefinitions $fixture.Definitions -ExecuteLayer $arguments.ExecuteLayer
        $script:OwnerCalls -join ',' | Should -Be authority
        $fixture.After.lattice_origin_m='different-geometry'
        { Get-ProjectWorldAcceptedRefinementOperation -ProjectRoot $projectRoot -ActiveSet $current -MapScopeId $fixture.MapScope `
            -CompileResult $compilePath -ProfileId fixture -DataRoot $dataRoot -InputIdentity $arguments.InputIdentity `
            -GeneratorFingerprints $fixture.Fingerprints -MapGeneratorFingerprint ('f'*64) -ValidationArguments @{} `
            -EvidenceDirectory $arguments.EvidenceDirectory -PresentationProfilePath $nextPath -MaterialManifestPath $materialManifest } |
            Should -Throw '*shared geometry contract changed*'
        $fixture.After.lattice_origin_m=$fixture.Before.lattice_origin_m
        Write-ProjectWorldJson -Document @{records=@(@{binding_id='road.next';output_object_path='/ProjectMaterial/Surfaces/Road/MI_Foreign.MI_Foreign'})} -Path $materialManifest
        { Get-ProjectWorldAcceptedRefinementOperation -ProjectRoot $projectRoot -ActiveSet $current -MapScopeId $fixture.MapScope `
            -CompileResult $compilePath -ProfileId fixture -DataRoot $dataRoot -InputIdentity $arguments.InputIdentity `
            -GeneratorFingerprints $fixture.Fingerprints -MapGeneratorFingerprint ('f'*64) -ValidationArguments @{} `
            -EvidenceDirectory $arguments.EvidenceDirectory -PresentationProfilePath $nextPath -MaterialManifestPath $materialManifest } |
            Should -Throw '*retargeted adopted instance*'
    }
    It 'carries a later accepted Building realization using its proved inputs rather than initial writer permission' {
        $accepted=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $infos=@{layer_fixture_buildings=@{Contract=$accepted.Manifests.layer_fixture_buildings.layer_contract;Records=@()}}
        $proof=$accepted.Record.canonical_input_compatibility[0]
        $candidates=@(New-ProjectWorldLayerManifests -ProjectRoot $projectRoot -ActiveSet $accepted -OperationId repair `
            -Generations @{layer_fixture_buildings=2} -Definitions $fixture.Definitions -CandidateInfos $infos -Fingerprints $fixture.Fingerprints `
            -InputIdentity (New-ProjectWorldLayerInputIdentity $arguments.InputIdentity) -MapScopeId $fixture.MapScope -CanonicalCompatibility $proof -AcceptedRefinementReplay)
        $candidates.Count | Should -Be 0
    }
    It 'reconstructs the accepted effective tree with exact mixed lineage and refuses changed carried output' {
        $accepted=Save-CompatibilityFixture -Fixture $fixture -ProjectRoot $projectRoot -ManifestRoot $manifestRoot
        $infos=@{};$generations=@{}
        foreach ($scope in $fixture.Definitions.Keys) {
            $infos[$scope]=@{Contract=$accepted.Manifests[$scope].layer_contract;Records=@()}
            $generations[$scope]=[int]$accepted.Manifests[$scope].generation+1
        }
        $candidateArgs=@{ProjectRoot=$projectRoot;ActiveSet=$accepted;OperationId='reconstruct';Generations=$generations;
            Definitions=$fixture.Definitions;CandidateInfos=$infos;Fingerprints=$fixture.Fingerprints;
            InputIdentity=(New-ProjectWorldLayerInputIdentity $arguments.InputIdentity);MapScopeId=$fixture.MapScope;
            CanonicalCompatibility=$accepted.Record.canonical_input_compatibility[0];AcceptedRefinementReplay=$true}
        $candidates=@(New-ProjectWorldLayerManifests @candidateArgs)
        $candidates.Count | Should -Be 0
        $published=Publish-ProjectWorldActiveSet -ProjectRoot $projectRoot -ManifestRoot $manifestRoot -PriorActiveSet $accepted `
            -TransactionId ('5'*32) -OperationId reconstruct -CandidateManifests $candidates -CanonicalInputCompatibility $candidateArgs.CanonicalCompatibility
        $published.Sha256 | Should -Be $accepted.Sha256
        $infos.layer_fixture_buildings.Records=@(@{path='Plugins/World/ProjectWorldTestData/Content/Generated/fixture/buildings/extra.uasset';
            kind='asset';digest_kind='sha256';digest=('0'*64)})
        { New-ProjectWorldLayerManifests @candidateArgs } | Should -Throw '*carried refinement owner differs*'
        (Get-ProjectWorldFileSha256 (Join-Path $manifestRoot 'active_set.json')) | Should -Be $accepted.Sha256
    }
}
