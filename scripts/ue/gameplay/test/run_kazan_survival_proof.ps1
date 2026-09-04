# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [int]$GameTimeoutSeconds = 720
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$packageScript = Join-Path $projectRoot 'scripts\ue\package\package_release.ps1'
$runtimeProfile = Join-Path $projectRoot `
    'Plugins\World\ProjectWorldData\Data\Runtime\kazan_territory_512_1536_v1.json'
$mapPackage = '/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory'
$runId = [Guid]::NewGuid().ToString('N')
$operationId = "kazan_survival_$runId"
$evidenceRoot = Join-Path $projectRoot "Saved\Validation\Gameplay\KazanSurvival\$runId"
$ownerRoot = Join-Path $projectRoot 'tmp\gameplay\kazan_survival\package_proof'
$runtimeRoot = Join-Path $ownerRoot 'runtime'
$developmentPackage = Join-Path $runtimeRoot 'development'
$shippingPackage = Join-Path $runtimeRoot 'shipping'
$packageRoot = Join-Path $projectRoot 'Saved\PackageRelease\KazanSurvival'
$candidatePackage = Join-Path $packageRoot 'Candidate'
$previousCandidatePackage = Join-Path $packageRoot 'PreviousCandidate'
$previewFlightLauncher = Join-Path $PSScriptRoot 'launch_kazan_preview_flight.cmd'
. (Join-Path $projectRoot `
    'scripts\ue\world\test\performance\project_world_performance_evidence.ps1')

function Assert-SurvivalProof {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) { throw $Message }
}

function Get-SurvivalSourceDigest {
    $parts = [Collections.Generic.List[string]]::new()
    $parts.Add((@(& git -C $projectRoot diff --binary --no-ext-diff HEAD) -join "`n"))
    Assert-SurvivalProof ($LASTEXITCODE -eq 0) 'Unable to read tracked source state.'
    foreach ($relative in @(& git -C $projectRoot ls-files --others --exclude-standard | Sort-Object)) {
        $path = Join-Path $projectRoot $relative
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        $parts.Add("$relative|$hash")
    }
    Assert-SurvivalProof ($LASTEXITCODE -eq 0) 'Unable to read untracked source state.'
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes(($parts -join "`n"))
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}

function Assert-SurvivalSourceFrozen {
    param(
        [Parameter(Mandatory = $true)][string]$SourceHash,
        [Parameter(Mandatory = $true)][string]$RuntimeHash,
        [Parameter(Mandatory = $true)][string]$Stage
    )
    Assert-SurvivalProof ((Get-SurvivalSourceDigest) -ceq $SourceHash) `
        "Source state changed during Kazan survival proof at $Stage."
    $observedRuntimeHash = (Get-FileHash -LiteralPath $runtimeProfile -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-SurvivalProof ($observedRuntimeHash -ceq $RuntimeHash) `
        "Runtime profile changed during Kazan survival proof at $Stage."
}

