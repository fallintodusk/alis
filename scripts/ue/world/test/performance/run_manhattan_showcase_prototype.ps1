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
    [switch]$SkipShipping
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

function Assert-ManhattanShowcase {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) {
        throw $Message
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
        [Parameter(Mandatory = $true)][string]$LogPath
    )
    $arguments = @(
        "-ProjectMenuPlayAutoExperience=$experienceId",
        '-ProjectMenuPlayAutoMode=SinglePlayer',
        '-ProjectWorldProductRouteGate',
        '-ProjectWorldProductRouteRestorePreviewFlight',
        # The showcase has no gameplay-placement layer by design; this is the only
        # invocation permitted to omit the interaction gate.
        '-ProjectWorldProductRouteSkipInteraction',
        "-ProjectWorldProductOperation=$RunOperationId",
        "-ProjectWorldProductResult=$CorrectnessPath",
        "-ProjectWorldProductMap=$mapPackage",
        '-ProjectWorldProductRuntime=manhattan_showcase_512_1536_v1',
        "-ProjectWorldProductRuntimeHash=$script:runtimeProfileHash",
        '-ProjectWorldProductMachine=rtx4070_primary',
        "-ProjectWorldProductEdge=$edgeArgument",
        '-ResX=2560', '-ResY=1440', '-Windowed', '-ForceRes',
        '-RenderOffScreen', '-novsync', '-unattended', '-nosplash', '-NoMessaging',
        "-abslog=$LogPath"
    )
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
    Assert-ManhattanShowcase (-not [bool]$receipt.gameplay_interaction_required) `
        "$Configuration Manhattan receipt did not record the non-interactive policy."

    Assert-ManhattanShowcase (Test-Path -LiteralPath ([string]$receipt.screenshot) -PathType Leaf) `
        "$Configuration Manhattan product-route screenshot is missing."
    return $receipt
}

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
