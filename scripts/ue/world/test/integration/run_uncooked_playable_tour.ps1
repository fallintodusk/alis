# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#Requires -Version 5.1

[CmdletBinding()]
param(
    [ValidateSet('Kazan', 'Manhattan', 'Both')][string]$City = 'Both',
    [switch]$KnownBadManhattanReturnControl,
    [ValidateRange(60, 900)][int]$TimeoutSeconds = 720
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../..'))
. (Join-Path $repoRoot 'scripts/config/Resolve-UEConfig.ps1')
. (Join-Path $PSScriptRoot 'uncooked_playable_tour_contract.ps1')
. (Join-Path $PSScriptRoot '../performance/project_world_product_route_arguments.ps1')
$config = Resolve-UEConfig -ConfigDir (Join-Path $repoRoot 'scripts/config')
$editor = Join-Path $config.UE_PATH 'Engine/Binaries/Win64/UnrealEditor.exe'
$project = Join-Path $repoRoot 'Alis.uproject'
$root = Join-Path $repoRoot ('tmp/world/playable_tour/uncooked/' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
$sourceTool = Join-Path $repoRoot 'scripts/ue/package/prepare_release.py'
$authority = Join-Path $repoRoot 'Plugins/World/ProjectWorldData'
$sourceRevision = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Unable to identify current revision.' }

function Get-CurrentSourceState {
    $state = @(& python $sourceTool source-state --source-root $repoRoot)
    if ($LASTEXITCODE -ne 0 -or $state.Count -ne 1 -or $state[0] -cnotmatch '^[a-f0-9]{64}$') {
        throw 'Unable to authenticate current source state.'
    }
    return $state[0]
}

function Get-CurrentModuleInventory {
    $rows = [ordered]@{}
    $paths = @()
    $pending = [Collections.Generic.Queue[string]]::new()
    $pending.Enqueue((Join-Path $repoRoot 'Plugins'))
    while ($pending.Count -gt 0) {
        $directory = $pending.Dequeue()
        if (@(Get-ChildItem -LiteralPath $directory -File -Filter '*.uplugin').Count) {
            $binaries = Join-Path $directory 'Binaries/Win64'
            if (Test-Path -LiteralPath $binaries -PathType Container) {
                # Enumerate each plugin explicitly, including existing junctions.
                $paths += @(Get-ChildItem -LiteralPath $binaries -File -Filter '*.dll')
            }
        }
        else {
            foreach ($child in @(Get-ChildItem -LiteralPath $directory -Directory)) {
                $pending.Enqueue($child.FullName)
            }
        }
    }
    $paths += @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Binaries/Win64') `
            -File -Filter 'UnrealEditor-*.dll')
    foreach ($file in @($paths | Sort-Object FullName)) {
        $relative = $file.FullName.Substring($repoRoot.Length + 1).Replace('\', '/')
        $rows[$relative] = Get-ProjectWorldPerformanceFileHash -Path $file.FullName
    }
    if ($rows.Count -eq 0) { throw 'Current Editor module inventory is empty.' }
    return $rows
}

$rows = [Collections.Generic.List[object]]::new()
$status = 'rejected'
$failure = $null
$modulesBefore = $null
$sourceState = Get-CurrentSourceState
$authorityBefore = Get-ProjectWorldPackagePayloadDigest -Path $authority
$editorHash = Get-ProjectWorldPerformanceFileHash -Path $editor
try {
    & (Join-Path $repoRoot 'scripts/ue/build/build.bat') AlisEditor Win64 Development `
        > (Join-Path $root 'build.log') 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Editor build failed. See $root/build.log" }
    $modulesBefore = Get-CurrentModuleInventory
    foreach ($test in @('CenterReturnPolicy', 'OverhangRecovery')) {
        $name = 'Project.World.PlayableTour.' + $test
        & powershell -NoProfile -ExecutionPolicy Bypass -File `
            (Join-Path $repoRoot 'scripts/ue/test/unit/run_cpp_tests_safe.ps1') `
            -TestFilter $name -ExpectedTestNames $name -NoRHI -Mode Gate -TimeoutSeconds 180 `
            -LogDirectory (Join-Path $root ('native/' + $test)) `
            > (Join-Path $root ($test + '.log')) 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Native playable policy failed: $name. See $root/$test.log" }
    }
    $cities = if ($City -ceq 'Both') { @('Kazan', 'Manhattan') } else { @($City) }
    foreach ($name in $cities) {
        $runner = if ($name -ceq 'Kazan') { 'run_kazan_playable_tour.ps1' } else {
            'run_manhattan_showcase_prototype.ps1'
        }
        $operation = & (Join-Path $PSScriptRoot "../performance/$runner") -DescribeOperation
        $operation | Add-Member NoteProperty RuntimeHash `
            (Get-ProjectWorldPerformanceFileHash -Path $operation.RuntimePath)
        $directory = Join-Path $root $name
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        $operationId = 'uncooked_' + [Guid]::NewGuid().ToString('N')
        $paths = @{}
        foreach ($entry in @(@('CorrectnessPath', 'product-route.json'), @('LogPath', 'game.log'),
                @('PerformancePath', 'traversal.json'), @('CsvPath', 'diagnostic.csv'),
                @('SamplePath', 'samples.csv'), @('ScreenshotPath', 'slide.png'))) {
            $paths[$entry[0]] = Join-Path $directory $entry[1]
        }
        $parameters = @{
            Experience = $operation.Experience; Map = $operation.Map
            Runtime = $operation.Runtime; RuntimeHash = $operation.RuntimeHash
            Edge = $operation.Edge; OperationId = $operationId
            SkipInteraction = -not $operation.InteractionRequired
            PreciseCenterReturn = $operation.PreciseCenterReturn
        } + $paths
        if ($KnownBadManhattanReturnControl -and $name -ceq 'Manhattan') {
            $parameters.PreciseCenterReturn = $false
            $operation.Radius = 2500.0
        }
        $arguments = @(('"' + $project + '"'), '/MainMenuWorld/Maps/MainMenu_Persistent',
            '-ExecCmds="Automation RunTests Project.World.ProductRoute.UncookedSession"') +
            @(Get-ProjectWorldProductRouteArguments @parameters)
        $arguments = @($arguments | ForEach-Object {
                if ($_.Contains(' ') -and -not $_.Contains('"')) { '"' + $_ + '"' } else { $_ }
            })
        $process = $null
        $loadedModules = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        try {
            $process = Start-Process -FilePath $editor -ArgumentList $arguments `
                -WorkingDirectory $repoRoot -WindowStyle Hidden -PassThru
            $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            while (-not $process.HasExited -and [DateTime]::UtcNow -lt $deadline) {
                foreach ($module in @($process.Modules)) {
                    if ($module.FileName.StartsWith($repoRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
                        [void]$loadedModules.Add($module.FileName.Substring($repoRoot.Length + 1).Replace('\', '/'))
                    }
                }
                Start-Sleep -Milliseconds 1000
                $process.Refresh()
            }
            if (-not $process.HasExited) { throw "Uncooked $name exceeded ${TimeoutSeconds}s." }
            $exitCode = $process.ExitCode
        }
        finally {
            if ($null -ne $process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
        }
        $log = Get-Content -LiteralPath $paths.LogPath -Raw
        if (-not $log.Contains('LogExit: Exiting.') -or -not $log.Contains('Log file closed') -or
            $log -match 'Assertion failed:|Fatal error:|Ensure condition failed:') {
            throw "Uncooked $name did not terminate normally. See $($paths.LogPath)"
        }
        if ($loadedModules -notcontains 'Plugins/World/ProjectWorld/Binaries/Win64/UnrealEditor-ProjectWorld.dll') {
            throw 'The launched process did not authenticate the current World runtime module.'
        }
        foreach ($module in $loadedModules) {
            if (-not $modulesBefore.Contains($module)) { throw "Unexpected loaded module: $module" }
        }
        $correctness = Get-Content -LiteralPath $paths.CorrectnessPath -Raw | ConvertFrom-Json
        $receipt = Get-Content -LiteralPath $paths.PerformancePath -Raw | ConvertFrom-Json
        $row = [ordered]@{
            city = $name; operation_id = $operationId; map = $operation.Map
            runtime = $operation.Runtime; runtime_sha256 = $operation.RuntimeHash
            expected_radius_cm = $operation.Radius; actual_radius_cm = $receipt.final_center_arrival_radius_cm
            native_status = $receipt.status; native_exit_code = $exitCode
            slide_cm = $receipt.slide_displacement_cm; loaded_modules = @($loadedModules | Sort-Object)
            paths = $paths
        }
        $rows.Add($row)
        Assert-ProjectWorldUncookedGameplayReceipt -Receipt $receipt -Correctness $correctness `
            -Operation $operation -OperationId $operationId -CorrectnessPath $paths.CorrectnessPath `
            -ProcessExitCode $exitCode
        if ([string]$receipt.build_configuration -cne 'Development' -or
            -not ([IO.Path]::GetFullPath([string]$receipt.executable)).Equals(
                [IO.Path]::GetFullPath($editor), [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Uncooked executable or build configuration mismatch.'
        }
        foreach ($field in @('raw_sample_capture', 'csv_capture', 'playable_tour_screenshot')) {
            $expected = switch ($field) {
                'raw_sample_capture' { $paths.SamplePath }
                'csv_capture' { $paths.CsvPath }
                'playable_tour_screenshot' { $paths.ScreenshotPath }
            }
            if (-not ([IO.Path]::GetFullPath([string]$receipt.$field)).Equals(
                    $expected, [StringComparison]::OrdinalIgnoreCase)) { throw "Wrong artifact binding: $field" }
        }
        $row.artifact_sha256 = [ordered]@{}
        foreach ($path in @($paths.Values) + @([string]$correctness.screenshot)) {
            $row.artifact_sha256[$path] = Get-ProjectWorldPerformanceFileHash -Path $path
        }
        $row.status = 'accepted'
        Write-Host "Uncooked $name gameplay accepted: radius=$($row.actual_radius_cm) slide=$($row.slide_cm)cm"
    }
    $status = if ($KnownBadManhattanReturnControl) { 'diagnostic' } else { 'accepted' }
}
catch {
    $failure = $_.Exception.Message
    throw
}
finally {
    $authorityAfter = Get-ProjectWorldPackagePayloadDigest -Path $authority
    $sourceAfter = Get-CurrentSourceState
    $modulesAfter = Get-CurrentModuleInventory
    $unchanged = $authorityBefore -ceq $authorityAfter -and $sourceState -ceq $sourceAfter -and
        $editorHash -ceq (Get-ProjectWorldPerformanceFileHash -Path $editor) -and
        $null -ne $modulesBefore -and
        (($modulesBefore | ConvertTo-Json -Compress) -ceq ($modulesAfter | ConvertTo-Json -Compress))
    if (-not $unchanged) { $status = 'rejected' }
    $output = [ordered]@{
        schema = 'project-world-uncooked-gameplay-admission:v1'; status = $status
        acceptance_scope = 'uncooked_gameplay'; performance_certified = $false
        execution_envelope = 'editor_pie'
        source_revision = $sourceRevision; source_state_sha256 = $sourceState
        source_state_after_sha256 = $sourceAfter; editor = $editor; editor_sha256 = $editorHash
        module_sha256 = $modulesBefore; authority_before_sha256 = $authorityBefore
        authority_after_sha256 = $authorityAfter; unchanged = $unchanged
        known_bad_control = [bool]$KnownBadManhattanReturnControl; cities = @($rows)
        failure = $failure; modules_sampled = ($null -ne $modulesBefore)
    }
    $receiptPath = Join-Path $root 'admission.json'
    [IO.File]::WriteAllText($receiptPath, ($output | ConvertTo-Json -Depth 15) + "`n",
        [Text.UTF8Encoding]::new($false))
    Write-Host "Uncooked admission receipt: $receiptPath"
    if (-not $unchanged -and $null -eq $failure) {
        throw 'Source, runtime modules or generated authority changed during the probe.'
    }
}