function Remove-SurvivalWorkspace {
    param([Parameter(Mandatory = $true)][string]$Path)
    $owner = [IO.Path]::GetFullPath($ownerRoot).TrimEnd('\', '/')
    $target = [IO.Path]::GetFullPath($Path)
    if (-not $target.StartsWith(
            $owner + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Survival proof cleanup escaped its owner root: $target"
    }
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

function Remove-SurvivalPackage {
    param([Parameter(Mandatory = $true)][string]$Path)
    $target = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $allowed = @($candidatePackage, $previousCandidatePackage) | ForEach-Object {
        [IO.Path]::GetFullPath($_).TrimEnd('\', '/')
    }
    Assert-SurvivalProof (@($allowed | Where-Object {
                $_.Equals($target, [StringComparison]::OrdinalIgnoreCase)
            }).Count -eq 1) "Package cleanup escaped the Kazan survival owner: $target"
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

function Assert-SurvivalCandidateReplaceable {
    if (-not (Test-Path -LiteralPath $candidatePackage)) {
        return
    }

    $probePackage = Join-Path $packageRoot 'CandidatePublicationProbe'
    Assert-SurvivalProof (-not (Test-Path -LiteralPath $probePackage)) `
        "Candidate publication probe already exists: $probePackage"
    try {
        Move-Item -LiteralPath $candidatePackage -Destination $probePackage -ErrorAction Stop
    }
    catch {
        throw "Candidate cannot be rotated before packaging. Close any game or File Explorer window browsing inside '$candidatePackage', then rerun. $($_.Exception.Message)"
    }
    finally {
        if (Test-Path -LiteralPath $probePackage) {
            Move-Item -LiteralPath $probePackage -Destination $candidatePackage -ErrorAction Stop
        }
    }
}

function Get-SurvivalExecutable {
    param([Parameter(Mandatory = $true)][string]$PackagePath)
    $candidates = @(Get-ChildItem -LiteralPath (Join-Path $PackagePath 'Windows') `
        -Recurse -File -Filter 'Alis*.exe' |
        Where-Object { $_.FullName -match '[\\/]Alis[\\/]Binaries[\\/]Win64[\\/]' } |
        Sort-Object FullName)
    Assert-SurvivalProof ($candidates.Count -eq 1) `
        "Expected one staged game executable under $PackagePath."
    return $candidates[0].FullName
}

function Get-SurvivalTreeDigest {
    param([Parameter(Mandatory = $true)][string]$Path)
    $root = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $lines = @(Get-ChildItem -LiteralPath $root -Recurse -File -Force |
        Sort-Object FullName | ForEach-Object {
            $relative = $_.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            '{0}|{1}|{2}' -f $relative, $_.Length, $hash
        })
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash(
                    [Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}

function Invoke-SurvivalPackage {
    param(
        [Parameter(Mandatory = $true)][string]$OutputRoot,
        [Parameter(Mandatory = $true)][string]$Configuration
    )
    & $packageScript -OutputDir $OutputRoot -ClientConfig $Configuration -RequiredCookMap $mapPackage
    Assert-SurvivalProof ($LASTEXITCODE -eq 0) "$Configuration launcher-engine packaging failed."
}

function Invoke-SurvivalRoute {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][ValidateSet('success', 'failure')][string]$Route,
        [Parameter(Mandatory = $true)][string]$ResultPath,
        [Parameter(Mandatory = $true)][string]$ScreenshotPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [string]$PerformancePath,
        [string]$CsvPath,
        [string]$SamplePath,
        [string]$PerformanceScreenshotPath
    )
    $arguments = @(
        '-ProjectMenuPlayAutoExperience=KazanTerritory',
        '-ProjectMenuPlayAutoMode=SinglePlayer',
        '-ProjectMenuPlayAutoScenario=UrbanSurvivalProofV1',
        '-ProjectMenuPlayAutoTraversal=PreviewFlight',
        '-ProjectSinglePlayScenarioGate',
        "-ProjectSinglePlayScenarioOperation=$operationId",
        "-ProjectSinglePlayScenarioRoute=$Route",
        "-ProjectSinglePlayScenarioResult=$ResultPath",
        "-ProjectSinglePlayScenarioScreenshot=$ScreenshotPath",
        '-ProjectWorldRuntimeProfile=kazan_territory_512_1536_v1',
        "-ProjectWorldRuntimeProfileSha256=$script:runtimeProfileHash",
        '-ProjectWorldMachineProfile=rtx4070_primary',
        '-ResX=2560', '-ResY=1440', '-Windowed', '-ForceRes',
        '-RenderOffScreen', '-novsync', '-unattended', '-nosplash',
        '-NoMessaging', "-abslog=$LogPath"
    )
    if ($Configuration -ceq 'Development' -and $Route -ceq 'success') {
        $arguments += @(
            '-ProjectWorldProductPerformanceGate',
            '-ProjectWorldPlayableTour',
            "-ProjectWorldPerformanceResult=$PerformancePath",
            "-ProjectWorldPerformanceCorrectness=$ResultPath",
            "-ProjectWorldPerformanceCsv=$CsvPath",
            "-ProjectWorldPerformanceSamples=$SamplePath",
            "-ProjectWorldPerformanceScreenshot=$PerformanceScreenshotPath"
        )
    }
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments `
        -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit($GameTimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "$Configuration $Route scenario process exceeded the bounded timeout."
    }
    return $process.ExitCode
}

function Read-SurvivalReceipt {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][ValidateSet('success', 'failure')][string]$Route
    )
    Assert-SurvivalProof (Test-Path -LiteralPath $Path -PathType Leaf) `
        "$Configuration $Route scenario receipt is missing."
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $common = [string]$receipt.status -ceq 'accepted' -and
        [string]$receipt.correctness_contract -ceq 'single-play-scenario-v1' -and
        [string]$receipt.operation_id -ceq $operationId -and
        [string]$receipt.route -ceq $Route -and
        [string]$receipt.build_configuration -ceq $Configuration -and
        [string]$receipt.map_package -ceq $mapPackage -and
        [string]$receipt.runtime_profile_sha256 -ceq $runtimeProfileHash -and
        [bool]$receipt.project_loading_provenance -and [bool]$receipt.possessed_player
    if ($Route -ceq 'success') {
        $common = $common -and [bool]$receipt.gameplay_interaction -and
            [bool]$receipt.inventory_opened -and [bool]$receipt.pouch_equipped -and
            [bool]$receipt.capacity_rejected -and
            [string]$receipt.capacity_rejection -ceq `
                'Target inventory container would exceed its weight limit.' -and
            [bool]$receipt.water_used -and [double]$receipt.hydration_after -gt 0.20 -and
            [bool]$receipt.ration_carried
    }
    else {
        $common = $common -and [bool]$receipt.failure_observed -and [bool]$receipt.restart_observed
    }
    Assert-SurvivalProof $common "$Configuration $Route scenario contract was rejected."
    Assert-SurvivalProof (Test-Path -LiteralPath ([string]$receipt.screenshot) -PathType Leaf) `
        "$Configuration $Route screenshot is missing."
    return $receipt
}

function Read-SurvivalPerformance {
    param([Parameter(Mandatory = $true)][string]$Path)
    Assert-SurvivalProof (Test-Path -LiteralPath $Path -PathType Leaf) `
        'Development performance receipt is missing.'
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $initialCenterCells = @($receipt.initial_center_cell_ids)
    $unloadedCenterCells = @($receipt.unloaded_center_cell_ids)
    $reloadedCenterCells = @($receipt.reloaded_center_cell_ids)
    $completedCenterCycles = @($unloadedCenterCells | Where-Object {
            $reloadedCenterCells -contains $_ -and $initialCenterCells -contains $_
        })
    Assert-SurvivalProof (
        [string]$receipt.status -ceq 'accepted' -and
        [bool]$receipt.playable_tour -and
        (Test-ProjectWorldPerformanceEnvelopeReceipt -Receipt $receipt) -and
        [double]$receipt.frame_p95_ms -le 16.67 -and
        [int]$receipt.streaming_failures -eq 0 -and
        [bool]$receipt.center_cell_streaming_cycle -and
        $completedCenterCycles.Count -gt 0 -and
        [string]$receipt.input_method -ceq `
            'APlayerController::InputKey/FInputKeyEventArgs::CreateSimulated' -and
        [int]$receipt.input_event_count -gt 0 -and
        [bool]$receipt.collision_blocked_descent -and
        [bool]$receipt.collision_slide) `
        'Development scenario performance/streaming contract was rejected.'
    Assert-SurvivalProof (Test-Path -LiteralPath ([string]$receipt.csv_capture) -PathType Leaf) `
        'Development performance CSV capture is missing.'
    Assert-SurvivalProof (
        $null -ne $receipt.PSObject.Properties['raw_sample_capture'] -and
        (Test-Path -LiteralPath ([string]$receipt.raw_sample_capture) -PathType Leaf)) `
        'Development exact performance sample capture is missing.'
    $null = @(Import-ProjectWorldPerformanceSamples `
            -Path ([string]$receipt.raw_sample_capture) `
            -ExpectedCount ([int]$receipt.sample_count))
    Assert-SurvivalProof (
        Test-Path -LiteralPath ([string]$receipt.playable_tour_screenshot) -PathType Leaf) `
        'Development playable-tour screenshot is missing.'
    return $receipt
}

Remove-SurvivalWorkspace -Path $runtimeRoot
New-Item -ItemType Directory -Path $runtimeRoot, $evidenceRoot, $packageRoot -Force | Out-Null
$accepted = $false
$candidateRotated = $false
try {
    $runtimeProfileHash = (Get-FileHash -LiteralPath $runtimeProfile -Algorithm SHA256).Hash.ToLowerInvariant()
    $sourceStateHash = Get-SurvivalSourceDigest
    $sourceRevision = (& git -C $projectRoot rev-parse HEAD).Trim()
    Assert-SurvivalProof ($LASTEXITCODE -eq 0) 'Unable to freeze source revision.'
    Assert-SurvivalCandidateReplaceable

    $developmentRoot = Join-Path $evidenceRoot 'development'
    $shippingRoot = Join-Path $evidenceRoot 'shipping'
    New-Item -ItemType Directory -Path $developmentRoot, $shippingRoot -Force | Out-Null
    Assert-SurvivalSourceFrozen $sourceStateHash $runtimeProfileHash 'before Development packaging'
    Invoke-SurvivalPackage $developmentPackage 'Development'
    Assert-SurvivalSourceFrozen $sourceStateHash $runtimeProfileHash 'after Development packaging'
    $developmentExecutable = Get-SurvivalExecutable $developmentPackage

    $devSuccess = Join-Path $developmentRoot 'success.json'
    $devSuccessScreenshot = Join-Path $developmentRoot 'success.png'
    $devPerformance = Join-Path $developmentRoot 'performance.json'
    $devCsv = Join-Path $developmentRoot 'performance.csv'
    $devSamples = Join-Path $developmentRoot 'performance.samples.csv'
    $devTourScreenshot = Join-Path $developmentRoot 'performance-tour.png'
    $devSuccessLog = Join-Path $developmentRoot 'success.log'
    $exitCode = Invoke-SurvivalRoute $developmentExecutable 'Development' 'success' `
        $devSuccess $devSuccessScreenshot $devSuccessLog $devPerformance $devCsv `
        $devSamples $devTourScreenshot
    Assert-SurvivalProof ($exitCode -eq 0) "Development success exited with $exitCode."
    $devSuccessReceipt = Read-SurvivalReceipt $devSuccess 'Development' 'success'
    $devPerformanceReceipt = Read-SurvivalPerformance $devPerformance

    $devFailure = Join-Path $developmentRoot 'failure.json'
    $devFailureScreenshot = Join-Path $developmentRoot 'failure.png'
    $devFailureLog = Join-Path $developmentRoot 'failure.log'
    $exitCode = Invoke-SurvivalRoute $developmentExecutable 'Development' 'failure' `
        $devFailure $devFailureScreenshot $devFailureLog
    Assert-SurvivalProof ($exitCode -eq 0) "Development failure route exited with $exitCode."
    $devFailureReceipt = Read-SurvivalReceipt $devFailure 'Development' 'failure'
    Assert-SurvivalSourceFrozen $sourceStateHash $runtimeProfileHash 'after Development routes'

    Invoke-SurvivalPackage $shippingPackage 'Shipping'
    Assert-SurvivalSourceFrozen $sourceStateHash $runtimeProfileHash 'after Shipping packaging'
    $shippingExecutable = Get-SurvivalExecutable $shippingPackage
    $shippingSuccess = Join-Path $shippingRoot 'success.json'
    $shippingSuccessScreenshot = Join-Path $shippingRoot 'success.png'
    $shippingSuccessLog = Join-Path $shippingRoot 'success.log'
    $exitCode = Invoke-SurvivalRoute $shippingExecutable 'Shipping' 'success' `
        $shippingSuccess $shippingSuccessScreenshot $shippingSuccessLog
    Assert-SurvivalProof ($exitCode -eq 0) "Shipping success exited with $exitCode."
    $shippingSuccessReceipt = Read-SurvivalReceipt $shippingSuccess 'Shipping' 'success'

    $shippingFailure = Join-Path $shippingRoot 'failure.json'
    $shippingFailureScreenshot = Join-Path $shippingRoot 'failure.png'
    $shippingFailureLog = Join-Path $shippingRoot 'failure.log'
    $exitCode = Invoke-SurvivalRoute $shippingExecutable 'Shipping' 'failure' `
        $shippingFailure $shippingFailureScreenshot $shippingFailureLog
    Assert-SurvivalProof ($exitCode -eq 0) "Shipping failure route exited with $exitCode."
    $shippingFailureReceipt = Read-SurvivalReceipt $shippingFailure 'Shipping' 'failure'
    Assert-SurvivalSourceFrozen $sourceStateHash $runtimeProfileHash 'before candidate publication'

    Remove-SurvivalPackage $previousCandidatePackage
    if (Test-Path -LiteralPath $candidatePackage) {
        Move-Item -LiteralPath $candidatePackage -Destination $previousCandidatePackage
        $candidateRotated = $true
    }
    Move-Item -LiteralPath $shippingPackage -Destination $candidatePackage
    $candidatePreviewFlightLauncher = Join-Path $candidatePackage 'Launch_Kazan_PreviewFlight.cmd'
    Copy-Item -LiteralPath $previewFlightLauncher -Destination $candidatePreviewFlightLauncher
    Assert-SurvivalProof (Test-Path -LiteralPath $candidatePreviewFlightLauncher) `
        'PreviewFlight candidate launcher was not published.'
    $candidateHash = Get-SurvivalTreeDigest $candidatePackage
    $composite = [ordered]@{
        schema_version = 1
        status = 'technical_gates_accepted_awaiting_operator_walkthrough'
        operation_id = $operationId
        revision = $sourceRevision
        source_state_sha256 = $sourceStateHash
        runtime_profile_sha256 = $runtimeProfileHash
        candidate_package = $candidatePackage
        preview_flight_launcher = $candidatePreviewFlightLauncher
        previous_candidate_package = if ($candidateRotated) { $previousCandidatePackage } else { $null }
        candidate_package_sha256 = $candidateHash
        development_success = $devSuccess
        development_failure = $devFailure
        development_performance = $devPerformance
        shipping_success = $shippingSuccess
        shipping_failure = $shippingFailure
        frame_p95_ms = [double]$devPerformanceReceipt.frame_p95_ms
        streaming_failures = [int]$devPerformanceReceipt.streaming_failures
        cleanup = 'owner_tmp_cleaned'
    }
    $compositePath = Join-Path $evidenceRoot 'composite.json'
    [IO.File]::WriteAllText(
        $compositePath,
        ($composite | ConvertTo-Json -Depth 8) + "`n",
        [Text.UTF8Encoding]::new($false))
    $accepted = $true
    Write-Host "[KazanSurvivalProof] Technical gates accepted: $compositePath" -ForegroundColor Green
    Write-Host "[KazanSurvivalProof] Operator candidate: $candidatePackage" -ForegroundColor Green
}
finally {
    if (-not $accepted -and -not (Test-Path -LiteralPath $candidatePackage) -and
        (Test-Path -LiteralPath $previousCandidatePackage)) {
        Move-Item -LiteralPath $previousCandidatePackage -Destination $candidatePackage
    }
    Remove-SurvivalWorkspace -Path $runtimeRoot
}

if (-not $accepted) { exit 1 }
