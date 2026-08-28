# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [int]$GameTimeoutSeconds = 720
)

$ErrorActionPreference = 'Stop'
$worldRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $worldRoot))
$packageScript = Join-Path $projectRoot 'scripts\ue\package\package_release.ps1'
$compilerBootstrap = Join-Path $projectRoot 'tools\World\CanonicalCompilation\bootstrap.py'
$compilerProfile = Join-Path $projectRoot `
    'Plugins\World\ProjectWorldData\Data\Profiles\CanonicalCompilation\kazan_territory_v1.compile.json'
$runtimeProfile = Join-Path $projectRoot `
    'Plugins\World\ProjectWorldData\Data\Runtime\kazan_territory_512_1536_v1.json'
$mapPackage = '/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory'
$runId = [Guid]::NewGuid().ToString('N')
$operationId = "kazan_playable_tour_$runId"
$evidenceRoot = Join-Path $projectRoot "Saved\Validation\WorldRealization\playable-tour\$runId"
$ownerRoot = Join-Path $projectRoot 'tmp\world\playable_tour'
$workRoot = Join-Path $ownerRoot $runId
$runtimeRoot = Join-Path $ownerRoot 'runtime'
$developmentPackage = Join-Path $runtimeRoot 'development'
$shippingPackage = Join-Path $runtimeRoot 'shipping'
$packageRoot = Join-Path $projectRoot 'Saved\PackageRelease\KazanPlayableTour'
$finalPackage = Join-Path $projectRoot 'Saved\PackageRelease\KazanPlayableTour\Candidate'
$previousFinalPackage = Join-Path $projectRoot 'Saved\PackageRelease\KazanPlayableTour\PreviousCandidate'

