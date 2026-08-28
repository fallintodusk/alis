#Requires -Version 5.1
# License terms: see repository root LICENSE.

<#
.SYNOPSIS
    Prune superseded World scratch while retaining reusable caches and rollback.

.DESCRIPTION
    Dry-run by default. Pass -Apply to remove only the explicit owner paths
    listed below. The current canonical materialization, source cache, latest
    end-to-end run, execution environment, and final_p0 rollback snapshot are
    deliberately retained.
#>

param(
    [switch]$Apply
)

$ErrorActionPreference = "Stop"

function Get-TreeBytes {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return [Int64]0
    }
    $Item = Get-Item -LiteralPath $Path
    if (-not $Item.PSIsContainer) {
        return [Int64]$Item.Length
    }
    $Files = @(Get-ChildItem -LiteralPath $Path -File -Recurse -Force -ErrorAction SilentlyContinue)
    if ($Files.Count -eq 0) {
        return [Int64]0
    }
    $Sum = ($Files | Measure-Object Length -Sum).Sum
    return [Int64]$(if ($null -eq $Sum) { 0 } else { $Sum })
}

function Assert-UnderOwnerRoot {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string[]]$OwnerRoots
    )

    $FullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    foreach ($OwnerRoot in $OwnerRoots) {
        $FullOwner = [IO.Path]::GetFullPath($OwnerRoot).TrimEnd('\', '/')
        if ($FullPath.Equals($FullOwner, [StringComparison]::OrdinalIgnoreCase) -or
            $FullPath.StartsWith(
                $FullOwner + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            return
        }
    }
    throw "Refusing cleanup outside declared owner roots: $FullPath"
}

function ConvertTo-RepoRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $FullRoot = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    $FullPath = [IO.Path]::GetFullPath($Path)
    if (-not $FullPath.StartsWith(
        $FullRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the project root: $FullPath"
    }
    return $FullPath.Substring($FullRoot.Length + 1).Replace('\', '/')
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$WorldTmp = Join-Path $ProjectRoot "tmp\world"
$BootstrapTmp = Join-Path $ProjectRoot "tmp\clean_bootstrap_backup"
$PackageTmp = Join-Path $ProjectRoot "tmp\package"
$OwnerRoots = @($WorldTmp, $BootstrapTmp, $PackageTmp)

$Targets = [System.Collections.Generic.List[object]]::new()
function Add-CleanupTarget {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Owner,
        [Parameter(Mandatory = $true)][string]$Reason
    )

    if (Test-Path -LiteralPath $Path) {
        Assert-UnderOwnerRoot -Path $Path -OwnerRoots $OwnerRoots
        $Targets.Add([pscustomobject]@{
            path = [IO.Path]::GetFullPath($Path)
            owner = $Owner
            reason = $Reason
            bytes = Get-TreeBytes -Path $Path
        })
    }
}

Add-CleanupTarget `
    -Path (Join-Path $BootstrapTmp "old_packages") `
    -Owner "world-bootstrap" `
    -Reason "Superseded package copies; final_p0 is retained as the rollback snapshot."
Add-CleanupTarget `
    -Path (Join-Path $WorldTmp "world_realization\transactions") `
    -Owner "world-realization" `
    -Reason "Orphan transaction snapshots after accepted or restored operations."
Add-CleanupTarget `
    -Path (Join-Path $WorldTmp "world_realization\content_mutation.lock") `
    -Owner "world-realization" `
    -Reason "Disposable lock endpoint with no active owner process."
Add-CleanupTarget `
    -Path (Join-Path $WorldTmp "runtime_profile_locality") `
    -Owner "runtime-profile-locality" `
    -Reason "Completed isolated candidate snapshots; accepted receipts live under Saved."
Add-CleanupTarget `
    -Path (Join-Path $WorldTmp "runtime_profile_tournament") `
    -Owner "runtime-profile-tournament" `
    -Reason "Completed candidate packages and snapshots; tournament receipts live under Saved."
Add-CleanupTarget `
    -Path (Join-Path $WorldTmp "playable_tour\diagnostic") `
    -Owner "playable-tour" `
    -Reason "Completed non-publishing diagnostics; authenticated package receipts live under Saved."
Add-CleanupTarget `
    -Path (Join-Path $WorldTmp "generated_recovery") `
    -Owner "generated-recovery" `
    -Reason "Completed bounded generated-state recovery; accepted bytes are restored in the owner tree."
Add-CleanupTarget `
    -Path (Join-Path $WorldTmp "manifest_metadata_migration") `
    -Owner "manifest-metadata-migration" `
    -Reason "Completed one-time migration scripts; immutable manifest generations retain rollback."
Add-CleanupTarget `
    -Path (Join-Path $WorldTmp "visual_verification") `
    -Owner "visual-verification" `
    -Reason "Scratch descriptors and receipts promoted to Saved validation evidence."
Add-CleanupTarget `
    -Path (Join-Path $WorldTmp "presentation\landscape\fingerprint_migration") `
    -Owner "landscape-presentation" `
    -Reason "Completed material-binding fingerprint migration scratch."
Add-CleanupTarget `
    -Path (Join-Path $WorldTmp "source_ingestion\tests") `
    -Owner "source-ingestion" `
    -Reason "Completed test fixtures; production runs and acquisition cache are retained."

$CanonicalScratchNames = @(
    "3-pre-membership-final-a",
    "3-pre-membership-final-b",
    "3-pre-water-final-a",
    "3-pre-water-final-b",
    "r2-corrected-a",
    "r2-corrected-b",
    "r2-corrected-authority-a",
    "r2-corrected-authority-b",
    "water_overlap_audit",
    "water-geometry-smoke",
    "water-geometry-smoke-2",
    "water-raster-smoke"
)
foreach ($Name in $CanonicalScratchNames) {
    Add-CleanupTarget `
        -Path (Join-Path $WorldTmp "canonical_compilation\$Name") `
        -Owner "canonical-compilation" `
        -Reason "Superseded comparison or smoke workspace."
}
Get-ChildItem -LiteralPath $WorldTmp -Directory -Filter "package-locality-fixture-smoke-*" -ErrorAction SilentlyContinue |
    ForEach-Object {
        Add-CleanupTarget `
            -Path $_.FullName `
            -Owner "world-package-locality-test" `
            -Reason "Completed package-locality fixture."
    }

$TargetBytes = if ($Targets.Count -eq 0) {
    [Int64]0
} else {
    [Int64](($Targets | Measure-Object bytes -Sum).Sum)
}
Write-Host "World workspace cleanup" -ForegroundColor Cyan
Write-Host "MODE             = $(if ($Apply) { 'apply' } else { 'dry-run' })"
Write-Host "TARGETS          = $($Targets.Count)"
Write-Host "RECLAIMABLE_GIB  = $([math]::Round($TargetBytes / 1GB, 3))"
Write-Host "PRESERVE         = final_p0 rollback, current materialized authority, source cache/runs, latest L3 run, tools"
foreach ($Target in $Targets) {
    $Relative = ConvertTo-RepoRelativePath -ProjectRoot $ProjectRoot -Path $Target.path
    Write-Host ("  {0:N3} GiB  {1}  [{2}]" -f ($Target.bytes / 1GB), $Relative, $Target.owner)
}

if (-not $Apply) {
    Write-Host "Dry-run only. Pass -Apply to execute this exact plan."
    exit 0
}

$BlockingProcesses = @(Get-Process `
    UnrealEditor, UnrealEditor-Cmd, AutomationTool, UnrealBuildTool, ShaderCompileWorker `
    -ErrorAction SilentlyContinue)
if ($BlockingProcesses.Count -gt 0) {
    throw "World cleanup requires Unreal and build processes to be stopped."
}

$ActiveJournals = @(Get-ChildItem `
    -LiteralPath (Join-Path $ProjectRoot "Plugins\World") `
    -Filter "journal.json" `
    -File `
    -Recurse `
    -ErrorAction SilentlyContinue)
if ($ActiveJournals.Count -gt 0) {
    throw "World cleanup refused because a durable transaction journal exists."
}

$Removed = [System.Collections.Generic.List[object]]::new()
foreach ($Target in $Targets) {
    if (-not (Test-Path -LiteralPath $Target.path)) {
        continue
    }
    Assert-UnderOwnerRoot -Path $Target.path -OwnerRoots $OwnerRoots
    Remove-Item -LiteralPath $Target.path -Recurse -Force
    $Removed.Add($Target)
}

if (Test-Path -LiteralPath $PackageTmp) {
    $PackageChildren = @(Get-ChildItem -LiteralPath $PackageTmp -Force)
    if ($PackageChildren.Count -eq 0) {
        Assert-UnderOwnerRoot -Path $PackageTmp -OwnerRoots $OwnerRoots
        Remove-Item -LiteralPath $PackageTmp -Force
    }
}

$EvidenceRoot = Join-Path $ProjectRoot "Saved\Validation\WorldCleanup"
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$ReceiptPath = Join-Path $EvidenceRoot ((Get-Date -Format "yyyyMMdd_HHmmss") + ".json")
$RemovedBytes = if ($Removed.Count -eq 0) {
    [Int64]0
} else {
    [Int64](($Removed | Measure-Object bytes -Sum).Sum)
}
$Receipt = [ordered]@{
    schema_version = 1
    status = "accepted"
    removed_bytes = $RemovedBytes
    removed = @($Removed | ForEach-Object {
        [ordered]@{
            path = ConvertTo-RepoRelativePath -ProjectRoot $ProjectRoot -Path $_.path
            owner = $_.owner
            bytes = [Int64]$_.bytes
            reason = $_.reason
        }
    })
    retained_rollback = "tmp/clean_bootstrap_backup/final_p0"
}
$Receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReceiptPath -Encoding Ascii

Write-Host "[OK] Removed $($Removed.Count) owner workspaces."
Write-Host "[OK] Reclaimed $([math]::Round($Receipt.removed_bytes / 1GB, 3)) GiB."
Write-Host "Receipt: $ReceiptPath"
