#Requires -Version 5.1
<#
.SYNOPSIS
    Verify an ALIS release directory using its bundled public key.

.DESCRIPTION
    Reads an explicit or bundled ALIS public key, falling back to the site key URL
    only for older releases. Verifies the expected fingerprint, checks the detached
    signature on SHA256SUMS.txt, and validates every manifest hash locally by
    default. RequiredAsset limits local hashing to an explicitly downloaded
    subset while retaining the same signed publisher authority.
#>

param(
    [string]$ReleaseDir,
    [string]$GpgPath,
    [string]$PublicKeyPath,
    [string]$BundledPublicKeyName = "ALIS_PUBLIC_KEY.asc",
    [string]$ManifestRelativePath = "SHA256SUMS.txt",
    [string]$SignatureRelativePath = "SHA256SUMS.txt.asc",
    [string]$PublicKeyUrl = "https://fall.is/assets/security/public-key.asc",
    [string]$ExpectedFingerprint = "3B9885F0C2D8D927C27FAB58F61A530034CFB5E7",
    [string]$TrustPageUrl = "https://fall.is/trust/",
    [string]$TempGpgHome,
    [string[]]$RequiredAsset,
    [string]$SummaryPath,
    [switch]$AllowRelativeAssetPaths,
    [switch]$RequireExactInventory,
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
        [string]$ResolvedGpgPath
    )

    $GpgDir = Split-Path -Parent $ResolvedGpgPath
    $PathEntries = $env:PATH -split ";"
    if (-not ($PathEntries | Where-Object { $_ -eq $GpgDir })) {
        $env:PATH = "$GpgDir;$env:PATH"
    }

    # Verification uses only public-key operations and gpgv. Do not launch an
    # agent or initialize the user's default GPG home.
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

function ConvertTo-GpgHomeArgument {
    param(
        [string]$ResolvedGpgPath,
        [string]$ResolvedGpgHome
    )

    if ($ResolvedGpgPath -match '(?i)[\\/]Git[\\/]usr[\\/]bin[\\/]gpg(?:\.exe)?$' -and $ResolvedGpgHome -match '^[A-Za-z]:[\\/]') {
        $Drive = $ResolvedGpgHome.Substring(0, 1).ToLowerInvariant()
        $Remainder = $ResolvedGpgHome.Substring(2).Replace('\', '/')
        return "/$Drive$Remainder"
    }

    return $ResolvedGpgHome
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
        [string]$ResolvedReleaseDir,
        [string]$BundledFileName,
        [string]$DownloadUrl,
        [string]$DownloadRoot
    )

    if ($RequestedPath) {
        if (-not (Test-Path $RequestedPath)) {
            throw "Public key file was not found: $RequestedPath"
        }

        return (Resolve-Path $RequestedPath).Path
    }

    $BundledPath = Join-Path $ResolvedReleaseDir $BundledFileName
    if (Test-Path $BundledPath) {
        return (Resolve-Path $BundledPath).Path
    }

    if ([string]::IsNullOrWhiteSpace($DownloadUrl)) {
        throw "No public key was provided or bundled, and PublicKeyUrl is empty."
    }

    $DownloadedPath = Join-Path $DownloadRoot "public-key.asc"
    Invoke-WebRequest $DownloadUrl -OutFile $DownloadedPath
    return $DownloadedPath
}

function Get-KeyFingerprint {
    param(
        [string]$ResolvedGpgPath,
        [string]$ResolvedPublicKeyPath,
        [string]$TempGpgHomeArgument
    )

    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $Output = & $ResolvedGpgPath --homedir $TempGpgHomeArgument --with-colons --show-keys $ResolvedPublicKeyPath 2>&1
        $GpgExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $PreviousErrorActionPreference
    }

    if ($GpgExitCode -ne 0) {
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
        [string]$KeyringPath,
        [string]$TempGpgHomeArgument
    )

    & $ResolvedGpgPath --homedir $TempGpgHomeArgument --dearmor --yes --output $KeyringPath $ResolvedPublicKeyPath
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

