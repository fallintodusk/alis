# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

<#
.SYNOPSIS
    Manhattan showcase prototype gate (D6): one Development Candidate plus a Shipping smoke
    through the real product route.

.DESCRIPTION
    This is the explicit non-interactive showcase operation. The showcase map intentionally
    has no gameplay-placement layer, so this runner - and only this runner - selects the
    product route's non-interactive policy with -ProjectWorldProductRouteSkipInteraction.
    Every other product gate stays mandatory, and the receipt records the policy through
    gameplay_interaction_required so the omission is authenticated rather than assumed.

    The policy is chosen by the operation, not inferred from the city or map inside
    ProjectWorld. Kazan and default runners pass no skip flag and stay strict.

    This is a prototype smoke, not the Kazan release performance campaign.
#>

#Requires -Version 5.1

[CmdletBinding()]
param(
    [int]$GameTimeoutSeconds = 720,
    [switch]$SkipShipping,
    [switch]$DescribeOperation,
    [switch]$AcceptInconclusivePerformance,
    [string]$ExistingDevelopmentPackageRoot,
    [string]$ExpectedPackagePayloadSha256,
    [string]$ExpectedExecutableSha256
)

$ErrorActionPreference = 'Stop'
$worldRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $worldRoot))
$packageScript = Join-Path $projectRoot 'scripts\ue\package\package_release.ps1'
$runtimeProfile = Join-Path $projectRoot `
    'Plugins\World\ProjectWorldData\Data\Runtime\manhattan_showcase_512_1536_v1.json'
$mapPackage = '/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase'
$experienceId = 'ManhattanShowcase'

# Edge target: canonical cell x8:y-5, the densest far cell (3566 owned features), ~8.8 km
# from the centre so the centre must unload at the 1536 m loading range. Z clears the
# accepted 541 m maximum building height.
$edgeArgument = '651041,511455,60000'

$runId = [Guid]::NewGuid().ToString('N')
$operationId = "manhattan_showcase_prototype_$runId"
$evidenceRoot = Join-Path $projectRoot "Saved\Validation\WorldRealization\manhattan-showcase\$runId"
$ownerRoot = Join-Path $projectRoot 'tmp\world\manhattan_showcase'
$runtimeRoot = Join-Path $ownerRoot 'runtime'
$packageRoot = Join-Path $projectRoot 'Saved\PackageRelease\ManhattanShowcase'
$finalPackage = Join-Path $packageRoot 'Candidate'
. (Join-Path $PSScriptRoot 'project_world_performance_evidence.ps1')
. (Join-Path $PSScriptRoot 'project_world_product_route_arguments.ps1')

function Assert-ManhattanShowcase {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

function Test-ManhattanSamePath {
    param([string]$Actual, [string]$Expected)
    if ([string]::IsNullOrWhiteSpace($Actual) -or
        [string]::IsNullOrWhiteSpace($Expected)) {
        return $false
    }
    try {
        return [IO.Path]::GetFullPath($Actual).Equals(
            [IO.Path]::GetFullPath($Expected),
            [StringComparison]::OrdinalIgnoreCase)
    }
    catch {
        return $false
    }
}

function Get-ManhattanShowcaseSourceStateDigest {
    # Same shape as the accepted Kazan playable-tour contract: tracked diff against HEAD plus
    # the hashed untracked set, so a dirty tree is identified rather than assumed clean.
    $parts = [Collections.Generic.List[string]]::new()
    $parts.Add((@(& git -C $projectRoot diff --binary --no-ext-diff HEAD) -join "`n"))
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0) 'Unable to read tracked source state.'
    $untracked = @(& git -C $projectRoot ls-files --others --exclude-standard | Sort-Object)
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0) 'Unable to read untracked source state.'
    foreach ($relative in $untracked) {
        $path = Join-Path $projectRoot $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        $parts.Add("$relative|$hash")
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($parts -join "`n"))
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Assert-ManhattanShowcaseSourceState {
    param(
        [Parameter(Mandatory = $true)][string]$ExpectedSourceHash,
        [Parameter(Mandatory = $true)][string]$ExpectedRuntimeHash,
        [Parameter(Mandatory = $true)][string]$Stage
    )
    Assert-ManhattanShowcase ((Get-ManhattanShowcaseSourceStateDigest) -ceq $ExpectedSourceHash) `
        "Source state changed during the Manhattan showcase transaction at $Stage."
    $currentRuntimeHash = (Get-FileHash -LiteralPath $runtimeProfile -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-ManhattanShowcase ($currentRuntimeHash -ceq $ExpectedRuntimeHash) `
        "Runtime profile changed during the Manhattan showcase transaction at $Stage."
}

