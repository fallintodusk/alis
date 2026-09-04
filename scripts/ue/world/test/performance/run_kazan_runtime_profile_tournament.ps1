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
$realizeScript = Join-Path $worldRoot 'realize_canonical_world.ps1'
$packageScript = Join-Path $projectRoot 'scripts\ue\package\package_release.ps1'
$cleanupScript = Join-Path $worldRoot 'cleanup_workspace.ps1'
$compilerBootstrap = Join-Path $projectRoot 'tools\World\CanonicalCompilation\bootstrap.py'
. (Join-Path $worldRoot 'generated_content_transaction.ps1')
. (Join-Path $worldRoot 'generated_manifest.ps1')
. (Join-Path $worldRoot 'realization_layer_operation.ps1')
. (Join-Path $worldRoot 'runtime_profile_tournament.ps1')
. (Join-Path $PSScriptRoot '..\runtime_profile_test_helpers.ps1')
. (Join-Path $PSScriptRoot 'project_world_performance_evidence.ps1')

function Resolve-TournamentPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([IO.Path]::IsPathRooted($Path)) {
        return [IO.Path]::GetFullPath($Path)
    }
    return [IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Assert-Tournament {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) { throw $Message }
}

function Get-TournamentTreeBytes {
    param([Parameter(Mandatory = $true)][string]$Path)
    $sum = (Get-ChildItem -LiteralPath $Path -File -Recurse -Force |
        Measure-Object Length -Sum).Sum
    return [Int64]$(if ($null -eq $sum) { 0 } else { $sum })
}

