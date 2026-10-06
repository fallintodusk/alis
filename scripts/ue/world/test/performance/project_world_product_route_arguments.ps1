# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#Requires -Version 5.1

function Get-ProjectWorldProductRouteArguments {
    param(
        [Parameter(Mandatory)][string]$Experience,
        [Parameter(Mandatory)][string]$Map,
        [Parameter(Mandatory)][string]$Runtime,
        [Parameter(Mandatory)][string]$RuntimeHash,
        [Parameter(Mandatory)][string]$Edge,
        [Parameter(Mandatory)][string]$OperationId,
        [Parameter(Mandatory)][string]$CorrectnessPath,
        [Parameter(Mandatory)][string]$LogPath,
        [string]$PerformancePath, [string]$CsvPath, [string]$SamplePath,
        [string]$ScreenshotPath,
        [switch]$SkipInteraction, [switch]$PreciseCenterReturn
    )
    $arguments = @(
        "-ProjectMenuPlayAutoExperience=$Experience",
        '-ProjectMenuPlayAutoMode=SinglePlayer',
        '-ProjectWorldProductRouteGate', '-ProjectWorldProductRouteRestorePreviewFlight',
        "-ProjectWorldProductOperation=$OperationId", "-ProjectWorldProductResult=$CorrectnessPath",
        "-ProjectWorldProductMap=$Map", "-ProjectWorldProductRuntime=$Runtime",
        "-ProjectWorldProductRuntimeHash=$RuntimeHash", '-ProjectWorldProductMachine=rtx4070_primary',
        "-ProjectWorldProductEdge=$Edge", '-ResX=2560', '-ResY=1440', '-Windowed', '-ForceRes',
        '-RenderOffScreen', '-novsync', '-unattended', '-nosplash', '-NoMessaging',
        "-abslog=$LogPath"
    )
    if ($SkipInteraction) { $arguments += '-ProjectWorldProductRouteSkipInteraction' }
    if ($PerformancePath) {
        if (-not $CsvPath -or -not $SamplePath -or -not $ScreenshotPath) {
            throw 'Playable traversal requires CSV, raw samples and screenshot paths.'
        }
        $arguments += @('-ProjectWorldProductPerformanceGate', '-ProjectWorldPlayableTour',
            "-ProjectWorldPerformanceResult=$PerformancePath",
            "-ProjectWorldPerformanceCorrectness=$CorrectnessPath",
            "-ProjectWorldPerformanceCsv=$CsvPath", "-ProjectWorldPerformanceSamples=$SamplePath",
            "-ProjectWorldPerformanceScreenshot=$ScreenshotPath")
        if ($PreciseCenterReturn) { $arguments += '-ProjectWorldPlayableTourPreciseCenterReturn' }
    }
    elseif ($PreciseCenterReturn) { throw 'Precise return requires playable traversal.' }
    return $arguments
}

function Get-ProjectWorldCanonicalProductEdge {
    param(
        [Parameter(Mandatory)][string]$ProjectRoot,
        [Parameter(Mandatory)][string]$CompilerProfile,
        [Parameter(Mandatory)][string]$CellFile,
        [Parameter(Mandatory)][double]$HeightCentimeters
    )
    $resolver = Join-Path $ProjectRoot 'tools/World/ExecutionEnvironment/resolve_python_host.ps1'
    $python = @(& $resolver)
    if ($LASTEXITCODE -ne 0 -or $python.Count -ne 1) { throw 'World Python resolution failed.' }
    $bootstrap = Join-Path $ProjectRoot 'tools/World/CanonicalCompilation/bootstrap.py'
    $output = @(& $python[0] -S $bootstrap materialize --profile $CompilerProfile)
    if ($LASTEXITCODE -ne 0 -or $output.Count -eq 0) { throw 'Canonical materialization failed.' }
    $materialized = $output[-1] | ConvertFrom-Json
    if ($materialized.status -cne 'accepted') { throw 'Canonical authority was not accepted.' }
    $result = [string]$materialized.result_path
    if (-not [IO.Path]::IsPathRooted($result)) { $result = Join-Path $ProjectRoot $result }
    $root = Split-Path -Parent $result
    $cell = Get-Content -LiteralPath (Join-Path $root "canonical/cells/$CellFile") -Raw | ConvertFrom-Json
    $coverage = Get-Content -LiteralPath (Join-Path $root 'canonical/coverage.json') -Raw | ConvertFrom-Json
    $x = (([double]$cell.bounds[0] + [double]$cell.bounds[2]) / 2.0 -
        [double]$coverage.engine_georeference_origin[0]) * 100.0
    $y = -((([double]$cell.bounds[1] + [double]$cell.bounds[3]) / 2.0 -
        [double]$coverage.engine_georeference_origin[1]) * 100.0)
    return '{0},{1},{2}' -f $x.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture),
        $y.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture),
        $HeightCentimeters.ToString('0.###', [Globalization.CultureInfo]::InvariantCulture)
}
