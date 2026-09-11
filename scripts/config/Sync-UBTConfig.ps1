#Requires -Version 5.1
<#
.SYNOPSIS
    Materialize the project-local UBT config from its committed SOT.

.DESCRIPTION
    Source of truth: scripts/config/BuildConfiguration.xml (committed).
    Destination:    <ProjectRoot>/Saved/UnrealBuildTool/BuildConfiguration.xml
                    (gitignored; survives Saved/ clean-ups via this sync).

    UBT reads the destination path with highest precedence among static
    XmlConfig locations when invoked with project context (BuildCookRun
    -project=...). See XmlConfig.cs:120-130 in the engine source.

    The function is idempotent. It only overwrites the destination when its
    bytes differ from the SOT, so cached XmlConfigCache.bin is invalidated
    only on real config drift.

.EXAMPLE
    . scripts\config\Sync-UBTConfig.ps1
    Sync-UBTConfig -ProjectRoot (Resolve-Path .)
#>

function Sync-UBTConfig {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    # Normalize once so callers can pass a PathInfo, relative path, or string.
    $ProjectRoot = (Resolve-Path $ProjectRoot).ProviderPath

    $Source = Join-Path $ProjectRoot "scripts\config\BuildConfiguration.xml"
    if (-not (Test-Path $Source)) {
        throw "UBT config SOT not found: $Source"
    }

    $DestDir = Join-Path $ProjectRoot "Saved\UnrealBuildTool"
    $Dest    = Join-Path $DestDir "BuildConfiguration.xml"
    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null

    $needsCopy = $true
    if (Test-Path $Dest) {
        $srcHash = (Get-FileHash $Source -Algorithm SHA256).Hash
        $dstHash = (Get-FileHash $Dest   -Algorithm SHA256).Hash
        if ($srcHash -eq $dstHash) {
            $needsCopy = $false
        }
    }

    if ($needsCopy) {
        Copy-Item $Source $Dest -Force
        Write-Host "[Sync-UBTConfig] Materialized: $Dest"
    } else {
        Write-Host "[Sync-UBTConfig] Up-to-date: $Dest"
    }
}
