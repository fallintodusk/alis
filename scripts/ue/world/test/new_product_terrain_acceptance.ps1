# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$CompileResultPath,
    [Parameter(Mandatory = $true)][string]$CompilerProfilePath,
    [Parameter(Mandatory = $true)][string]$RuntimeProfilePath,
    [Parameter(Mandatory = $true)][string]$MapPackage,
    [Parameter(Mandatory = $true)]
    [ValidateSet('project_mesh_terrain:v1')]
    [string]$TerrainGeneratorId,
    [Parameter(Mandatory = $true)][string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$compileResultFile = [IO.Path]::GetFullPath($CompileResultPath)
$compilerProfileFile = [IO.Path]::GetFullPath($CompilerProfilePath)
$runtimeProfileFile = [IO.Path]::GetFullPath($RuntimeProfilePath)
$outputFile = [IO.Path]::GetFullPath($OutputPath)
$allowedOutputRoots = @(
    [IO.Path]::GetFullPath((Join-Path $projectRoot 'tmp\world')).TrimEnd('\', '/'),
    [IO.Path]::GetFullPath((Join-Path $projectRoot 'Saved\Validation\WorldRealization')).TrimEnd('\', '/')
)

function Assert-Acceptance([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Read-Json([string]$Path) {
    Assert-Acceptance (Test-Path -LiteralPath $Path -PathType Leaf) "Required input is missing: $Path"
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Test-UnderRoot([string]$Path, [string]$Root) {
    return $Path.StartsWith(
        $Root + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)
}

Assert-Acceptance (@($allowedOutputRoots | Where-Object { Test-UnderRoot $outputFile $_ }).Count -eq 1) `
    'Terrain acceptance output must remain under tmp/world or Saved/Validation/WorldRealization.'
Assert-Acceptance ($MapPackage.StartsWith('/ProjectWorldData/Generated/')) `
    'Terrain acceptance supports only generated ProjectWorldData product maps.'

$compileResult = Read-Json $compileResultFile
$compilerProfile = Read-Json $compilerProfileFile
$runtimeProfile = Read-Json $runtimeProfileFile
Assert-Acceptance ([string]$compileResult.status -ceq 'accepted') `
    'Terrain acceptance requires an accepted compile result.'
Assert-Acceptance ([string]$compileResult.profile_id -ceq [string]$compilerProfile.profile_id) `
    'Compile result and compiler profile identities differ.'
Assert-Acceptance ([string]$runtimeProfile.profile_kind -ceq 'territory_product') `
    'Terrain acceptance requires a territory_product runtime profile.'

$engineOrigin = @($compilerProfile.engine_georeference_origin | ForEach-Object { [double]$_ })
$verticalOrigin = [double]$compilerProfile.grid.vertical_origin_m
$runtimeHash = Get-Sha256 $runtimeProfileFile
$gridId = [string]$runtimeProfile.grid_id
$loadingRadiusCm = [double]$runtimeProfile.runtime_partition.loading_range_m * 100.0
Assert-Acceptance ($engineOrigin.Count -eq 2 -and $loadingRadiusCm -gt 0.0) `
    'Compiler origin or runtime loading range is invalid.'

$compileRoot = [IO.Path]::GetDirectoryName($compileResultFile).TrimEnd('\', '/')
$compilePrefix = $compileRoot + [IO.Path]::DirectorySeparatorChar
$cells = [Collections.Generic.List[object]]::new()
foreach ($output in @($compileResult.outputs | Where-Object {
            [string]$_.path -match '^canonical/terrain/[^/]+\.json$'
        })) {
    $relative = ([string]$output.path).Replace('/', [IO.Path]::DirectorySeparatorChar)
    $path = [IO.Path]::GetFullPath((Join-Path $compileRoot $relative))
    Assert-Acceptance ($path.StartsWith($compilePrefix, [StringComparison]::OrdinalIgnoreCase)) `
        "Canonical terrain path escaped its compile root: $relative"
    Assert-Acceptance ((Get-Sha256 $path) -ceq ([string]$output.sha256).ToLowerInvariant()) `
        "Canonical terrain hash failed: $relative"
    $cell = Read-Json $path
    $bounds = @($cell.bounds | ForEach-Object { [double]$_ })
    $spacing = @($cell.sample_spacing | ForEach-Object { [double]$_ })
    $rows = @($cell.core_samples)
    Assert-Acceptance ([string]$cell.grid_id -ceq $gridId -and $bounds.Count -eq 4 -and
        $spacing.Count -eq 2 -and $rows.Count -ge 2) "Canonical terrain cell is invalid: $relative"
    $columns = @($rows[0]).Count
    Assert-Acceptance ($columns -ge 2) "Canonical terrain cell has too few columns: $relative"
    $hydroRows = @($cell.surface_semantics.core_samples.hydro_transition)
    Assert-Acceptance ($hydroRows.Count -eq $rows.Count) `
        "Canonical terrain cell has no aligned hydro-transition channel: $relative"
    $cells.Add([pscustomobject]@{
            id = [string]$cell.cell_id
            bounds = $bounds
            spacing = $spacing
            rows = $rows
            hydro = $hydroRows
            columns = $columns
            source_accuracy_m = [double]$cell.vertical_provenance.source_accuracy_m
            quantization_m = [double]$cell.height_quantization
            sha256 = ([string]$output.sha256).ToLowerInvariant()
        })
}
Assert-Acceptance ($cells.Count -gt 0) 'Compile result has no authenticated canonical terrain cells.'

function New-Sample([object]$Cell, [int]$Row, [int]$Column) {
    $easting = [double]$Cell.bounds[0] + $Column * [double]$Cell.spacing[0]
    $northing = [double]$Cell.bounds[3] - $Row * [double]$Cell.spacing[1]
    $height = [double]$Cell.rows[$Row][$Column]
    return [pscustomobject]@{
        cell_id = [string]$Cell.id
        row = $Row
        column = $Column
        easting = $easting
        northing = $northing
        height = $height
        x = ($easting - $engineOrigin[0]) * 100.0
        y = -($northing - $engineOrigin[1]) * 100.0
        z = ($height - $verticalOrigin) * 100.0
        hydro = [double]$Cell.hydro[$Row][$Column]
        tolerance_cm = [Math]::Max([double]$Cell.quantization_m, 1.0 / 128.0) * 100.0
    }
}

function Get-CellCenter([object]$Cell) {
    return [pscustomobject]@{
        cell = $Cell
        x = (([double]$Cell.bounds[0] + [double]$Cell.bounds[2]) * 0.5 - $engineOrigin[0]) * 100.0
        y = -((([double]$Cell.bounds[1] + [double]$Cell.bounds[3]) * 0.5) - $engineOrigin[1]) * 100.0
    }
}

$centers = @($cells | ForEach-Object { Get-CellCenter $_ })
$edgeCenter = $centers | Sort-Object @{ Expression = {
            $_.x * $_.x + $_.y * $_.y
        }; Descending = $true } | Select-Object -First 1
$edgeCell = $edgeCenter.cell
$edgeX = [double]$edgeCenter.x
$edgeY = [double]$edgeCenter.y
Assert-Acceptance ([Math]::Sqrt($edgeX * $edgeX + $edgeY * $edgeY) -gt 2.0 * $loadingRadiusCm) `
    'Territory cannot provide non-overlapping center and edge streaming domains.'

$samples = [Collections.Generic.List[object]]::new()
foreach ($cell in $cells) {
    for ($row = 0; $row -lt $cell.rows.Count; ++$row) {
        Assert-Acceptance (@($cell.rows[$row]).Count -eq $cell.columns -and
            @($cell.hydro[$row]).Count -eq $cell.columns) "Canonical sample rows are ragged: $($cell.id)"
        for ($column = 0; $column -lt $cell.columns; ++$column) {
            $sample = New-Sample $cell $row $column
            $centerDistance = [Math]::Sqrt($sample.x * $sample.x + $sample.y * $sample.y)
            $edgeDistance = [Math]::Sqrt(
                [Math]::Pow($sample.x - $edgeX, 2.0) + [Math]::Pow($sample.y - $edgeY, 2.0))
            if ($centerDistance -le $loadingRadiusCm * 0.8 -or $edgeDistance -le $loadingRadiusCm * 0.8) {
                Add-Member -InputObject $sample -NotePropertyName stage -NotePropertyValue `
                    $(if ($centerDistance -le $edgeDistance) { 'center' } else { 'edge' })
                $samples.Add($sample)
            }
        }
    }
}
Assert-Acceptance ($samples.Count -gt 0) 'No canonical samples fall within the two runtime proof domains.'

$centerSample = $samples | Where-Object stage -ceq 'center' | Sort-Object @{ Expression = {
            $_.x * $_.x + $_.y * $_.y
        } } | Select-Object -First 1
$perimeterSample = @(
    New-Sample $edgeCell 0 0
    New-Sample $edgeCell 0 ($edgeCell.columns - 1)
    New-Sample $edgeCell ($edgeCell.rows.Count - 1) 0
    New-Sample $edgeCell ($edgeCell.rows.Count - 1) ($edgeCell.columns - 1)
) | Sort-Object @{ Expression = { $_.x * $_.x + $_.y * $_.y }; Descending = $true } | Select-Object -First 1
Add-Member -InputObject $perimeterSample -NotePropertyName stage -NotePropertyValue 'edge'

$cellById = @{}
foreach ($cell in $cells) { $cellById[$cell.id] = $cell }
$seamCandidates = [Collections.Generic.List[object]]::new()
foreach ($left in $cells) {
    foreach ($right in $cells) {
        if ($left.id -eq $right.id) { continue }
        $sameNorthSouth = [Math]::Abs([double]$left.bounds[1] - [double]$right.bounds[1]) -lt 0.001 -and
            [Math]::Abs([double]$left.bounds[3] - [double]$right.bounds[3]) -lt 0.001
        if ($sameNorthSouth -and [Math]::Abs([double]$left.bounds[2] - [double]$right.bounds[0]) -lt 0.001) {
            $row = [Math]::Round(($left.bounds[3] - $engineOrigin[1]) / $left.spacing[1])
            $row = [Math]::Max(1, [Math]::Min($left.rows.Count - 2, $row))
            $start = New-Sample $left $row ($left.columns - 2)
            $end = New-Sample $right $row 1
            $centerDistance = [Math]::Sqrt($start.x * $start.x + $start.y * $start.y)
            $edgeDistance = [Math]::Sqrt(
                [Math]::Pow($start.x - $edgeX, 2.0) + [Math]::Pow($start.y - $edgeY, 2.0))
            $slope = [Math]::Abs($start.height - $end.height)
            $seamCandidates.Add([pscustomobject]@{
                    start = $start; end = $end
                    distance = [Math]::Min($centerDistance, $edgeDistance)
                    stage = $(if ($centerDistance -le $edgeDistance) { 'center' } else { 'edge' })
                    slope = $slope
                    boundary = "easting:$($left.bounds[2])"
                })
        }
    }
}
$seam = $seamCandidates | Where-Object {
    $_.distance -le $loadingRadiusCm * 0.8 -and $_.slope -le 3.0
} | Sort-Object distance, slope, boundary | Select-Object -First 1
Assert-Acceptance ($null -ne $seam) 'No bounded, low-slope canonical seam is available for navigation proof.'
Add-Member -InputObject $seam.start -NotePropertyName stage -NotePropertyValue $seam.stage

$selectedKeys = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
function Get-SampleKey([object]$Sample) { return '{0:F3}|{1:F3}' -f $Sample.easting, $Sample.northing }
foreach ($sample in @($centerSample, $perimeterSample, $seam.start)) {
    [void]$selectedKeys.Add((Get-SampleKey $sample))
}
function Select-Distinct([object[]]$Candidates) {
    foreach ($candidate in $Candidates) {
        if ($selectedKeys.Add((Get-SampleKey $candidate))) { return $candidate }
    }
    return $null
}
$highSample = Select-Distinct @($samples | Sort-Object height -Descending)
$lowSample = Select-Distinct @($samples | Sort-Object height)
$hydroSample = Select-Distinct @($samples | Where-Object { $_.hydro -gt 0.0 -and $_.hydro -lt 1.0 } |
        Sort-Object @{ Expression = { [Math]::Abs($_.hydro - 0.5) } }, cell_id, row, column)
Assert-Acceptance ($null -ne $highSample -and $null -ne $lowSample -and $null -ne $hydroSample) `
    'Canonical proof domains do not contain distinct high, low, and hydro-transition samples.'

function New-Probe([string]$Id, [string]$Kind, [object]$Sample) {
    return [ordered]@{
        id = $Id
        kind = $Kind
        stage = [string]$Sample.stage
        cell_id = [string]$Sample.cell_id
        canonical_m = @([double]$Sample.easting, [double]$Sample.northing, [double]$Sample.height)
        unreal_cm = @([double]$Sample.x, [double]$Sample.y, [double]$Sample.z)
        tolerance_cm = [double]$Sample.tolerance_cm
    }
}

$contract = [ordered]@{
    schema = 'project-world-product-terrain-acceptance:v1'
    map_package = $MapPackage
    terrain_generator_id = $TerrainGeneratorId
    compile_result_sha256 = Get-Sha256 $compileResultFile
    compiler_profile_sha256 = Get-Sha256 $compilerProfileFile
    runtime_profile_sha256 = $runtimeHash
    grid_id = $gridId
    streaming_edge_unreal_cm = @(
        [double]$edgeX,
        [double]$edgeY,
        ([double]$perimeterSample.z + [double]$runtimeProfile.product_spawn.height_above_terrain_m * 100.0))
    navigation = [ordered]@{
        start_unreal_cm = @([double]$seam.start.x, [double]$seam.start.y, [double]$seam.start.z)
        end_unreal_cm = @([double]$seam.end.x, [double]$seam.end.y, [double]$seam.end.z)
        start_cell_id = [string]$seam.start.cell_id
        end_cell_id = [string]$seam.end.cell_id
        crossed_boundary = [string]$seam.boundary
        generation_radius_cm = 30000.0
        removal_radius_cm = 40000.0
    }
    height_probes = @(
        New-Probe 'center' 'center' $centerSample
        New-Probe 'perimeter' 'perimeter' $perimeterSample
        New-Probe 'cell_boundary' 'cell_boundary' $seam.start
        New-Probe 'representative_high' 'high' $highSample
        New-Probe 'representative_low' 'low' $lowSample
        New-Probe 'hydro_transition' 'hydro_transition' $hydroSample
    )
}
New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($outputFile)) -Force | Out-Null
[IO.File]::WriteAllText(
    $outputFile,
    ($contract | ConvertTo-Json -Depth 10) + "`n",
    [Text.UTF8Encoding]::new($false))
[pscustomobject][ordered]@{
    path = $outputFile
    sha256 = Get-Sha256 $outputFile
    height_probe_count = $contract.height_probes.Count
    center_cell = $centerSample.cell_id
    edge_cell = $edgeCell.id
    navigation_boundary = $seam.boundary
} | ConvertTo-Json -Depth 4
