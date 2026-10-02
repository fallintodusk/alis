# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,

    [Parameter(Mandatory = $true)]
    [string]$ResultPath,

    [Parameter(Mandatory = $true)]
    [string]$LogPath,

    [string]$MapPackage =
        '/ProjectWorldTestData/Generated/MeshTerrainStreamingFar/L_ProjectWorldMeshTerrainStreamingFar',

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$SourceIdentity,

    [Parameter(Mandatory = $true)]
    [string]$CompileResultPath,

    [Parameter(Mandatory = $true)]
    [string]$CompilerProfilePath,

    [Parameter(Mandatory = $true)]
    [string]$RuntimeProfilePath,

    [string]$ProbeDerivationPath,

    [ValidateRange(10, 180)]
    [int]$TimeoutSeconds = 150,

    [switch]$ProbeDerivationOnly
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot '..\..\..\..\..'))
$tmpWorldRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $projectRoot 'tmp\world')).TrimEnd('\', '/')
$evidenceRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $projectRoot 'Saved\Validation\WorldRealization')).TrimEnd('\', '/')
$packagePath = [System.IO.Path]::GetFullPath($PackageRoot).TrimEnd('\', '/')
$result = [System.IO.Path]::GetFullPath($ResultPath)
$log = [System.IO.Path]::GetFullPath($LogPath)
$compileResultPathFull = [System.IO.Path]::GetFullPath($CompileResultPath)
$compilerProfilePathFull = [System.IO.Path]::GetFullPath($CompilerProfilePath)
$runtimeProfilePathFull = [System.IO.Path]::GetFullPath($RuntimeProfilePath)
$probeDerivation = if ([string]::IsNullOrWhiteSpace($ProbeDerivationPath)) {
    $result + '.probe-derivation.json'
}
else {
    [System.IO.Path]::GetFullPath($ProbeDerivationPath)
}

function Assert-UnderRoot {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Label
    )
    $prefix = $Root.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if (-not $Path.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label must remain under $Root."
    }
}

Assert-UnderRoot -Path $packagePath -Root $tmpWorldRoot -Label 'Package root'
Assert-UnderRoot -Path $result -Root $evidenceRoot -Label 'Result path'
Assert-UnderRoot -Path $probeDerivation `
    -Root $(if ($ProbeDerivationOnly) { $tmpWorldRoot } else { $evidenceRoot }) `
    -Label 'Probe derivation path'
Assert-UnderRoot -Path $log -Root $tmpWorldRoot -Label 'Log path'
$supportedMapRoots = @('/ProjectWorldTestData/Generated/', '/ProjectWorldData/Generated/')
if (@($supportedMapRoots | Where-Object { $MapPackage.StartsWith($_) }).Count -ne 1) {
    throw 'Runtime gate map must be a ProjectWorld generated map.'
}
foreach ($inputPath in @($compileResultPathFull, $compilerProfilePathFull, $runtimeProfilePathFull)) {
    if (-not (Test-Path -LiteralPath $inputPath -PathType Leaf)) {
        throw "Runtime gate derivation input does not exist: $inputPath"
    }
}

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Convert-CanonicalToUnrealCentimeters {
    param(
        [Parameter(Mandatory = $true)][double]$Easting,
        [Parameter(Mandatory = $true)][double]$Northing,
        [Parameter(Mandatory = $true)][double]$Height,
        [Parameter(Mandatory = $true)][double[]]$EngineOrigin,
        [Parameter(Mandatory = $true)][double]$VerticalOrigin
    )
    $x = ($Easting - $EngineOrigin[0]) * 100.0
    $y = -($Northing - $EngineOrigin[1]) * 100.0
    $z = ($Height - $VerticalOrigin) * 100.0
    return [double[]]@($x, $y, $z)
}

$compileResult = Get-Content -LiteralPath $compileResultPathFull -Raw | ConvertFrom-Json
$compilerProfile = Get-Content -LiteralPath $compilerProfilePathFull -Raw | ConvertFrom-Json
$runtimeProfile = Get-Content -LiteralPath $runtimeProfilePathFull -Raw | ConvertFrom-Json
if ([string]$compileResult.status -cne 'accepted' -or
    [string]$compileResult.profile_id -cne [string]$compilerProfile.profile_id) {
    throw 'Runtime probe derivation requires an accepted compile result for the supplied compiler profile.'
}
$engineOrigin = @($compilerProfile.engine_georeference_origin | ForEach-Object { [double]$_ })
$verticalOrigin = [double]$compilerProfile.grid.vertical_origin_m
$runtimeGridId = [string]$runtimeProfile.grid_id
$loadingRangeMeters = [double]$runtimeProfile.runtime_partition.loading_range_m
$spawnHeightMeters = [double]$runtimeProfile.product_spawn.height_above_terrain_m
if ($engineOrigin.Count -ne 2 -or
    [string]$runtimeProfile.product_spawn.anchor -cne 'engine_georeference_origin' -or
    $loadingRangeMeters -le 0.0 -or $spawnHeightMeters -le 0.0) {
    throw 'Runtime probe derivation requires the accepted engine-origin spawn and a positive loading range.'
}

$compileRoot = [System.IO.Path]::GetDirectoryName($compileResultPathFull).TrimEnd('\', '/')
$compileRootPrefix = $compileRoot + [System.IO.Path]::DirectorySeparatorChar
$terrainCells = [Collections.Generic.List[object]]::new()
foreach ($output in @($compileResult.outputs | Where-Object {
            [string]$_.path -match '^canonical/terrain/[^/]+\.json$'
        })) {
    $relativePath = ([string]$output.path).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
    $cellPath = [System.IO.Path]::GetFullPath((Join-Path $compileRoot $relativePath))
    if (-not $cellPath.StartsWith($compileRootPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $cellPath -PathType Leaf) -or
        (Get-Sha256 -Path $cellPath) -cne ([string]$output.sha256).ToLowerInvariant()) {
        throw "Canonical terrain output is missing, escapes its compile root, or failed hash authentication: $relativePath"
    }
    $cell = Get-Content -LiteralPath $cellPath -Raw | ConvertFrom-Json
    $bounds = @($cell.bounds | ForEach-Object { [double]$_ })
    if ($bounds.Count -ne 4 -or [string]$cell.grid_id -cne $runtimeGridId) {
        throw "Canonical terrain cell does not match the accepted runtime grid: $relativePath"
    }
    $heightValues = [Collections.Generic.List[double]]::new()
    foreach ($row in @($cell.core_samples)) {
        foreach ($height in @($row)) {
            $heightValues.Add([double]$height)
        }
    }
    if ($heightValues.Count -eq 0) {
        throw "Canonical terrain cell has no core samples: $relativePath"
    }
    $canonicalCenterEasting = ($bounds[0] + $bounds[2]) * 0.5
    $canonicalCenterNorthing = ($bounds[1] + $bounds[3]) * 0.5
    $canonicalCenter = @($canonicalCenterEasting, $canonicalCenterNorthing)
    $unrealCenter = Convert-CanonicalToUnrealCentimeters `
        -Easting $canonicalCenter[0] -Northing $canonicalCenter[1] -Height $verticalOrigin `
        -EngineOrigin $engineOrigin -VerticalOrigin $verticalOrigin
    $terrainCells.Add([pscustomobject][ordered]@{
            cell_id = [string]$cell.cell_id
            bounds = $bounds
            canonical_center_m = $canonicalCenter
            unreal_center_cm = $unrealCenter
            minimum_height_m = [double](($heightValues | Measure-Object -Minimum).Minimum)
            maximum_height_m = [double](($heightValues | Measure-Object -Maximum).Maximum)
            document_sha256 = ([string]$output.sha256).ToLowerInvariant()
        })
}
if ($terrainCells.Count -eq 0) {
    throw 'Accepted compile result contains no authenticated canonical terrain cells.'
}

$centerMatches = @($terrainCells | Where-Object {
        $engineOrigin[0] -ge $_.bounds[0] -and $engineOrigin[0] -le $_.bounds[2] -and
        $engineOrigin[1] -ge $_.bounds[1] -and $engineOrigin[1] -le $_.bounds[3]
    })
if ($centerMatches.Count -ne 1) {
    throw 'Engine georeference origin must intersect exactly one canonical terrain cell.'
}
$centerCell = $centerMatches[0]
$edgeCell = $terrainCells | Sort-Object -Property @{ Expression = {
            [Math]::Pow([double]$_.unreal_center_cm[0], 2.0) +
            [Math]::Pow([double]$_.unreal_center_cm[1], 2.0)
        }; Descending = $true } | Select-Object -First 1
$centerX = 0.0
$centerY = 0.0
$edgeX = [double]$edgeCell.unreal_center_cm[0]
$edgeY = [double]$edgeCell.unreal_center_cm[1]
$radiusCm = $loadingRangeMeters * 100.0
$separationCm = [Math]::Sqrt($edgeX * $edgeX + $edgeY * $edgeY)
if ($separationCm -le 2.0 * $radiusCm) {
    throw 'Canonical terrain extent cannot provide a non-overlapping center/edge streaming proof.'
}
$probeZ = ([Math]::Max(
        [double]$centerCell.maximum_height_m,
        [double]$edgeCell.maximum_height_m) - $verticalOrigin + $spawnHeightMeters) * 100.0

New-Item -ItemType Directory -Path (Split-Path -Parent $probeDerivation) -Force | Out-Null
$probeContract = [ordered]@{
    schema = 'project-world-mesh-terrain-probe-derivation:v1'
    status = 'accepted'
    source_identity_sha256 = $SourceIdentity.ToLowerInvariant()
    map = $MapPackage
    compile_result_sha256 = Get-Sha256 -Path $compileResultPathFull
    compiler_profile_id = [string]$compilerProfile.profile_id
    compiler_profile_sha256 = Get-Sha256 -Path $compilerProfilePathFull
    runtime_profile_id = [string]$runtimeProfile.profile_id
    runtime_profile_sha256 = Get-Sha256 -Path $runtimeProfilePathFull
    grid_id = $runtimeGridId
    coordinate_transform = 'x_cm=(easting-origin_easting)*100;y_cm=-(northing-origin_northing)*100;z_cm=(height-vertical_origin)*100'
    engine_georeference_origin_m = $engineOrigin
    vertical_origin_m = $verticalOrigin
    center = [ordered]@{
        derivation = 'runtime product_spawn anchor engine_georeference_origin'
        canonical_m = @($engineOrigin[0], $engineOrigin[1])
        unreal_cm = @($centerX, $centerY, $probeZ)
        cell_id = $centerCell.cell_id
        cell_bounds_m = $centerCell.bounds
        cell_document_sha256 = $centerCell.document_sha256
    }
    edge = [ordered]@{
        derivation = 'farthest authenticated canonical terrain cell center'
        canonical_m = $edgeCell.canonical_center_m
        unreal_cm = @($edgeX, $edgeY, $probeZ)
        cell_id = $edgeCell.cell_id
        cell_bounds_m = $edgeCell.bounds
        cell_document_sha256 = $edgeCell.document_sha256
    }
    probe_z = [ordered]@{
        centimeters = $probeZ
        maximum_relevant_terrain_height_m = [Math]::Max(
            [double]$centerCell.maximum_height_m,
            [double]$edgeCell.maximum_height_m)
        runtime_spawn_height_above_terrain_m = $spawnHeightMeters
    }
    source_radius = [ordered]@{
        centimeters = $radiusCm
        derivation = 'runtime_partition.loading_range_m * 100'
    }
    center_edge_separation_cm = $separationCm
    non_overlapping_source_diameters = $separationCm -gt 2.0 * $radiusCm
}
[System.IO.File]::WriteAllText(
    $probeDerivation,
    ($probeContract | ConvertTo-Json -Depth 10) + "`n",
    [System.Text.UTF8Encoding]::new($false))
$probeDerivationSha256 = Get-Sha256 -Path $probeDerivation
if ($ProbeDerivationOnly) {
    $probeContract | ConvertTo-Json -Depth 10
    return
}

$executable = Join-Path $packagePath 'Windows\Alis\Binaries\Win64\Alis.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Packaged game executable does not exist: $executable"
}

$packageSaved = Join-Path $packagePath 'Windows\Alis\Saved'
$childResult = Join-Path $packageSaved 'Validation\WorldRealization\mesh-terrain-runtime-gate.json'
New-Item -ItemType Directory -Path (Split-Path -Parent $result) -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $log) -Force | Out-Null
Remove-Item -LiteralPath $result, $childResult, $log -Force -ErrorAction SilentlyContinue

