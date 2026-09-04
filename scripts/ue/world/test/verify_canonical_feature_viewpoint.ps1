# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$CompileResult,
    [Parameter(Mandatory = $true)][string]$ActiveManifestSet,
    [Parameter(Mandatory = $true)][double]$UnrealXCentimeters,
    [Parameter(Mandatory = $true)][double]$UnrealYCentimeters,
    [Parameter(Mandatory = $true)][string]$FeatureId,
    [Parameter(Mandatory = $true)][string]$FeatureClass,
    [Parameter(Mandatory = $true)][string]$ReceiptPath
)

$ErrorActionPreference = 'Stop'

function Assert-Viewpoint {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

function Test-PointInRing {
    param([double]$X, [double]$Y, [object[]]$Ring)
    Assert-Viewpoint ($Ring.Count -ge 4) 'Canonical polygon ring is invalid.'
    $inside = $false
    $prior = $Ring.Count - 1
    for ($index = 0; $index -lt $Ring.Count; ++$index) {
        $x1 = [double]$Ring[$index][0]
        $y1 = [double]$Ring[$index][1]
        $x2 = [double]$Ring[$prior][0]
        $y2 = [double]$Ring[$prior][1]
        $cross = ($X - $x1) * ($y2 - $y1) - ($Y - $y1) * ($x2 - $x1)
        $onSegment = [Math]::Abs($cross) -le 0.000001 -and
            $X -ge [Math]::Min($x1, $x2) -and $X -le [Math]::Max($x1, $x2) -and
            $Y -ge [Math]::Min($y1, $y2) -and $Y -le [Math]::Max($y1, $y2)
        if ($onSegment) {
            return $true
        }
        if ((($y1 -gt $Y) -ne ($y2 -gt $Y)) -and
            ($X -lt (($x2 - $x1) * ($Y - $y1) / ($y2 - $y1) + $x1))) {
            $inside = -not $inside
        }
        $prior = $index
    }
    return $inside
}

function Test-PointInPolygon {
    param([double]$X, [double]$Y, [object[]]$Rings)
    if ($Rings.Count -eq 0 -or -not (Test-PointInRing -X $X -Y $Y -Ring $Rings[0])) {
        return $false
    }
    for ($index = 1; $index -lt $Rings.Count; ++$index) {
        if (Test-PointInRing -X $X -Y $Y -Ring $Rings[$index]) {
            return $false
        }
    }
    return $true
}

$compilePath = [IO.Path]::GetFullPath($CompileResult)
$activeSetPath = [IO.Path]::GetFullPath($ActiveManifestSet)
$outputPath = [IO.Path]::GetFullPath($ReceiptPath)
Assert-Viewpoint (Test-Path -LiteralPath $compilePath -PathType Leaf) 'Compile result is missing.'
Assert-Viewpoint (Test-Path -LiteralPath $activeSetPath -PathType Leaf) 'Active manifest set is missing.'

$activeSet = Get-Content -LiteralPath $activeSetPath -Raw | ConvertFrom-Json
$waterScope = @($activeSet.scopes | Where-Object {
        [string]$_.scope_id -ceq 'layer_kazan_territory_v1_water'
    })
Assert-Viewpoint ($waterScope.Count -eq 1) 'Active Water scope is missing or ambiguous.'
$waterManifestPath = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $activeSetPath) `
    ([string]$waterScope[0].manifest_path)))
$waterManifestHash = (Get-FileHash -LiteralPath $waterManifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
Assert-Viewpoint ($waterManifestHash -ceq [string]$waterScope[0].manifest_sha256) `
    'Active Water manifest hash does not match active_set.json.'