function Get-ManhattanShowcaseExecutable {
    param([Parameter(Mandatory = $true)][string]$PackageRoot)
    $candidates = @(Get-ChildItem -LiteralPath (Join-Path $PackageRoot 'Windows') `
        -Recurse -File -Filter 'Alis*.exe' |
        Where-Object { $_.FullName -match '[\\/]Alis[\\/]Binaries[\\/]Win64[\\/]' } |
        Sort-Object FullName)
    Assert-ManhattanShowcase ($candidates.Count -eq 1) `
        "Expected exactly one staged game executable under $PackageRoot."
    return $candidates[0].FullName
}

function Invoke-ManhattanShowcasePackage {
    param(
        [Parameter(Mandatory = $true)][string]$OutputRoot,
        [Parameter(Mandatory = $true)][string]$Configuration
    )
    # No -EngineRoot: packaging resolves UE_PATH (launcher engine), so the source engine is
    # never touched by this prototype gate.
    & $packageScript -OutputDir $OutputRoot -ClientConfig $Configuration `
        -RequiredCookMap $mapPackage
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0) `
        "$Configuration launcher-engine packaging failed."
}

function Invoke-ManhattanShowcaseGame {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$RunOperationId,
        [Parameter(Mandatory = $true)][string]$CorrectnessPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [string]$PerformancePath,
        [string]$CsvPath,
        [string]$SamplePath,
        [string]$ScreenshotPath
    )
    if ($PerformancePath -and ([string]::IsNullOrWhiteSpace($CsvPath) -or
            [string]::IsNullOrWhiteSpace($SamplePath) -or
            [string]::IsNullOrWhiteSpace($ScreenshotPath))) {
        throw 'Manhattan performance requires CSV, raw-sample, and screenshot paths.'
    }
    $routeParameters = @{
        Experience = $experienceId; Map = $mapPackage
        Runtime = 'manhattan_showcase_512_1536_v1'; RuntimeHash = $script:runtimeProfileHash
        Edge = $edgeArgument; OperationId = $RunOperationId
        CorrectnessPath = $CorrectnessPath; LogPath = $LogPath
    }
    if ($PerformancePath) {
        $routeParameters.PerformancePath = $PerformancePath
        $routeParameters.CsvPath = $CsvPath; $routeParameters.SamplePath = $SamplePath
        $routeParameters.ScreenshotPath = $ScreenshotPath
        $routeParameters.PreciseCenterReturn = $true
    }
    $routeParameters.SkipInteraction = $true
    $arguments = @(Get-ProjectWorldProductRouteArguments @routeParameters)
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments `
        -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit($GameTimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "$Configuration Manhattan showcase process exceeded the bounded timeout."
    }
    return $process.ExitCode
}

function Read-ManhattanShowcaseCorrectness {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$ExpectedOperationId
    )
    Assert-ManhattanShowcase (Test-Path -LiteralPath $Path -PathType Leaf) `
        "$Configuration product-route receipt is missing."
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json

    # Identity, real route, and every gate that is NOT interaction stays mandatory.
    Assert-ManhattanShowcase (
        [string]$receipt.status -ceq 'accepted' -and
        [string]$receipt.operation_id -ceq $ExpectedOperationId -and
        [string]$receipt.map_package -ceq $mapPackage -and
        [string]$receipt.runtime_profile -ceq 'manhattan_showcase_512_1536_v1' -and
        [string]$receipt.runtime_profile_sha256 -ceq $script:runtimeProfileHash -and
        [string]$receipt.build_configuration -ceq $Configuration -and
        [string]$receipt.game_mode -ceq '/Script/ProjectSinglePlay.SinglePlayerGameMode' -and
        [string]$receipt.pawn_class -ceq '/Script/ProjectCharacter.DefinitionCharacter' -and
        [bool]$receipt.project_loading_provenance -and
        [bool]$receipt.possessed_player -and
        [bool]$receipt.normal_movement -and
        [bool]$receipt.terrain_collision -and
        [bool]$receipt.road_collision -and
        [bool]$receipt.building_collision -and
        [bool]$receipt.center_unloaded_at_edge -and
        [bool]$receipt.edge_loaded -and
        [bool]$receipt.center_reloaded -and
        [bool]$receipt.preview_flight_restored) `
        "$Configuration Manhattan receipt failed its identity/correctness contract."

    # The non-interactive policy must be explicit in the receipt, not merely absent.
    Assert-ManhattanShowcase ($null -ne $receipt.PSObject.Properties['gameplay_interaction_required'] -and
        -not [bool]$receipt.gameplay_interaction_required) `
        "$Configuration Manhattan receipt did not record the non-interactive policy."

    Assert-ManhattanShowcase (Test-Path -LiteralPath ([string]$receipt.screenshot) -PathType Leaf) `
        "$Configuration Manhattan product-route screenshot is missing."
    return $receipt
}

function Get-ManhattanPackageSourceIdentity {
    param([Parameter(Mandatory = $true)][string]$PackageRoot)
    $summaryPath = Join-Path $PackageRoot 'package_summary.txt'
    Assert-ManhattanShowcase (Test-Path -LiteralPath $summaryPath -PathType Leaf) `
        'Existing Development package has no package summary.'
    $lines = @(Get-Content -LiteralPath $summaryPath)
    $revisions = @($lines | Where-Object { $_ -clike 'SourceRevision=*' })
    $states = @($lines | Where-Object { $_ -clike 'SourceStateSha256=*' })
    Assert-ManhattanShowcase ($revisions.Count -eq 1 -and $states.Count -eq 1 -and
        $revisions[0] -cmatch '^SourceRevision=[a-f0-9]{40}$' -and
        $states[0] -cmatch '^SourceStateSha256=[a-f0-9]{64}$') `
        'Existing Development package source identity is missing or ambiguous.'
    return [pscustomobject]@{
        Revision = $revisions[0].Substring('SourceRevision='.Length)
        StateHash = $states[0].Substring('SourceStateSha256='.Length)
    }
}

function Assert-ManhattanPerformanceChild {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$CorrectnessPath,
        [Parameter(Mandatory = $true)][string]$Operation,
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$SamplePath,
        [Parameter(Mandatory = $true)][string]$CsvPath,
        [Parameter(Mandatory = $true)][string]$ScreenshotPath,
        [Parameter(Mandatory = $true)][int]$ProcessExitCode
    )
    Assert-ManhattanShowcase (Test-Path -LiteralPath $Path -PathType Leaf) `
        'Manhattan Development performance receipt is missing.'
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $outcome = Get-ProjectWorldPerformanceChildOutcome -Receipt $receipt `
        -ProcessExitCode $ProcessExitCode
    Assert-ManhattanShowcase ($outcome.valid -and
        [string]$receipt.operation_id -ceq $Operation -and
        [string]$receipt.map_package -ceq $mapPackage -and
        [string]$receipt.runtime_profile -ceq 'manhattan_showcase_512_1536_v1' -and
        [string]$receipt.runtime_profile_sha256 -ceq $script:runtimeProfileHash -and
        [string]$receipt.build_configuration -ceq 'Development' -and
        $receipt.requires_cooked_data -is [bool] -and $receipt.requires_cooked_data -and
        [string]$receipt.correctness_status -ceq 'accepted' -and
        (Test-ManhattanSamePath -Actual ([string]$receipt.correctness_receipt) `
            -Expected $CorrectnessPath) -and
        (Test-ManhattanSamePath -Actual ([string]$receipt.executable) `
            -Expected $Executable) -and
        [string]$receipt.machine_profile_id -ceq 'rtx4070_primary' -and
        [string]$receipt.gpu_adapter -ceq 'NVIDIA GeForce RTX 4070' -and
        [string]$receipt.rhi -ceq 'D3D12' -and
        [string]$receipt.quality_preset -ceq 'High' -and
        [int]$receipt.resolution_x -eq 2560 -and
        [int]$receipt.resolution_y -eq 1440 -and
        [bool]$receipt.playable_tour -and
        [double]$receipt.final_center_arrival_radius_cm -eq 500.0 -and
        $null -ne $receipt.PSObject.Properties['gameplay_interaction_required'] -and
        -not [bool]$receipt.gameplay_interaction_required -and
        [int]$receipt.streaming_failures -eq 0 -and
        [bool]$receipt.center_cell_streaming_cycle -and
        [string]$receipt.input_method -ceq `
            'APlayerController::InputKey/FInputKeyEventArgs::CreateSimulated' -and
        [int]$receipt.input_event_count -gt 0 -and
        [bool]$receipt.pause_menu_opened -and
        [bool]$receipt.pause_menu_closed -and
        [int]$receipt.waypoints_reached -ge 3 -and
        [double]$receipt.ascent_cm -gt 4000.0 -and
        [double]$receipt.descent_cm -gt 3000.0 -and
        [double]$receipt.horizontal_displacement_cm -gt 100000.0 -and
        [bool]$receipt.collision_blocked_descent -and
        [bool]$receipt.collision_slide -and
        [double]$receipt.slide_displacement_cm -ge 100.0 -and
        (Test-ManhattanSamePath -Actual ([string]$receipt.raw_sample_capture) `
            -Expected $SamplePath) -and
        (Test-ManhattanSamePath -Actual ([string]$receipt.csv_capture) `
            -Expected $CsvPath) -and
        (Test-ManhattanSamePath -Actual ([string]$receipt.playable_tour_screenshot) `
            -Expected $ScreenshotPath) -and
        (Test-Path -LiteralPath $ScreenshotPath -PathType Leaf)) `
        'Manhattan performance identity, product route, or non-interactive policy was rejected.'
    return $receipt
}

function Assert-ManhattanNormalExit {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$ChildName
    )
    Assert-ManhattanShowcase (Test-Path -LiteralPath $LogPath -PathType Leaf) `
        "Manhattan Development child $ChildName has no process log."
    $log = Get-Content -LiteralPath $LogPath -Raw
    Assert-ManhattanShowcase (
        $log.Contains('LogExit: Exiting.') -and
        $log.Contains('Log file closed') -and
        -not $log.Contains('Assertion failed:') -and
        -not $log.Contains('Fatal error:')) `
        "Manhattan Development child $ChildName did not exit cleanly."
}

