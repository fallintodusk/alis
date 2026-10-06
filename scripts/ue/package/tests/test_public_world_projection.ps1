#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackageRoot = Split-Path -Parent $ScriptDir
. (Join-Path $PackageRoot 'public_world_projection.ps1')
. (Join-Path $PackageRoot 'isolated_source_workspace.ps1')
. (Join-Path $PackageRoot 'release_definition_assets.ps1')
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PackageRoot))
. (Join-Path $ProjectRoot 'scripts\ue\world\generated_content_transaction.ps1')
$TestParent = Join-Path $ProjectRoot 'tmp\release\tests\public-world-projection'
$TestRoot = Join-Path $TestParent ([Guid]::NewGuid().ToString('N'))
$CleanupWorkParent = $null

try {
    New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null
    $tokens = $null
    $parseErrors = $null
    $projectionAst = [System.Management.Automation.Language.Parser]::ParseFile(
        (Join-Path $PackageRoot 'public_world_projection.ps1'), [ref]$tokens, [ref]$parseErrors)
    if ($parseErrors.Count -ne 0) {
        throw 'Public World projection script did not parse.'
    }
    $transactionCalls = @($projectionAst.FindAll({
        param($node)
        $node -is [System.Management.Automation.Language.CommandAst] -and
            $node.GetCommandName() -in @('Get-ProjectWorldGeneratedPaths', 'Remove-ProjectWorldGeneratedPaths')
    }, $true))
    if ($transactionCalls.Count -ne 2) {
        throw "Expected two generated-content transaction calls in public projection; found $($transactionCalls.Count)."
    }
    foreach ($call in $transactionCalls) {
        $parameters = (Get-Command $call.GetCommandName()).Parameters
        foreach ($argument in @($call.CommandElements | Where-Object {
                    $_ -is [System.Management.Automation.Language.CommandParameterAst]
                })) {
            if (-not $parameters.ContainsKey($argument.ParameterName)) {
                throw "Public World projection passes unknown $($call.GetCommandName()) parameter: $($argument.ParameterName)"
            }
        }
    }
    $realizerCalls = @($projectionAst.FindAll({
        param($node)
        $node -is [System.Management.Automation.Language.CommandAst] -and
            $node.CommandElements.Count -gt 0 -and
            $node.CommandElements[0].Extent.Text -ceq '$realizer'
    }, $true))
    if ($realizerCalls.Count -ne 1) {
        throw "Expected one public World realizer call; found $($realizerCalls.Count)."
    }
    $realizerParameters = (Get-Command (Join-Path $ProjectRoot `
        'scripts\ue\world\realize_canonical_world.ps1')).Parameters
    foreach ($argument in @($realizerCalls[0].CommandElements | Where-Object {
                $_ -is [System.Management.Automation.Language.CommandParameterAst]
            })) {
        if (-not $realizerParameters.ContainsKey($argument.ParameterName)) {
            throw "Public World projection passes unknown realizer parameter: $($argument.ParameterName)"
        }
    }
    $runtimeArguments = @($realizerCalls[0].CommandElements | Where-Object {
            $_ -is [System.Management.Automation.Language.CommandParameterAst] -and
            $_.ParameterName -ceq 'RuntimeProfile'
        })
    if ($runtimeArguments.Count -ne 1 -or
        $realizerCalls[0].CommandElements[$realizerCalls[0].CommandElements.IndexOf($runtimeArguments[0]) + 1].Extent.Text -cne '$world.Runtime') {
        throw 'Public World realization does not pass the pinned runtime profile.'
    }
    $dataRoot = Join-Path $ProjectRoot 'Plugins\World\ProjectWorldData\Data'
    $publicRuntimeIds = [ordered]@{
        kazan_territory_public_v1 = 'kazan_territory_512_1536_v1'
        manhattan_showcase_public_v1 = 'manhattan_showcase_512_1536_v1'
    }
    foreach ($name in $publicRuntimeIds.Keys) {
        $profile = Get-Content -LiteralPath (Join-Path $dataRoot "Profiles\Realization\$name.realization.json") `
            -Raw | ConvertFrom-Json
        $runtimePath = Resolve-ProjectWorldPublicRuntimeProfile `
            -DataRoot $dataRoot -RealizationProfile $profile
        if (@($profile.layers | Where-Object { $_.layer_id -ceq 'vegetation' }).Count -ne 0) {
            throw "Reduced public World profile unexpectedly includes vegetation: $name."
        }
        if ([string]$profile.runtime_profile_id -cne $publicRuntimeIds[$name] -or
            $runtimePath -ceq '' -or
            [string](Get-Content -LiteralPath $runtimePath -Raw | ConvertFrom-Json).profile_id -cne
                [string]$profile.runtime_profile_id) {
            throw "Public World runtime profile is absent or has the wrong identity: $name."
        }
    }
    $fixtureRuntime = Join-Path $TestRoot 'Runtime'
    New-Item -ItemType Directory -Path $fixtureRuntime -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $fixtureRuntime 'pinned.json'), '{"profile_id":"pinned"}')
    $pinnedPath = Resolve-ProjectWorldPublicRuntimeProfile -DataRoot $TestRoot `
        -RealizationProfile ([pscustomobject]@{ runtime_profile_id = 'pinned' })
    if ($pinnedPath -cne (Join-Path $fixtureRuntime 'pinned.json')) {
        throw 'Pinned public World runtime profile did not resolve.'
    }
    [IO.File]::WriteAllText((Join-Path $fixtureRuntime 'pinned.json'), '{"profile_id":"other"}')
    $mismatchRejected = $false
    try {
        Resolve-ProjectWorldPublicRuntimeProfile -DataRoot $TestRoot `
            -RealizationProfile ([pscustomobject]@{ runtime_profile_id = 'pinned' }) | Out-Null
    }
    catch {
        $mismatchRejected = $_.Exception.Message -like '*identity drift*'
    }
    if (-not $mismatchRejected) {
        throw 'Mismatched public World runtime profile was accepted.'
    }
    $rejectedReceipt = Join-Path $TestRoot 'rejected-realization.json'
    [IO.File]::WriteAllText($rejectedReceipt,
        '{"status":"rejected","errors":[{"code":"runtime-acceptance","message":"Structural budget failed.","detail":"generated_actor_count=5001"}]}')
    $failure = Get-ProjectWorldPublicRealizationFailure `
        -WorldName 'kazan' -ExitCode 5 -ReceiptPath $rejectedReceipt
    if ($failure -notlike '*runtime-acceptance*' -or
        $failure -notlike '*generated_actor_count=5001*') {
        throw 'Public World projection hid the realizer rejection detail.'
    }
    $file = Join-Path $TestRoot 'asset.uasset'
    [IO.File]::WriteAllBytes($file, [byte[]](1, 2, 3))
    $definitionAssets = @(Get-ProjectReleaseDefinitionAssets -Root $ProjectRoot)
    if (@($definitionAssets | Where-Object { $_.path -like 'Plugins/Resources/ProjectObject/*' }).Count -eq 0 -or
        @($definitionAssets | Where-Object { $_.path -like 'Plugins/Resources/ProjectExperienceData/*' }).Count -eq 0) {
        throw 'Developer release contract did not select both definition owners.'
    }
    Assert-ProjectReleaseDefinitionAssets -Root $ProjectRoot -Assets $definitionAssets
    $corruptDefinitionFailed = $false
    try {
        Assert-ProjectReleaseDefinitionAssets -Root $TestRoot -Assets @(
            [pscustomobject]@{ path = 'asset.uasset'; sha256 = ('0' * 64) })
    }
    catch {
        $corruptDefinitionFailed = $_.Exception.Message -like '*differs from its accepted manifest*'
    }
    if (-not $corruptDefinitionFailed) {
        throw 'A corrupted developer definition asset was accepted.'
    }
    $records = @([pscustomobject]@{ Source = $file })
    $before = Get-ProjectWorldProjectionDigest -Records $records
    [IO.File]::WriteAllBytes($file, [byte[]](1, 2, 4))
    $after = Get-ProjectWorldProjectionDigest -Records $records
    if ($before -ceq $after) {
        throw 'Projection restoration digest did not detect changed bytes.'
    }
    $resolved = Convert-ProjectWorldPackageRootToContentPath `
        -ContentRoot $TestRoot -PackageRoot '/ProjectWorldData/Generated/Territory/Water/'
    if (-not $resolved.EndsWith('Generated\Territory\Water')) {
        throw "Public package root resolved incorrectly: $resolved"
    }
    $rejected = $false
    try {
        Convert-ProjectWorldPackageRootToContentPath `
            -ContentRoot $TestRoot -PackageRoot '/Other/Generated/Territory/Water/' | Out-Null
    }
    catch {
        $rejected = $_.Exception.Message -like '*Unsupported public World artifact root*'
    }
    if (-not $rejected) {
        throw 'Foreign public World asset owner was not rejected.'
    }
    $inputSource = Get-Content -LiteralPath (Join-Path $PackageRoot 'prepare_release_inputs.ps1') -Raw
    if ($inputSource -match 'Find-PublicAssetRoot' -or
        $inputSource -match 'Sort-Object\s+LastWriteTimeUtc') {
        throw 'Release inputs still discover a historical public World projection.'
    }
    if ($inputSource -notmatch 'Invoke-WithProjectWorldPublicProjection') {
        throw 'Release inputs do not use the current ProjectWorldData projection owner.'
    }
    $producerInputs = @(Get-ProjectWorldPublicProducerInputs -ProjectRoot $ProjectRoot)
    $vegetationDescriptor = Get-Content -LiteralPath (Join-Path $ProjectRoot `
        'Plugins\World\ProjectWorld\Data\Producers\project_vegetation_instances.json') -Raw |
        ConvertFrom-Json
    if (@($vegetationDescriptor.data_inputs | Where-Object { $_ -cnotin $producerInputs }).Count -gt 0 -or
        $inputSource -notmatch 'Get-ProjectWorldPublicProducerInputs' -or
        $inputSource -notmatch '(?s)\$RequiredReleaseAssets\s*=\s*@\(.*?\$ProducerInputs.*?\)' -or
        $inputSource -notmatch '-RequiredPaths\s+\$RequiredReleaseAssets') {
        throw 'Release inputs do not materialize World producer data inputs before projection.'
    }
    $assetMaterializationStep = $inputSource.IndexOf("-RequiredPaths @('Content', 'Plugins')", [StringComparison]::Ordinal)
    $worldBuildStep = $inputSource.IndexOf('Isolated World projection Editor build', [StringComparison]::Ordinal)
    $projectionStep = $inputSource.IndexOf('Invoke-WithProjectWorldPublicProjection', [StringComparison]::Ordinal)
    if ($assetMaterializationStep -lt 0 -or $assetMaterializationStep -gt $worldBuildStep) {
        throw 'Release projection exposes Git LFS pointers to the Unreal asset registry.'
    }
    if ($worldBuildStep -lt 0 -or $worldBuildStep -gt $projectionStep) {
        throw 'Release projection runs before its isolated Editor modules are built.'
    }
    if ($inputSource -notmatch 'git\s+-c\s+core\.longpaths=true\s+-c\s+advice\.detachedHead=false\s+clone\s+--quiet') {
        throw 'Public clean checkout does not enable long paths and suppress expected clone progress and detached-tag advice.'
    }
    if ($inputSource -notmatch 'git\s+-C\s+\$CleanCheckout\s+config\s+core\.longpaths\s+true') {
        throw 'Public clean checkout does not persist Windows long-path support for consumer scripts.'
    }
    $mirrorSource = Get-Content -LiteralPath `
        (Join-Path $ProjectRoot 'scripts\git\mirror\mirror_to_github.sh') -Raw
    if ($mirrorSource -notmatch 'config core\.longpaths true') {
        throw 'Reviewed public source repositories do not persist Windows long-path support.'
    }
    if ($inputSource -notmatch 'Join-Path \$PrivateProjectRoot "tmp\\release\\c"' -or
        $inputSource -match '\$CleanCheckout\s*=\s*Join-Path \$WorkRoot') {
        throw 'Public clean checkout is not isolated under the short release-owned path.'
    }
    if ($inputSource -notmatch 'New-ProjectIsolatedCommittedWorkspace' -or
        $inputSource -notmatch '\$ProjectRoot\s*=\s*\[string\]\$IsolatedWorkspace.checkout_root') {
        throw 'Public projection is not bound to the exact committed checkout.'
    }
    if ($inputSource -notmatch '(?s)finally\s*\{.*Remove-Item\s+-LiteralPath\s+\$CleanCheckout\s+-Recurse\s+-Force' -or
        $inputSource -notmatch '(?s)finally\s*\{.*-not\s+\$InputsPromoted.*Remove-Item\s+-LiteralPath\s+\$WorkRoot\s+-Recurse\s+-Force') {
        throw 'Release input preparation does not clean ephemeral checkouts and failed work trees.'
    }
    $CleanupVersion = "9.8.$([DateTime]::UtcNow.Ticks)"
    $CleanupTag = "v$CleanupVersion"
    $CleanupWorkParent = Join-Path $ProjectRoot "tmp\release\work\$CleanupTag"
    $CleanupInputRoot = Join-Path $TestRoot 'failed-inputs'
    $MissingCommitFailed = $false
    try {
        & (Join-Path $PackageRoot 'prepare_release_inputs.ps1') `
            -ReleaseVersion $CleanupVersion -InputRoot $CleanupInputRoot *> $null
    }
    catch {
        $MissingCommitFailed = $_.Exception.Message -like '*exact 40-digit SourceCommit*'
    }
    if (-not $MissingCommitFailed -or (Test-Path -LiteralPath $CleanupInputRoot) -or
        (Test-Path -LiteralPath $CleanupWorkParent)) {
        throw 'Missing SourceCommit did not refuse before release input mutation.'
    }
    $PreparationFailed = $false
    try {
        & (Join-Path $PackageRoot 'prepare_release_inputs.ps1') `
            -ReleaseVersion $CleanupVersion `
            -InputRoot $CleanupInputRoot `
            -PublicAssetRoot $TestRoot *> $null
    }
    catch {
        $PreparationFailed = $true
    }
    if (-not $PreparationFailed) {
        throw 'Invalid release inputs unexpectedly passed preparation.'
    }
    if (Test-Path -LiteralPath $CleanupWorkParent) {
        throw 'Failed release input preparation left its work tree behind.'
    }
    Write-Host '[OK] Public World release projection is current-owner-bound and drift-sensitive.'
}
finally {
    if ($CleanupWorkParent -and (Test-Path -LiteralPath $CleanupWorkParent)) {
        Remove-Item -LiteralPath $CleanupWorkParent -Recurse -Force
    }
    if (Test-Path -LiteralPath $TestRoot) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force
    }
}
