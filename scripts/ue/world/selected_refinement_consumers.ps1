# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
# Read-only consumers run while the selected target's existing outer snapshot is active.
. (Join-Path $PSScriptRoot 'selected_refinement_gameplay.ps1')
. (Join-Path $PSScriptRoot 'capture_input_paths.ps1')
function Assert-ProjectWorldSelectedConsumers {
    param($ProjectRoot,$Request,$Profile,$Settings)
    if (-not $Request.PSObject.Properties['consumer_verification']) { return }
    if ($Settings.world_data_plugin -cne 'ProjectWorldData') { throw 'Runtime verification requires the product World data owner.' }
    $plan=Resolve-ProjectWorldCapturePlanPath -ProjectRoot $ProjectRoot -VantagePlan (Join-Path $ProjectRoot $Request.consumer_verification.vantage_plan_path)
    if ((Get-ProjectWorldFileSha256 $plan) -cne $Request.consumer_verification.vantage_plan_sha256) { throw 'Runtime vantage plan changed.' }
    $operations=@()
    foreach($pair in @(@('Kazan','run_kazan_playable_tour.ps1'),@('Manhattan','run_manhattan_showcase_prototype.ps1'))) {
        $operation=& (Join-Path $ProjectRoot ('scripts/ue/world/test/performance/'+$pair[1])) -DescribeOperation
        $matches=@($Profile.profiles.PSObject.Properties | Where-Object {
            $_.Value.world_data_plugin -ceq 'ProjectWorldData' -and $_.Value.map_package -ceq $operation.Map -and
            [IO.Path]::GetFullPath((Join-Path $ProjectRoot $_.Value.runtime_profile)) -ceq [IO.Path]::GetFullPath($operation.RuntimePath)
        })
        if($matches.Count -ne 1){throw 'Runtime verification requires both existing product operations and their exact runtime profiles.'}
        $operations+= $operation
    }
    if (@($operations | Where-Object Map -CEQ $Settings.map_package).Count -ne 1) { throw 'Selected map has no supported product operation.' }
}