function Assert-ManhattanEvidenceInventory {
    param([Parameter(Mandatory = $true)][string]$ReceiptPath)
    $receipt = Get-Content -LiteralPath $ReceiptPath -Raw | ConvertFrom-Json
    $suffixes = @('correctness', 'product_screenshot', 'performance', 'samples',
        'csv', 'playable_screenshot', 'log')
    $expectedCount = 3 * $suffixes.Count
    Assert-ManhattanShowcase ($null -ne $receipt.artifacts -and
        $null -ne $receipt.artifact_sha256 -and
        @($receipt.artifacts.PSObject.Properties.Name).Count -eq $expectedCount -and
        @($receipt.artifact_sha256.PSObject.Properties.Name).Count -eq $expectedCount) `
        'Manhattan evidence inventory is incomplete.'
    for ($index = 1; $index -le 3; ++$index) {
        $name = 'run-{0:D2}' -f $index
        foreach ($suffix in $suffixes) {
            $key = "${name}_$suffix"
            $path = [string]$receipt.artifacts.$key
            $hash = [string]$receipt.artifact_sha256.$key
            Assert-ManhattanShowcase (-not [string]::IsNullOrWhiteSpace($path) -and
                $hash -cmatch '^[a-f0-9]{64}$' -and
                (Get-ProjectWorldPerformanceFileHash -Path $path) -ceq $hash) `
                "Manhattan evidence artifact changed or is missing: $key"
        }
    }
}

