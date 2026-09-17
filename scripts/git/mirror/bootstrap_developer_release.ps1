#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Destination,
    [string]$RepositoryUrl = "https://github.com/fallintodusk/alis.git"
)

$ErrorActionPreference = "Stop"
$DeveloperRoot = $PSScriptRoot
$ManifestCandidates = @(Get-ChildItem -LiteralPath $DeveloperRoot -Filter "*.developer-payload.json" -File)
if ($ManifestCandidates.Count -ne 1) {
    throw "Expected exactly one developer payload manifest beside this bootstrap."
}
$ManifestPath = $ManifestCandidates[0].FullName
$Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if ($Manifest.schema_version -ne 2 -or $Manifest.public_source.tag -notmatch "^v[0-9]+\.[0-9]+\.[0-9]+$" -or
    $Manifest.public_source.revision -notmatch "^[0-9a-f]{40}$") {
    throw "Unsupported developer payload manifest."
}

$Parent = Split-Path -Parent $DeveloperRoot
$ReleaseRoot = if ((Test-Path -LiteralPath (Join-Path $DeveloperRoot "SHA256SUMS.txt") -PathType Leaf)) {
    $DeveloperRoot
} elseif ((Test-Path -LiteralPath (Join-Path $Parent "SHA256SUMS.txt") -PathType Leaf)) {
    $Parent
} else {
    throw "Signed verification files are missing. Keep SHA256SUMS.txt beside this bootstrap."
}
if (-not (Test-Path -LiteralPath (Join-Path $ReleaseRoot "SHA256SUMS.txt.asc") -PathType Leaf)) {
    throw "Signed release files are incomplete: SHA256SUMS.txt.asc is missing."
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git for Windows is required: https://git-scm.com/download/win"
}

if ([string]::IsNullOrWhiteSpace($Destination)) {
    $DefaultParent = Split-Path -Parent $ReleaseRoot
    $DefaultDestination = Join-Path $DefaultParent ("Alis-" + $Manifest.public_source.tag)
    $Answer = Read-Host "Developer checkout location [$DefaultDestination] (press Enter to accept)"
    $Destination = if ([string]::IsNullOrWhiteSpace($Answer)) { $DefaultDestination } else { $Answer }
}
$Destination = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $Destination) {
    throw "Developer checkout destination already exists: $Destination"
}

$OwnsDestination = $false
try {
    & git -c core.longpaths=true -c advice.detachedHead=false clone --quiet --branch $Manifest.public_source.tag --depth 1 $RepositoryUrl $Destination
    $OwnsDestination = Test-Path -LiteralPath $Destination -PathType Container
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to clone the exact ALIS release tag."
    }
    & git -C $Destination config core.longpaths true
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to persist Git long-path support in the developer checkout."
    }
    $Revision = (& git -C $Destination rev-parse HEAD 2>$null | Out-String).Trim().ToLowerInvariant()
    if ($LASTEXITCODE -ne 0 -or $Revision -ne ([string]$Manifest.public_source.revision).ToLowerInvariant()) {
        throw "Cloned developer source revision does not match the signed payload."
    }
    $TrustedInstaller = Join-Path $Destination "scripts\git\mirror\install_developer_payload.ps1"
    if (-not (Test-Path -LiteralPath $TrustedInstaller -PathType Leaf)) {
        throw "Trusted checkout-local developer installer is missing."
    }
    & $TrustedInstaller `
        -ProjectRoot $Destination `
        -ReleaseDir $ReleaseRoot `
        -ManifestPath $ManifestPath `
        -RequireReleaseSignature
    if ($LASTEXITCODE -ne 0) {
        throw "Developer payload installation failed."
    }
} catch {
    if ($OwnsDestination -and (Test-Path -LiteralPath $Destination)) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    throw
}

Write-Host "[OK] ALIS developer project installed: $Destination"
Write-Host "Next: copy scripts\config\ue_path.conf.example to scripts\config\ue_path.local.conf"
Write-Host "Then run: .\scripts\ue\standalone\build.ps1"
