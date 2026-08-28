# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [switch]$Apply,
    [string[]]$EditorDiagnosticFile = @()
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$cinematicTmp = Join-Path $projectRoot 'tmp\cinematic'
$auditRoot = Join-Path $projectRoot 'Saved\Validation\CinematicRelease\PackageAudit'
$movieRenderRoot = Join-Path $projectRoot 'Saved\MovieRenders'
$screenshotRoot = Join-Path $projectRoot 'Saved\Screenshots'
$autosaveRoot = Join-Path $projectRoot 'Saved\Autosaves\ProjectWorldData\Generated\Territory'
$ownerRoots = @($cinematicTmp, $auditRoot, $movieRenderRoot, $screenshotRoot, $autosaveRoot)

function Assert-OwnerPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $full = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    foreach ($root in $ownerRoots) {
        $owner = [IO.Path]::GetFullPath($root).TrimEnd('\', '/')
        if ($full.Equals($owner, [StringComparison]::OrdinalIgnoreCase) -or
            $full.StartsWith($owner + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            return
        }
    }
    throw "[ProjectCinematic] Cleanup target escaped its owner roots: $full"
}

function Get-Bytes {
    param([Parameter(Mandatory = $true)][string]$Path)

    $item = Get-Item -LiteralPath $Path
    if (-not $item.PSIsContainer) {
        return [Int64]$item.Length
    }
    $sum = (Get-ChildItem -LiteralPath $Path -Recurse -File -Force |
        Measure-Object Length -Sum).Sum
    return [Int64]$(if ($null -eq $sum) { 0 } else { $sum })
}

$targets = [Collections.Generic.List[object]]::new()
function Add-Target {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Reason
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    Assert-OwnerPath -Path $Path
    $targets.Add([pscustomobject]@{
            path = [IO.Path]::GetFullPath($Path)
            reason = $Reason
            bytes = Get-Bytes -Path $Path
        })
}

foreach ($name in @('inspection', 'monitor', 'release_capture')) {
    Add-Target -Path (Join-Path $cinematicTmp $name) `
        -Reason 'Disposable ProjectCinematic working data.'
}

Get-ChildItem -LiteralPath $auditRoot -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like '*.capture-census.log' -or $_.Length -gt 1MB } |
    ForEach-Object {
        Add-Target -Path $_.FullName `
            -Reason 'Superseded verbose package census; compact authenticated audit is retained.'
    }

$currentReceipt = Join-Path $projectRoot 'Saved\CinematicRelease\Kazan\Current\receipt.json'
if (Test-Path -LiteralPath $currentReceipt -PathType Leaf) {
    $receipt = Get-Content -LiteralPath $currentReceipt -Raw | ConvertFrom-Json
    if ($receipt.status -eq 'technically_accepted') {
        Add-Target -Path (Join-Path $movieRenderRoot 'LS_KazanRelease_v1.mov') `
            -Reason 'Obsolete pre-wrapper render superseded by Current plus Previous rollback.'
    }
}

foreach ($relative in $EditorDiagnosticFile) {
    if ([IO.Path]::IsPathRooted($relative)) {
        throw '[ProjectCinematic] EditorDiagnosticFile must be repository-relative.'
    }
    Add-Target -Path (Join-Path $projectRoot $relative) `
        -Reason 'Explicit editor diagnostic owned by the completed capture operation.'
}

$total = [Int64]$(if ($targets.Count -eq 0) {
        0
    }
    else {
        ($targets | Measure-Object bytes -Sum).Sum
    })
Write-Host "[ProjectCinematic] Cleanup mode: $(if ($Apply) { 'apply' } else { 'dry-run' })"
Write-Host "[ProjectCinematic] Targets: $($targets.Count); bytes: $total"
foreach ($target in $targets) {
    $relative = $target.path.Substring($projectRoot.Length).TrimStart('\').Replace('\', '/')
    Write-Host "  $($target.bytes) bytes - $relative"
}
if (-not $Apply) {
    return
}

$blocking = @(Get-Process UnrealEditor, UnrealEditor-Cmd, AutomationTool,
    UnrealBuildTool, ShaderCompileWorker -ErrorAction SilentlyContinue)
if ($blocking.Count -gt 0) {
    throw '[ProjectCinematic] Cleanup requires Unreal and build processes to be stopped.'
}

foreach ($target in $targets) {
    Assert-OwnerPath -Path $target.path
    Remove-Item -LiteralPath $target.path -Recurse -Force
}

$evidenceRoot = Join-Path $projectRoot 'Saved\Validation\CinematicRelease\Cleanup'
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$evidencePath = Join-Path $evidenceRoot ((Get-Date -Format 'yyyyMMdd_HHmmss') + '.json')
$cleanupReceipt = [ordered]@{
    schema_version = 1
    status = 'accepted'
    removed_bytes = $total
    preserved = @(
        'Saved/CinematicRelease/Kazan/Current',
        'Saved/CinematicRelease/Kazan/Previous'
    )
    removed = @($targets | ForEach-Object {
            [ordered]@{
                path = $_.path.Substring($projectRoot.Length).TrimStart('\').Replace('\', '/')
                reason = $_.reason
                bytes = $_.bytes
            }
        })
}
[IO.File]::WriteAllText(
    $evidencePath,
    ($cleanupReceipt | ConvertTo-Json -Depth 5) + "`n",
    [Text.UTF8Encoding]::new($false))
Write-Host "[ProjectCinematic] Cleanup accepted: $evidencePath"
