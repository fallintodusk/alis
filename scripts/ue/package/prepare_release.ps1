#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ReleaseDir,
    [Parameter(Mandatory = $true)][string]$PublicSourceRoot,
    [Parameter(Mandatory = $true)][string]$PlayerPackageRoot,
    [Parameter(Mandatory = $true)][string]$PlayerEvidence,
    [Parameter(Mandatory = $true)][string]$DeveloperReleaseDir,
    [Parameter(Mandatory = $true)][string]$DeveloperPayloadManifest,
    [Parameter(Mandatory = $true)][string]$ComponentManifest,
    [Parameter(Mandatory = $true)][string]$DependencyReport,
    [Parameter(Mandatory = $true)][string]$PrivacyReport,
    [Parameter(Mandatory = $true)][string]$MapLoadReport,
    [Parameter(Mandatory = $true)][string]$AttributionNotice,
    [Parameter(Mandatory = $true)][string]$ProductTerms,
    [string]$ReleaseVersion = "2.0.0",
    [string]$ReleaseTag = "v2.0.0",
    [int]$SplitSizeMiB = 1900,
    [string]$SevenZip
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$Implementation = Join-Path $ScriptDir "prepare_release.py"
$WorkspaceTool = Join-Path $ScriptDir "release_workspace.py"
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

$ReleaseRoot = Join-Path $TmpRoot "release"
$WorkParent = Join-Path $ReleaseRoot "work"
$WorkRoot = Join-Path $WorkParent ("{0}-workspace-{1}" -f $ReleaseTag, [Guid]::NewGuid().ToString("N"))
$PreparedGitHubDir = Join-Path $WorkRoot "github"
$PreserveWorkRoot = $false

$ArchiveArgs = @(
    $Implementation, "archive-player",
    "--package-root", $PlayerPackageRoot,
    "--output-dir", $PreparedGitHubDir,
    "--release-version", $ReleaseVersion,
    "--split-size-mib", $SplitSizeMiB
)
if ($SevenZip) {
    $ArchiveArgs += @("--seven-zip", $SevenZip)
}
try {
    & $Python.Source @ArchiveArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Player archive preparation failed."
    }

    $ArchiveReport = Join-Path $PreparedGitHubDir "player-archive.json"
    $PrepareArgs = @(
        $Implementation, "prepare",
        "--release-version", $ReleaseVersion,
        "--release-tag", $ReleaseTag,
        "--private-source-root", $ProjectRoot,
        "--public-source-root", $PublicSourceRoot,
        "--player-package-root", $PlayerPackageRoot,
        "--player-evidence", $PlayerEvidence,
        "--player-archive-report", $ArchiveReport,
        "--developer-release-dir", $DeveloperReleaseDir,
        "--developer-payload-manifest", $DeveloperPayloadManifest,
        "--component-manifest", $ComponentManifest,
        "--dependency-report", $DependencyReport,
        "--privacy-report", $PrivacyReport,
        "--map-load-report", $MapLoadReport,
        "--attribution-notice", $AttributionNotice,
        "--product-terms", $ProductTerms,
        "--output-dir", $PreparedGitHubDir
    )
    & $Python.Source @PrepareArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Release manifest preparation failed."
    }

    & $Python.Source $WorkspaceTool initialize `
        --workspace-root $WorkRoot `
        --candidate-root $PlayerPackageRoot `
        --release-version $ReleaseVersion
    if ($LASTEXITCODE -ne 0) {
        throw "Release workspace initialization failed."
    }

    $PromotionAttempts = 6
    for ($Attempt = 1; $Attempt -le $PromotionAttempts; $Attempt++) {
        try {
            [IO.Directory]::Move($WorkRoot, $ResolvedReleaseDir)
            break
        }
        catch {
            $PromotionFailure = $_.Exception
            while ($PromotionFailure.InnerException) {
                $PromotionFailure = $PromotionFailure.InnerException
            }
            if (($PromotionFailure -isnot [IO.IOException]) -and
                ($PromotionFailure -isnot [UnauthorizedAccessException])) {
                throw
            }
            if ($Attempt -eq $PromotionAttempts) {
                $PreserveWorkRoot = $true
                throw "Release workspace promotion failed after $PromotionAttempts attempts. Prepared workspace preserved at: $WorkRoot"
            }
            Write-Warning "Release workspace promotion attempt $Attempt/$PromotionAttempts was blocked; retrying."
            Start-Sleep -Milliseconds (250 * $Attempt)
        }
    }
    & $Python.Source $WorkspaceTool adopt `
        --workspace-root $ResolvedReleaseDir `
        --candidate-root $PlayerPackageRoot
    if ($LASTEXITCODE -ne 0) {
        throw "Release workspace game adoption failed. Rerun the release command to resume it."
    }
}
finally {
    if ((-not $PreserveWorkRoot) -and (Test-Path -LiteralPath $WorkRoot)) {
        Remove-Item -LiteralPath $WorkRoot -Recurse -Force
    }
}

Write-Host "Release workspace prepared for owner review: $ResolvedReleaseDir" -ForegroundColor Green