function Invoke-ManhattanExistingPackagePerformance {
    param(
        [Parameter(Mandatory = $true)][string]$PackageRoot,
        [string]$ExpectedPackageHash,
        [string]$ExpectedExecutableHash
    )
    Assert-ManhattanShowcase (Test-Path -LiteralPath $PackageRoot -PathType Container) `
        'Existing Development package root is missing.'
    $resolvedPackage = (Resolve-Path -LiteralPath $PackageRoot).Path
    $executable = Get-ManhattanShowcaseExecutable -PackageRoot $resolvedPackage
    $packageHash = Get-ProjectWorldPackagePayloadDigest -Path $resolvedPackage
    $executableHash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($ExpectedPackageHash) {
        Assert-ManhattanShowcase ($packageHash -ceq $ExpectedPackageHash) `
            'Existing package does not match the Kazan Development package payload.'
    }
    if ($ExpectedExecutableHash) {
        Assert-ManhattanShowcase ($executableHash -ceq $ExpectedExecutableHash) `
            'Existing executable does not match the Kazan Development executable.'
    }
    $source = Get-ManhattanPackageSourceIdentity -PackageRoot $resolvedPackage
    $revision = (& git -C $projectRoot rev-parse HEAD).Trim()
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0 -and $revision -ceq $source.Revision) `
        'Existing package does not match the current source revision.'
    $sourceTool = Join-Path $projectRoot 'scripts\ue\package\prepare_release.py'
    $sourceState = @(& python $sourceTool source-state --source-root $projectRoot)
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0 -and $sourceState.Count -eq 1 -and
        $sourceState[0] -ceq $source.StateHash) `
        'Existing package does not match the current source state.'
    $script:runtimeProfileHash = (Get-FileHash -LiteralPath $runtimeProfile `
        -Algorithm SHA256).Hash.ToLowerInvariant()
    $script:operationId = "manhattan_release_performance_$runId"
    New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
    $children = [Collections.Generic.List[object]]::new()
    $hostLoadWindows = [Collections.Generic.List[object]]::new()
    $artifactPaths = [ordered]@{}
    $artifactHashes = [ordered]@{}
    for ($index = 1; $index -le 3; ++$index) {
        $name = 'run-{0:D2}' -f $index
        $childRoot = Join-Path $evidenceRoot $name
        New-Item -ItemType Directory -Path $childRoot -Force | Out-Null
        $childOperation = "$operationId-$name"
        $correctnessPath = Join-Path $childRoot 'product-route.json'
        $performancePath = Join-Path $childRoot 'performance.json'
        $csvPath = Join-Path $childRoot 'performance.csv'
        $samplePath = Join-Path $childRoot 'performance.samples.csv'
        $screenshotPath = Join-Path $childRoot 'playable-tour.png'
        $logPath = Join-Path $childRoot 'game.log'
        foreach ($phase in @('before', 'after')) {
            if ($phase -ceq 'before') {
                $load = Measure-ProjectWorldPerformanceHostLoad
                $hostLoadWindows.Add([pscustomobject]@{
                    phase = "$name-before"; cpu_percent = $load.cpu_percent;
                    gpu_percent = $load.gpu_percent })
                $exitCode = Invoke-ManhattanShowcaseGame -Executable $executable `
                    -Configuration Development -RunOperationId $childOperation `
                    -CorrectnessPath $correctnessPath -LogPath $logPath `
                    -PerformancePath $performancePath -CsvPath $csvPath `
                    -SamplePath $samplePath -ScreenshotPath $screenshotPath
            }
            else {
                $load = Measure-ProjectWorldPerformanceHostLoad
                $hostLoadWindows.Add([pscustomobject]@{
                    phase = "$name-after"; cpu_percent = $load.cpu_percent;
                    gpu_percent = $load.gpu_percent })
            }
        }
        Assert-ManhattanShowcase ($exitCode -eq 0 -or $exitCode -eq 10) `
            "Manhattan Development child $name exited abnormally with code $exitCode."
        Assert-ManhattanNormalExit -LogPath $logPath -ChildName $name
        $correctness = Read-ManhattanShowcaseCorrectness -Path $correctnessPath `
            -Configuration Development -ExpectedOperationId $childOperation
        $performance = Assert-ManhattanPerformanceChild -Path $performancePath `
            -CorrectnessPath $correctnessPath -Operation $childOperation `
            -Executable $executable -SamplePath $samplePath -CsvPath $csvPath `
            -ScreenshotPath $screenshotPath -ProcessExitCode $exitCode
        $artifactPaths["${name}_correctness"] = $correctnessPath
        $artifactPaths["${name}_product_screenshot"] = [string]$correctness.screenshot
        $artifactPaths["${name}_performance"] = $performancePath
        $artifactPaths["${name}_samples"] = $samplePath
        $artifactPaths["${name}_csv"] = $csvPath
        $artifactPaths["${name}_playable_screenshot"] = [string]$performance.playable_tour_screenshot
        $artifactPaths["${name}_log"] = $logPath
        foreach ($suffix in @('correctness', 'product_screenshot', 'performance',
                'samples', 'csv', 'playable_screenshot', 'log')) {
            $key = "${name}_$suffix"
            $artifactPaths[$key] = [IO.Path]::GetFullPath([string]$artifactPaths[$key])
            $artifactHashes[$key] = Get-ProjectWorldPerformanceFileHash -Path $artifactPaths[$key]
        }
        Assert-ManhattanShowcase ((Get-ProjectWorldPackagePayloadDigest -Path $resolvedPackage) -ceq
            $packageHash -and
            (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant() -ceq
            $executableHash) "Existing package payload changed during $name."
        $children.Add([pscustomobject]@{
            ExpectedOperationId = $childOperation
            ExpectedProcessExitCode = $exitCode
            ReceiptPath = $performancePath
            CorrectnessPath = $correctnessPath
            SamplePath = $samplePath
            RichCsvPath = $csvPath
        })
    }
    $aggregate = New-ProjectWorldPerformanceAggregate -Children @($children) `
        -OperationId $operationId -SourceRevision $revision `
        -SourceStateSha256 $sourceState[0] -RuntimeProfileSha256 $script:runtimeProfileHash `
        -ExpectedExecutable $executable -ExpectedExecutableSha256 $executableHash `
        -ExpectedPackage $resolvedPackage -ExpectedPackageSha256 $packageHash `
        -HostLoadWindows @($hostLoadWindows) -ExpectedMapPackage $mapPackage `
        -ExpectedRuntimeProfile 'manhattan_showcase_512_1536_v1' `
        -RequireCorrectnessBinding -RequireNonInteractivePolicy
    Assert-ManhattanShowcase ((Get-ProjectWorldPackagePayloadDigest -Path $resolvedPackage) -ceq
        $packageHash) 'Existing package payload changed after aggregation.'
    $finalSourceState = @(& python $sourceTool source-state --source-root $projectRoot)
    Assert-ManhattanShowcase ($LASTEXITCODE -eq 0 -and $finalSourceState.Count -eq 1 -and
        $finalSourceState[0] -ceq $sourceState[0] -and
        (Get-FileHash -LiteralPath $runtimeProfile -Algorithm SHA256).Hash.ToLowerInvariant() -ceq
        $script:runtimeProfileHash) 'Source or runtime profile changed during Manhattan performance.'
    $receipt = [ordered]@{
        status = [string]$aggregate.status
        operation_id = $operationId
        source_revision = $revision
        source_state_sha256 = $sourceState[0]
        package_root = $resolvedPackage
        package_payload_sha256 = $packageHash
        executable = $executable
        executable_sha256 = $executableHash
        map_package = $mapPackage
        runtime_profile = 'manhattan_showcase_512_1536_v1'
        runtime_profile_sha256 = $script:runtimeProfileHash
        gameplay_interaction_required = $false
        performance = $aggregate
        artifacts = $artifactPaths
        artifact_sha256 = $artifactHashes
    }
    $receiptPath = Join-Path $evidenceRoot 'manhattan-release-performance.json'
    [IO.File]::WriteAllText($receiptPath, ($receipt | ConvertTo-Json -Depth 14) + "`n",
        [Text.UTF8Encoding]::new($false))
    Assert-ManhattanEvidenceInventory -ReceiptPath $receiptPath
    if ($AcceptInconclusivePerformance) {
        $aggregatePath = Join-Path $evidenceRoot 'performance-aggregate.json'
        [IO.File]::WriteAllText($aggregatePath, ($aggregate | ConvertTo-Json -Depth 12))
        $policy = Join-Path $projectRoot 'scripts/ue/package/release_performance.py'
        $decision = @(& python $policy decide --receipt $aggregatePath --accept-inconclusive-performance)
        Assert-ManhattanShowcase ($LASTEXITCODE -eq 0 -and $decision.Count -eq 1) `
            'Release performance waiver rejected the Manhattan evidence.'
        $receipt.performance_review = $decision[0] | ConvertFrom-Json
        Assert-ManhattanShowcase ([string]$receipt.performance_review.status -cin @('accepted', 'inconclusive')) `
            'Release performance decision is unknown.'
        if ($receipt.performance_review.status -ceq 'inconclusive') {
            $receipt.status = 'accepted_with_performance_waiver'
        }
        [IO.File]::WriteAllText($receiptPath, ($receipt | ConvertTo-Json -Depth 14) + "`n",
            [Text.UTF8Encoding]::new($false))
    }
    Assert-ManhattanShowcase ($AcceptInconclusivePerformance -or [string]$aggregate.status -ceq 'accepted') `
        "Manhattan pooled performance rejected: $($aggregate.acceptance_reason)"
    Write-Host "Manhattan existing-package decision $($receipt.status): $receiptPath"
    return $receiptPath
}

if ($DescribeOperation) {
    return [pscustomobject]@{
        Name = 'Manhattan'; Experience = $experienceId; Map = $mapPackage
        Runtime = 'manhattan_showcase_512_1536_v1'; RuntimePath = $runtimeProfile
        Edge = $edgeArgument; InteractionRequired = $false; PreciseCenterReturn = $true; Radius = 500.0
    }
}

if ($PSBoundParameters.ContainsKey('ExistingDevelopmentPackageRoot')) {
    Assert-ManhattanShowcase (-not [string]::IsNullOrWhiteSpace($ExistingDevelopmentPackageRoot)) `
        'ExistingDevelopmentPackageRoot cannot be empty.'
    Assert-ManhattanShowcase (-not $SkipShipping) `
        'SkipShipping is not part of existing Development package performance mode.'
    Invoke-ManhattanExistingPackagePerformance `
        -PackageRoot $ExistingDevelopmentPackageRoot `
        -ExpectedPackageHash $ExpectedPackagePayloadSha256 `
        -ExpectedExecutableHash $ExpectedExecutableSha256
    return
}
Assert-ManhattanShowcase (-not $ExpectedPackagePayloadSha256 -and
    -not $ExpectedExecutableSha256) `
    'Expected package and executable hashes require existing Development package mode.'
Assert-ManhattanShowcase (-not $AcceptInconclusivePerformance) `
    'Performance waiver is available only through the existing-package release route.'

