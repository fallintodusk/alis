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
$waterVerifier = Join-Path $projectRoot 'scripts\ue\world\test\water_temporal_stability.ps1'
$waterViewpointVerifier = Join-Path $projectRoot `
    'scripts\ue\world\test\verify_canonical_feature_viewpoint.ps1'
$compilerBootstrap = Join-Path $projectRoot 'tools\World\CanonicalCompilation\bootstrap.py'
$compilerProfile = Join-Path $projectRoot `
    'Plugins\World\ProjectWorldData\Data\Profiles\CanonicalCompilation\kazan_territory_v1.compile.json'
$runtimeProfile = Join-Path $projectRoot `
    'Plugins\World\ProjectWorldData\Data\Runtime\kazan_territory_512_1536_v1.json'
$activeManifestSet = Join-Path $projectRoot `
    'Plugins\World\ProjectWorldData\Data\Manifests\active_set.json'
$mapPackage = '/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory'
$shippingWaterTargetXCentimeters = 0.0
$shippingWaterTargetYCentimeters = -80000.0
$runId = [Guid]::NewGuid().ToString('N')
$operationId = "kazan_playable_tour_$runId"
$evidenceRoot = Join-Path $projectRoot "Saved\Validation\WorldRealization\playable-tour\$runId"
$ownerRoot = Join-Path $projectRoot 'tmp\world\playable_tour'
$workRoot = Join-Path $ownerRoot $runId
$runtimeRoot = Join-Path $ownerRoot 'runtime'
$focusedProbeRoot = Join-Path $ownerRoot 'focused_slide'
$developmentPackage = Join-Path $runtimeRoot 'development'
$shippingPackage = Join-Path $runtimeRoot 'shipping'
$packageRoot = Join-Path $projectRoot 'Saved\PackageRelease\KazanPlayableTour'
$finalPackage = Join-Path $projectRoot 'Saved\PackageRelease\KazanPlayableTour\Candidate'
$previousFinalPackage = Join-Path $projectRoot 'Saved\PackageRelease\KazanPlayableTour\PreviousCandidate'
. (Join-Path $PSScriptRoot 'project_world_performance_evidence.ps1')

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

