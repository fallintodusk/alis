#Requires -Version 5.1
<#
.SYNOPSIS
    Generate SHA256SUMS.txt and SHA256SUMS.txt.asc for an ALIS release output.

.DESCRIPTION
    Hashes release assets in a packaged ALIS release directory, signs the
    resulting SHA256SUMS.txt with the ALIS site trust key, exports the matching
    public key into the release, and verifies the detached signature by default.
#>

param(
    [string]$ReleaseDir,
    [string]$GpgPath,
    [string]$GpgHome,
    [string]$SigningKeyFingerprint = "3B9885F0C2D8D927C27FAB58F61A530034CFB5E7",
    [string]$TrustPageUrl = "https://fall.is/trust/",
    [string]$PublicKeyUrl = "https://fall.is/assets/security/public-key.asc",
    [string]$SummaryPath,
    [switch]$SkipVerify
)

$ErrorActionPreference = "Stop"
$CanonicalSigningKeyFingerprint = "3B9885F0C2D8D927C27FAB58F61A530034CFB5E7"

$NormalizedRequestedFingerprint = ($SigningKeyFingerprint -replace "[^0-9A-Fa-f]", "").ToUpperInvariant()
if (-not $GpgHome -and $NormalizedRequestedFingerprint -ne $CanonicalSigningKeyFingerprint) {
    throw "A non-canonical signing fingerprint requires -GpgHome. Throwaway or replacement-key tests must never use the user's default GPG home."
}

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
        "C:\Program Files\Git\usr\bin\gpg.exe",
        "C:\Program Files\Git\mingw64\bin\gpg.exe"
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
        [string]$ResolvedGpgPath,
        [string]$GpgHomeArgument
    )

    $GpgDir = Split-Path -Parent $ResolvedGpgPath
    $PathEntries = $env:PATH -split ";"
    if (-not ($PathEntries | Where-Object { $_ -eq $GpgDir })) {
        $env:PATH = "$GpgDir;$env:PATH"
    }

    $GpgConfPath = Join-Path $GpgDir "gpgconf.exe"
    if (Test-Path $GpgConfPath) {
        $GpgConfArgs = @()
        if ($GpgHomeArgument) {
            $GpgConfArgs += @("--homedir", $GpgHomeArgument)
        }
        $GpgConfArgs += @("--launch", "gpg-agent")

        & $GpgConfPath @GpgConfArgs | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "gpgconf failed while launching gpg-agent."
        }
    }
}

function Resolve-GpgHome {
    param(
        [string]$RequestedPath
    )

    if (-not $RequestedPath) {
        return $null
    }

    if (-not (Test-Path -LiteralPath $RequestedPath -PathType Container)) {
        throw "GPG home does not exist: $RequestedPath. The calling operation must create and own an isolated keyring before signing."
    }

    $ResolvedPath = (Resolve-Path -LiteralPath $RequestedPath).Path
    return $ResolvedPath
}

