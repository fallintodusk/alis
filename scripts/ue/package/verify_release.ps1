#Requires -Version 5.1
<#
.SYNOPSIS
    Verify an ALIS release directory using the site-published public key.

.DESCRIPTION
    Downloads or reads the ALIS public key, verifies the published fingerprint,
    checks the detached signature on SHA256SUMS.txt, and then validates every
    hash listed in the manifest against the release assets in the directory.
#>

param(
    [string]$ReleaseDir,
    [string]$GpgPath,
    [string]$PublicKeyPath,
    [string]$PublicKeyUrl = "https://fall.is/assets/security/public-key.asc",
    [string]$ExpectedFingerprint = "3B9885F0C2D8D927C27FAB58F61A530034CFB5E7",
    [string]$TrustPageUrl = "https://fall.is/about/",
    [string]$TempGpgHome,
    [switch]$KeepTempKeyring
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

function Resolve-GpgvPath {
    param(
        [string]$ResolvedGpgPath
    )

    $GpgDir = Split-Path -Parent $ResolvedGpgPath
    $GpgvPath = Join-Path $GpgDir "gpgv.exe"
    if (-not (Test-Path $GpgvPath)) {
        throw "gpgv.exe was not found next to gpg.exe: $GpgvPath"
    }

    return $GpgvPath
}

function Resolve-ReleaseDir {
    param(
        [string]$RequestedPath,
        [string]$ScriptDir
    )

    if ($RequestedPath) {
        if (-not (Test-Path $RequestedPath)) {
            throw "Release directory was not found: $RequestedPath"
        }

        return (Resolve-Path $RequestedPath).Path
    }

    $CurrentDir = (Get-Location).Path
    if ((Test-Path (Join-Path $CurrentDir "SHA256SUMS.txt")) -and (Test-Path (Join-Path $CurrentDir "SHA256SUMS.txt.asc"))) {
        return $CurrentDir
    }

    if ($ScriptDir -and (Test-Path (Join-Path $ScriptDir "SHA256SUMS.txt")) -and (Test-Path (Join-Path $ScriptDir "SHA256SUMS.txt.asc"))) {
        return (Resolve-Path $ScriptDir).Path
    }

    throw "ReleaseDir is required unless the current directory or script directory already contains SHA256SUMS.txt and SHA256SUMS.txt.asc."
}

function Get-NormalizedFingerprint {
    param(
        [string]$Fingerprint
    )

    return ($Fingerprint -replace "[^0-9A-Fa-f]", "").ToUpperInvariant()
}

function Resolve-PublicKeyPath {
    param(
        [string]$RequestedPath,
        [string]$DownloadUrl,
        [string]$DownloadRoot
    )

    if ($RequestedPath) {
        if (-not (Test-Path $RequestedPath)) {
            throw "Public key file was not found: $RequestedPath"
        }

        return (Resolve-Path $RequestedPath).Path
    }

    $DownloadedPath = Join-Path $DownloadRoot "public-key.asc"
    Invoke-WebRequest $DownloadUrl -OutFile $DownloadedPath
    return $DownloadedPath
}

function Get-KeyFingerprint {
    param(
        [string]$ResolvedGpgPath,
        [string]$ResolvedPublicKeyPath
    )

    $Output = & $ResolvedGpgPath --with-colons --show-keys $ResolvedPublicKeyPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "gpg failed while reading the public key file: $ResolvedPublicKeyPath"
    }

    $FingerprintLine = $Output | Where-Object { $_ -like "fpr:*" } | Select-Object -First 1
    if (-not $FingerprintLine) {
        throw "No fingerprint was found in public key file: $ResolvedPublicKeyPath"
    }

    return ($FingerprintLine -split ":")[9]
}

function Build-VerificationKeyring {
    param(
        [string]$ResolvedGpgPath,
        [string]$ResolvedPublicKeyPath,
        [string]$KeyringPath
    )

    & $ResolvedGpgPath --dearmor --yes --output $KeyringPath $ResolvedPublicKeyPath
    if ($LASTEXITCODE -ne 0) {
        throw "gpg failed while building the verification keyring from $ResolvedPublicKeyPath."
    }
}

