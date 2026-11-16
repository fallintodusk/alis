# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
# Saved product walking reuses the regional reference checker without treating its map as production evidence.
function New-ProjectWorldSavedRegionProof {
    param([string]$ProjectRoot,[string]$Map,[string]$CompileSha256,[object]$Reference,[string]$OutputDirectory)
    $scratch=Join-Path $ProjectRoot 'tmp/world'
    $inputPath=ConvertFrom-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -RelativePath $Reference.path
    if(-not (Test-ProjectWorldOuterPathUnder -Path $inputPath -Root $scratch)){throw 'Saved gameplay reference is outside scratch.'}
    if((Get-ProjectWorldFileSha256 $inputPath) -cne $Reference.sha256){throw 'Saved gameplay reference changed.'}
    $routes=Get-Content $inputPath -Raw | ConvertFrom-Json
    $scenePath=ConvertFrom-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -RelativePath $routes.scene_input.path
    if(-not (Test-ProjectWorldOuterPathUnder -Path $scenePath -Root $scratch)){throw 'Saved gameplay scene is outside scratch.'}
    if((Get-ProjectWorldFileSha256 $scenePath) -cne $routes.scene_input.sha256){throw 'Saved gameplay scene reference changed.'}
    $scene=Get-Content $scenePath -Raw | ConvertFrom-Json
    $originalPath=ConvertFrom-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -RelativePath $scene.original_fixture
    if(-not (Test-ProjectWorldOuterPathUnder -Path $originalPath -Root $scratch)){throw 'Saved gameplay source is outside scratch.'}
    if((Get-ProjectWorldFileSha256 $originalPath) -cne $scene.saved_files.($scene.original_fixture)){throw 'Saved gameplay source reference changed.'}
    $original=Get-Content $originalPath -Raw | ConvertFrom-Json
    if($original.canonical_result.sha256 -cne $CompileSha256){throw 'Saved gameplay reference consumes another canonical result.'}
    $plugins=@($scene.saved_files.PSObject.Properties.Name | Where-Object {$_ -clike '*.uplugin'})
    if($plugins.Count -ne 1){throw 'The reference needs its one existing private content mount.'}
    $plugin=ConvertFrom-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -RelativePath $plugins[0]
    if(-not (Test-ProjectWorldOuterPathUnder -Path $plugin -Root $scratch) -or
        (Get-ProjectWorldFileSha256 $plugin) -cne $scene.saved_files.($plugins[0])){throw 'The reference content mount is foreign or changed.'}
    $descriptor=Get-Content $plugin -Raw | ConvertFrom-Json
    if(-not $descriptor.CanContainContent -or ($descriptor.PSObject.Properties['Modules'] -and @($descriptor.Modules).Count -gt 0)) {
        throw 'The reference mount must remain content-only.'
    }
    if(-not (Test-ProjectWorldOuterPathUnder -Path $OutputDirectory -Root $scratch)){throw 'Saved gameplay evidence is outside scratch.'}
    if(Test-Path $OutputDirectory){throw 'Saved gameplay requires fresh evidence.'}
    $roots=Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName ProjectWorldData
    $active=Read-ProjectWorldActiveSet -ManifestRoot $roots.ManifestRoot -ProjectRoot $ProjectRoot
    $scope=Get-ProjectWorldMapScopeId -MapPackage $Map -GeneratedPackageRoot $roots.GeneratedPackageRoot
    if($null -eq $active -or -not $active.Manifests.Contains($scope)){throw 'Saved gameplay has no accepted product map.'}
    $selected=@($active.Record.scopes | Where-Object {
        $manifest=$active.Manifests[$_.scope_id]
        $_.scope_id -ceq $scope -or ($scope -cin @($manifest.consumer_references) -and $manifest.owning_layer -cin @('terrain','structures'))
    })
    if(@($selected | Where-Object {$active.Manifests[$_.scope_id].owning_layer -ceq 'terrain'}).Count -ne 1){throw 'Saved gameplay needs one actual Terrain owner.'}
    New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
    $files=[ordered]@{}
    function Copy-ProofPin($Key,$Source,$Name) {
        $target=Join-Path $OutputDirectory $Name;Copy-Item -LiteralPath $Source -Destination $target
        $files[$Key]=$target
        return @{path=(ConvertTo-ProjectWorldOuterRelativePath -ProjectRoot $ProjectRoot -Path $target);sha256=(Get-ProjectWorldFileSha256 $target)}
    }
    $world=Copy-ProofPin 'active' (Join-Path $roots.ManifestRoot 'active_set.json') 'active.json'
    $manifests=@(foreach($entry in $selected) {
        $manifest=$active.Manifests[$entry.scope_id]
        if($manifest.owning_layer -cne 'map' -and $manifest.input_identity.compile_result_sha256 -cne $CompileSha256){throw 'Saved floor owner has another effective input.'}
        Copy-ProofPin ('manifest_'+$manifest.owning_layer) (Join-Path $roots.ManifestRoot $entry.manifest_path) ($manifest.owning_layer+'.json')
    })
    $files.input=Join-Path $OutputDirectory 'input.json';Copy-Item -LiteralPath $inputPath -Destination $files.input
    $files.proof=Join-Path $OutputDirectory 'proof.json'
    Write-ProjectWorldJson -Path $files.proof -Document @{schema='project-world-saved-region-gameplay:v1';map=$Map;
        compile_result_sha256=$CompileSha256;reference_gameplay_sha256=$Reference.sha256;world_active=$world;manifests=$manifests}
    return [pscustomobject]@{Files=$files;Plugin=$plugin;Input=$files.input;Proof=$files.proof}
}

