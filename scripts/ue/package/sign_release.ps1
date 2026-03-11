#Requires -Version 5.1
<#
.SYNOPSIS
    Generate SHA256SUMS.txt and SHA256SUMS.txt.asc for an ALIS release output.

.DESCRIPTION
    Hashes root-level release assets in a packaged ALIS release directory, signs the
    resulting SHA256SUMS.txt with the ALIS site trust key, and verifies the
    detached signature by default.
#>

param(
    [string]$ReleaseDir,
    [string]$GpgPath,
    [string]$SigningKeyFingerprint = "3B9885F0C2D8D927C27FAB58F61A530034CFB5E7",
    [string]$TrustPageUrl = "https://fall.is/about/",
    [string]$PublicKeyUrl = "https://fall.is/assets/security/public-key.asc",
    [switch]$SkipVerify
)

$ErrorActionPreference = "Stop"

function Resolve-GpgPath {
    param(
        [string]$RequestedPath
    )

    if ($RequestedPath) {
        if (-not (Test-Path $RequestedPath)) {
            throw "GPG executable was not found: $RequestedPath"
        }

        return (Resolve-Path $RequestedPath).Path
    }

    $Command = Get-Command "gpg.exe" -ErrorAction SilentlyContinue
    if (-not $Command) {
        $Command = Get-Command "gpg" -ErrorAction SilentlyContinue
    }

    if ($Command) {
        return $Command.Source
    }

    $Candidates = @(
        "C:\Program Files\GnuPG\bin\gpg.exe",
        "C:\Program Files (x86)\GnuPG\bin\gpg.exe",
        "gpg",
        "gpg"
    )

    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) {
            return $Candidate
        }
    }

    throw "gpg.exe was not found. Install GnuPG or Git for Windows, or pass -GpgPath."
}

function Initialize-GpgEnvironment {
    param(
        [string]$ResolvedGpgPath
    )

    $GpgDir = Split-Path -Parent $ResolvedGpgPath
    $PathEntries = $env:PATH -split ";"
    if (-not ($PathEntries | Where-Object { $_ -eq $GpgDir })) {
        $env:PATH = "$GpgDir;$env:PATH"
    }

    $GpgConfPath = Join-Path $GpgDir "gpgconf.exe"
    if (Test-Path $GpgConfPath) {
        & $GpgConfPath --launch gpg-agent | Out-Null
    }
}

function Resolve-ReleaseDir {
    param(
        [string]$RequestedPath,
        [string]$ProjectRoot
    )

    if ($RequestedPath) {
        if (-not (Test-Path $RequestedPath)) {
            throw "Release directory was not found: $RequestedPath"
        }

        return (Resolve-Path $RequestedPath).Path
    }

    $CurrentDir = (Get-Location).Path
    if (Test-Path (Join-Path $CurrentDir "package_summary.txt")) {
        return $CurrentDir
    }

    $SavedReleaseRoot = Join-Path $ProjectRoot "Saved\PackageRelease"
    if (Test-Path $SavedReleaseRoot) {
        $LatestRelease = Get-ChildItem $SavedReleaseRoot -Directory |
            Where-Object { Test-Path (Join-Path $_.FullName "package_summary.txt") } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1

        if ($LatestRelease) {
            return $LatestRelease.FullName
        }
    }

    throw "ReleaseDir was not provided and no packaged release output was found."
}

function Get-ReleaseAssets {
    param(
        [string]$Directory
    )

    $ExcludedPatterns = @(
        "package_summary.txt",
        "sign_release_summary.txt",
        "verify_release_summary.txt",
        "SHA256SUMS.txt",
        "SHA256SUMS.txt.asc"
    )

    $Assets = Get-ChildItem $Directory -File | Where-Object {
        $IsExcluded = $false

        foreach ($Pattern in $ExcludedPatterns) {
            if ($_.Name -like $Pattern) {
                $IsExcluded = $true
                break
            }
        }

        return -not $IsExcluded
    } | Sort-Object Name

    return @($Assets)
}

