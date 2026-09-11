# scripts/config/env.ps1
# Shared UE build env for all PowerShell scripts (build.ps1, run.ps1, etc.)
# Uses Resolve-UEConfig as single source of truth for config resolution.
#
# AUTHORITY: conf (local > tracked) is authoritative. A pre-existing env
# UE_PATH is a derived cache; if it MISMATCHES the resolved conf value
# this script HARD-FAILS ("rerun setup_ue_env.ps1"). Fallback glob runs
# only when no conf declares UE_PATH.

. (Join-Path $PSScriptRoot "Resolve-UEConfig.ps1")

$config = Resolve-UEConfig -ConfigDir $PSScriptRoot

$resolvedUEPath = $config.UE_PATH
if (-not $resolvedUEPath) {
    $resolvedUEPath = Resolve-UELauncherFallback
    if ($resolvedUEPath) {
        Write-Host "[env.ps1] No conf UE_PATH; using highest valid install: $resolvedUEPath"
    }
}

if (-not $resolvedUEPath) {
    Write-Error ("UE_PATH not resolved. Set it in scripts/config/ue_path.conf " +
                 "(or ue_path.local.conf).")
    exit 1
}

$staleError = Test-UEStaleEnv -ResolvedUEPath $resolvedUEPath
if ($staleError) {
    Write-Error $staleError
    exit 1
}

$Env:UE_PATH        = $resolvedUEPath
$Env:UE_SOURCE_PATH = $config.UE_SOURCE_PATH
$Env:BUILD_TARGET   = $config.BUILD_TARGET
$Env:BUILD_CONFIG   = $config.BUILD_CONFIG
$Env:BUILD_PLATFORM = $config.BUILD_PLATFORM

Write-Host "=== env.ps1 ==="
Write-Host "UE_PATH        = $Env:UE_PATH"
Write-Host "BUILD_TARGET   = $Env:BUILD_TARGET"
Write-Host "BUILD_CONFIG   = $Env:BUILD_CONFIG"
Write-Host "BUILD_PLATFORM = $Env:BUILD_PLATFORM"
Write-Host "==============="