function Assert-PlayableTour {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

function Get-PlayableTourExecutable {
    param([Parameter(Mandatory = $true)][string]$PackageRoot)
    $candidates = @(Get-ChildItem -LiteralPath (Join-Path $PackageRoot 'Windows') `
        -Recurse -File -Filter 'Alis*.exe' |
        Where-Object { $_.FullName -match '[\\/]Alis[\\/]Binaries[\\/]Win64[\\/]' } |
        Sort-Object FullName)
    Assert-PlayableTour ($candidates.Count -eq 1) `
        "Expected exactly one staged game executable under $PackageRoot."
    return $candidates[0].FullName
}

function Get-PlayableTourTreeDigest {
    param([Parameter(Mandatory = $true)][string]$Path)
    $root = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $lines = @(Get-ChildItem -LiteralPath $root -Recurse -File -Force |
        Sort-Object FullName |
        ForEach-Object {
            $relative = $_.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            '{0}|{1}|{2}' -f $relative, $_.Length, $hash
        })
    $bytes = [Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Get-PlayableTourSourceStateDigest {
    $parts = [Collections.Generic.List[string]]::new()
    $parts.Add((@(& git -C $projectRoot diff --binary --no-ext-diff HEAD) -join "`n"))
    Assert-PlayableTour ($LASTEXITCODE -eq 0) 'Unable to read the tracked source state.'
    $untracked = @(& git -C $projectRoot ls-files --others --exclude-standard | Sort-Object)
    Assert-PlayableTour ($LASTEXITCODE -eq 0) 'Unable to read the untracked source state.'
    foreach ($relative in $untracked) {
        $path = Join-Path $projectRoot $relative
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

function Assert-PlayableTourSourceState {
    param(
        [Parameter(Mandatory = $true)][string]$ExpectedSourceHash,
        [Parameter(Mandatory = $true)][string]$ExpectedRuntimeHash,
        [Parameter(Mandatory = $true)][string]$Stage
    )
    Assert-PlayableTour ((Get-PlayableTourSourceStateDigest) -ceq $ExpectedSourceHash) `
        "Source state changed during the playable-tour transaction at $Stage."
    $currentRuntimeHash = (Get-FileHash -LiteralPath $runtimeProfile -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-PlayableTour ($currentRuntimeHash -ceq $ExpectedRuntimeHash) `
        "Runtime profile changed during the playable-tour transaction at $Stage."
}

function Remove-PlayableTourWorkspace {
    param([Parameter(Mandatory = $true)][string]$Path)
    $owner = [IO.Path]::GetFullPath($ownerRoot).TrimEnd('\', '/')
    $target = [IO.Path]::GetFullPath($Path)
    if (-not $target.StartsWith(
            $owner + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Playable-tour cleanup escaped its owner root: $target"
    }
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

function Remove-PlayableTourPackage {
    param([Parameter(Mandatory = $true)][string]$Path)
    $target = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $allowed = @($finalPackage, $previousFinalPackage) | ForEach-Object {
        [IO.Path]::GetFullPath($_).TrimEnd('\', '/')
    }
    if (@($allowed | Where-Object {
                $_.Equals($target, [StringComparison]::OrdinalIgnoreCase)
            }).Count -ne 1) {
        throw "Playable-tour package cleanup escaped its exact owner targets: $target"
    }
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

function Invoke-PlayableTourPackage {
    param(
        [Parameter(Mandatory = $true)][string]$OutputRoot,
        [Parameter(Mandatory = $true)][string]$Configuration
    )
    & $packageScript -OutputDir $OutputRoot -ClientConfig $Configuration `
        -RequiredCookMap $mapPackage
    Assert-PlayableTour ($LASTEXITCODE -eq 0) `
        "$Configuration launcher-engine packaging failed."
}

function Invoke-PlayableTourGame {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$CorrectnessPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [string]$PerformancePath,
        [string]$CsvPath,
        [string]$ScreenshotPath
    )
    $arguments = @(
        '-ProjectMenuPlayAutoExperience=KazanTerritory',
        '-ProjectMenuPlayAutoMode=SinglePlayer',
        '-ProjectWorldProductRouteGate',
        '-ProjectWorldProductRouteRestorePreviewFlight',
        "-ProjectWorldProductOperation=$operationId",
        "-ProjectWorldProductResult=$CorrectnessPath",
        "-ProjectWorldProductMap=$mapPackage",
        '-ProjectWorldProductRuntime=kazan_territory_512_1536_v1',
        "-ProjectWorldProductRuntimeHash=$script:runtimeProfileHash",
        '-ProjectWorldProductMachine=rtx4070_primary',
        "-ProjectWorldProductEdge=$script:edgeArgument",
        '-ResX=2560', '-ResY=1440', '-Windowed', '-ForceRes',
        '-RenderOffScreen', '-VSync=0', '-unattended', '-nosplash', '-NoMessaging',
        "-abslog=$LogPath"
    )
    if ($Configuration -ceq 'Development') {
        $arguments += @(
            '-ProjectWorldProductPerformanceGate',
            '-ProjectWorldPlayableTour',
            "-ProjectWorldPerformanceResult=$PerformancePath",
            "-ProjectWorldPerformanceCorrectness=$CorrectnessPath",
            "-ProjectWorldPerformanceCsv=$CsvPath",
            "-ProjectWorldPerformanceScreenshot=$ScreenshotPath"
        )
    }
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments `
        -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit($GameTimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "$Configuration playable-tour process exceeded the bounded timeout."
    }
    return $process.ExitCode
}

function Assert-PlayableTourNormalExit {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Configuration
    )
    Assert-PlayableTour (Test-Path -LiteralPath $LogPath -PathType Leaf) `
        "$Configuration playable-tour log is missing."
    $log = Get-Content -LiteralPath $LogPath -Raw
    Assert-PlayableTour (
        $log.Contains('LogExit: Exiting.') -and
        $log.Contains('Log file closed') -and
        -not $log.Contains('Assertion failed:') -and
        -not $log.Contains('Fatal error:')) `
        "$Configuration playable-tour log does not prove a clean terminal exit."
}

function Read-PlayableTourCorrectness {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Configuration
    )
    Assert-PlayableTour (Test-Path -LiteralPath $Path -PathType Leaf) `
        "$Configuration product-route receipt is missing."
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    Assert-PlayableTour (
        [string]$receipt.status -ceq 'accepted' -and
        [string]$receipt.operation_id -ceq $operationId -and
        [string]$receipt.map_package -ceq $mapPackage -and
        [string]$receipt.runtime_profile -ceq 'kazan_territory_512_1536_v1' -and
        [string]$receipt.runtime_profile_sha256 -ceq $runtimeProfileHash -and
        [string]$receipt.build_configuration -ceq $Configuration -and
        [string]$receipt.game_mode -ceq '/Script/ProjectSinglePlay.SinglePlayerGameMode' -and
        [string]$receipt.pawn_class -ceq '/Script/ProjectCharacter.DefinitionCharacter' -and
        [bool]$receipt.project_loading_provenance -and
        [bool]$receipt.possessed_player -and
        [bool]$receipt.grounded_player -and
        [bool]$receipt.normal_movement -and
        [bool]$receipt.terrain_collision -and
        [bool]$receipt.road_collision -and
        [bool]$receipt.building_collision -and
        [bool]$receipt.gameplay_interaction -and
        [bool]$receipt.center_unloaded_at_edge -and
        [bool]$receipt.edge_loaded -and
        [bool]$receipt.center_reloaded -and
        [bool]$receipt.preview_flight_restored -and
        [string]$receipt.gpu_adapter -ceq 'NVIDIA GeForce RTX 4070' -and
        [string]$receipt.rhi -ceq 'D3D12') `
        "$Configuration product-route receipt failed its identity/correctness contract."
    Assert-PlayableTour (Test-Path -LiteralPath ([string]$receipt.screenshot) -PathType Leaf) `
        "$Configuration product-route screenshot is missing."
    return $receipt
}

function Read-PlayableTourPerformance {
    param([Parameter(Mandatory = $true)][string]$Path)
    Assert-PlayableTour (Test-Path -LiteralPath $Path -PathType Leaf) `
        'Development performance receipt is missing.'
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $initialCenterCells = @($receipt.initial_center_cell_ids)
    $unloadedCenterCells = @($receipt.unloaded_center_cell_ids)
    $reloadedCenterCells = @($receipt.reloaded_center_cell_ids)
    $completedCenterCycles = @($unloadedCenterCells | Where-Object {
            $reloadedCenterCells -contains $_ -and $initialCenterCells -contains $_
        })
    Assert-PlayableTour (
        [string]$receipt.status -ceq 'accepted' -and
        [string]$receipt.operation_id -ceq $operationId -and
        [string]$receipt.build_configuration -ceq 'Development' -and
        [string]$receipt.gpu_adapter -ceq 'NVIDIA GeForce RTX 4070' -and
        [string]$receipt.rhi -ceq 'D3D12' -and
        [string]$receipt.quality_preset -ceq 'High' -and
        [int]$receipt.resolution_x -eq 2560 -and
        [int]$receipt.resolution_y -eq 1440 -and
        [double]$receipt.frame_p95_ms -le 16.67 -and
        [int]$receipt.streaming_failures -eq 0 -and
        [bool]$receipt.center_cell_streaming_cycle -and
        $initialCenterCells.Count -eq [int]$receipt.initial_center_cell_count -and
        $unloadedCenterCells.Count -eq [int]$receipt.unloaded_center_cell_count -and
        $reloadedCenterCells.Count -eq [int]$receipt.reloaded_center_cell_count -and
        $completedCenterCycles.Count -gt 0 -and
        [bool]$receipt.playable_tour -and
        [string]$receipt.input_method -ceq `
            'APlayerController::InputKey/FInputKeyEventArgs::CreateSimulated' -and
        [int]$receipt.input_event_count -gt 0 -and
        [int]$receipt.waypoints_reached -ge 3 -and
        [double]$receipt.ascent_cm -gt 4000.0 -and
        [double]$receipt.descent_cm -gt 3000.0 -and
        [double]$receipt.horizontal_displacement_cm -gt 100000.0 -and
        [bool]$receipt.collision_blocked_descent -and
        [bool]$receipt.collision_slide) `
        'Development playable-tour performance/input contract was rejected.'
    Assert-PlayableTour (Test-Path -LiteralPath ([string]$receipt.csv_capture) -PathType Leaf) `
        'Development CSV capture is missing.'
    Assert-PlayableTour (Test-Path -LiteralPath ([string]$receipt.playable_tour_screenshot) -PathType Leaf) `
        'Development playable-tour screenshot is missing.'
    return $receipt
}

New-Item -ItemType Directory -Path $evidenceRoot, $ownerRoot, $packageRoot -Force | Out-Null
if (Test-Path -LiteralPath $previousFinalPackage) {
    if (Test-Path -LiteralPath $finalPackage) {
        Remove-PlayableTourPackage -Path $previousFinalPackage
    }
    else {
        Move-Item -LiteralPath $previousFinalPackage -Destination $finalPackage
    }
}
Remove-PlayableTourWorkspace -Path $runtimeRoot
New-Item -ItemType Directory -Path $workRoot, $runtimeRoot -Force | Out-Null
if (Test-Path -LiteralPath $finalPackage) {
    Move-Item -LiteralPath $finalPackage -Destination $previousFinalPackage
}
$accepted = $false
try {
    $runtimeProfileHash = (Get-FileHash -LiteralPath $runtimeProfile -Algorithm SHA256).Hash.ToLowerInvariant()
    $sourceStateHash = Get-PlayableTourSourceStateDigest
    $sourceRevision = (& git -C $projectRoot rev-parse HEAD).Trim()
    Assert-PlayableTour ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($sourceRevision)) `
        'Unable to freeze the playable-tour source revision.'
    $materializeOutput = @(& python -S $compilerBootstrap materialize --profile $compilerProfile)
    Assert-PlayableTour ($LASTEXITCODE -eq 0 -and $materializeOutput.Count -gt 0) `
        'Canonical authority materialization failed.'
    $materialized = $materializeOutput[-1] | ConvertFrom-Json
    Assert-PlayableTour ([string]$materialized.status -ceq 'accepted') `
        'Canonical authority materialization was not accepted.'
    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'after authority materialization'
    $compileResultValue = [string]$materialized.result_path
    $compileResult = if ([IO.Path]::IsPathRooted($compileResultValue)) {
        [IO.Path]::GetFullPath($compileResultValue)
    }
    else {
        [IO.Path]::GetFullPath((Join-Path $projectRoot $compileResultValue))
    }
    $edgeCell = Get-Content -LiteralPath `
        (Join-Path (Split-Path -Parent $compileResult) 'canonical\cells\cell_x8_y-2.json') `
        -Raw | ConvertFrom-Json
    $coverage = Get-Content -LiteralPath `
        (Join-Path (Split-Path -Parent $compileResult) 'canonical\coverage.json') `
        -Raw | ConvertFrom-Json
    $edgeX = (([double]$edgeCell.bounds[0] + [double]$edgeCell.bounds[2]) / 2.0 -
        [double]$coverage.engine_georeference_origin[0]) * 100.0
    $edgeY = -((([double]$edgeCell.bounds[1] + [double]$edgeCell.bounds[3]) / 2.0 -
        [double]$coverage.engine_georeference_origin[1]) * 100.0)
    $edgeArgument = '{0},{1},10000' -f `
        $edgeX.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture), `
        $edgeY.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture)

    $developmentRoot = Join-Path $evidenceRoot 'development'
    New-Item -ItemType Directory -Path $developmentRoot -Force | Out-Null
    $developmentCorrectnessPath = Join-Path $developmentRoot 'product-route.json'
    $developmentPerformancePath = Join-Path $developmentRoot 'performance.json'
    $developmentCsvPath = Join-Path $developmentRoot 'performance.csv'
    $developmentScreenshotPath = Join-Path $developmentRoot 'playable-tour.png'
    $developmentLogPath = Join-Path $developmentRoot 'game.log'

    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'before Development packaging'
    Invoke-PlayableTourPackage -OutputRoot $developmentPackage -Configuration 'Development'
    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'after Development packaging'
    Move-Item -LiteralPath $developmentPackage -Destination $finalPackage
    $developmentExecutable = Get-PlayableTourExecutable -PackageRoot $finalPackage
    $developmentExitCode = Invoke-PlayableTourGame `
        -Executable $developmentExecutable -Configuration 'Development' `
        -CorrectnessPath $developmentCorrectnessPath -LogPath $developmentLogPath `
        -PerformancePath $developmentPerformancePath -CsvPath $developmentCsvPath `
        -ScreenshotPath $developmentScreenshotPath
    Assert-PlayableTour ($developmentExitCode -eq 0) `
        "Development playable-tour process exited abnormally with code $developmentExitCode."
    Assert-PlayableTourNormalExit -LogPath $developmentLogPath -Configuration 'Development'
    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'after Development execution'
    $developmentCorrectness = Read-PlayableTourCorrectness `
        -Path $developmentCorrectnessPath -Configuration 'Development'
    $developmentPerformance = Read-PlayableTourPerformance -Path $developmentPerformancePath
    $developmentPackageHash = Get-PlayableTourTreeDigest -Path $finalPackage
    $developmentExecutableHash = (Get-FileHash -LiteralPath $developmentExecutable `
        -Algorithm SHA256).Hash.ToLowerInvariant()
    Remove-PlayableTourPackage -Path $finalPackage

    $shippingRoot = Join-Path $evidenceRoot 'shipping'
    New-Item -ItemType Directory -Path $shippingRoot -Force | Out-Null
    $shippingCorrectnessPath = Join-Path $shippingRoot 'product-route.json'
    $shippingLogPath = Join-Path $shippingRoot 'game.log'
    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'before Shipping packaging'
    Invoke-PlayableTourPackage -OutputRoot $shippingPackage -Configuration 'Shipping'
    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'after Shipping packaging'
    Move-Item -LiteralPath $shippingPackage -Destination $finalPackage
    $shippingExecutable = Get-PlayableTourExecutable -PackageRoot $finalPackage
    $shippingExitCode = Invoke-PlayableTourGame `
        -Executable $shippingExecutable -Configuration 'Shipping' `
        -CorrectnessPath $shippingCorrectnessPath -LogPath $shippingLogPath
    Assert-PlayableTour ($shippingExitCode -eq 0) `
        "Shipping playable-tour process exited abnormally with code $shippingExitCode."
    # Launcher-engine Shipping has no logging support; exit status and the authenticated
    # same-process product receipt are its terminal evidence.
    if (Test-Path -LiteralPath $shippingLogPath -PathType Leaf) {
        Assert-PlayableTourNormalExit -LogPath $shippingLogPath -Configuration 'Shipping'
    }
    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'after Shipping execution'
    $shippingCorrectness = Read-PlayableTourCorrectness `
        -Path $shippingCorrectnessPath -Configuration 'Shipping'
    $shippingPackageHash = Get-PlayableTourTreeDigest -Path $finalPackage
    $shippingExecutableHash = (Get-FileHash -LiteralPath $shippingExecutable `
        -Algorithm SHA256).Hash.ToLowerInvariant()

    $artifactPaths = [ordered]@{
        development_correctness = $developmentCorrectnessPath
        development_performance = $developmentPerformancePath
        development_csv = $developmentCsvPath
        development_log = $developmentLogPath
        development_product_screenshot = [string]$developmentCorrectness.screenshot
        development_playable_screenshot = [string]$developmentPerformance.playable_tour_screenshot
        shipping_correctness = $shippingCorrectnessPath
        shipping_product_screenshot = [string]$shippingCorrectness.screenshot
    }
    if (Test-Path -LiteralPath $shippingLogPath -PathType Leaf) {
        $artifactPaths['shipping_log'] = $shippingLogPath
    }
    $artifactHashes = [ordered]@{}
    foreach ($entry in $artifactPaths.GetEnumerator()) {
        Assert-PlayableTour (Test-Path -LiteralPath $entry.Value -PathType Leaf) `
            "Composite artifact is missing: $($entry.Key)"
        $artifactHashes[$entry.Key] = (Get-FileHash -LiteralPath $entry.Value `
            -Algorithm SHA256).Hash.ToLowerInvariant()
    }

    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'before composite publication'
    $composite = [ordered]@{
        schema_version = 1
        status = 'accepted'
        operation_id = $operationId
        revision = $sourceRevision
        source_state_sha256 = $sourceStateHash
        map_package = $mapPackage
        runtime_profile = 'kazan_territory_512_1536_v1'
        runtime_profile_sha256 = $runtimeProfileHash
        engine_route = 'launcher_installed'
        machine_profile_id = 'rtx4070_primary'
        development_exit_code = $developmentExitCode
        development_executable_sha256 = $developmentExecutableHash
        development_package_sha256 = $developmentPackageHash
        shipping_exit_code = $shippingExitCode
        shipping_executable_sha256 = $shippingExecutableHash
        shipping_package_sha256 = $shippingPackageHash
        artifacts = $artifactPaths
        artifact_sha256 = $artifactHashes
        frame_p95_ms = [double]$developmentPerformance.frame_p95_ms
        frame_p99_ms = [double]$developmentPerformance.frame_p99_ms
        game_p95_ms = [double]$developmentPerformance.game_p95_ms
        render_p95_ms = [double]$developmentPerformance.render_p95_ms
        gpu_p95_ms = [double]$developmentPerformance.gpu_p95_ms
        peak_process_physical_bytes = [Int64]$developmentPerformance.peak_process_physical_bytes
        peak_gpu_local_bytes = [Int64]$developmentPerformance.peak_gpu_local_bytes
        streaming_failures = [int]$developmentPerformance.streaming_failures
        cleanup_result = 'pending'
        final_package = $finalPackage
        previous_package = if (Test-Path -LiteralPath $previousFinalPackage) {
            $previousFinalPackage
        } else { $null }
    }

    $resolvedFinalPackage = [IO.Path]::GetFullPath($finalPackage)
    $resolvedFinalRoot = [IO.Path]::GetFullPath(
        (Join-Path $projectRoot 'Saved\PackageRelease\KazanPlayableTour')).TrimEnd('\', '/')
    Assert-PlayableTour ($resolvedFinalPackage.StartsWith(
            $resolvedFinalRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) `
        'Final package destination escaped the playable-tour Saved root.'
    $composite.cleanup_result = 'owner_tmp_cleaned'
    $compositePath = Join-Path $evidenceRoot 'composite.json'
    $compositeJson = $composite | ConvertTo-Json -Depth 12
    [IO.File]::WriteAllText(
        $compositePath,
        $compositeJson + "`n",
        [Text.UTF8Encoding]::new($false))
    $accepted = $true
    Write-Host "[ProjectWorldPlayableTour] Accepted: $compositePath" -ForegroundColor Green
    Write-Host "[ProjectWorldPlayableTour] Final package: $finalPackage" -ForegroundColor Green
}
finally {
    if (-not $accepted) {
        Remove-PlayableTourPackage -Path $finalPackage
        if (Test-Path -LiteralPath $previousFinalPackage) {
            Move-Item -LiteralPath $previousFinalPackage -Destination $finalPackage
        }
    }
    Remove-PlayableTourWorkspace -Path $workRoot
    Remove-PlayableTourWorkspace -Path $runtimeRoot
}

if (-not $accepted) {
    exit 1
}