$waterManifest = Get-Content -LiteralPath $waterManifestPath -Raw | ConvertFrom-Json
$compileHash = (Get-FileHash -LiteralPath $compilePath -Algorithm SHA256).Hash.ToLowerInvariant()
Assert-Viewpoint ($compileHash -ceq [string]$waterManifest.input_identity.compile_result_sha256) `
    'Materialized canonical authority does not match the active Water manifest.'

$canonicalRoot = Join-Path (Split-Path -Parent $compilePath) 'canonical'
$coverage = Get-Content -LiteralPath (Join-Path $canonicalRoot 'coverage.json') -Raw | ConvertFrom-Json
$canonicalX = $UnrealXCentimeters * 0.01 + [double]$coverage.engine_georeference_origin[0]
$canonicalY = -$UnrealYCentimeters * 0.01 + [double]$coverage.engine_georeference_origin[1]
$cellWidth = [double]$coverage.grid.sample_spacing[0] * [int]$coverage.grid.cell_quads[0]
$cellHeight = [double]$coverage.grid.sample_spacing[1] * [int]$coverage.grid.cell_quads[1]
$cellX = [Math]::Floor(($canonicalX - [double]$coverage.grid.origin[0]) / $cellWidth)
$cellY = [Math]::Floor(($canonicalY - [double]$coverage.grid.origin[1]) / $cellHeight)
$cellName = 'cell_x{0}_y{1}.json' -f $cellX, $cellY
$cell = Get-Content -LiteralPath (Join-Path $canonicalRoot "cells\$cellName") -Raw | ConvertFrom-Json
$cellFeatureIds = @($cell.owned_feature_ids) + @($cell.referenced_feature_ids)
Assert-Viewpoint ($cellFeatureIds -ccontains $FeatureId) `
    'The target canonical cell does not reference the expected feature.'

$feature = $null
foreach ($featureFile in Get-ChildItem -LiteralPath (Join-Path $canonicalRoot 'features') -File -Filter '*.json') {
    $document = Get-Content -LiteralPath $featureFile.FullName -Raw | ConvertFrom-Json
    $candidate = @($document.features | Where-Object { [string]$_.feature_id -ceq $FeatureId })
    if ($candidate.Count -gt 0) {
        Assert-Viewpoint ($candidate.Count -eq 1 -and $null -eq $feature) `
            'Expected feature identity is duplicated in canonical authority.'
        $feature = $candidate[0]
    }
}
Assert-Viewpoint ($null -ne $feature -and [string]$feature.feature_class -ceq $FeatureClass) `
    'Expected canonical feature is missing or has the wrong class.'
Assert-Viewpoint (@($feature.intersecting_cell_ids) -ccontains [string]$cell.cell_id) `
    'Expected feature does not intersect the target canonical cell.'

$inside = $false
if ([string]$feature.geometry.type -ceq 'Polygon') {
    $inside = Test-PointInPolygon -X $canonicalX -Y $canonicalY -Rings $feature.geometry.coordinates
}
elseif ([string]$feature.geometry.type -ceq 'MultiPolygon') {
    foreach ($polygon in $feature.geometry.coordinates) {
        if (Test-PointInPolygon -X $canonicalX -Y $canonicalY -Rings $polygon) {
            $inside = $true
            break
        }
    }
}
Assert-Viewpoint $inside 'The requested viewpoint is outside the expected canonical feature geometry.'

$receipt = [ordered]@{
    schema_version = 1
    status = 'accepted'
    target_unreal_xy_cm = @($UnrealXCentimeters, $UnrealYCentimeters)
    target_canonical_xy_m = @($canonicalX, $canonicalY)
    target_cell_id = [string]$cell.cell_id
    feature_id = [string]$feature.feature_id
    feature_class = [string]$feature.feature_class
    water_class = [string]$feature.attributes.water_class
    feature_owner_cell_id = [string]$feature.owner_cell_id
    active_water_manifest = $waterManifestPath
    active_water_manifest_sha256 = $waterManifestHash
    compile_result_sha256 = $compileHash
}
New-Item -ItemType Directory -Path (Split-Path -Parent $outputPath) -Force | Out-Null
$staging = "$outputPath.tmp"
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $staging -Encoding UTF8
Move-Item -LiteralPath $staging -Destination $outputPath -Force
$receipt | ConvertTo-Json -Compress -Depth 6