$ownedProcessIds = [System.Collections.Generic.HashSet[int]]::new()
$process = $null

function Update-OwnedProcessIds {
    param([int]$RootProcessId)
    [void]$ownedProcessIds.Add($RootProcessId)
    $processes = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue)
    $changed = $true
    while ($changed) {
        $changed = $false
        foreach ($candidate in $processes) {
            if ($ownedProcessIds.Contains([int]$candidate.ParentProcessId) -and
                $ownedProcessIds.Add([int]$candidate.ProcessId)) {
                $changed = $true
            }
        }
    }
}

function Stop-OwnedProcessTree {
    param([int]$RootProcessId)
    Update-OwnedProcessIds -RootProcessId $RootProcessId
    $ordered = @($ownedProcessIds) | Sort-Object -Descending
    foreach ($processId in $ordered) {
        Stop-Process -Id $processId -Force -ErrorAction SilentlyContinue
    }
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        $remaining = @($ownedProcessIds | Where-Object {
            Get-Process -Id $_ -ErrorAction SilentlyContinue
        })
        if ($remaining.Count -eq 0) {
            return
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Packaged runtime left owned processes alive: $($remaining -join ', ')."
}

$arguments = @(
    'Alis',
    $MapPackage,
    '-game',
    '-ProjectSkipFrontEnd',
    '-ProjectWorldMeshTerrainRuntimeGate',
    "-ProjectWorldMeshTerrainMap=$MapPackage",
    "-ProjectWorldMeshTerrainResult=$childResult",
    "-ProjectWorldMeshTerrainSourceIdentity=$($SourceIdentity.ToLowerInvariant())",
    "-ProjectWorldMeshTerrainProbeDerivation=$probeDerivationSha256",
    "-ProjectWorldMeshTerrainCenterX=$CenterX",
    "-ProjectWorldMeshTerrainCenterY=$CenterY",
    "-ProjectWorldMeshTerrainEdgeX=$EdgeX",
    "-ProjectWorldMeshTerrainEdgeY=$EdgeY",
    "-ProjectWorldMeshTerrainProbeZ=$ProbeZ",
    "-ProjectWorldMeshTerrainRadiusCm=$RadiusCm",
    '-unattended',
    '-RenderOffScreen',
    '-ResX=640',
    '-ResY=360',
    '-NoSound',
    '-NoMessaging',
    '-nosplash',
    "-abslog=$log"
)

$exitCode = $null
try {
    $process = Start-Process -FilePath $executable -ArgumentList $arguments `
        -WorkingDirectory (Split-Path -Parent $executable) -WindowStyle Hidden -PassThru
    [void]$ownedProcessIds.Add($process.Id)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $process.HasExited -and [DateTime]::UtcNow -lt $deadline) {
        Update-OwnedProcessIds -RootProcessId $process.Id
        Start-Sleep -Milliseconds 250
        $process.Refresh()
    }
    if (-not $process.HasExited) {
        throw "Packaged Mesh Terrain runtime gate exceeded ${TimeoutSeconds}s."
    }
    $exitCode = $process.ExitCode
}
finally {
    if ($null -ne $process) {
        Stop-OwnedProcessTree -RootProcessId $process.Id
    }
}

if (-not (Test-Path -LiteralPath $childResult -PathType Leaf)) {
    throw "Packaged Mesh Terrain runtime receipt is missing after exit code $exitCode."
}
$receipt = Get-Content -LiteralPath $childResult -Raw | ConvertFrom-Json
Copy-Item -LiteralPath $childResult -Destination $result -Force
$loadedBuildVariants = @($receipt.loaded_build_variants)
$materialChains = @($receipt.material_chains)
if ($exitCode -ne 0) {
    throw "Packaged Mesh Terrain runtime gate exited with code $exitCode and receipt code $([string]$receipt.code)."
}
if ([string]$receipt.status -cne 'accepted' -or
    [string]$receipt.map -cne $MapPackage -or
    [string]$receipt.source_identity_sha256 -cne $SourceIdentity.ToLowerInvariant() -or
    [string]$receipt.probe_derivation_sha256 -cne $probeDerivationSha256 -or
    -not [bool]$receipt.streaming_source_enabled -or
    -not [bool]$receipt.source_streaming_completed -or
    [int]$receipt.loaded_tagged_section_count -lt 1 -or
    [int]$receipt.loaded_compiled_section_count -lt 1 -or
    [int]$receipt.renderable_static_mesh_component_count -lt 1 -or
    [int]$receipt.nanite_static_mesh_component_count -lt 1 -or
    [int]$receipt.section_material_instance_count -lt 1 -or
    $loadedBuildVariants -notcontains 'Collision' -or
    $loadedBuildVariants -notcontains 'Nanite' -or
    $materialChains.Count -ne 1 -or
    [string]$materialChains[0] -cne '/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default -> /ProjectMaterial/Surfaces/Terrain/M_ProjectTerrain.M_ProjectTerrain' -or
    [string]$receipt.trace_outcome -cne 'tagged_terrain_hit' -or
    -not [bool]$receipt.center_collision -or
    -not [bool]$receipt.edge_collision -or
    -not [bool]$receipt.center_unloaded_at_edge -or
    -not [bool]$receipt.center_reloaded -or
    [string]$receipt.center_marker -ceq [string]$receipt.edge_marker) {
    throw 'Packaged Mesh Terrain runtime receipt failed its acceptance contract.'
}
$receipt | ConvertTo-Json -Depth 8