function Get-PlayableTourSourceStateDigest {
    $parts = [Collections.Generic.List[string]]::new()
    $parts.Add((@(& git -C $projectRoot diff --binary --no-ext-diff HEAD) -join "`n"))
    Assert-PlayableTour ($LASTEXITCODE -eq 0) 'Unable to read tracked source state.'
    $untracked = @(& git -C $projectRoot ls-files --others --exclude-standard | Sort-Object)
    Assert-PlayableTour ($LASTEXITCODE -eq 0) 'Unable to read untracked source state.'
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
        try {
            Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction Stop
        }
        catch [System.IO.IOException] {
            # A terminal may keep the stable package root as its current directory.
            # Windows then refuses to rename/remove that directory even though its
            # one owned payload remains movable. Clear the payload and retain only
            # the empty stable root; publication below reuses it safely.
            $children = @(Get-ChildItem -LiteralPath $target -Force)
            foreach ($child in $children) {
                Remove-Item -LiteralPath $child.FullName -Recurse -Force
            }
            Assert-PlayableTour (@(Get-ChildItem -LiteralPath $target -Force).Count -eq 0) `
                "Playable-tour package root remained non-empty after bounded cleanup: $target"
            try {
                Remove-Item -LiteralPath $target -Force -ErrorAction Stop
            }
            catch [System.IO.IOException] {
                # Keeping an empty stable root is harmless and avoids fighting an
                # unrelated shell handle. Package payload authority is the Windows
                # child moved by Move-PlayableTourPackage.
            }
        }
    }
}

function Move-PlayableTourPackage {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $sourcePath = [IO.Path]::GetFullPath($Source).TrimEnd('\', '/')
    $destinationPath = [IO.Path]::GetFullPath($Destination).TrimEnd('\', '/')
    $allowed = @($finalPackage, $previousFinalPackage, $developmentPackage, $shippingPackage) |
        ForEach-Object { [IO.Path]::GetFullPath($_).TrimEnd('\', '/') }
    foreach ($candidate in @($sourcePath, $destinationPath)) {
        Assert-PlayableTour (@($allowed | Where-Object {
                    $_.Equals($candidate, [StringComparison]::OrdinalIgnoreCase)
                }).Count -eq 1) "Playable-tour package move escaped its exact owner roots: $candidate"
    }
    Assert-PlayableTour (Test-Path -LiteralPath $sourcePath -PathType Container) `
        "Playable-tour package move source is missing: $sourcePath"

    if (-not (Test-Path -LiteralPath $destinationPath)) {
        try {
            Move-Item -LiteralPath $sourcePath -Destination $destinationPath -ErrorAction Stop
            return
        }
        catch [System.IO.IOException] {
            # Fall through to payload rotation when Windows has the source root
            # open as another process's current directory.
        }
    }

    if (-not (Test-Path -LiteralPath $destinationPath)) {
        New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null
    }
    Assert-PlayableTour (@(Get-ChildItem -LiteralPath $destinationPath -Force).Count -eq 0) `
        "Playable-tour package move destination is not empty: $destinationPath"
    $payload = @(Get-ChildItem -LiteralPath $sourcePath -Force)
    $windows = @($payload | Where-Object { $_.PSIsContainer -and $_.Name -ceq 'Windows' })
    $unexpected = @($payload | Where-Object {
            -not ($_.PSIsContainer -and $_.Name -ceq 'Windows') -and
            -not (-not $_.PSIsContainer -and $_.Name -ceq 'package_summary.txt')
        })
    Assert-PlayableTour ($windows.Count -eq 1 -and $unexpected.Count -eq 0) `
        "Playable-tour package root has an invalid payload shape: $sourcePath"
    $moved = [Collections.Generic.List[string]]::new()
    try {
        foreach ($item in @($payload | Sort-Object @{ Expression = { $_.PSIsContainer } }, Name)) {
            Move-Item -LiteralPath $item.FullName -Destination $destinationPath -ErrorAction Stop
            $moved.Add($item.Name)
        }
    }
    catch {
        foreach ($name in @($moved | Select-Object -Last $moved.Count)) {
            $movedPath = Join-Path $destinationPath $name
            if (Test-Path -LiteralPath $movedPath) {
                Move-Item -LiteralPath $movedPath -Destination $sourcePath -ErrorAction SilentlyContinue
            }
        }
        throw
    }
    Assert-PlayableTour (@(Get-ChildItem -LiteralPath $sourcePath -Force).Count -eq 0) `
        "Playable-tour source root retained payload after rotation: $sourcePath"
    try {
        Remove-Item -LiteralPath $sourcePath -Force -ErrorAction Stop
    }
    catch [System.IO.IOException] {
        # Stable roots may remain empty while held by a shell. Transient roots
        # are removed by the owner cleanup in the transaction finally block.
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
        [Parameter(Mandatory = $true)][string]$RunOperationId,
        [Parameter(Mandatory = $true)][string]$CorrectnessPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [string]$PerformancePath,
        [string]$CsvPath,
        [string]$SamplePath,
        [string]$ScreenshotPath
    )
    $arguments = @(
        '-ProjectMenuPlayAutoExperience=KazanTerritory',
        '-ProjectMenuPlayAutoMode=SinglePlayer',
        '-ProjectWorldProductRouteGate',
        '-ProjectWorldProductRouteRestorePreviewFlight',
        "-ProjectWorldProductOperation=$RunOperationId",
        "-ProjectWorldProductResult=$CorrectnessPath",
        "-ProjectWorldProductMap=$mapPackage",
        '-ProjectWorldProductRuntime=kazan_territory_512_1536_v1',
        "-ProjectWorldProductRuntimeHash=$script:runtimeProfileHash",
        '-ProjectWorldProductMachine=rtx4070_primary',
        "-ProjectWorldProductEdge=$script:edgeArgument",
        '-ResX=2560', '-ResY=1440', '-Windowed', '-ForceRes',
        '-RenderOffScreen', '-novsync', '-unattended', '-nosplash', '-NoMessaging',
        "-abslog=$LogPath"
    )
    if ($Configuration -ceq 'Development') {
        $arguments += @(
            '-ProjectWorldProductPerformanceGate',
            '-ProjectWorldPlayableTour',
            "-ProjectWorldPerformanceResult=$PerformancePath",
            "-ProjectWorldPerformanceCorrectness=$CorrectnessPath",
            "-ProjectWorldPerformanceCsv=$CsvPath",
            "-ProjectWorldPerformanceSamples=$SamplePath",
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

function Invoke-ShippingWaterProof {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$ResultPath,
        [Parameter(Mandatory = $true)][string]$ReferencePath,
        [Parameter(Mandatory = $true)][string]$RepeatPath,
        [Parameter(Mandatory = $true)][string]$ProductPath
    )
    $arguments = @(
        '-ProjectMenuPlayAutoExperience=KazanTerritory',
        '-ProjectMenuPlayAutoMode=SinglePlayer',
        '-ProjectWorldShippingWaterProof',
        "-ProjectWorldShippingWaterOperation=$operationId",
        "-ProjectWorldShippingWaterResult=$ResultPath",
        "-ProjectWorldShippingWaterReference=$ReferencePath",
        "-ProjectWorldShippingWaterRepeat=$RepeatPath",
        "-ProjectWorldShippingWaterProduct=$ProductPath",
        "-ProjectWorldShippingWaterMap=$mapPackage",
        '-ProjectWorldShippingWaterRuntime=kazan_territory_512_1536_v1',
        "-ProjectWorldShippingWaterRuntimeHash=$script:runtimeProfileHash",
        '-ProjectWorldShippingWaterMachine=rtx4070_primary',
        ('-ProjectWorldShippingWaterTarget={0},{1},0' -f `
            $shippingWaterTargetXCentimeters.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture), `
            $shippingWaterTargetYCentimeters.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture)),
        '-ResX=2560', '-ResY=1440', '-Windowed', '-ForceRes',
        '-RenderOffScreen', '-novsync', '-unattended', '-nosplash', '-NoMessaging'
    )
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments `
        -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit($GameTimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw 'Shipping Water proof process exceeded the bounded timeout.'
    }
    return $process.ExitCode
}

