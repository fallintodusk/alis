# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

function Resolve-ProjectWorldCapturePlanPath {
    param([Parameter(Mandatory)][string]$ProjectRoot, [Parameter(Mandatory)][string]$VantagePlan)
    $candidate = if ([IO.Path]::IsPathRooted($VantagePlan)) { $VantagePlan } else { Join-Path $ProjectRoot $VantagePlan }
    $path = [IO.Path]::GetFullPath($candidate)
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $path = [IO.Path]::GetFullPath($VantagePlan)
    }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Vantage plan does not exist: $VantagePlan"
    }
    return $path
}