New-Item -ItemType Directory -Path $evidenceRoot, $runtimeRoot -Force | Out-Null
$script:runtimeProfileHash = (Get-FileHash -LiteralPath $runtimeProfile -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceRevision = (& git -C $projectRoot rev-parse HEAD).Trim()
Assert-ManhattanShowcase ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($sourceRevision)) `
    'Unable to freeze the Manhattan showcase source revision.'
# HEAD alone does not identify what was built while the tree is dirty, so the working state is
# frozen separately and re-checked before promotion.
$sourceStateHash = Get-ManhattanShowcaseSourceStateDigest
$summary = [ordered]@{
    operation_id           = $operationId
    source_revision        = $sourceRevision
    source_state_sha256    = $sourceStateHash
    map_package            = $mapPackage
    runtime_profile        = 'manhattan_showcase_512_1536_v1'
    runtime_profile_sha256 = $script:runtimeProfileHash
    edge                   = $edgeArgument
    configurations         = [ordered]@{}
}

foreach ($configuration in @('Development', 'Shipping')) {
    if ($configuration -ceq 'Shipping' -and $SkipShipping) {
        Write-Host 'Skipping Shipping smoke by request.' -ForegroundColor Yellow
        continue
    }

    Write-Host "=== Manhattan showcase $configuration ===" -ForegroundColor Cyan
    $stagingRoot = Join-Path $runtimeRoot $configuration.ToLowerInvariant()
    Invoke-ManhattanShowcasePackage -OutputRoot $stagingRoot -Configuration $configuration

    $executable = Get-ManhattanShowcaseExecutable -PackageRoot $stagingRoot
    $runOperationId = "${operationId}_$($configuration.ToLowerInvariant())"
    $configurationEvidence = Join-Path $evidenceRoot $configuration.ToLowerInvariant()
    New-Item -ItemType Directory -Path $configurationEvidence -Force | Out-Null
    $correctnessPath = Join-Path $configurationEvidence 'product-route.json'
    $logPath = Join-Path $configurationEvidence 'game.log'

    Assert-ManhattanShowcaseSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $script:runtimeProfileHash -Stage "after $configuration packaging"

    $exitCode = Invoke-ManhattanShowcaseGame -Executable $executable -Configuration $configuration `
        -RunOperationId $runOperationId -CorrectnessPath $correctnessPath -LogPath $logPath
    Assert-ManhattanShowcase ($exitCode -eq 0) `
        "$configuration Manhattan showcase exited with code $exitCode."

    $receipt = Read-ManhattanShowcaseCorrectness -Path $correctnessPath `
        -Configuration $configuration -ExpectedOperationId $runOperationId

    $summary.configurations[$configuration] = [ordered]@{
        package_root                  = $stagingRoot
        executable                    = $executable
        executable_sha256             = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
        package_payload_sha256        = Get-ProjectWorldPackagePayloadDigest -Path $stagingRoot
        receipt                       = $correctnessPath
        screenshot                    = [string]$receipt.screenshot
        gameplay_interaction_required = [bool]$receipt.gameplay_interaction_required
        gpu_adapter                   = [string]$receipt.gpu_adapter
        rhi                           = [string]$receipt.rhi
    }

    # Nothing is promoted here. Development is evidence and stays in staging; publishing it
    # before Shipping has passed would destroy the previous Candidate on a later failure.
}

