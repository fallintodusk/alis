#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ReleaseDir,
    [Parameter(Mandatory = $true)][string]$PublicSourceRoot,
    [Parameter(Mandatory = $true)][string]$PlayerPackageRoot,
    [Parameter(Mandatory = $true)][string]$PlayerAcceptance,
    [Parameter(Mandatory = $true)][string]$DeveloperReleaseDir,
    [Parameter(Mandatory = $true)][string]$DeveloperPayloadManifest,
    [Parameter(Mandatory = $true)][string]$ComponentManifest,
    [Parameter(Mandatory = $true)][string]$DependencyReport,
    [Parameter(Mandatory = $true)][string]$PrivacyReport,
    [Parameter(Mandatory = $true)][string]$AttributionNotice,
    [Parameter(Mandatory = $true)][string]$ProductTerms,
    [string]$ReleaseVersion = "2.0.0",
    [string]$ReleaseTag = "v2.0.0",
    [int]$SplitSizeMiB = 1700,
    [string]$SevenZip
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$Implementation = Join-Path $ScriptDir "prepare_release.py"
$Python = Get-Command python -ErrorAction SilentlyContinue
if (-not $Python) {
    throw "Python was not found on PATH."
}

$ResolvedReleaseDir = [IO.Path]::GetFullPath($ReleaseDir)
$TmpRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "tmp")).TrimEnd('\', '/')
if (-not $ResolvedReleaseDir.StartsWith(
        $TmpRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "ReleaseDir must remain under the project tmp directory: $ResolvedReleaseDir"
}
if (Test-Path -LiteralPath $ResolvedReleaseDir) {
    throw "ReleaseDir already exists: $ResolvedReleaseDir"
}

$ArchiveArgs = @(
    $Implementation, "archive-player",
    "--package-root", $PlayerPackageRoot,
    "--output-dir", $ResolvedReleaseDir,
    "--release-version", $ReleaseVersion,
    "--split-size-mib", $SplitSizeMiB
)
if ($SevenZip) {
    $ArchiveArgs += @("--seven-zip", $SevenZip)
}
& $Python.Source @ArchiveArgs
if ($LASTEXITCODE -ne 0) {
    throw "Player archive preparation failed."
}

$ArchiveReport = Join-Path $ResolvedReleaseDir "player-archive.json"
$PrepareArgs = @(
    $Implementation, "prepare",
    "--release-version", $ReleaseVersion,
    "--release-tag", $ReleaseTag,
    "--private-source-root", $ProjectRoot,
    "--public-source-root", $PublicSourceRoot,
    "--player-package-root", $PlayerPackageRoot,
    "--player-acceptance", $PlayerAcceptance,
    "--player-archive-report", $ArchiveReport,
    "--developer-release-dir", $DeveloperReleaseDir,
    "--developer-payload-manifest", $DeveloperPayloadManifest,
    "--component-manifest", $ComponentManifest,
    "--dependency-report", $DependencyReport,
    "--privacy-report", $PrivacyReport,
    "--attribution-notice", $AttributionNotice,
    "--product-terms", $ProductTerms,
    "--output-dir", $ResolvedReleaseDir
)
& $Python.Source @PrepareArgs
if ($LASTEXITCODE -ne 0) {
    throw "Release manifest preparation failed."
}

Write-Host "Release bundle prepared for owner review: $ResolvedReleaseDir" -ForegroundColor Green
