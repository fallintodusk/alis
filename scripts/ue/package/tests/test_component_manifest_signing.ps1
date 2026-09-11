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

    $ArtifactNames = @(
        "effective-component-manifest.json",
        "ALIS_Source.zip",
        "ALIS_Win64_fixture.zip",
        "ALIS_DeveloperProject_fixture.developer-payload.json",
        "PRODUCT_TERMS.txt",
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
        schema = "alis-release-manifest-v1"
        status = "pending_owner_approval"
        release_version = "fixture"
        release_tag = "fixture"
        unresolved_count = 0
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
    $InstallLines = @(Get-Content -LiteralPath (Join-Path $ReleaseDir "INSTALL.txt"))
    if ('Player:' -notin $InstallLines -or 'Developer:' -notin $InstallLines) {
        throw "Combined release INSTALL.txt does not expose separate Player and Developer routes"
    }
    if ('.\PRODUCT_TERMS.txt' -notin $InstallLines) {
        throw "Combined release INSTALL.txt does not route to Product terms"
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

    Write-Host "[OK] Component manifest is signed and verified"
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