function Parse-Manifest {
    param(
        [string]$ManifestPath
    )

    $Entries = @()

    foreach ($Line in Get-Content $ManifestPath) {
        if ([string]::IsNullOrWhiteSpace($Line)) {
            continue
        }

        if ($Line -notmatch "^(?<hash>[0-9A-Fa-f]{64}) \*(?<name>.+)$") {
            throw "Unsupported SHA256SUMS.txt line format: $Line"
        }

        $Entries += [PSCustomObject]@{
            Hash = $Matches["hash"].ToLowerInvariant()
            Name = $Matches["name"]
        }
    }

    return @($Entries)
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ResolvedReleaseDir = Resolve-ReleaseDir -RequestedPath $ReleaseDir -ScriptDir $ScriptDir
$ResolvedGpgPath = Resolve-GpgPath -RequestedPath $GpgPath
Initialize-GpgEnvironment -ResolvedGpgPath $ResolvedGpgPath
$ResolvedGpgvPath = Resolve-GpgvPath -ResolvedGpgPath $ResolvedGpgPath
$SummaryPath = Join-Path $ResolvedReleaseDir "verify_release_summary.txt"
$ManifestPath = Join-Path $ResolvedReleaseDir "SHA256SUMS.txt"
$SignaturePath = Join-Path $ResolvedReleaseDir "SHA256SUMS.txt.asc"

if (-not (Test-Path $ManifestPath)) {
    throw "Manifest was not found: $ManifestPath"
}

if (-not (Test-Path $SignaturePath)) {
    throw "Detached signature was not found: $SignaturePath"
}

$WorkingRoot = Join-Path $env:TEMP ("alis_verify_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force -Path $WorkingRoot | Out-Null

if (-not $TempGpgHome) {
    $TempGpgHome = Join-Path $WorkingRoot "keyring"
}

New-Item -ItemType Directory -Force -Path $TempGpgHome | Out-Null

$ResolvedPublicKeyPath = Resolve-PublicKeyPath -RequestedPath $PublicKeyPath -DownloadUrl $PublicKeyUrl -DownloadRoot $WorkingRoot
$ActualFingerprint = Get-NormalizedFingerprint -Fingerprint (Get-KeyFingerprint -ResolvedGpgPath $ResolvedGpgPath -ResolvedPublicKeyPath $ResolvedPublicKeyPath)
$ExpectedNormalizedFingerprint = Get-NormalizedFingerprint -Fingerprint $ExpectedFingerprint

if ($ActualFingerprint -ne $ExpectedNormalizedFingerprint) {
    throw "Public key fingerprint mismatch. Expected $ExpectedNormalizedFingerprint but found $ActualFingerprint."
}

$VerificationKeyringPath = Join-Path $TempGpgHome "alis-public-keyring.gpg"
Build-VerificationKeyring -ResolvedGpgPath $ResolvedGpgPath -ResolvedPublicKeyPath $ResolvedPublicKeyPath -KeyringPath $VerificationKeyringPath

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " ALIS Release Verification" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "RELEASE_DIR   = $ResolvedReleaseDir"
Write-Host "GPG_PATH      = $ResolvedGpgPath"
Write-Host "GPGV_PATH     = $ResolvedGpgvPath"
Write-Host "PUBLIC_KEY    = $ResolvedPublicKeyPath"
Write-Host "KEYRING       = $VerificationKeyringPath"
Write-Host "TRUST_PAGE    = $TrustPageUrl"
Write-Host "FINGERPRINT   = $ActualFingerprint"
Write-Host ""

$PreviousGnuPgHome = $env:GNUPGHOME
$env:GNUPGHOME = $TempGpgHome
Push-Location $TempGpgHome
try {
    & $ResolvedGpgvPath --keyring alis-public-keyring.gpg $SignaturePath $ManifestPath
    if ($LASTEXITCODE -ne 0) {
        throw "gpgv failed while verifying SHA256SUMS.txt.asc."
    }
}
finally {
    Pop-Location
    $env:GNUPGHOME = $PreviousGnuPgHome
}

$ManifestEntries = Parse-Manifest -ManifestPath $ManifestPath
$VerifiedCount = 0

foreach ($Entry in $ManifestEntries) {
    $AssetPath = Join-Path $ResolvedReleaseDir $Entry.Name
    if (-not (Test-Path $AssetPath)) {
        throw "Manifest entry was not found in release directory: $($Entry.Name)"
    }

    $ActualHash = (Get-FileHash $AssetPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($ActualHash -ne $Entry.Hash) {
        throw "Hash mismatch for $($Entry.Name). Expected $($Entry.Hash), got $ActualHash."
    }

    $VerifiedCount += 1
}

$SummaryLines = @(
    "ALIS Release Verification Summary",
    "ReleaseDir=$ResolvedReleaseDir",
    "Manifest=$ManifestPath",
    "Signature=$SignaturePath",
    "PublicKey=$ResolvedPublicKeyPath",
    "VerificationKeyring=$VerificationKeyringPath",
    "TrustPageUrl=$TrustPageUrl",
    "Fingerprint=$ActualFingerprint",
    "VerifiedAssets=$VerifiedCount"
)

foreach ($Entry in $ManifestEntries) {
    $SummaryLines += "VerifiedAsset=$($Entry.Name)"
}

$SummaryLines | Set-Content -Encoding Ascii $SummaryPath

Write-Host "Verification completed successfully." -ForegroundColor Green
Write-Host "Verified assets: $VerifiedCount"
Write-Host "Summary: $SummaryPath"

if (-not $KeepTempKeyring) {
    Remove-Item $WorkingRoot -Recurse -Force -ErrorAction SilentlyContinue
}