function ConvertFrom-GpgDirectoryPath {
    param(
        [string]$DirectoryPath
    )

    if ([string]::IsNullOrWhiteSpace($DirectoryPath)) {
        return $null
    }

    $DecodedPath = [Uri]::UnescapeDataString($DirectoryPath.Trim())
    if ($DecodedPath -match '^/([A-Za-z])/(.*)$') {
        return "{0}:\{1}" -f $Matches[1].ToUpperInvariant(), $Matches[2].Replace('/', '\')
    }

    return $DecodedPath
}

function Get-NormalizedPathForComparison {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    $ExpandedPath = [Environment]::ExpandEnvironmentVariables($Path)
    try {
        $FullPath = [System.IO.Path]::GetFullPath($ExpandedPath)
    }
    catch {
        $FullPath = $ExpandedPath
    }

    return $FullPath.TrimEnd([char[]]@('\', '/'))
}

function Get-DefaultGpgHomeCandidates {
    param(
        [string]$ResolvedGpgPath
    )

    $Candidates = @(
        $env:GNUPGHOME,
        $(if ($env:USERPROFILE) { Join-Path $env:USERPROFILE ".gnupg" }),
        $(if ($env:APPDATA) { Join-Path $env:APPDATA "gnupg" }),
        $env:USERPROFILE
    )

    $GpgConfPath = Join-Path (Split-Path -Parent $ResolvedGpgPath) "gpgconf.exe"
    if (Test-Path -LiteralPath $GpgConfPath) {
        $ReportedHome = & $GpgConfPath --list-dirs homedir 2>$null
        if ($LASTEXITCODE -eq 0 -and $ReportedHome) {
            $Candidates += ConvertFrom-GpgDirectoryPath -DirectoryPath ($ReportedHome | Select-Object -First 1)
        }
    }

    return @($Candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object {
        Get-NormalizedPathForComparison -Path $_
    } | Select-Object -Unique)
}

function Assert-IsolatedGpgHome {
    param(
        [string]$ResolvedGpgPath,
        [string]$ResolvedGpgHome
    )

    if (-not $ResolvedGpgHome) {
        return
    }

    $CandidatePath = Get-NormalizedPathForComparison -Path $ResolvedGpgHome
    foreach ($DefaultPath in Get-DefaultGpgHomeCandidates -ResolvedGpgPath $ResolvedGpgPath) {
        if ([string]::Equals($CandidatePath, $DefaultPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "-GpgHome must not point to the default user GPG home or user profile: $ResolvedGpgHome"
        }
    }
}

function ConvertTo-GpgHomeArgument {
    param(
        [string]$ResolvedGpgPath,
        [string]$ResolvedGpgHome
    )

    if (-not $ResolvedGpgHome) {
        return $null
    }

    if ($ResolvedGpgPath -match '(?i)[\\/]Git[\\/]usr[\\/]bin[\\/]gpg(?:\.exe)?$' -and $ResolvedGpgHome -match '^[A-Za-z]:[\\/]') {
        $Drive = $ResolvedGpgHome.Substring(0, 1).ToLowerInvariant()
        $Remainder = $ResolvedGpgHome.Substring(2).Replace('\', '/')
        return "/$Drive$Remainder"
    }

    return $ResolvedGpgHome
}

function Export-ReleasePublicKey {
    param(
        [string]$ResolvedGpgPath,
        [string]$GpgHomeArgument,
        [string]$Fingerprint,
        [string]$TargetPath
    )

    $ExportArgs = @()
    if ($GpgHomeArgument) {
        $ExportArgs += @("--homedir", $GpgHomeArgument)
    }
    $ExportArgs += @("--batch", "--yes", "--armor", "--output", $TargetPath, "--export", $Fingerprint)

    & $ResolvedGpgPath @ExportArgs
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $TargetPath)) {
        throw "gpg failed while exporting public key $Fingerprint to $TargetPath."
    }

    $InspectArgs = @()
    if ($GpgHomeArgument) {
        $InspectArgs += @("--homedir", $GpgHomeArgument)
    }
    $InspectArgs += @("--with-colons", "--show-keys", $TargetPath)

    $Output = & $ResolvedGpgPath @InspectArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "gpg failed while inspecting exported public key: $TargetPath"
    }

    $FingerprintLine = $Output | Where-Object { $_ -like "fpr:*" } | Select-Object -First 1
    if (-not $FingerprintLine) {
        throw "No fingerprint was found in exported public key: $TargetPath"
    }

    $ActualFingerprint = (($FingerprintLine -split ":")[9] -replace "[^0-9A-Fa-f]", "").ToUpperInvariant()
    $ExpectedFingerprint = ($Fingerprint -replace "[^0-9A-Fa-f]", "").ToUpperInvariant()
    if ($ActualFingerprint -ne $ExpectedFingerprint) {
        throw "Exported public key fingerprint mismatch. Expected $ExpectedFingerprint but found $ActualFingerprint."
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

    # The manifest and detached signature are protocol envelopes. The manifest
    # hashes release payload assets, but it must never hash itself or its signature.
    $ExcludedPatterns = @(
        "package_summary.txt",
        "sign_release_summary.txt",
        "verify_release_summary.txt",
        "SHA256SUMS.txt",
        "SHA256SUMS.txt.asc"
    )

    $Assets = Get-ChildItem $Directory -File -Recurse | Where-Object {
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

function Assert-SecretKeyAvailable {
    param(
        [string]$ResolvedGpgPath,
        [string]$GpgHomeArgument,
        [string]$Fingerprint
    )

    $ListArgs = @()
    if ($GpgHomeArgument) {
        $ListArgs += @("--homedir", $GpgHomeArgument)
    }
    $ListArgs += @("--list-secret-keys", "--with-colons", $Fingerprint)

    $Output = & $ResolvedGpgPath @ListArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "gpg failed while checking for secret key $Fingerprint."
    }

    if (-not ($Output | Where-Object { $_ -like "sec:*" })) {
        throw "No secret key for fingerprint $Fingerprint is available in GPG."
    }
}

function Assert-ReleaseReadyForSignature {
    param(
        [string]$Directory,
        [string]$ProjectRoot
    )

    $Verifier = Join-Path $ProjectRoot "scripts\ue\package\prepare_release.py"
    if (-not (Test-Path -LiteralPath $Verifier -PathType Leaf)) {
        throw "Release readiness verifier is missing: $Verifier"
    }

    $Python = Get-Command "python" -ErrorAction SilentlyContinue
    if (-not $Python) {
        throw "Python is required to verify release readiness before signing."
    }

    & $Python.Source $Verifier verify --release-dir $Directory --require-ready
    if ($LASTEXITCODE -ne 0) {
        throw "Release directory is not ready for signature. Complete prepare_release.py approval first."
    }
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$ResolvedReleaseDir = Resolve-ReleaseDir -RequestedPath $ReleaseDir -ProjectRoot $ProjectRoot
Assert-ReleaseReadyForSignature -Directory $ResolvedReleaseDir -ProjectRoot $ProjectRoot
$ResolvedGpgPath = Resolve-GpgPath -RequestedPath $GpgPath
$ResolvedGpgHome = Resolve-GpgHome -RequestedPath $GpgHome
Assert-IsolatedGpgHome -ResolvedGpgPath $ResolvedGpgPath -ResolvedGpgHome $ResolvedGpgHome
$GpgHomeArgument = ConvertTo-GpgHomeArgument -ResolvedGpgPath $ResolvedGpgPath -ResolvedGpgHome $ResolvedGpgHome
Initialize-GpgEnvironment -ResolvedGpgPath $ResolvedGpgPath -GpgHomeArgument $GpgHomeArgument
$PublicKeyAssetName = "ALIS_PUBLIC_KEY.asc"
$PublicKeyAssetPath = Join-Path $ResolvedReleaseDir $PublicKeyAssetName
Remove-Item (Join-Path $ResolvedReleaseDir "VERIFY_RELEASE.ps1"), (Join-Path $ResolvedReleaseDir "VERIFY_RELEASE.bat") -Force -ErrorAction SilentlyContinue
Remove-Item $PublicKeyAssetPath -Force -ErrorAction SilentlyContinue

Assert-SecretKeyAvailable -ResolvedGpgPath $ResolvedGpgPath -GpgHomeArgument $GpgHomeArgument -Fingerprint $SigningKeyFingerprint
Export-ReleasePublicKey -ResolvedGpgPath $ResolvedGpgPath -GpgHomeArgument $GpgHomeArgument -Fingerprint $SigningKeyFingerprint -TargetPath $PublicKeyAssetPath

Write-ReleaseVerifyHelpers -Directory $ResolvedReleaseDir -ProjectRoot $ProjectRoot

$Assets = Get-ReleaseAssets -Directory $ResolvedReleaseDir

if ($Assets.Count -eq 0) {
    throw "No release assets were found in $ResolvedReleaseDir."
}
$DuplicateAssetNames = @($Assets | Group-Object Name | Where-Object Count -gt 1)
if ($DuplicateAssetNames.Count -gt 0) {
    throw "Release assets must have unique public file names: $($DuplicateAssetNames.Name -join ', ')"
}

$ManifestPath = Join-Path $ResolvedReleaseDir "SHA256SUMS.txt"
$SignaturePath = Join-Path $ResolvedReleaseDir "SHA256SUMS.txt.asc"
Remove-Item $ManifestPath, $SignaturePath -Force -ErrorAction SilentlyContinue

$HashLines = foreach ($Asset in $Assets) {
    $Hash = (Get-FileHash $Asset.FullName -Algorithm SHA256).Hash.ToLower()
    "{0} *{1}" -f $Hash, $Asset.Name
}

$HashLines | Set-Content -Encoding Ascii $ManifestPath

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " ALIS Release Signing" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "RELEASE_DIR   = $ResolvedReleaseDir"
Write-Host "GPG_PATH      = $ResolvedGpgPath"
Write-Host "GPG_HOME      = $(if ($ResolvedGpgHome) { $ResolvedGpgHome } else { '<default>' })"
Write-Host "FINGERPRINT   = $SigningKeyFingerprint"
Write-Host "TRUST_PAGE    = $TrustPageUrl"
Write-Host "PUBLIC_KEY    = $PublicKeyAssetPath"
Write-Host "KEY_MIRROR    = $PublicKeyUrl"
Write-Host "VERIFY_AFTER  = $(-not $SkipVerify)"
Write-Host ""

$SignArgs = @()
if ($GpgHomeArgument) {
    $SignArgs += @("--homedir", $GpgHomeArgument)
}
$SignArgs += @("--yes", "--armor", "--detach-sign", "--local-user", $SigningKeyFingerprint, $ManifestPath)

& $ResolvedGpgPath @SignArgs
if ($LASTEXITCODE -ne 0) {
    throw "gpg failed while creating detached signature for $ManifestPath."
}

$Verified = $false
if (-not $SkipVerify) {
    $VerifyArgs = @()
    if ($GpgHomeArgument) {
        $VerifyArgs += @("--homedir", $GpgHomeArgument)
    }
    $VerifyArgs += @("--verify", $SignaturePath, $ManifestPath)

    & $ResolvedGpgPath @VerifyArgs
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
    "GpgHome=$(if ($ResolvedGpgHome) { $ResolvedGpgHome } else { '<default>' })",
    "SigningKeyFingerprint=$SigningKeyFingerprint",
    "TrustPageUrl=$TrustPageUrl",
    "PublicKeyAsset=$PublicKeyAssetPath",
    "PublicKeyUrl=$PublicKeyUrl",
    "Verified=$Verified",
    "AssetCount=$($Assets.Count)"
)

foreach ($Asset in $Assets) {
    $SummaryLines += "Asset=$($Asset.Name)"
}

if ($SummaryPath) {
    $ResolvedSummaryPath = [IO.Path]::GetFullPath($SummaryPath)
    $SummaryParent = Split-Path -Parent $ResolvedSummaryPath
    New-Item -ItemType Directory -Path $SummaryParent -Force | Out-Null
    $SummaryLines | Set-Content -Encoding Ascii $ResolvedSummaryPath
}

Write-Host "Signing completed successfully." -ForegroundColor Green
Write-Host "Manifest:  $ManifestPath"
Write-Host "Signature: $SignaturePath"
if ($SummaryPath) {
    Write-Host "Summary:   $ResolvedSummaryPath"
}