function Write-ReleaseVerifyHelpers {
    param(
        [string]$Directory,
        [string]$ProjectRoot
    )

    $SourcePs1 = Join-Path $ProjectRoot "scripts\ue\package\verify_release.ps1"
    $TargetPs1 = Join-Path $Directory "VERIFY_RELEASE.ps1"
    Copy-Item $SourcePs1 $TargetPs1 -Force

    $TargetBat = Join-Path $Directory "VERIFY_RELEASE.bat"
    @(
        "@echo off",
        "setlocal",
        "",
        "powershell -ExecutionPolicy Bypass -File ""%~dp0VERIFY_RELEASE.ps1"" %*",
        "exit /b %ERRORLEVEL%"
    ) | Set-Content -Encoding Ascii $TargetBat
}

function Write-ReleaseReadme {
    param(
        [string]$Directory,
        [System.IO.FileInfo[]]$Assets,
        [string]$TrustPageUrl,
        [string]$PublicKeyUrl,
        [string]$Fingerprint
    )

    $ReadmePath = Join-Path $Directory "INSTALL.txt"
    $ArchiveAssets = @($Assets | Where-Object { $_.Name -like "*.zip*" -or $_.Name -like "*.7z*" })

    $Lines = @(
        "ALIS Install Guide",
        "",
        "Fast install:",
        "1. Download all archive parts to one folder.",
        "2. Install 7-Zip if needed:",
        "   - Website: https://www.7-zip.org/",
        "   - Windows package manager: winget install --id 7zip.7zip -e",
        "3. Right-click the first archive part and extract it with 7-Zip.",
        "4. Run Alis.exe.",
        "",
        "Normal users do not need to verify signatures before install.",
        "Advanced users can verify authenticity before extraction.",
        "",
        "Archive parts in this release:"
    )

    foreach ($Asset in $ArchiveAssets) {
        $Lines += "- $($Asset.Name)"
    }

    $Lines += @(
        "",
        "Trust source of truth:",
        "Trust page: $TrustPageUrl",
        "Public key: $PublicKeyUrl",
        "Fingerprint: $Fingerprint",
        "",
        "Fast advanced path on Windows:",
        ".\VERIFY_RELEASE.bat",
        "",
        "What VERIFY_RELEASE.bat does:",
        "- checks the ALIS public key fingerprint",
        "- verifies SHA256SUMS.txt.asc",
        "- verifies the hashes of all release assets in this folder",
        "",
        "Advanced verify:",
        "1. Download SHA256SUMS.txt and SHA256SUMS.txt.asc from this release.",
        "2. Download the ALIS public key.",
        "3. Verify the detached signature.",
        "4. Verify file hashes from SHA256SUMS.txt.",
        "",
        "If gpg is not on PATH, use the bundled verifier or call gpg by full path.",
        "",
        "Manual PowerShell + GPG quick path:",
        "Invoke-WebRequest $PublicKeyUrl -OutFile .\public-key.asc",
        "gpg --import .\public-key.asc",
        "gpg --verify .\SHA256SUMS.txt.asc .\SHA256SUMS.txt",
        "",
        "PowerShell hash check:",
        "Get-Content .\SHA256SUMS.txt | ForEach-Object {",
        "  if (`$_ -match '^(?<hash>[0-9a-f]{64}) \*(?<name>.+)$') {",
        "    `$actual = (Get-FileHash `$Matches.name -Algorithm SHA256).Hash.ToLower()",
        "    if (`$actual -eq `$Matches.hash) { ""[OK] `$(`$Matches.name)"" } else { ""[FAIL] `$(`$Matches.name)"" }",
        "  }",
        "}",
        "",
        "If the signature is good and every hash is [OK], the release files are authentic.",
        "",
        "Important for split archives:",
        "- keep all parts in the same folder",
        "- extract the first part only",
        "- do not try to open part 2 by itself",
        "",
        "Bundled helper files in this release:",
        "- INSTALL.txt",
        "- VERIFY_RELEASE.ps1",
        "- VERIFY_RELEASE.bat"
    )

    $Lines | Set-Content -Encoding Ascii $ReadmePath
    return $ReadmePath
}

