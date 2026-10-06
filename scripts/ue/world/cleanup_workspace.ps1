#Requires -Version 5.1
# License terms: see repository root LICENSE.

<#
.SYNOPSIS
    Prune superseded World scratch while retaining reusable caches and rollback.

.DESCRIPTION
    Dry-run by default. Pass -Apply to remove only the explicit owner paths
    listed below. The current canonical materialization, source cache, latest
    end-to-end run, and execution environment are deliberately retained.
    -Apply plans and deletes under the shared generated-content lock as its
    owner, so it refuses while any generator runs, while it is itself
    delegated, and while an outer-recovery marker is pending.
#>

param(
    [switch]$Apply,

    # Project whose scratch is pruned; defaults to this checkout.
    [string]$ProjectRoot = ''
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
. (Join-Path $ScriptDir '..\generated_content\generated_content_mutation_lock.ps1')
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
}
$ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot)
$WorldTmp = Join-Path $ProjectRoot "tmp\world"
$PackageTmp = Join-Path $ProjectRoot "tmp\package"
$OwnerRoots = @($WorldTmp, $PackageTmp)

$ContentLock = $null
try {
    if ($Apply) {
        # Taken before planning: a live generator, a delegated run, or a pending
        # outer recovery refuses here, before anything is listed or deleted.
        $ContentLock = Enter-ProjectGeneratedContentMutationLock `
            -ProjectRoot $ProjectRoot `
            -OwnerName 'World workspace cleanup' `
            -RequireOwnership
    }
    $RecoveryPending = Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $ProjectRoot

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
        -Path (Join-Path $WorldTmp "world_realization\transactions") `
        -Owner "world-realization" `
        -Reason "Orphan transaction snapshots after accepted or restored operations."
    if (-not $RecoveryPending) {
        # With no marker pending under the held lock, every entry is debris of a
        # committed or recovered outer operation.
        Add-CleanupTarget `
            -Path (Join-Path $WorldTmp "world_realization\outer_recovery") `
            -Owner "generated-content-recovery" `
            -Reason "Debris of committed or recovered outer operations."
    }
    Add-CleanupTarget `
        -Path (Join-Path $WorldTmp "runtime_profile_locality") `
        -Owner "runtime-profile-locality" `
        -Reason "Completed isolated candidate snapshots; accepted receipts live under Saved."
    Add-CleanupTarget `
        -Path (Join-Path $WorldTmp "runtime_profile_tournament") `
        -Owner "runtime-profile-tournament" `
        -Reason "Completed candidate packages and snapshots; tournament receipts live under Saved."
    Add-CleanupTarget `
        -Path (Join-Path $WorldTmp "visual_verification") `
        -Owner "visual-verification" `
        -Reason "Scratch descriptors and receipts promoted to Saved validation evidence."
    Add-CleanupTarget `
        -Path (Join-Path $WorldTmp "source_ingestion\tests") `
        -Owner "source-ingestion" `
        -Reason "Completed test fixtures; production runs and acquisition cache are retained."
    $TargetBytes = if ($Targets.Count -eq 0) {
        [Int64]0
    } else {
        [Int64](($Targets | Measure-Object bytes -Sum).Sum)
    }
    Write-Host "World workspace cleanup" -ForegroundColor Cyan
    Write-Host "MODE             = $(if ($Apply) { 'apply' } else { 'dry-run' })"
    Write-Host "TARGETS          = $($Targets.Count)"
    Write-Host "RECLAIMABLE_GIB  = $([math]::Round($TargetBytes / 1GB, 3))"
    Write-Host "PRESERVE         = content lock, current materialized authority, source cache/runs, latest L3 run, tools"
    foreach ($Target in $Targets) {
        $Relative = ConvertTo-RepoRelativePath -ProjectRoot $ProjectRoot -Path $Target.path
        Write-Host ("  {0:N3} GiB  {1}  [{2}]" -f ($Target.bytes / 1GB), $Relative, $Target.owner)
    }

    if (-not $Apply) {
        if ($RecoveryPending) {
            Write-Host "PENDING OUTER RECOVERY: -Apply will refuse until scripts/ue/generated_content/recover_generated_content.ps1 resolves it." -ForegroundColor Yellow
        }
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
    }
    $Receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReceiptPath -Encoding Ascii

    Write-Host "[OK] Removed $($Removed.Count) owner workspaces."
    Write-Host "[OK] Reclaimed $([math]::Round($Receipt.removed_bytes / 1GB, 3)) GiB."
    Write-Host "Receipt: $ReceiptPath"
}
finally {
    if ($null -ne $ContentLock) {
        $ContentLock.Dispose()
    }
}