function Remove-TournamentWorkspace {
    param([Parameter(Mandatory = $true)][string]$Path)
    $parent = [IO.Path]::GetFullPath($transactionParent).TrimEnd('\', '/')
    $target = [IO.Path]::GetFullPath($Path)
    if (-not $target.StartsWith(
        $parent + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Tournament cleanup target escaped its owner root: $target"
    }
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

function Initialize-TournamentAuthority {
    param(
        [Parameter(Mandatory = $true)][object]$Active,
        [Parameter(Mandatory = $true)][string]$DestinationRoot
    )
    New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null
    $operationId = [Guid]::NewGuid().ToString('N')
    $candidates = [Collections.Generic.List[object]]::new()
    foreach ($scopeId in @($Active.Manifests.Keys | Sort-Object)) {
        $prior = $Active.Manifests[$scopeId]
        $producerId = Get-ProjectWorldManifestProducerId -Manifest $prior
        $fingerprint = Get-ProjectWorldGeneratorFingerprint `
            -ProjectRoot $projectRoot -ProducerId $producerId
        $candidate = New-ProjectWorldFingerprintMigrationCandidate `
            -PriorManifest $prior -Generation 1 -OperationId $operationId `
            -GeneratorFingerprint $fingerprint
        $candidate.'$schema' = $script:ManifestSchemaId
        $candidates.Add($candidate)
    }
    Publish-ProjectWorldActiveSet `
        -ManifestRoot $DestinationRoot -ProjectRoot $projectRoot `
        -TransactionId $operationId -OperationId $operationId `
        -CandidateManifests @($candidates) | Out-Null
}

function Invoke-TournamentApply {
    param(
        [Parameter(Mandatory = $true)][string]$ProfilePath,
        [Parameter(Mandatory = $true)][string]$RealizationProfilePath,
        [Parameter(Mandatory = $true)][string]$ReceiptPath
    )
    $arguments = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $realizeScript,
        '-CompileResult', $compileResultPath, '-Mode', 'Apply',
        '-Map', $mapPackage, '-WorldDataPlugin', 'ProjectWorldData',
        '-PresentationProfile', $presentationPath,
        '-RuntimeProfile', $ProfilePath,
        '-AuthoredOverlayProfile', $authoredPath,
        '-RealizationProfile', $RealizationProfilePath,
        '-TransientRealizationProfileRoot', $workRoot,
        '-ManifestRoot', $transientManifestRoot,
        '-EvidencePath', $ReceiptPath,
        '-MaxRoads', '1000', '-MaxBuildings', '1000',
        '-RequireLandscape', '-NonInteractive'
    )
    & $powerShellExe @arguments | Out-Host
    $exitCode = $LASTEXITCODE
    $receipt = if (Test-Path -LiteralPath $ReceiptPath -PathType Leaf) {
        Get-Content -LiteralPath $ReceiptPath -Raw | ConvertFrom-Json
    }
    Assert-Tournament ($exitCode -eq 0 -and $null -ne $receipt -and
        [string]$receipt.status -ceq 'accepted') "Runtime-profile Apply failed: $ProfilePath"
    foreach ($inventory in @($receipt.layer_inventories)) {
        Assert-Tournament (@($inventory.final_dirty_units).Count -eq 0) `
            "Runtime-only candidate dirtied generated layer: $($inventory.layer_id)"
    }
    return $receipt
}

function New-TournamentRealizationProfile {
    param(
        [Parameter(Mandatory = $true)][object]$RuntimeProfile,
        [Parameter(Mandatory = $true)][string]$OutputPath
    )
    $document = $realization | ConvertTo-Json -Depth 20 | ConvertFrom-Json
    $document.runtime_profile_id = [string]$RuntimeProfile.profile_id
    Set-ProjectWorldTransientSchemaReference -Document $document `
        -DocumentPath $OutputPath -SchemaPath (Join-Path $projectRoot `
            'Plugins\World\ProjectWorld\Data\Schemas\project_world_realization_profile.schema.json')
    Write-ProjectWorldJson -Path $OutputPath -Document $document
    return $OutputPath
}

function Invoke-TournamentPackage {
    param(
        [Parameter(Mandatory = $true)][string]$OutputRoot,
        [Parameter(Mandatory = $true)][bool]$SkipBuild
    )
    $parameters = @{
        OutputDir = $OutputRoot
        ClientConfig = 'Development'
        RequiredCookMap = $mapPackage
    }
    if ($SkipBuild) { $parameters.SkipBuild = $true }
    & $packageScript @parameters
    Assert-Tournament ($LASTEXITCODE -eq 0) "Launcher-engine packaging failed: $OutputRoot"
}

function Invoke-TournamentGame {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$OperationId,
        [Parameter(Mandatory = $true)][object]$Profile,
        [Parameter(Mandatory = $true)][string]$ProfileHash,
        [Parameter(Mandatory = $true)][string]$CorrectnessPath,
        [Parameter(Mandatory = $true)][string]$PerformancePath,
        [Parameter(Mandatory = $true)][string]$CsvPath,
        [Parameter(Mandatory = $true)][string]$SamplePath
    )
    $arguments = @(
        '-ProjectMenuPlayAutoExperience=KazanTerritory',
        '-ProjectMenuPlayAutoMode=SinglePlayer',
        '-ProjectWorldProductRouteGate',
        '-ProjectWorldProductPerformanceGate',
        "-ProjectWorldProductOperation=$OperationId",
        "-ProjectWorldProductResult=$CorrectnessPath",
        "-ProjectWorldProductMap=$mapPackage",
        "-ProjectWorldProductRuntime=$($Profile.profile_id)",
        "-ProjectWorldProductRuntimeHash=$ProfileHash",
        '-ProjectWorldProductMachine=rtx4070_primary',
        "-ProjectWorldProductEdge=$edgeArgument",
        "-ProjectWorldPerformanceResult=$PerformancePath",
        "-ProjectWorldPerformanceCorrectness=$CorrectnessPath",
        "-ProjectWorldPerformanceCsv=$CsvPath",
        "-ProjectWorldPerformanceSamples=$SamplePath",
        '-ResX=2560', '-ResY=1440', '-Windowed', '-ForceRes',
        '-RenderOffScreen', '-novsync', '-unattended', '-nosplash'
    )
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments `
        -WorkingDirectory (Split-Path -Parent $Executable) -PassThru
    if (-not $process.WaitForExit($GameTimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "Packaged tournament candidate exceeded the bounded game timeout: $($Profile.profile_id)"
    }
    return $process.ExitCode
}

function Read-TournamentCandidate {
    param(
        [Parameter(Mandatory = $true)][object]$Profile,
        [Parameter(Mandatory = $true)][string]$ProfileHash,
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$CorrectnessPath,
        [Parameter(Mandatory = $true)][string]$PerformancePath,
        [Parameter(Mandatory = $true)][string]$CsvPath,
        [Parameter(Mandatory = $true)][string]$SamplePath,
        [Parameter(Mandatory = $true)][Int64]$PackageBytes,
        [Parameter(Mandatory = $true)][Int64]$CookedPayloadBytes,
        [Parameter(Mandatory = $true)][int]$ExitCode
    )
    Assert-Tournament (Test-Path -LiteralPath $CorrectnessPath -PathType Leaf) `
        "Product-route receipt is missing: $($Profile.profile_id)"
    Assert-Tournament (Test-Path -LiteralPath $PerformancePath -PathType Leaf) `
        "Performance receipt is missing: $($Profile.profile_id)"
    $correctness = Get-Content -LiteralPath $CorrectnessPath -Raw | ConvertFrom-Json
    $performance = Get-Content -LiteralPath $PerformancePath -Raw | ConvertFrom-Json
    if (@($performance.routes).Count -eq 0) {
        $details = @($performance.errors | ForEach-Object { [string]$_.message }) -join '; '
        throw "Performance capture did not start for $($Profile.profile_id): $details"
    }
    Assert-Tournament (Test-Path -LiteralPath $CsvPath -PathType Leaf) `
        "Native CSV capture is missing: $($Profile.profile_id)"
    Assert-Tournament (Test-Path -LiteralPath $SamplePath -PathType Leaf) `
        "Exact sample capture is missing: $($Profile.profile_id)"
    $null = @(Import-ProjectWorldPerformanceSamples -Path $SamplePath `
            -ExpectedCount ([int]$performance.sample_count))
    $requiredRoutes = @('dense_centre', 'long_diagonal', 'perimeter', 'backtrack', 'higher_speed_stress')
    $actualRoutes = @($performance.routes | ForEach-Object { [string]$_.route } | Sort-Object)
    $identityAccepted = (
        [string]$correctness.status -ceq 'accepted' -and
        [string]$correctness.runtime_profile -ceq [string]$Profile.profile_id -and
        [string]$performance.runtime_profile -ceq [string]$Profile.profile_id -and
        [string]$correctness.runtime_profile_sha256 -ceq $ProfileHash -and
        [string]$performance.runtime_profile_sha256 -ceq $ProfileHash -and
        [string]$performance.correctness_status -ceq 'accepted' -and
        [string]$performance.gpu_adapter -ceq 'NVIDIA GeForce RTX 4070' -and
        -not [string]::IsNullOrWhiteSpace([string]$performance.gpu_driver) -and
        [string]$performance.rhi -ceq 'D3D12' -and
        [string]$performance.build_configuration -ceq 'Development' -and
        [bool]$performance.render_offscreen -and
        [string]$performance.quality_preset -ceq 'High' -and
        [int]$performance.quality_level -eq 2 -and
        [int]$performance.resolution_x -eq 2560 -and
        [int]$performance.resolution_y -eq 1440 -and
        (Test-ProjectWorldPerformanceEnvelopeReceipt -Receipt $performance) -and
        $null -ne $performance.PSObject.Properties['raw_sample_capture'] -and
        ([IO.Path]::GetFullPath([string]$performance.raw_sample_capture)).Equals(
            [IO.Path]::GetFullPath($SamplePath), [StringComparison]::OrdinalIgnoreCase) -and
        [int]$performance.sample_count -ge 300 -and
        ($actualRoutes -join '|') -ceq (($requiredRoutes | Sort-Object) -join '|'))
    return [pscustomobject][ordered]@{
        profile_id = [string]$Profile.profile_id
        runtime_cell_size_m = [int]$Profile.runtime_partition.cell_size_m
        loading_range_m = [int]$Profile.runtime_partition.loading_range_m
        runtime_profile_sha256 = $ProfileHash
        identity_accepted = $identityAccepted
        correctness_status = [string]$correctness.status
        performance_status = [string]$performance.status
        process_exit_code = $ExitCode
        executable_sha256 = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant()
        executable = [string]$performance.executable
        build_configuration = [string]$performance.build_configuration
        engine_version = [string]$performance.engine_version
        gpu_adapter = [string]$performance.gpu_adapter
        gpu_driver = [string]$performance.gpu_driver
        rhi = [string]$performance.rhi
        render_offscreen = [bool]$performance.render_offscreen
        quality_preset = [string]$performance.quality_preset
        resolution_x = [int]$performance.resolution_x
        resolution_y = [int]$performance.resolution_y
        time_to_ready_seconds = [double]$performance.time_to_ready_seconds
        sample_count = [int]$performance.sample_count
        frame_p95_ms = [double]$performance.frame_p95_ms
        frame_p99_ms = [double]$performance.frame_p99_ms
        frame_max_ms = [double]$performance.frame_max_ms
        game_p95_ms = [double]$performance.game_p95_ms
        render_p95_ms = [double]$performance.render_p95_ms
        gpu_p95_ms = [double]$performance.gpu_p95_ms
        peak_process_physical_bytes = [Int64]$performance.peak_process_physical_bytes
        peak_gpu_local_bytes = [Int64]$performance.peak_gpu_local_bytes
        activation_transitions = [Int64]$performance.activation_transitions
        streaming_failures = [int]$performance.streaming_failures
        package_bytes = $PackageBytes
        cooked_payload_bytes = $CookedPayloadBytes
        correctness_receipt = $CorrectnessPath
        performance_receipt = $PerformancePath
        csv_capture = $CsvPath
        raw_sample_capture = $SamplePath
        routes = @($performance.routes)
        package_deleted = $true
    }
}

$compilerProfilePath = Resolve-TournamentPath `
    'Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/kazan_territory_v1.compile.json'
$realizationPath = Resolve-TournamentPath `
    'Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json'
$presentationPath = Resolve-TournamentPath `
    'Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json'
$authoredPath = Resolve-TournamentPath `
    'Plugins/World/ProjectWorldData/Data/Authored/kazan_slice0_v1.json'
$runtimePaths = @(
    Resolve-TournamentPath 'Plugins/World/ProjectWorldData/Data/Runtime/kazan_territory_128_768_v1.json'
    Resolve-TournamentPath 'Plugins/World/ProjectWorldData/Data/Runtime/kazan_territory_256_768_v1.json'
    Resolve-TournamentPath 'Plugins/World/ProjectWorldData/Data/Runtime/kazan_territory_512_1536_v1.json'
)
$profiles = @($runtimePaths | ForEach-Object { Get-Content -LiteralPath $_ -Raw | ConvertFrom-Json })
$expectedProfiles = @($profiles | ForEach-Object { [string]$_.profile_id })
$realization = Get-Content -LiteralPath $realizationPath -Raw | ConvertFrom-Json
$mapPackage = [string]$realization.map_package
$runId = [Guid]::NewGuid().ToString('N')
$evidenceRoot = Join-Path $projectRoot "Saved\Validation\WorldRealization\runtime-profile-tournament\$runId"
$summaryPath = Join-Path $evidenceRoot 'summary.json'
$transactionParent = Join-Path $projectRoot 'tmp\world\runtime_profile_tournament'
$workRoot = Join-Path $transactionParent $runId
$snapshotRoot = Join-Path $workRoot 'snapshot'
$transientManifestRoot = Join-Path $workRoot 'manifests'
$powerShellExe = (Get-Process -Id $PID).Path
$priorToken = $env:ALIS_WORLD_CONTENT_LOCK_TOKEN
$contentLock = $null
$snapshotRecords = @()
$roots = $null
$summary = $null
$failureMessage = $null
$candidates = [Collections.Generic.List[object]]::new()

New-Item -ItemType Directory -Path $evidenceRoot, $workRoot -Force | Out-Null
try {
    $materializeOutput = @(& python -S $compilerBootstrap materialize --profile $compilerProfilePath)
    Assert-Tournament ($LASTEXITCODE -eq 0 -and $materializeOutput.Count -gt 0) `
        'Canonical authority materialization failed.'
    $materialized = $materializeOutput[-1] | ConvertFrom-Json
    Assert-Tournament ([string]$materialized.status -ceq 'accepted') `
        'Canonical authority materialization was not accepted.'
    $compileResultPath = Resolve-TournamentPath ([string]$materialized.result_path)
    $coverage = Get-Content -LiteralPath `
        (Join-Path (Split-Path -Parent $compileResultPath) 'canonical\coverage.json') `
        -Raw | ConvertFrom-Json
    $edgeCell = Get-Content -LiteralPath `
        (Join-Path (Split-Path -Parent $compileResultPath) 'canonical\cells\cell_x8_y-2.json') `
        -Raw | ConvertFrom-Json
    $edgeX = (([double]$edgeCell.bounds[0] + [double]$edgeCell.bounds[2]) / 2.0 -
        [double]$coverage.engine_georeference_origin[0]) * 100.0
    $edgeY = -((([double]$edgeCell.bounds[1] + [double]$edgeCell.bounds[3]) / 2.0 -
        [double]$coverage.engine_georeference_origin[1]) * 100.0)
    $edgeArgument = '{0},{1},10000' -f `
        $edgeX.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture), `
        $edgeY.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture)

    $roots = Resolve-ProjectWorldDataRoots -ProjectRoot $projectRoot -PluginName 'ProjectWorldData'
    $resolvedLayers = Resolve-ProjectWorldRealizationLayers `
        -RealizationDocument $realization -WorldDataRoots $roots
    $layerPaths = @($resolvedLayers.Definitions.Keys | Sort-Object | ForEach-Object {
        $resolvedLayers.ScopePaths[$_]
    })
    Assert-Tournament ($layerPaths.Count -eq 6) 'The Kazan tournament requires exactly six generated layers.'
    $contentLock = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
    if ([string]::IsNullOrWhiteSpace($priorToken)) {
        $lockPath = Join-Path $projectRoot 'tmp\world\world_realization\content_mutation.lock'
        $env:ALIS_WORLD_CONTENT_LOCK_TOKEN = (Get-Content -LiteralPath $lockPath -Raw).Trim()
    }
    $durableActive = Read-ProjectWorldActiveSet `
        -ManifestRoot $roots.ManifestRoot -ProjectRoot $projectRoot
    Initialize-TournamentAuthority -Active $durableActive -DestinationRoot $transientManifestRoot
    $snapshotRecords = @(New-ProjectWorldGeneratedSnapshot `
        -ContentRoot $roots.ContentRoot -MapPackage $mapPackage `
        -GeneratedPackageRoot $roots.GeneratedPackageRoot `
        -SnapshotRoot $snapshotRoot -AdditionalPaths $layerPaths)

    for ($index = 0; $index -lt $profiles.Count; $index++) {
        $profile = $profiles[$index]
        $profilePath = $runtimePaths[$index]
        $profileHash = (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash.ToLowerInvariant()
        $candidateRoot = Join-Path $evidenceRoot ([string]$profile.profile_id)
        $applyReceipt = Join-Path $candidateRoot 'realization.json'
        $correctnessReceipt = Join-Path $candidateRoot 'product-route.json'
        $performanceReceipt = Join-Path $candidateRoot 'performance.json'
        $csvPath = Join-Path $candidateRoot 'performance.csv'
        $samplePath = Join-Path $candidateRoot 'performance.samples.csv'
        $packageRoot = Join-Path $workRoot "packages\$($profile.profile_id)"
        $candidateRealizationPath = New-TournamentRealizationProfile `
            -RuntimeProfile $profile `
            -OutputPath (Join-Path $workRoot "$($profile.profile_id).realization.json")
        New-Item -ItemType Directory -Path $candidateRoot -Force | Out-Null
        Write-Host "[ProjectWorldRuntimeProfileTournament] Apply $($profile.profile_id)" -ForegroundColor Cyan
        Invoke-TournamentApply -ProfilePath $profilePath `
            -RealizationProfilePath $candidateRealizationPath `
            -ReceiptPath $applyReceipt | Out-Null
        try {
            Invoke-TournamentPackage -OutputRoot $packageRoot -SkipBuild ($index -gt 0)
            $executable = Join-Path $packageRoot 'Windows\Alis\Binaries\Win64\Alis.exe'
            Assert-Tournament (Test-Path -LiteralPath $executable -PathType Leaf) `
                "Packaged Development executable is missing: $($profile.profile_id)"
            $packageBytes = Get-TournamentTreeBytes -Path $packageRoot
            $payloadBytes = [Int64]((Get-ChildItem -LiteralPath $packageRoot -File -Recurse |
                Where-Object { $_.Extension -in @('.pak', '.ucas', '.utoc') } |
                Measure-Object Length -Sum).Sum)
            $operationId = "kazan_runtime_tournament_$($profile.profile_id)_$runId"
            $exitCode = Invoke-TournamentGame `
                -Executable $executable -OperationId $operationId -Profile $profile `
                -ProfileHash $profileHash -CorrectnessPath $correctnessReceipt `
                -PerformancePath $performanceReceipt -CsvPath $csvPath `
                -SamplePath $samplePath
            $candidate = Read-TournamentCandidate `
                -Profile $profile -ProfileHash $profileHash -Executable $executable `
                -CorrectnessPath $correctnessReceipt -PerformancePath $performanceReceipt `
                -CsvPath $csvPath -SamplePath $samplePath -PackageBytes $packageBytes `
                -CookedPayloadBytes $payloadBytes -ExitCode $exitCode
            $candidates.Add($candidate)
        }
        finally {
            Remove-TournamentWorkspace -Path $packageRoot
        }
    }

    $executableHash = Assert-ProjectWorldTournamentIdentity `
        -Candidates @($candidates) -ExpectedProfiles $expectedProfiles
    $winner = Select-ProjectWorldRuntimeProfileWinner -Candidates @($candidates)
    $summary = [ordered]@{
        schema_version = 1
        status = 'accepted'
        selection_order = @('hard_gates', 'frame_p99_ms', 'peak_process_physical_bytes', 'activation_transitions')
        physical_gpu_required = 'NVIDIA GeForce RTX 4070'
        executable_sha256 = $executableHash
        executable_identity_shared = $true
        engine_route = 'launcher_installed'
        performance_build_configuration = 'Development'
        shipping_csv_qualification = 'unsupported_by_launcher_engine'
        realization_sot_field_switched_per_candidate = $true
        map_package = $mapPackage
        canonical_compile_result = $compileResultPath
        winner_profile_id = [string]$winner.profile_id
        candidates = @($candidates)
        scratch_cleaned = $true
        retained_rollback = 'tmp/clean_bootstrap_backup/final_p0'
    }
}
catch {
    $failureMessage = $_.Exception.Message
    $summary = [ordered]@{
        schema_version = 1
        status = 'rejected'
        error = $failureMessage
        candidates = @($candidates)
        scratch_cleaned = $true
        retained_rollback = 'tmp/clean_bootstrap_backup/final_p0'
    }
}
finally {
    if ($snapshotRecords.Count -gt 0 -and $null -ne $roots) {
        Restore-ProjectWorldGeneratedSnapshot `
            -ContentRoot $roots.ContentRoot -MapPackage $mapPackage `
            -GeneratedPackageRoot $roots.GeneratedPackageRoot -Records $snapshotRecords
    }
    Remove-TournamentWorkspace -Path $workRoot
    if ([string]::IsNullOrWhiteSpace($priorToken)) {
        Remove-Item Env:ALIS_WORLD_CONTENT_LOCK_TOKEN -ErrorAction SilentlyContinue
    }
    if ($null -ne $contentLock) { $contentLock.Dispose() }
}

Write-ProjectWorldJson -Path $summaryPath -Document $summary
& $cleanupScript -Apply
if ($null -ne $failureMessage) {
    throw "Runtime-profile tournament rejected. Receipt: $summaryPath. $failureMessage"
}
Write-Host "[ProjectWorldRuntimeProfileTournament] accepted: $summaryPath" -ForegroundColor Green
Write-Host "[ProjectWorldRuntimeProfileTournament] winner: $($summary.winner_profile_id)" -ForegroundColor Green