function Resolve-ManifestAssetPath {
    param(
        [string]$Directory,
        [string]$Name,
        [bool]$AllowRelativePaths
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        throw "Unsafe SHA256SUMS.txt asset name: $Name"
    }
    $Segments = @($Name -split "/", -1)
    if ($Name.Contains('\') -or $Segments.Count -eq 0 -or
        @($Segments | Where-Object { $_ -in @("", ".", "..") }).Count -gt 0) {
        throw "Unsafe SHA256SUMS.txt asset name: $Name"
    }
    if (-not $AllowRelativePaths -and [IO.Path]::GetFileName($Name) -cne $Name) {
        throw "Unsafe SHA256SUMS.txt asset name: $Name"
    }
    if ([IO.Path]::IsPathRooted($Name)) {
        throw "Unsafe SHA256SUMS.txt asset name: $Name"
    }
    $NormalizedName = $Name.Replace('/', [IO.Path]::DirectorySeparatorChar)
    $Candidate = [IO.Path]::GetFullPath((Join-Path $Directory $NormalizedName))
    $ResolvedRoot = [IO.Path]::GetFullPath($Directory).TrimEnd('\', '/')
    if (-not $Candidate.StartsWith(
            $ResolvedRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe SHA256SUMS.txt asset name: $Name"
    }
    if ($AllowRelativePaths) {
        if (-not (Test-Path -LiteralPath $Candidate -PathType Leaf)) {
            throw "Manifest entry does not resolve to a release asset: $Name"
        }
        return (Resolve-Path -LiteralPath $Candidate).Path
    }
    $FlatPath = Join-Path $Directory $Name
    if (Test-Path -LiteralPath $FlatPath -PathType Leaf) {
        return (Resolve-Path -LiteralPath $FlatPath).Path
    }
    $Matches = @(Get-ChildItem -LiteralPath $Directory -File -Recurse | Where-Object Name -CEQ $Name)
    if ($Matches.Count -ne 1) {
        throw "Manifest entry must resolve to exactly one release asset: $Name"
    }
    return $Matches[0].FullName
}

function Resolve-SafeReleaseRelativePath {
    param(
        [string]$Directory,
        [string]$RelativePath,
        [string]$Description
    )

    if ([string]::IsNullOrWhiteSpace($RelativePath) -or [IO.Path]::IsPathRooted($RelativePath)) {
        throw "$Description must be a safe relative path: $RelativePath"
    }
    $ResolvedRoot = [IO.Path]::GetFullPath($Directory).TrimEnd('\', '/')
    $Resolved = [IO.Path]::GetFullPath((Join-Path $ResolvedRoot $RelativePath))
    if (-not $Resolved.StartsWith(
            $ResolvedRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description escapes the release directory: $RelativePath"
    }
    return $Resolved
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ResolvedReleaseDir = Resolve-ReleaseDir -RequestedPath $ReleaseDir -ScriptDir $ScriptDir
$ResolvedGpgPath = Resolve-GpgPath -RequestedPath $GpgPath
Initialize-GpgEnvironment -ResolvedGpgPath $ResolvedGpgPath
$ResolvedGpgvPath = Resolve-GpgvPath -ResolvedGpgPath $ResolvedGpgPath
$ManifestPath = Resolve-SafeReleaseRelativePath -Directory $ResolvedReleaseDir -RelativePath $ManifestRelativePath -Description "ManifestRelativePath"
$SignaturePath = Resolve-SafeReleaseRelativePath -Directory $ResolvedReleaseDir -RelativePath $SignatureRelativePath -Description "SignatureRelativePath"

if (-not (Test-Path $ManifestPath)) {
    throw "Manifest was not found: $ManifestPath"
}

if (-not (Test-Path $SignaturePath)) {
    throw "Detached signature was not found: $SignaturePath"
}

$WorkingRoot = Join-Path $env:TEMP ("alis_verify_{0}" -f ([guid]::NewGuid().ToString("N")))
New-Item -ItemType Directory -Force -Path $WorkingRoot | Out-Null

if (-not $TempGpgHome) {
    $TempGpgHome = Join-Path $WorkingRoot "keyring"
}

New-Item -ItemType Directory -Force -Path $TempGpgHome | Out-Null
$ResolvedTempGpgHome = (Resolve-Path $TempGpgHome).Path
$TempGpgHomeArgument = ConvertTo-GpgHomeArgument -ResolvedGpgPath $ResolvedGpgPath -ResolvedGpgHome $ResolvedTempGpgHome

try {
    $ResolvedPublicKeyPath = Resolve-PublicKeyPath -RequestedPath $PublicKeyPath -ResolvedReleaseDir $ResolvedReleaseDir -BundledFileName $BundledPublicKeyName -DownloadUrl $PublicKeyUrl -DownloadRoot $WorkingRoot
    $ActualFingerprint = Get-NormalizedFingerprint -Fingerprint (Get-KeyFingerprint -ResolvedGpgPath $ResolvedGpgPath -ResolvedPublicKeyPath $ResolvedPublicKeyPath -TempGpgHomeArgument $TempGpgHomeArgument)
    $ExpectedNormalizedFingerprint = Get-NormalizedFingerprint -Fingerprint $ExpectedFingerprint

    if ($ActualFingerprint -ne $ExpectedNormalizedFingerprint) {
        throw "Public key fingerprint mismatch. Expected $ExpectedNormalizedFingerprint but found $ActualFingerprint."
    }

    $VerificationKeyringPath = Join-Path $ResolvedTempGpgHome "alis-public-keyring.gpg"
    Build-VerificationKeyring -ResolvedGpgPath $ResolvedGpgPath -ResolvedPublicKeyPath $ResolvedPublicKeyPath -KeyringPath $VerificationKeyringPath -TempGpgHomeArgument $TempGpgHomeArgument

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

    Push-Location $ResolvedTempGpgHome
    try {
        & $ResolvedGpgvPath --homedir $TempGpgHomeArgument --keyring alis-public-keyring.gpg $SignaturePath $ManifestPath
        if ($LASTEXITCODE -ne 0) {
            throw "gpgv failed while verifying SHA256SUMS.txt.asc."
        }
    }
    finally {
        Pop-Location
    }

    $ManifestEntries = Parse-Manifest -ManifestPath $ManifestPath
    if ($RequireExactInventory) {
        $ManifestRelative = $ManifestPath.Substring($ResolvedReleaseDir.TrimEnd('\', '/').Length + 1).Replace('\', '/')
        $SignatureRelative = $SignaturePath.Substring($ResolvedReleaseDir.TrimEnd('\', '/').Length + 1).Replace('\', '/')
        $ExpectedNames = @($ManifestEntries | ForEach-Object Name | Sort-Object -CaseSensitive)
        $ActualNames = @(Get-ChildItem -LiteralPath $ResolvedReleaseDir -File -Recurse |
            ForEach-Object { $_.FullName.Substring($ResolvedReleaseDir.TrimEnd('\', '/').Length + 1).Replace('\', '/') } |
            Where-Object { $_ -cne $ManifestRelative -and $_ -cne $SignatureRelative } |
            Sort-Object -CaseSensitive)
        $InventoryDifference = @(Compare-Object -ReferenceObject $ExpectedNames -DifferenceObject $ActualNames -CaseSensitive)
        if ($InventoryDifference.Count -gt 0) {
            throw "Signed release inventory mismatch: $($InventoryDifference.InputObject -join ', ')"
        }
    }
    $EntriesToVerify = $ManifestEntries
    if ($RequiredAsset.Count -gt 0) {
        $Requested = @{}
        $EntriesToVerify = @($RequiredAsset | ForEach-Object {
            $Name = [string]$_
            if ([string]::IsNullOrWhiteSpace($Name) -or
                (-not $AllowRelativeAssetPaths -and [IO.Path]::GetFileName($Name) -cne $Name)) {
                throw "Unsafe required release asset name: $Name"
            }
            if ($Requested.ContainsKey($Name)) {
                throw "Duplicate required release asset name: $Name"
            }
            $Requested[$Name] = $true
            $Matches = @($ManifestEntries | Where-Object Name -CEQ $Name)
            if ($Matches.Count -ne 1) {
                throw "Required release asset must appear exactly once in SHA256SUMS.txt: $Name"
            }
            $Matches[0]
        })
    }
    $VerifiedCount = 0

    foreach ($Entry in $EntriesToVerify) {
        $AssetPath = Resolve-ManifestAssetPath `
            -Directory $ResolvedReleaseDir `
            -Name $Entry.Name `
            -AllowRelativePaths $AllowRelativeAssetPaths

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

    if ($SummaryPath) {
        $ResolvedSummaryPath = [IO.Path]::GetFullPath($SummaryPath)
        $SummaryParent = Split-Path -Parent $ResolvedSummaryPath
        New-Item -ItemType Directory -Path $SummaryParent -Force | Out-Null
        $SummaryLines | Set-Content -Encoding Ascii $ResolvedSummaryPath
    }

    Write-Host "Verification completed successfully." -ForegroundColor Green
    Write-Host "Verified assets: $VerifiedCount"
    if ($SummaryPath) {
        Write-Host "Summary: $ResolvedSummaryPath"
    }
}
finally {
    if (-not $KeepTempKeyring) {
        Remove-Item $WorkingRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