function Invoke-ProjectWorldSavedRegionGameplay {
    param([string]$ProjectRoot,[string]$Map,[string]$CompileSha256,[object]$Reference,[string]$OutputDirectory)
    $prepared=New-ProjectWorldSavedRegionProof @PSBoundParameters
    $names=@('PROJECT_WORLD_STRUCTURE_GAME_MODE','PROJECT_WORLD_STRUCTURE_PAWN_CLASS','PROJECT_WORLD_STRUCTURE_PAWN_DEFINITION')
    $prior=@{};foreach($name in $names){$prior[$name]=[Environment]::GetEnvironmentVariable($name)}
    try {
        [Environment]::SetEnvironmentVariable($names[0],'/Script/ProjectSinglePlay.SinglePlayerGameMode')
        [Environment]::SetEnvironmentVariable($names[1],'/Script/ProjectCharacter.DefinitionCharacter')
        [Environment]::SetEnvironmentVariable($names[2],'ObjectDefinition:Hero')
        $extra=@('-ProjectSkipFrontEnd','-SCCProvider=None','-nop4','-RenderOffScreen','-ResX=1280','-ResY=720',
            ('-PLUGIN="'+$prepared.Plugin+'"'),('-ProjectWorldRegionalGameplay="'+$prepared.Input+'"'),
            ('-ProjectWorldRegionalGameplayHash='+$Reference.sha256),('-ProjectWorldSavedRegionGameplay="'+$prepared.Proof+'"'),
            ('-ProjectWorldSavedRegionGameplayHash='+(Get-ProjectWorldFileSha256 $prepared.Proof)),
            ('-EditorPerProjectUserSettingsINI="'+(Join-Path $OutputDirectory 'editor.ini')+'"'))
        $test='Project.World.Realization.MeshTerrain.CanonicalRegionGameplay'
        & (Join-Path $ProjectRoot 'scripts/ue/test/unit/run_cpp_tests_safe.ps1') -Map $Map -TestFilter ('^'+$test+'$') `
            -ExpectedTestNames @($test) -Mode Gate -UseTestExitOnly -TimeoutSeconds 720 `
            -LogDirectory (Join-Path $OutputDirectory 'native') -ExtraArgs ($extra -join ' ') | Out-Host
        if($LASTEXITCODE -ne 0){throw 'Actual saved-region Hero walking refused.'}
        $receipt=Join-Path $OutputDirectory 'gameplay.json'
        $native=Get-Content $receipt -Raw | ConvertFrom-Json
        if($native.status -cne 'accepted' -or $native.saved_product_map -cne $Map -or
            $native.compile_result_sha256 -cne $CompileSha256 -or -not $native.foreign_floor_actual_hero_control_passed){throw 'Saved-region Hero receipt is incomplete.'}
        $prepared.Files.receipt=$receipt
        return $prepared
    } finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$prior[$name])}}
}