# ---------------------------------------------------------------------------------------------
# Promotion: only after BOTH configurations passed and the source state is still the frozen one.
# Shipping is the product the operator inspects; Development stays in staging as evidence.
# If anything above threw, we never reach here and the previous Candidate is untouched.
# ---------------------------------------------------------------------------------------------
if (-not $SkipShipping) {
    Assert-ManhattanShowcase ($summary.configurations.Contains('Development') -and
        $summary.configurations.Contains('Shipping')) `
        'Both configurations must pass before the Candidate is promoted.'
    Assert-ManhattanShowcaseSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $script:runtimeProfileHash -Stage 'before Candidate promotion'

    $previousPackage = Join-Path $packageRoot 'PreviousCandidate'
    if (Test-Path -LiteralPath $previousPackage) {
        Remove-Item -LiteralPath $previousPackage -Recurse -Force
    }
    New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
    if (Test-Path -LiteralPath $finalPackage) {
        Move-Item -LiteralPath $finalPackage -Destination $previousPackage
    }

    $shippingStaging = Join-Path $runtimeRoot 'shipping'
    Move-Item -LiteralPath $shippingStaging -Destination $finalPackage

    # Hashes are recomputed at the final path so the summary authenticates the Candidate the
    # operator actually opens, not a staging directory that no longer exists.
    $candidateExecutable = Get-ManhattanShowcaseExecutable -PackageRoot $finalPackage
    $summary.candidate = [ordered]@{
        path                   = $finalPackage
        configuration          = 'Shipping'
        executable             = $candidateExecutable
        executable_sha256      = (Get-FileHash -LiteralPath $candidateExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
        package_payload_sha256 = Get-ProjectWorldPackagePayloadDigest -Path $finalPackage
        previous_candidate     = if (Test-Path -LiteralPath $previousPackage) { $previousPackage } else { '' }
    }
    $summary.configurations['Shipping'].package_root = $finalPackage
    $summary.configurations['Shipping'].executable = $candidateExecutable
}

$summaryPath = Join-Path $evidenceRoot 'manhattan-showcase-summary.json'
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host 'Manhattan showcase prototype accepted.' -ForegroundColor Green
Write-Host "Evidence: $summaryPath"
