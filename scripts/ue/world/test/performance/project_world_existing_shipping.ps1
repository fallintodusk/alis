#Requires -Version 5.1
# License terms: see repository root LICENSE.

function Invoke-ManhattanExistingShippingProof {
    param(
        [Parameter(Mandatory)][string]$PackageRoot,
        [Parameter(Mandatory)][string]$ExpectedPackageHash,
        [Parameter(Mandatory)][string]$ExpectedExecutableHash
    )
    $resolvedPackage = (Resolve-Path -LiteralPath $PackageRoot).Path
    $executable = Get-ManhattanShowcaseExecutable -PackageRoot $resolvedPackage
    Assert-ManhattanShowcase ((Get-ProjectWorldPackagePayloadDigest -Path $resolvedPackage) -ceq
        $ExpectedPackageHash -and (Get-FileHash -LiteralPath $executable -Algorithm SHA256).
        Hash.ToLowerInvariant() -ceq $ExpectedExecutableHash) 'Existing Shipping package identity differs.'
    $source = Get-ManhattanPackageSourceIdentity -PackageRoot $resolvedPackage
    $revision = (& git -C $projectRoot rev-parse HEAD).Trim()
    $sourceTool = Join-Path $projectRoot 'scripts/ue/package/prepare_release.py'
    $sourceState = @(& python $sourceTool source-state --source-root $projectRoot)
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0 -and $revision -ceq $source.Revision -and
        $sourceState.Count -eq 1 -and $sourceState[0] -ceq $source.StateHash) `
        'Existing Shipping package does not match the current source.'
    $script:runtimeProfileHash = (Get-FileHash -LiteralPath $runtimeProfile -Algorithm SHA256).
        Hash.ToLowerInvariant()
    $script:operationId = "manhattan_shipping_$runId"
    New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
    $authorityPath = Join-Path $evidenceRoot 'water-viewpoint-authority.json'
    $resolver = Join-Path $projectRoot 'tools/World/ExecutionEnvironment/resolve_python_host.ps1'
    $pythonHost = @(& $resolver)
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0 -and $pythonHost.Count -eq 1) 'World Python resolution failed.'
    $profile = Join-Path $projectRoot `
        'Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/manhattan_showcase_v1.compile.json'
    $materialized = @(& $pythonHost[0] -S (Join-Path $projectRoot `
        'tools/World/CanonicalCompilation/bootstrap.py') materialize --profile $profile)
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0 -and $materialized.Count -gt 0) 'Manhattan materialization failed.'
    $canonical = $materialized[-1] | ConvertFrom-Json
    Assert-ManhattanShowcase ($canonical.status -ceq 'accepted') 'Manhattan canonical authority was refused.'
    $compileResult = [string]$canonical.result_path
    if (-not [IO.Path]::IsPathRooted($compileResult)) { $compileResult = Join-Path $projectRoot $compileResult }
    # Source-selected triangle centroid inside the pinned Hudson River polygon.
    $target = @(-201898.333, 30363.667)
    & (Join-Path $projectRoot 'scripts/ue/world/test/verify_canonical_feature_viewpoint.ps1') `
        -CompileResult $compileResult -ActiveManifestSet (Join-Path $projectRoot `
            'Plugins/World/ProjectWorldData/Data/Manifests/active_set.json') `
        -WaterScopeId 'layer_manhattan_showcase_v1_water' `
        -UnrealXCentimeters $target[0] -UnrealYCentimeters $target[1] `
        -FeatureId 'alis:osm:relation:2389611' -FeatureClass water -ReceiptPath $authorityPath | Out-Null
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0) 'Manhattan Water viewpoint authority failed.'
    $correctnessPath = Join-Path $evidenceRoot 'shipping-product-route.json'
    $script:buildingReferencePath = New-ProjectWorldBuildingReference -ProjectRoot $projectRoot `
        -MapPackage $mapPackage -RuntimeProfileHash $script:runtimeProfileHash -EvidenceRoot $evidenceRoot `
        -RealizationProfile (Join-Path $projectRoot `
            'Plugins/World/ProjectWorldData/Data/Profiles/Realization/manhattan_showcase_v1.realization.json')
    $logPath = Join-Path $evidenceRoot 'shipping-game.log'
    $exitCode = Invoke-ManhattanShowcaseGame -Executable $executable -Configuration Shipping `
        -RunOperationId $operationId -CorrectnessPath $correctnessPath -LogPath $logPath
    Assert-ManhattanShowcase ($exitCode -eq 0) 'Manhattan Shipping gameplay process failed.'
    $correctness = Read-ManhattanShowcaseCorrectness -Path $correctnessPath -Configuration Shipping `
        -ExpectedOperationId $operationId
    $waterPath = Join-Path $evidenceRoot 'shipping-water.json'
    $referencePath = Join-Path $evidenceRoot 'water.png'
    $repeatPath = Join-Path $evidenceRoot 'water.control.png'
    $productPath = Join-Path $evidenceRoot 'water-product.png'
    $waterExitCode = Invoke-ProjectWorldShippingWaterProof -OperationId $operationId `
        -Experience $experienceId -MapPackage $mapPackage -RuntimeProfileId 'manhattan_showcase_512_1536_v1' `
        -RuntimeProfileHash $script:runtimeProfileHash -Target $target -GameTimeoutSeconds $GameTimeoutSeconds `
        -Executable $executable -ResultPath $waterPath -ReferencePath $referencePath `
        -RepeatPath $repeatPath -ProductPath $productPath
    Assert-ManhattanShowcase ($waterExitCode -eq 0) 'Manhattan Shipping Water process failed.'
    $water = Read-ProjectWorldShippingWaterProof -Path $waterPath -OperationId $operationId `
        -MapPackage $mapPackage -RuntimeProfileId 'manhattan_showcase_512_1536_v1' `
        -RuntimeProfileHash $script:runtimeProfileHash
    $temporalPath = Join-Path $evidenceRoot 'water-temporal.json'
    & (Join-Path $projectRoot 'scripts/ue/world/test/water_temporal_stability.ps1') `
        -ReferencePath $referencePath -RepeatPath $repeatPath -ReceiptPath $temporalPath
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0 -and
        (Get-Content -LiteralPath $temporalPath -Raw | ConvertFrom-Json).status -ceq 'accepted') `
        'Manhattan Shipping Water temporal proof failed.'
    Assert-ManhattanShowcase ((Get-ProjectWorldPackagePayloadDigest -Path $resolvedPackage) -ceq
        $ExpectedPackageHash -and (Get-FileHash -LiteralPath $executable -Algorithm SHA256).
        Hash.ToLowerInvariant() -ceq $ExpectedExecutableHash) 'Shipping payload changed during Manhattan proof.'
    $afterState = @(& python $sourceTool source-state --source-root $projectRoot)
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0 -and $afterState.Count -eq 1 -and
        $afterState[0] -ceq $source.StateHash) 'Source changed during Manhattan Shipping proof.'
    $artifacts = [ordered]@{
        shipping_correctness = $correctnessPath; shipping_product_screenshot = [string]$correctness.screenshot
        shipping_building_reference = $script:buildingReferencePath
        shipping_water_proof = $waterPath; shipping_water_viewpoint_authority = $authorityPath
        shipping_water_reference = [string]$water.reference_screenshot
        shipping_water_repeat = [string]$water.repeat_screenshot
        shipping_water_product = [string]$water.final_color_screenshot
        shipping_water_temporal = $temporalPath
    }
    $hashes = [ordered]@{}
    foreach ($entry in $artifacts.GetEnumerator()) {
        $hashes[$entry.Key] = (Get-FileHash -LiteralPath $entry.Value -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $receipt = [ordered]@{
        schema_version = 1; status = 'accepted'; operation_id = $operationId
        revision = $source.Revision; source_state_sha256 = $source.StateHash
        map_package = $mapPackage; runtime_profile = 'manhattan_showcase_512_1536_v1'
        runtime_profile_sha256 = $script:runtimeProfileHash; shipping_exit_code = $exitCode
        shipping_water_exit_code = $waterExitCode; shipping_executable_sha256 = $ExpectedExecutableHash
        shipping_package_sha256 = $ExpectedPackageHash; artifacts = $artifacts; artifact_sha256 = $hashes
    }
    $receiptPath = Join-Path $evidenceRoot 'shipping-proof.json'
    [IO.File]::WriteAllText($receiptPath, ($receipt | ConvertTo-Json -Depth 14) + "`n",
        [Text.UTF8Encoding]::new($false))
    return $receiptPath
}
