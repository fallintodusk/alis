#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = "Stop"

function Resolve-TestGpg {
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
        if (Test-Path -LiteralPath $Candidate) {
            return $Candidate
        }
    }
    throw "gpg was not found"
}

function ConvertTo-GpgHomeArgument {
    param(
        [string]$GpgPath,
        [string]$GpgHome
    )

    if ($GpgPath -match '(?i)[\\/]Git[\\/]usr[\\/]bin[\\/]gpg(?:\.exe)?$' -and $GpgHome -match '^[A-Za-z]:[\\/]') {
        $Drive = $GpgHome.Substring(0, 1).ToLowerInvariant()
        $Remainder = $GpgHome.Substring(2).Replace('\', '/')
        return "/$Drive$Remainder"
    }
    return $GpgHome
}

function Assert-CommandSucceeded {
    param(
        [string]$Action
    )

    if ($LASTEXITCODE -ne 0) {
        throw "$Action failed with exit code $LASTEXITCODE"
    }
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackageDir = Split-Path -Parent $ScriptDir
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PackageDir))
$SignScript = Join-Path $PackageDir "sign_release.ps1"
$VerifyScript = Join-Path $PackageDir "verify_release.ps1"
$GpgPath = Resolve-TestGpg
$GpgDir = Split-Path -Parent $GpgPath
$GpgConfPath = Join-Path $GpgDir "gpgconf.exe"
$GpgAgentPath = Join-Path $GpgDir "gpg-agent.exe"
$env:PATH = "$GpgDir;$env:PATH"
$ProjectTmpRoot = Join-Path $ProjectRoot "tmp\sg"
$TestRoot = Join-Path $ProjectTmpRoot ([Guid]::NewGuid().ToString("N").Substring(0, 8))
$ReleaseDir = Join-Path $TestRoot "r"
$GpgHome = Join-Path $TestRoot "g"

New-Item -ItemType Directory -Path $ReleaseDir, $GpgHome -Force | Out-Null
$GpgHomeArgument = ConvertTo-GpgHomeArgument -GpgPath $GpgPath -GpgHome $GpgHome