function Assert-SecretKeyAvailable {
    param(
        [string]$ResolvedGpgPath,
        [string]$Fingerprint
    )

    $Output = & $ResolvedGpgPath --list-secret-keys --with-colons $Fingerprint 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "gpg failed while checking for secret key $Fingerprint."
    }

    if (-not ($Output | Where-Object { $_ -like "sec:*" })) {
        throw "No secret key for fingerprint $Fingerprint is available in GPG."
    }
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$ResolvedReleaseDir = Resolve-ReleaseDir -RequestedPath $ReleaseDir -ProjectRoot $ProjectRoot
$ResolvedGpgPath = Resolve-GpgPath -RequestedPath $GpgPath
Initialize-GpgEnvironment -ResolvedGpgPath $ResolvedGpgPath
$ReadmePath = Join-Path $ResolvedReleaseDir "INSTALL.txt"
Remove-Item $ReadmePath -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $ResolvedReleaseDir "README_RELEASE.txt") -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $ResolvedReleaseDir "VERIFY_RELEASE.ps1"), (Join-Path $ResolvedReleaseDir "VERIFY_RELEASE.bat") -Force -ErrorAction SilentlyContinue

Write-ReleaseVerifyHelpers -Directory $ResolvedReleaseDir -ProjectRoot $ProjectRoot

$PreReadmeAssets = Get-ReleaseAssets -Directory $ResolvedReleaseDir
[void](Write-ReleaseReadme -Directory $ResolvedReleaseDir -Assets $PreReadmeAssets -TrustPageUrl $TrustPageUrl -PublicKeyUrl $PublicKeyUrl -Fingerprint $SigningKeyFingerprint)
$Assets = Get-ReleaseAssets -Directory $ResolvedReleaseDir

if ($Assets.Count -eq 0) {
    throw "No root-level release assets were found in $ResolvedReleaseDir. Package with -CreateReleaseArchive first, or place release assets in the release root before signing."
}

$ManifestPath = Join-Path $ResolvedReleaseDir "SHA256SUMS.txt"
$SignaturePath = Join-Path $ResolvedReleaseDir "SHA256SUMS.txt.asc"
$SummaryPath = Join-Path $ResolvedReleaseDir "sign_release_summary.txt"

Remove-Item $ManifestPath, $SignaturePath -Force -ErrorAction SilentlyContinue

$HashLines = foreach ($Asset in $Assets) {
    $Hash = (Get-FileHash $Asset.FullName -Algorithm SHA256).Hash.ToLower()
    "{0} *{1}" -f $Hash, $Asset.Name
}

$HashLines | Set-Content -Encoding Ascii $ManifestPath

Assert-SecretKeyAvailable -ResolvedGpgPath $ResolvedGpgPath -Fingerprint $SigningKeyFingerprint

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " ALIS Release Signing" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "RELEASE_DIR   = $ResolvedReleaseDir"
Write-Host "GPG_PATH      = $ResolvedGpgPath"
Write-Host "FINGERPRINT   = $SigningKeyFingerprint"
Write-Host "TRUST_PAGE    = $TrustPageUrl"
Write-Host "PUBLIC_KEY    = $PublicKeyUrl"
Write-Host "VERIFY_AFTER  = $(-not $SkipVerify)"
Write-Host ""

& $ResolvedGpgPath --yes --armor --detach-sign --local-user $SigningKeyFingerprint $ManifestPath
if ($LASTEXITCODE -ne 0) {
    throw "gpg failed while creating detached signature for $ManifestPath."
}

$Verified = $false
if (-not $SkipVerify) {
    & $ResolvedGpgPath --verify $SignaturePath $ManifestPath
    if ($LASTEXITCODE -ne 0) {
        throw "gpg failed while verifying $SignaturePath."
    }

    $Verified = $true
}

$SummaryLines = @(
    "ALIS Release Signing Summary",
    "ReleaseDir=$ResolvedReleaseDir",
    "Manifest=$ManifestPath",
    "Signature=$SignaturePath",
    "GpgPath=$ResolvedGpgPath",
    "SigningKeyFingerprint=$SigningKeyFingerprint",
    "TrustPageUrl=$TrustPageUrl",
    "PublicKeyUrl=$PublicKeyUrl",
    "Verified=$Verified",
    "AssetCount=$($Assets.Count)"
)

foreach ($Asset in $Assets) {
    $SummaryLines += "Asset=$($Asset.Name)"
}

$SummaryLines | Set-Content -Encoding Ascii $SummaryPath

Write-Host "Signing completed successfully." -ForegroundColor Green
Write-Host "Manifest:  $ManifestPath"
Write-Host "Signature: $SignaturePath"
Write-Host "Summary:   $SummaryPath"
