# Complete engine-config resolver conformance gate.

param([string]$RepoRoot)

$ErrorActionPreference = "Stop"
if (-not $RepoRoot) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
}

function Assert-NativeSuccess {
    param([Parameter(Mandatory)][string]$Step)
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE"
    }
}

$pester = Invoke-Pester -Path (Join-Path $RepoRoot "scripts\config\test"), `
    (Join-Path $RepoRoot "scripts\setup\test") -Output Minimal -PassThru
if ($pester.FailedCount -ne 0) {
    throw "PowerShell/batch/setup conformance failed"
}

& python (Join-Path $RepoRoot "scripts\config\test\conformance_py.py")
Assert-NativeSuccess "Python conformance"

$bash = Get-Command bash -ErrorAction Stop
$shellScript = (Join-Path $RepoRoot `
    "scripts\config\test\conformance_sh.sh") -replace '\\', '/'
if ($shellScript -match '^([A-Za-z]):/(.*)$') {
    $drive = $Matches[1].ToLower()
    $suffix = $Matches[2]
    if ($bash.Source -match '(?i)(Windows\\System32|WindowsApps).*bash\.exe$') {
        $shellScript = "/mnt/$drive/$suffix"
    } else {
        $shellScript = "/$drive/$suffix"
    }
}
& $bash.Source $shellScript
Assert-NativeSuccess "shell/Make conformance"

& cargo test --manifest-path (Join-Path $RepoRoot "tools\BuildService\Cargo.toml") `
    -p engine_config -p build_service_executor `
    -p build_service_publisher -p build_service_cli
Assert-NativeSuccess "BuildService engine-config conformance"

Write-Host "[OK] complete engine-config conformance"