function Read-ShippingWaterProof {
    param([Parameter(Mandatory = $true)][string]$Path)
    Assert-PlayableTour (Test-Path -LiteralPath $Path -PathType Leaf) `
        'Shipping Water proof receipt is missing.'
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $start = @($receipt.start_player_location_cm)
    $target = @($receipt.requested_water_target_cm)
    $allowedTargetError = [double]$receipt.allowed_target_xy_error_cm
    $requestedTargetDistance = [double]$receipt.requested_target_distance_cm
    $minimumRequiredTravel = $requestedTargetDistance - $allowedTargetError
    $computedTargetDistance = if ($start.Count -eq 3 -and $target.Count -eq 3) {
        [Math]::Sqrt(
            [Math]::Pow([double]$target[0] - [double]$start[0], 2) +
            [Math]::Pow([double]$target[1] - [double]$start[1], 2))
    }
    else { [double]::NaN }
    Assert-PlayableTour (
        [string]$receipt.status -ceq 'accepted' -and
        [string]$receipt.operation_id -ceq $operationId -and
        [string]$receipt.map_package -ceq $mapPackage -and
        [string]$receipt.runtime_profile_sha256 -ceq $script:runtimeProfileHash -and
        [string]$receipt.build_configuration -ceq 'Shipping' -and
        [string]$receipt.gpu_adapter -ceq 'NVIDIA GeForce RTX 4070' -and
        [string]$receipt.rhi -ceq 'D3D12' -and
        [bool]$receipt.project_loading_provenance -and
        [bool]$receipt.preview_flight -and
        [string]$receipt.pawn_class -ceq '/Script/ProjectCharacter.DefinitionCharacter' -and
        [string]$receipt.input_method -ceq `
            'APlayerController::InputKey/FInputKeyEventArgs::CreateSimulated' -and
        [int]$receipt.input_event_count -gt 0 -and
        [int]$receipt.waypoints_reached -ge 1 -and
        [double]$receipt.ascent_cm -gt 4000.0 -and
        $requestedTargetDistance -gt $allowedTargetError -and
        [Math]::Abs($requestedTargetDistance - $computedTargetDistance) -le 1.0 -and
        [double]$receipt.horizontal_displacement_cm + 1.0 -ge $minimumRequiredTravel -and
        $null -ne $receipt.PSObject.Properties['target_xy_error_cm'] -and
        [double]$receipt.target_xy_error_cm -le $allowedTargetError -and
        [string]$receipt.capture_source -ceq 'SCS_BaseColor' -and
        [string]$receipt.capture_projection -ceq 'orthographic' -and
        [double]$receipt.capture_ortho_width_cm -eq 20000.0 -and
        [bool]$receipt.same_capture_session -and
        -not [string]::IsNullOrWhiteSpace([string]$receipt.capture_session_id) -and
        [string]$receipt.reference_capture_session_id -ceq [string]$receipt.capture_session_id -and
        [string]$receipt.repeat_capture_session_id -ceq [string]$receipt.capture_session_id -and
        [int]$receipt.capture_session_written_image_count -eq 2 -and
        [bool]$receipt.reference_captured -and
        [bool]$receipt.repeat_captured -and
        [string]$receipt.final_color_capture_source -ceq 'SCS_FinalColorLDR' -and
        [string]$receipt.final_color_capture_projection -ceq 'perspective' -and
        [double]$receipt.final_color_field_of_view_degrees -eq 70.0 -and
        -not [string]::IsNullOrWhiteSpace([string]$receipt.final_color_capture_session_id) -and
        [int]$receipt.final_color_capture_session_written_image_count -eq 1 -and
        [bool]$receipt.final_color_captured) `
        'Shipping Water proof receipt failed its product/input/capture contract.'
    Assert-PlayableTour (Test-Path -LiteralPath ([string]$receipt.reference_screenshot) -PathType Leaf) `
        'Shipping Water reference screenshot is missing.'
    Assert-PlayableTour (Test-Path -LiteralPath ([string]$receipt.repeat_screenshot) -PathType Leaf) `
        'Shipping Water repeat screenshot is missing.'
    Assert-PlayableTour (Test-Path -LiteralPath ([string]$receipt.final_color_screenshot) -PathType Leaf) `
        'Shipping Water final-color screenshot is missing.'
    return $receipt
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
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$ExpectedOperationId
    )
    Assert-PlayableTour (Test-Path -LiteralPath $Path -PathType Leaf) `
        "$Configuration product-route receipt is missing."
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    Assert-PlayableTour (
        [string]$receipt.status -ceq 'accepted' -and
        [string]$receipt.operation_id -ceq $ExpectedOperationId -and
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
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ExpectedOperationId,
        [Parameter(Mandatory = $true)][string]$ExpectedSamplePath,
        [Parameter(Mandatory = $true)][int]$ProcessExitCode
    )
    Assert-PlayableTour (Test-Path -LiteralPath $Path -PathType Leaf) `
        'Development performance receipt is missing.'
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $initialCenterCells = @($receipt.initial_center_cell_ids)
    $unloadedCenterCells = @($receipt.unloaded_center_cell_ids)
    $reloadedCenterCells = @($receipt.reloaded_center_cell_ids)
    $completedCenterCycles = @($unloadedCenterCells | Where-Object {
            $reloadedCenterCells -contains $_ -and $initialCenterCells -contains $_
        })
    $outcome = Get-ProjectWorldPerformanceChildOutcome -Receipt $receipt `
        -ProcessExitCode $ProcessExitCode
    Assert-PlayableTour $outcome.valid `
        'Development child failed outside the aggregatable Frame p95 decision.'
    Assert-PlayableTour (
        [string]$receipt.operation_id -ceq $ExpectedOperationId -and
        [string]$receipt.build_configuration -ceq 'Development' -and
        [string]$receipt.gpu_adapter -ceq 'NVIDIA GeForce RTX 4070' -and
        [string]$receipt.rhi -ceq 'D3D12' -and
        [string]$receipt.quality_preset -ceq 'High' -and
        [int]$receipt.resolution_x -eq 2560 -and
        [int]$receipt.resolution_y -eq 1440 -and
        (Test-ProjectWorldPerformanceEnvelopeReceipt -Receipt $receipt) -and
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
        [bool]$receipt.pause_menu_opened -and
        [bool]$receipt.pause_menu_closed -and
        [int]$receipt.waypoints_reached -ge 3 -and
        [double]$receipt.ascent_cm -gt 4000.0 -and
        [double]$receipt.descent_cm -gt 3000.0 -and
        [double]$receipt.horizontal_displacement_cm -gt 100000.0 -and
        [bool]$receipt.collision_blocked_descent -and
        [bool]$receipt.collision_slide) `
        'Development playable-tour performance/input contract was rejected.'
    Assert-PlayableTour (Test-Path -LiteralPath ([string]$receipt.csv_capture) -PathType Leaf) `
        'Development CSV capture is missing.'
    Assert-PlayableTour (
        $null -ne $receipt.PSObject.Properties['raw_sample_capture'] -and
        ([IO.Path]::GetFullPath([string]$receipt.raw_sample_capture)).Equals(
            [IO.Path]::GetFullPath($ExpectedSamplePath),
            [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $ExpectedSamplePath -PathType Leaf)) `
        'Development exact collector-sample capture is missing or unauthenticated.'
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
        Move-PlayableTourPackage -Source $previousFinalPackage -Destination $finalPackage
    }
}
Remove-PlayableTourWorkspace -Path $runtimeRoot
Remove-PlayableTourWorkspace -Path $focusedProbeRoot
New-Item -ItemType Directory -Path $workRoot, $runtimeRoot -Force | Out-Null
if (Test-Path -LiteralPath $finalPackage) {
    Move-PlayableTourPackage -Source $finalPackage -Destination $previousFinalPackage
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
    $waterAuthorityPath = Join-Path $evidenceRoot 'water-viewpoint-authority.json'
    & $waterViewpointVerifier -CompileResult $compileResult `
        -ActiveManifestSet $activeManifestSet `
        -UnrealXCentimeters $shippingWaterTargetXCentimeters `
        -UnrealYCentimeters $shippingWaterTargetYCentimeters `
        -FeatureId 'alis:osm:relation:7493502' -FeatureClass 'water' `
        -ReceiptPath $waterAuthorityPath | Out-Null
    Assert-PlayableTour ($LASTEXITCODE -eq 0) 'Shipping Water viewpoint authority verification failed.'
    $waterAuthority = Get-Content -LiteralPath $waterAuthorityPath -Raw | ConvertFrom-Json
    Assert-PlayableTour (
        [string]$waterAuthority.status -ceq 'accepted' -and
        [string]$waterAuthority.target_cell_id -ceq 'grid_413718bc833994e5:x1:y1' -and
        [string]$waterAuthority.feature_id -ceq 'alis:osm:relation:7493502' -and
        [string]$waterAuthority.feature_class -ceq 'water') `
        'Shipping Water viewpoint authority receipt is invalid.'
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
    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'before Development packaging'
    Invoke-PlayableTourPackage -OutputRoot $developmentPackage -Configuration 'Development'
    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'after Development packaging'
    Move-PlayableTourPackage -Source $developmentPackage -Destination $finalPackage
    $developmentExecutable = Get-PlayableTourExecutable -PackageRoot $finalPackage
    $developmentPackageHash = Get-ProjectWorldPackagePayloadDigest -Path $finalPackage
    $developmentExecutableHash = (Get-FileHash -LiteralPath $developmentExecutable `
        -Algorithm SHA256).Hash.ToLowerInvariant()
    $developmentChildren = [Collections.Generic.List[object]]::new()
    $developmentExitCodes = [Collections.Generic.List[int]]::new()
    for ($developmentIndex = 1; $developmentIndex -le 3; ++$developmentIndex) {
        $childName = "run-{0:D2}" -f $developmentIndex
        $childRoot = Join-Path $developmentRoot $childName
        New-Item -ItemType Directory -Path $childRoot -Force | Out-Null
        $childOperationId = "$operationId-development-$childName"
        $childCorrectnessPath = Join-Path $childRoot 'product-route.json'
        $childPerformancePath = Join-Path $childRoot 'performance.json'
        $childCsvPath = Join-Path $childRoot 'performance.csv'
        $childSamplePath = Join-Path $childRoot 'performance.samples.csv'
        $childScreenshotPath = Join-Path $childRoot 'playable-tour.png'
        $childLogPath = Join-Path $childRoot 'game.log'
        $childExitCode = Invoke-PlayableTourGame `
            -Executable $developmentExecutable -Configuration 'Development' `
            -RunOperationId $childOperationId `
            -CorrectnessPath $childCorrectnessPath -LogPath $childLogPath `
            -PerformancePath $childPerformancePath -CsvPath $childCsvPath `
            -SamplePath $childSamplePath -ScreenshotPath $childScreenshotPath
        Assert-PlayableTour ($childExitCode -eq 0 -or $childExitCode -eq 10) `
            "Development child $childName exited abnormally with code $childExitCode."
        Assert-PlayableTourNormalExit -LogPath $childLogPath -Configuration 'Development'
        Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
            -ExpectedRuntimeHash $runtimeProfileHash `
            -Stage "after Development execution $childName"
        $childCorrectness = Read-PlayableTourCorrectness `
            -Path $childCorrectnessPath -Configuration 'Development' `
            -ExpectedOperationId $childOperationId
        $childPerformance = Read-PlayableTourPerformance `
            -Path $childPerformancePath -ExpectedOperationId $childOperationId `
            -ExpectedSamplePath $childSamplePath -ProcessExitCode $childExitCode
        Assert-PlayableTour ((Get-ProjectWorldPackagePayloadDigest -Path $finalPackage) -ceq `
                $developmentPackageHash) `
            "Development package payload bytes changed during $childName."
        $developmentExitCodes.Add($childExitCode)
        $developmentChildren.Add([pscustomobject][ordered]@{
                ExpectedOperationId = $childOperationId
                ExpectedProcessExitCode = $childExitCode
                ReceiptPath = $childPerformancePath
                SamplePath = $childSamplePath
                RichCsvPath = $childCsvPath
                CorrectnessPath = $childCorrectnessPath
                LogPath = $childLogPath
                ProductScreenshotPath = [string]$childCorrectness.screenshot
                PlayableScreenshotPath = [string]$childPerformance.playable_tour_screenshot
            })
    }
    $developmentPerformance = New-ProjectWorldPerformanceAggregate `
        -Children @($developmentChildren) -OperationId $operationId `
        -SourceRevision $sourceRevision -SourceStateSha256 $sourceStateHash `
        -RuntimeProfileSha256 $runtimeProfileHash `
        -ExpectedExecutable $developmentExecutable `
        -ExpectedExecutableSha256 $developmentExecutableHash `
        -ExpectedPackage $finalPackage -ExpectedPackageSha256 $developmentPackageHash
    $developmentAggregatePath = Join-Path $developmentRoot 'performance-aggregate.json'
    [IO.File]::WriteAllText(
        $developmentAggregatePath,
        ($developmentPerformance | ConvertTo-Json -Depth 12) + "`n",
        [Text.UTF8Encoding]::new($false))
    Assert-PlayableTour ([string]$developmentPerformance.status -ceq 'accepted') `
        ("Development pooled performance rejected: {0}" -f `
            [string]$developmentPerformance.acceptance_reason)
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
    Move-PlayableTourPackage -Source $shippingPackage -Destination $finalPackage
    $shippingExecutable = Get-PlayableTourExecutable -PackageRoot $finalPackage
    $shippingExitCode = Invoke-PlayableTourGame `
        -Executable $shippingExecutable -Configuration 'Shipping' `
        -RunOperationId $operationId `
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
        -Path $shippingCorrectnessPath -Configuration 'Shipping' `
        -ExpectedOperationId $operationId
    $shippingWaterResultPath = Join-Path $shippingRoot 'water-proof.json'
    $shippingWaterReferencePath = Join-Path $shippingRoot 'water.png'
    $shippingWaterRepeatPath = Join-Path $shippingRoot 'water.control.png'
    $shippingWaterProductPath = Join-Path $shippingRoot 'water-product.png'
    $shippingWaterTemporalPath = Join-Path $shippingRoot 'water-temporal.json'
    $shippingWaterExitCode = Invoke-ShippingWaterProof `
        -Executable $shippingExecutable -ResultPath $shippingWaterResultPath `
        -ReferencePath $shippingWaterReferencePath -RepeatPath $shippingWaterRepeatPath `
        -ProductPath $shippingWaterProductPath
    Assert-PlayableTour ($shippingWaterExitCode -eq 0) `
        "Shipping Water proof exited abnormally with code $shippingWaterExitCode."
    $shippingWater = Read-ShippingWaterProof -Path $shippingWaterResultPath
    & $waterVerifier -ReferencePath $shippingWaterReferencePath `
        -RepeatPath $shippingWaterRepeatPath -ReceiptPath $shippingWaterTemporalPath
    Assert-PlayableTour ($LASTEXITCODE -eq 0) 'Shipping Water temporal verification failed.'
    $shippingWaterTemporal = Get-Content -LiteralPath $shippingWaterTemporalPath -Raw | ConvertFrom-Json
    Assert-PlayableTour ([string]$shippingWaterTemporal.status -ceq 'accepted') `
        'Shipping Water temporal receipt was not accepted.'
    Assert-PlayableTourSourceState -ExpectedSourceHash $sourceStateHash `
        -ExpectedRuntimeHash $runtimeProfileHash -Stage 'after Shipping Water proof'
    $shippingPackageHash = Get-ProjectWorldPackagePayloadDigest -Path $finalPackage
    $shippingExecutableHash = (Get-FileHash -LiteralPath $shippingExecutable `
        -Algorithm SHA256).Hash.ToLowerInvariant()

    $artifactPaths = [ordered]@{
        development_performance_aggregate = $developmentAggregatePath
        shipping_correctness = $shippingCorrectnessPath
        shipping_product_screenshot = [string]$shippingCorrectness.screenshot
        shipping_water_proof = $shippingWaterResultPath
        shipping_water_viewpoint_authority = $waterAuthorityPath
        shipping_water_reference = [string]$shippingWater.reference_screenshot
        shipping_water_repeat = [string]$shippingWater.repeat_screenshot
        shipping_water_product = [string]$shippingWater.final_color_screenshot
        shipping_water_temporal = $shippingWaterTemporalPath
    }
    for ($developmentIndex = 0; $developmentIndex -lt $developmentChildren.Count; ++$developmentIndex) {
        $child = $developmentChildren[$developmentIndex]
        $prefix = "development_run_{0:D2}" -f ($developmentIndex + 1)
        $artifactPaths["${prefix}_correctness"] = [string]$child.CorrectnessPath
        $artifactPaths["${prefix}_performance"] = [string]$child.ReceiptPath
        $artifactPaths["${prefix}_samples"] = [string]$child.SamplePath
        $artifactPaths["${prefix}_csv"] = [string]$child.RichCsvPath
        $artifactPaths["${prefix}_log"] = [string]$child.LogPath
        $artifactPaths["${prefix}_product_screenshot"] = [string]$child.ProductScreenshotPath
        $artifactPaths["${prefix}_playable_screenshot"] = [string]$child.PlayableScreenshotPath
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
        development_exit_codes = @($developmentExitCodes)
        development_executable_sha256 = $developmentExecutableHash
        development_package_sha256 = $developmentPackageHash
        development_package_digest_scope = 'payload_excluding_runtime_state'
        shipping_exit_code = $shippingExitCode
        shipping_water_exit_code = $shippingWaterExitCode
        shipping_executable_sha256 = $shippingExecutableHash
        shipping_package_sha256 = $shippingPackageHash
        shipping_package_digest_scope = 'payload_excluding_runtime_state'
        package_digest_excluded_runtime_state = @(
            'Windows/Alis/LocalAppData',
            'Windows/Alis/Saved',
            'Windows/Engine/Saved'
        )
        shipping_water_target_xy_error_cm = [double]$shippingWater.target_xy_error_cm
        shipping_water_target_cell_id = [string]$waterAuthority.target_cell_id
        shipping_water_feature_id = [string]$waterAuthority.feature_id
        shipping_water_final_color_capture_source = [string]$shippingWater.final_color_capture_source
        shipping_water_final_color_camera_location_cm = @($shippingWater.final_color_camera_location_cm)
        shipping_water_final_color_camera_rotation_deg = @($shippingWater.final_color_camera_rotation_deg)
        shipping_water_final_color_field_of_view_degrees = `
            [double]$shippingWater.final_color_field_of_view_degrees
        shipping_water_blue_pixel_union = [Int64]$shippingWaterTemporal.metrics.blue_pixel_union
        shipping_water_blue_classification_flip_ratio = `
            [double]$shippingWaterTemporal.metrics.blue_classification_flip_ratio
        shipping_water_mean_blue_channel_delta = `
            [double]$shippingWaterTemporal.metrics.mean_blue_channel_delta
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
            Move-PlayableTourPackage -Source $previousFinalPackage -Destination $finalPackage
        }
    }
    Remove-PlayableTourWorkspace -Path $workRoot
    Remove-PlayableTourWorkspace -Path $runtimeRoot
}

if (-not $accepted) {
    exit 1
}