try {
    if (Test-Path -LiteralPath $GpgAgentPath) {
        & $GpgAgentPath --homedir $GpgHomeArgument --daemon
        Assert-CommandSucceeded -Action "Test gpg-agent startup"
    }
    if (Test-Path -LiteralPath $GpgConfPath) {
        & $GpgConfPath --homedir $GpgHomeArgument --launch gpg-agent
        Assert-CommandSucceeded -Action "Test gpg-agent launch"
    }

    & $GpgPath `
        --homedir $GpgHomeArgument `
        --batch `
        --pinentry-mode loopback `
        --passphrase "" `
        --quick-generate-key `
        "ALIS Release Test <test@localhost>" `
        ed25519 `
        sign `
        0
    Assert-CommandSucceeded -Action "Test-key generation"

    $KeyListing = & $GpgPath `
        --homedir $GpgHomeArgument `
        --batch `
        --with-colons `
        --list-secret-keys
    Assert-CommandSucceeded -Action "Test-key inspection"
    $FingerprintLine = $KeyListing |
        Where-Object { $_ -like "fpr:*" } |
        Select-Object -First 1
    if (-not $FingerprintLine) {
        throw "Generated test key has no fingerprint"
    }
    $Fingerprint = ($FingerprintLine -split ":")[9]

    $ComponentManifestPath = Join-Path $ReleaseDir "effective-component-manifest.json"
    '{"schema":"alis-effective-component-manifest-v1"}' |
        Set-Content -LiteralPath $ComponentManifestPath -Encoding Ascii
    "source archive fixture" |
        Set-Content -LiteralPath (Join-Path $ReleaseDir "ALIS_Source.zip") -Encoding Ascii
    "game archive fixture" |
        Set-Content -LiteralPath (Join-Path $ReleaseDir "ALIS_Win64_fixture.zip") -Encoding Ascii
    '{"schema_version":1}' |
        Set-Content -LiteralPath (Join-Path $ReleaseDir "ALIS_DeveloperProject_fixture.developer-payload.json") -Encoding Ascii
    "fixture terms" |
        Set-Content -LiteralPath (Join-Path $ReleaseDir "PRODUCT_TERMS.txt") -Encoding Ascii
    "fixture rights" |
        Set-Content -LiteralPath (Join-Path $ReleaseDir "release-rights-review.json") -Encoding Ascii
    @(
        "ALIS fixture",
        "",
        "PLAY ON WINDOWS",
        "DEVELOP OR CONTRIBUTE",
        "Product terms: PRODUCT_TERMS.txt"
    ) | Set-Content -LiteralPath (Join-Path $ReleaseDir "README.txt") -Encoding Ascii

    $ArtifactNames = @(
        "README.txt",
        "ALIS_Win64_fixture.zip",
        "PRODUCT_TERMS.txt",
        "effective-component-manifest.json",
        "ALIS_Source.zip",
        "ALIS_DeveloperProject_fixture.developer-payload.json",
        "release-rights-review.json"
    )
    $Artifacts = @($ArtifactNames | ForEach-Object {
        $Path = Join-Path $ReleaseDir $_
        @{
            name = $_
            byte_size = (Get-Item -LiteralPath $Path).Length
            sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    })
    $ReleaseManifestPath = Join-Path $ReleaseDir "release_manifest.json"
    $ReleaseManifest = @{
        schema = "alis-release-manifest-v3"
        status = "pending_owner_approval"
        release_version = "fixture"
        release_tag = "fixture"
        unresolved_count = 0
        product_review = @{ status = "pending_owner_approval" }
        rights_review = @{ status = "pending_owner_approval" }
        artifacts = $Artifacts
    }
    $ReleaseManifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $ReleaseManifestPath -Encoding Ascii

    $RejectedPending = $false
    try {
        & $SignScript `
            -ReleaseDir $ReleaseDir `
            -GpgPath $GpgPath `
            -GpgHome $GpgHome `
            -SigningKeyFingerprint $Fingerprint
    }
    catch {
        $RejectedPending = $_.Exception.Message -like "*not ready for signature*"
    }
    if (-not $RejectedPending) {
        throw "sign_release.ps1 did not reject a pending release manifest"
    }
    if (Test-Path -LiteralPath (Join-Path $ReleaseDir "SHA256SUMS.txt")) {
        throw "Pending release rejection mutated signature outputs"
    }

    $ReleaseManifest.status = "ready_for_signature"
    $ReleaseManifest.product_review = @{ status = "accepted" }
    $ReleaseManifest.rights_review = @{ status = "accepted" }
    $ReleaseManifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $ReleaseManifestPath -Encoding Ascii

    & $SignScript `
        -ReleaseDir $ReleaseDir `
        -GpgPath $GpgPath `
        -GpgHome $GpgHome `
        -SigningKeyFingerprint $Fingerprint
    if (-not $?) {
        throw "sign_release.ps1 failed"
    }

    $ExpectedHash = (Get-FileHash $ComponentManifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $ExpectedLine = "$ExpectedHash *effective-component-manifest.json"
    $HashManifestPath = Join-Path $ReleaseDir "SHA256SUMS.txt"
    if ($ExpectedLine -notin (Get-Content -LiteralPath $HashManifestPath)) {
        throw "SHA256SUMS.txt does not cover the exact component manifest"
    }
    $ReadmeLines = @(Get-Content -LiteralPath (Join-Path $ReleaseDir "README.txt"))
    if ("PLAY ON WINDOWS" -notin $ReadmeLines -or "DEVELOP OR CONTRIBUTE" -notin $ReadmeLines) {
        throw "Combined release README.txt does not expose separate Player and Developer paths"
    }
    if ("Product terms: PRODUCT_TERMS.txt" -notin $ReadmeLines) {
        throw "Combined release README.txt does not route to Product terms"
    }

    & $VerifyScript `
        -ReleaseDir $ReleaseDir `
        -GpgPath $GpgPath `
        -PublicKeyPath (Join-Path $ReleaseDir "ALIS_PUBLIC_KEY.asc") `
        -ExpectedFingerprint $Fingerprint `
        -PublicKeyUrl ""
    if (-not $?) {
        throw "verify_release.ps1 failed"
    }

    $DeveloperSubsetDir = Join-Path $TestRoot "developer-subset"
    New-Item -ItemType Directory -Path $DeveloperSubsetDir | Out-Null
    Copy-Item -LiteralPath `
        $HashManifestPath, `
        (Join-Path $ReleaseDir "SHA256SUMS.txt.asc"), `
        (Join-Path $ReleaseDir "ALIS_PUBLIC_KEY.asc"), `
        $ComponentManifestPath `
        -Destination $DeveloperSubsetDir
    & $VerifyScript `
        -ReleaseDir $DeveloperSubsetDir `
        -RequiredAsset "effective-component-manifest.json" `
        -GpgPath $GpgPath `
        -PublicKeyPath (Join-Path $DeveloperSubsetDir "ALIS_PUBLIC_KEY.asc") `
        -ExpectedFingerprint $Fingerprint `
        -PublicKeyUrl ""
    if (-not $?) {
        throw "Signed Developer subset verification failed"
    }
    if ((Test-Path -LiteralPath (Join-Path $ReleaseDir "sign_release_summary.txt")) -or
        (Test-Path -LiteralPath (Join-Path $ReleaseDir "verify_release_summary.txt")) -or
        (Test-Path -LiteralPath (Join-Path $DeveloperSubsetDir "verify_release_summary.txt"))) {
        throw "Signing or verification diagnostics leaked into the public asset directory"
    }
    $RejectedMissingEntry = $false
    try {
        & $VerifyScript `
            -ReleaseDir $DeveloperSubsetDir `
            -RequiredAsset "not-in-signed-manifest.bin" `
            -GpgPath $GpgPath `
            -PublicKeyPath (Join-Path $DeveloperSubsetDir "ALIS_PUBLIC_KEY.asc") `
            -ExpectedFingerprint $Fingerprint `
            -PublicKeyUrl ""
    }
    catch {
        $RejectedMissingEntry = $_.Exception.Message -like "*must appear exactly once*"
    }
    if (-not $RejectedMissingEntry) {
        throw "Subset verification accepted an asset absent from the signed manifest"
    }

    Write-Host "[OK] Flat full-release and signed Developer-subset verification passed"
}
finally {
    if (Test-Path -LiteralPath $GpgConfPath) {
        & $GpgConfPath --homedir $GpgHomeArgument --kill gpg-agent 2>$null
    }

    if (Test-Path -LiteralPath $TestRoot) {
        $ResolvedTemp = [IO.Path]::GetFullPath($ProjectTmpRoot).TrimEnd('\', '/')
        $ResolvedTestRoot = (Resolve-Path -LiteralPath $TestRoot).Path
        if (-not $ResolvedTestRoot.StartsWith(
                $ResolvedTemp + [IO.Path]::DirectorySeparatorChar,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected test path: $ResolvedTestRoot"
        }
        Remove-Item -LiteralPath $ResolvedTestRoot -Recurse -Force
    }
}