function Invoke-ProjectWorldSelectedConsumers {
    param($ProjectRoot,$Request,$Profile,$Settings,$Candidate,$EvidenceRoot,$WorldPath,$CanonicalPath)
    Assert-ProjectWorldSelectedConsumers -ProjectRoot $ProjectRoot -Request $Request -Profile $Profile -Settings $Settings
    . (Join-Path $ProjectRoot 'scripts/ue/world/test/performance/project_world_performance_evidence.ps1')
    $directory=Join-Path $ProjectRoot ('Saved/Validation/WorldRealization/refinement-consumers/'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $owner=Join-Path $ProjectRoot 'Plugins/World/ProjectWorldData'
    $before=Get-ProjectWorldPackagePayloadDigest -Path $owner
    $worldBefore=Get-ProjectWorldFileSha256 $WorldPath
    $canonicalBefore=Get-ProjectWorldFileSha256 $CanonicalPath
    $report=[ordered]@{status='rejected';world_sha256=$worldBefore;canonical_sha256=$canonicalBefore;
        authority_before_sha256=$before;authority_after_sha256='';unchanged=$false;files=[ordered]@{};operations=@()}
    function Add-ConsumerPin($Key,$Path) {
        $full=[IO.Path]::GetFullPath($Path)
        if (-not $full.StartsWith($ProjectRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Consumer evidence is outside project.'}
        $report.files[$Key]=@{path=$full.Substring($ProjectRoot.TrimEnd('\').Length+1).Replace('\','/');sha256=Get-ProjectWorldFileSha256 $full}
    }
    try {
        Add-ConsumerPin 'plan' (Join-Path $ProjectRoot $Request.consumer_verification.vantage_plan_path)
        Add-ConsumerPin 'profile' (Join-Path $ProjectRoot $Request.validation_profile_path)
        $native=Get-Content (Join-Path $EvidenceRoot 'saved-native.json') -Raw | ConvertFrom-Json
        $terrain=@($native.layer_inventories | Where-Object layer_id -CEQ 'terrain')
        $postPath=Join-Path $EvidenceRoot 'terrain-postbuild-inventory.json'
        $post=Get-Content $postPath -Raw | ConvertFrom-Json
        if($terrain.Count -ne 1 -or $post.status -cne 'accepted' -or $post.compile_result_sha256 -cne $Candidate.result_sha256){throw 'Selected Terrain census is unqualified.'}
        $active=Get-Content $WorldPath -Raw | ConvertFrom-Json
        $scope=@($active.scopes | Where-Object scope_id -CEQ $terrain[0].scope_id)
        if($scope.Count -ne 1){throw 'Selected Terrain manifest is missing.'}
        $manifest=Join-Path (Split-Path $WorldPath) $scope[0].manifest_path
        if((Get-ProjectWorldFileSha256 $manifest) -cne $scope[0].manifest_sha256){throw 'Selected Terrain manifest changed.'}
        $manifestCopy=Join-Path $EvidenceRoot 'selected-terrain-manifest.json'
        Copy-Item -LiteralPath $manifest -Destination $manifestCopy
        Add-ConsumerPin 'terrain_manifest' $manifestCopy
        Add-ConsumerPin 'postbuild' $postPath
        $integration=Join-Path $ProjectRoot 'scripts/ue/world/test/integration'
        & (Join-Path $integration 'verify_terrain_projection.ps1') -Map $Settings.map_package `
            -CompileResult (Join-Path $ProjectRoot $Candidate.materialized_result_path) -WorldDataPlugin $Settings.world_data_plugin `
            -ExpectedBaseCount @($terrain[0].canonical_inputs).Count -ExpectedSectionCount @($post.sections).Count `
            -EvidencePath (Join-Path $directory 'terrain.json') | Out-Host
        if($LASTEXITCODE -ne 0){throw 'Selected Terrain projection failed.'}
        Add-ConsumerPin 'terrain' (Join-Path $directory 'terrain.json')
        & (Join-Path $integration 'verify_saved_world_projection.ps1') -Map $Settings.map_package `
            -RealizationProfile (Join-Path $ProjectRoot $Settings.realization_profile) -WorldDataPlugin $Settings.world_data_plugin `
            -EvidencePath (Join-Path $directory 'world.json') | Out-Host
        if($LASTEXITCODE -ne 0){throw 'Selected saved World projection failed.'}
        Add-ConsumerPin 'world' (Join-Path $directory 'world.json')
        $plan=Get-Content (Join-Path $ProjectRoot $Request.consumer_verification.vantage_plan_path) -Raw | ConvertFrom-Json
        if($plan.PSObject.Properties['gameplay_scenes']) {
            foreach($scene in $plan.gameplay_scenes) {
                $config=$Profile.profiles.($scene.city.ToLowerInvariant())
                $played=Invoke-ProjectWorldSavedRegionGameplay -ProjectRoot $ProjectRoot -Map $config.map_package `
                    -CompileSha256 $scene.canonical_result_sha256 -Reference $scene.reference `
                    -OutputDirectory (Join-Path $EvidenceRoot ('saved-gameplay/'+$scene.city))
                foreach($key in $played.Files.Keys){Add-ConsumerPin ('gameplay_'+$scene.city+'_'+$key) $played.Files[$key]}
            }
        }
        foreach($pair in @(@('Kazan','run_kazan_playable_tour.ps1'),@('Manhattan','run_manhattan_showcase_prototype.ps1'))) {
            $operation=& (Join-Path $integration ('../performance/'+$pair[1])) -DescribeOperation
            $report.operations+=@{city=$operation.Name;map=$operation.Map;runtime=$operation.Runtime;
                runtime_sha256=Get-ProjectWorldFileSha256 $operation.RuntimePath}
            Add-ConsumerPin ('runtime_'+$operation.Name) $operation.RuntimePath
            & (Join-Path $ProjectRoot 'scripts/ue/world/audit_runtime_partition.ps1') -Map $operation.Map `
                -RuntimeProfiles @($operation.RuntimePath) -SelectedProfile $operation.Runtime `
                -EvidencePath (Join-Path $directory ($operation.Name+'-partition.json')) | Out-Host
            if($LASTEXITCODE -ne 0){throw 'Selected city partition audit failed.'}
            Add-ConsumerPin ('partition_'+$operation.Name) (Join-Path $directory ($operation.Name+'-partition.json'))
        }
        & (Join-Path $ProjectRoot 'scripts/ue/world/capture_visual_evidence.ps1') -Map $Settings.map_package `
            -VantagePlan (Join-Path $ProjectRoot $Request.consumer_verification.vantage_plan_path) `
            -WorldDataPlugin $Settings.world_data_plugin -OutputDirectory (Join-Path $EvidenceRoot 'selected-images') `
            -ReceiptPath (Join-Path $EvidenceRoot 'selected-capture.json') | Out-Host
        if($LASTEXITCODE -ne 0){throw 'Selected city visual capture failed.'}
        $capturePath=Join-Path $EvidenceRoot 'selected-capture.json'
        & python (Join-Path $ProjectRoot 'tools/World/VisualVerification/app/verify_capture.py') $capturePath | Out-Host
        if($LASTEXITCODE -ne 0){throw 'Selected city visual capture did not authenticate.'}
        Add-ConsumerPin 'capture' $capturePath
        $capture=Get-Content $capturePath -Raw | ConvertFrom-Json
        foreach($view in $capture.views){Add-ConsumerPin ('image_'+$view.name) (Join-Path $capture.screenshot_dir $view.file)}
        $tourLines=@(& (Join-Path $integration 'run_uncooked_playable_tour.ps1') -City Both 6>&1)
        $tourLines | Out-Host
        $marker=@($tourLines | ForEach-Object {[string]$_} | Where-Object {$_ -match '^Uncooked admission receipt: '})
        if($marker.Count -ne 1){throw 'Both-city product route emitted no unique admission receipt.'}
        $tourPath=$marker[0].Substring('Uncooked admission receipt: '.Length)
        Add-ConsumerPin 'tour' $tourPath
        $tour=Get-Content $tourPath -Raw | ConvertFrom-Json
        if($tour.status -cne 'accepted'){throw 'Both-city product route rejected.'}
        $index=0
        foreach($city in $tour.cities){foreach($artifact in $city.artifact_sha256.PSObject.Properties){Add-ConsumerPin ('tour_artifact_'+($index++)) $artifact.Name}}
        $report.status='accepted'
    }
    finally {
        $report.authority_after_sha256=Get-ProjectWorldPackagePayloadDigest -Path $owner
        $report.unchanged=$before -ceq $report.authority_after_sha256 -and
            $worldBefore -ceq (Get-ProjectWorldFileSha256 $WorldPath) -and $canonicalBefore -ceq (Get-ProjectWorldFileSha256 $CanonicalPath)
        if(-not $report.unchanged){$report.status='rejected'}
        $path=Join-Path $EvidenceRoot 'consumer-verification.json'
        Write-ProjectWorldJson -Path $path -Document $report
    }
    if($report.status -cne 'accepted'){throw 'Selected consumer verification rejected or changed authority.'}
    return $path
}
