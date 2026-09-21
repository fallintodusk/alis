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
$ReleaseScript = Join-Path $PackageDir "release.ps1"
$WorkspaceTool = Join-Path $PackageDir "release_workspace.py"
$GpgPath = Resolve-TestGpg
$GpgDir = Split-Path -Parent $GpgPath
$GpgConfPath = Join-Path $GpgDir "gpgconf.exe"
$GpgAgentPath = Join-Path $GpgDir "gpg-agent.exe"
$env:PATH = "$GpgDir;$env:PATH"
$ProjectTmpRoot = Join-Path $ProjectRoot "tmp\sg"
$TestRoot = Join-Path $ProjectTmpRoot ([Guid]::NewGuid().ToString("N").Substring(0, 8))
$ReleaseDir = Join-Path $TestRoot "r"
$GameDir = Join-Path $TestRoot "game"
$GpgHome = Join-Path $TestRoot "g"

New-Item -ItemType Directory -Path $ReleaseDir, $GameDir, $GpgHome -Force | Out-Null
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
    '{"schema_version":2,"archive":{"parts":[{"name":"ALIS_Source.zip"}]}}' |
        Set-Content -LiteralPath (Join-Path $ReleaseDir "ALIS_DeveloperProject_fixture.developer-payload.json") -Encoding Ascii
    '{"schema":"fixture-notices"}' |
        Set-Content -LiteralPath (Join-Path $ReleaseDir "ALIS_DeveloperProject_fixture.notices.json") -Encoding Ascii
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
        "ALIS_DeveloperProject_fixture.notices.json",
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
        release_version = "2.0.0"
        release_tag = "v2.0.0"
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
    $ReleaseManifest.approval_scope = "game"
    $ReleaseManifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $ReleaseManifestPath -Encoding Ascii

    $RejectedGameScopeForGitHub = $false
    try {
        & $SignScript `
            -ReleaseDir $ReleaseDir `
            -Projection GitHub `
            -GpgPath (Join-Path $TestRoot "must-not-be-read-gpg.exe") `
            -GpgHome $GpgHome `
            -SigningKeyFingerprint $Fingerprint
    }
    catch {
        $RejectedGameScopeForGitHub = $_.Exception.Message -like "*requires full release approval*"
    }
    if (-not $RejectedGameScopeForGitHub -or
        (Test-Path -LiteralPath (Join-Path $ReleaseDir "SHA256SUMS.txt"))) {
        throw "GitHub signing did not reject game-only approval before GPG access"
    }
    $ReleaseManifest.approval_scope = "full"
    $ReleaseManifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $ReleaseManifestPath -Encoding Ascii

    $CoordinatedWorkspace = Join-Path $TestRoot "workspace"
    $CoordinatedGame = Join-Path $CoordinatedWorkspace "game"
    $CoordinatedGitHub = Join-Path $CoordinatedWorkspace "github"
    New-Item -ItemType Directory -Path `
        (Join-Path $CoordinatedGame "Alis\Binaries\Win64"), `
        (Join-Path $CoordinatedGame "Alis\Content"), `
        (Join-Path $CoordinatedGame "Engine\Content"), `
        $CoordinatedGitHub -Force | Out-Null
    "launcher" | Set-Content -LiteralPath (Join-Path $CoordinatedGame "Alis.exe") -Encoding Ascii
    "shipping" | Set-Content -LiteralPath (Join-Path $CoordinatedGame "Alis\Binaries\Win64\Alis-Win64-Shipping.exe") -Encoding Ascii
    "alis data" | Set-Content -LiteralPath (Join-Path $CoordinatedGame "Alis\Content\fixture.bin") -Encoding Ascii
    "engine data" | Set-Content -LiteralPath (Join-Path $CoordinatedGame "Engine\Content\fixture.bin") -Encoding Ascii
    "accepted" | Set-Content -LiteralPath (Join-Path $CoordinatedWorkspace "package_summary.txt") -Encoding Ascii
    Get-ChildItem -LiteralPath $ReleaseDir -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $CoordinatedGitHub
    }
    Remove-Item -LiteralPath (Join-Path $CoordinatedGitHub "ALIS_Win64_fixture.zip")
    "unsigned player archive" | Set-Content -LiteralPath (Join-Path $CoordinatedGitHub "ALIS_Win64_v2.0.89.zip.001") -Encoding Ascii
    $CoordinatedManifestPath = Join-Path $CoordinatedGitHub "release_manifest.json"
    $CoordinatedManifest = Get-Content -LiteralPath $CoordinatedManifestPath -Raw | ConvertFrom-Json
    $CoordinatedManifest.release_version = "2.0.89"
    $CoordinatedManifest.release_tag = "v2.0.89"
    $CoordinatedManifest.artifacts = @(Get-ChildItem -LiteralPath $CoordinatedGitHub -File |
        Where-Object Name -ne "release_manifest.json" |
        ForEach-Object {
            @{
                name = $_.Name
                byte_size = $_.Length
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        })
    $CoordinatedShipping = Join-Path $CoordinatedGame "Alis\Binaries\Win64\Alis-Win64-Shipping.exe"
    $CoordinatedPackageTree = & python $WorkspaceTool package-tree --workspace-root $CoordinatedWorkspace
    Assert-CommandSucceeded -Action "Coordinated package identity"
    $CoordinatedManifest | Add-Member -NotePropertyName player_source -NotePropertyValue @{
        package_tree_sha256 = [string]$CoordinatedPackageTree
        shipping_executable_sha256 = (Get-FileHash -LiteralPath $CoordinatedShipping -Algorithm SHA256).Hash.ToLowerInvariant()
    } -Force
    $CoordinatedManifest | ConvertTo-Json -Depth 10 |
        Set-Content -LiteralPath $CoordinatedManifestPath -Encoding Ascii
    @{
        schema = "alis-release-workspace-v1"
        status = "complete"
        release_version = "2.0.89"
        release_tag = "v2.0.89"
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $CoordinatedWorkspace "release-workspace.json") -Encoding Ascii

    $PendingFullWorkspace = Join-Path $TestRoot "pending-full-workspace"
    Copy-Item -LiteralPath $CoordinatedWorkspace -Destination $PendingFullWorkspace -Recurse
    $PendingFullGame = Join-Path $PendingFullWorkspace "game"
    $PendingFullGitHub = Join-Path $PendingFullWorkspace "github"
    $PendingFullManifestPath = Join-Path $PendingFullGitHub "release_manifest.json"
    $PendingFullManifest = Get-Content -LiteralPath $PendingFullManifestPath -Raw | ConvertFrom-Json
    $PendingFullManifest.status = "pending_owner_approval"
    $PendingFullManifest.release_version = "2.0.90"
    $PendingFullManifest.release_tag = "v2.0.90"
    $PendingFullManifest.product_review = @{ status = "pending_owner_approval" }
    $PendingFullManifest.rights_review = @{ status = "pending_owner_approval" }
    $PendingFullManifest.PSObject.Properties.Remove("approval_scope")
    $PendingFullManifest.artifacts = @($PendingFullManifest.artifacts | Where-Object {
            $_.name -ne "release-rights-review.json"
        })
    Remove-Item -LiteralPath (Join-Path $PendingFullGitHub "release-rights-review.json")

    $PendingFullSource = Join-Path $TestRoot "pending-full-source"
    New-Item -ItemType Directory -Path $PendingFullSource -Force | Out-Null
    & git -c init.defaultBranch=main init -q $PendingFullSource
    & git -C $PendingFullSource config user.name "release-test"
    & git -C $PendingFullSource config user.email "release-test@localhost"
    "source" | Set-Content -LiteralPath (Join-Path $PendingFullSource "README.md") -Encoding Ascii
    & git -C $PendingFullSource add README.md
    & git -C $PendingFullSource commit -q -m "fixture"
    Assert-CommandSucceeded -Action "Pending full-release source fixture"
    $PendingFullRevision = [string](& git -C $PendingFullSource rev-parse HEAD)
    $PendingFullTree = [string](& git -C $PendingFullSource rev-parse 'HEAD^{tree}')
    Assert-CommandSucceeded -Action "Pending full-release source identity"
    $PendingFullManifest | Add-Member -NotePropertyName public_source -NotePropertyValue @{
        revision = $PendingFullRevision
        tree = $PendingFullTree
        branch = "main"
    } -Force
    $PendingFullManifest | ConvertTo-Json -Depth 10 |
        Set-Content -LiteralPath $PendingFullManifestPath -Encoding Ascii
    $PendingFullWorkspaceManifest = Get-Content `
        -LiteralPath (Join-Path $PendingFullWorkspace "release-workspace.json") -Raw |
        ConvertFrom-Json
    $PendingFullWorkspaceManifest.release_version = "2.0.90"
    $PendingFullWorkspaceManifest.release_tag = "v2.0.90"
    $PendingFullWorkspaceManifest | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath (Join-Path $PendingFullWorkspace "release-workspace.json") -Encoding Ascii

    & $ReleaseScript `
        -ReleaseVersion "2.0.90" `
        -ReleaseDir $PendingFullWorkspace `
        -FinalPublicSourceRoot $PendingFullSource `
        -GpgPath $GpgPath `
        -GpgHome $GpgHome `
        -SigningKeyFingerprint $Fingerprint
    if (-not $?) {
        throw "Pending full-release finalization failed"
    }
    $PendingFullApproved = Get-Content -LiteralPath $PendingFullManifestPath -Raw | ConvertFrom-Json
    if ($PendingFullApproved.approval_scope -ne "full" -or
        -not (Test-Path -LiteralPath (Join-Path $PendingFullGame "Verification\SHA256SUMS.txt.asc")) -or
        -not (Test-Path -LiteralPath (Join-Path $PendingFullGitHub "SHA256SUMS.txt.asc"))) {
        throw "Pending full release did not record full approval and sign both projections"
    }

    $UnexpectedVerification = Join-Path $CoordinatedGame "Verification\unexpected.bin"
    New-Item -ItemType Directory -Path (Split-Path -Parent $UnexpectedVerification) -Force | Out-Null
    "unexpected" | Set-Content -LiteralPath $UnexpectedVerification -Encoding Ascii
    $RejectedUnknownVerificationFile = $false
    try {
        & $SignScript `
            -ReleaseDir $CoordinatedGame `
            -ApprovalDir $CoordinatedGitHub `
            -Projection Game `
            -GpgPath (Join-Path $TestRoot "must-not-be-read-gpg.exe") `
            -GpgHome $GpgHome `
            -SigningKeyFingerprint $Fingerprint
    }
    catch {
        $RejectedUnknownVerificationFile = $_.Exception.Message -like "*workspace verification failed*"
    }
    if (-not $RejectedUnknownVerificationFile) {
        throw "Game signing did not reject an unknown Verification file before GPG access"
    }
    Remove-Item -LiteralPath $UnexpectedVerification

    & $ReleaseScript `
        -ReleaseVersion "2.0.89" `
        -ReleaseDir $CoordinatedWorkspace `
        -Target Game `
        -PublicRemoteUrl (Join-Path $TestRoot "must-not-be-read-public-remote") `
        -GpgPath $GpgPath `
        -GpgHome $GpgHome `
        -SigningKeyFingerprint $Fingerprint
    if (-not $?) {
        throw "Game-only finalization failed"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $CoordinatedGame "Verification\SHA256SUMS.txt.asc")) -or
        (Test-Path -LiteralPath (Join-Path $CoordinatedGitHub "SHA256SUMS.txt.asc")) -or
        @(Get-ChildItem -LiteralPath $CoordinatedGitHub -Filter "ALIS_Win64_v2.0.89.zip*").Count -ne 1) {
        throw "Game-only finalization did not remain isolated from GitHub signing and archive refresh"
    }

    & $ReleaseScript `
        -ReleaseVersion "2.0.89" `
        -ReleaseDir $CoordinatedWorkspace `
        -GpgPath $GpgPath `
        -GpgHome $GpgHome `
        -SigningKeyFingerprint $Fingerprint
    if (-not $?) {
        throw "Coordinated game/GitHub finalization failed"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $CoordinatedGame "Verification\SHA256SUMS.txt.asc")) -or
        -not (Test-Path -LiteralPath (Join-Path $CoordinatedGitHub "SHA256SUMS.txt.asc")) -or
        @(Get-ChildItem -LiteralPath $CoordinatedGitHub -Filter "ALIS_Win64_v2.0.89.zip*").Count -lt 1) {
        throw "Coordinated finalization did not produce both signatures and the refreshed Player archive"
    }

    $MultiWorkspace = Join-Path $TestRoot "multi-workspace"
    $MultiWindows = Join-Path $MultiWorkspace "game\windows-x86_64"
    $MultiLinux = Join-Path $MultiWorkspace "game\linux-x86_64"
    $MultiGitHub = Join-Path $MultiWorkspace "github"
    New-Item -ItemType Directory -Path `
        (Join-Path $MultiWindows "Alis\Binaries\Win64"), `
        (Join-Path $MultiLinux "Alis\Binaries\Linux"), `
        $MultiGitHub -Force | Out-Null
    "launcher" | Set-Content -LiteralPath (Join-Path $MultiWindows "Alis.exe") -Encoding Ascii
    "windows" | Set-Content -LiteralPath (Join-Path $MultiWindows "Alis\Binaries\Win64\Alis-Win64-Shipping.exe") -Encoding Ascii
    "#!/bin/sh" | Set-Content -LiteralPath (Join-Path $MultiLinux "Alis.sh") -Encoding Ascii
    [IO.File]::WriteAllBytes(
        (Join-Path $MultiLinux "Alis\Binaries\Linux\Alis-Linux-Shipping"),
        [byte[]](0x7f, 0x45, 0x4c, 0x46, 0x01)
    )
    Get-ChildItem -LiteralPath $ReleaseDir -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $MultiGitHub
    }
    $MultiManifestPath = Join-Path $MultiGitHub "release_manifest.json"
    $MultiManifest = Get-Content -LiteralPath $MultiManifestPath -Raw | ConvertFrom-Json
    $MultiManifest.schema = "alis-release-manifest-v4"
    $MultiManifest.release_version = "9.9.1"
    $MultiManifest.release_tag = "v9.9.1"
    $MultiManifest.status = "pending_owner_approval"
    $MultiManifest.product_review = @{ status = "pending_owner_approval" }
    $MultiManifest.rights_review = @{ status = "pending_owner_approval" }
    $MultiManifest | Add-Member -NotePropertyName public_source -NotePropertyValue @{
        revision = "cccccccccccccccccccccccccccccccccccccccc"
        tree = "dddddddddddddddddddddddddddddddddddddddd"
    } -Force
    $MultiManifest.PSObject.Properties.Remove("approval_scope")
    $MultiManifest.artifacts = @($MultiManifest.artifacts | Where-Object {
            $_.name -ne "release-rights-review.json"
        })
    Remove-Item -LiteralPath (Join-Path $MultiGitHub "release-rights-review.json")
    $MultiManifest.PSObject.Properties.Remove("player_source")
    $env:ALIS_TEST_PACKAGE_DIR = $PackageDir
    $env:ALIS_TEST_WINDOWS_GAME = $MultiWindows
    $env:ALIS_TEST_LINUX_GAME = $MultiLinux
    try {
        $MultiDigests = & python -c "import os,sys; from pathlib import Path; sys.path.insert(0,os.environ['ALIS_TEST_PACKAGE_DIR']); import release_workspace as w; print(w.platform_game_tree_digest(Path(os.environ['ALIS_TEST_WINDOWS_GAME']),'windows-x86_64')); print(w.platform_game_tree_digest(Path(os.environ['ALIS_TEST_LINUX_GAME']),'linux-x86_64'))"
        Assert-CommandSucceeded -Action "Multi-platform game identities"
    }
    finally {
        Remove-Item Env:ALIS_TEST_PACKAGE_DIR, Env:ALIS_TEST_WINDOWS_GAME, Env:ALIS_TEST_LINUX_GAME -ErrorAction SilentlyContinue
    }
    $MultiManifest | Add-Member -NotePropertyName player_sources -NotePropertyValue @{
        "windows-x86_64" = @{
            revision = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            source_state_sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
            runtime_payload_tree_sha256 = [string]$MultiDigests[0]
            shipping_executable = "Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
            shipping_executable_sha256 = (Get-FileHash -LiteralPath (Join-Path $MultiWindows "Alis\Binaries\Win64\Alis-Win64-Shipping.exe") -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        "linux-x86_64" = @{
            revision = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            source_state_sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
            runtime_payload_tree_sha256 = [string]$MultiDigests[1]
            shipping_executable = "Alis/Binaries/Linux/Alis-Linux-Shipping"
            shipping_executable_sha256 = (Get-FileHash -LiteralPath (Join-Path $MultiLinux "Alis\Binaries\Linux\Alis-Linux-Shipping") -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    } -Force
    $MultiManifest | ConvertTo-Json -Depth 10 |
        Set-Content -LiteralPath $MultiManifestPath -Encoding Ascii
    @{
        schema = "alis-release-workspace-v2"
        status = "complete"
        release_version = "9.9.1"
        release_tag = "v9.9.1"
        platforms = @("windows-x86_64", "linux-x86_64")
    } | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath (Join-Path $MultiWorkspace "release-workspace.json") -Encoding Ascii

    & $ReleaseScript `
        -ReleaseVersion "9.9.1" `
        -ReleaseDir $MultiWorkspace `
        -Target Game `
        -PublicRemoteUrl (Join-Path $TestRoot "must-not-be-read-multi-public-remote") `
        -GpgPath $GpgPath `
        -GpgHome $GpgHome `
        -SigningKeyFingerprint $Fingerprint
    if (-not $? -or
        -not (Test-Path -LiteralPath (Join-Path $MultiLinux "VERIFY_ALIS.sh")) -or
        -not (Test-Path -LiteralPath (Join-Path $MultiWindows "VERIFY_ALIS.bat")) -or
        -not (Test-Path -LiteralPath (Join-Path $MultiWindows "Verification\SHA256SUMS.txt.asc")) -or
        -not (Test-Path -LiteralPath (Join-Path $MultiLinux "Verification\SHA256SUMS.txt.asc")) -or
        (Test-Path -LiteralPath (Join-Path $MultiGitHub "SHA256SUMS.txt.asc")) -or
        (Test-Path -LiteralPath (Join-Path $MultiLinux "VERIFY_ALIS.bat"))) {
        throw "Workspace-v2 game finalization did not sign both platform projections only"
    }
    $MultiApproved = Get-Content -LiteralPath $MultiManifestPath -Raw | ConvertFrom-Json
    if ($MultiApproved.approval_scope -ne "game") {
        throw "Workspace-v2 game finalization did not record bounded game approval"
    }
    & python $WorkspaceTool verify --workspace-root $MultiWorkspace
    Assert-CommandSucceeded -Action "Signed multi-platform workspace verification"
    $Wsl = Get-Command "wsl.exe" -ErrorAction SilentlyContinue
    if ($Wsl) {
        $ShellVerifierFixture = Join-Path $TestRoot "verify_release_test_key.sh"
        $ShellVerifierText = Get-Content -LiteralPath (Join-Path $PackageDir "verify_release.sh") -Raw
        $ProductionFingerprint = "3B9885F0C2D8D927C27FAB58F61A530034CFB5E7"
        if (-not $ShellVerifierText.Contains($ProductionFingerprint)) {
            throw "Linux verifier does not bind the production release-key fingerprint"
        }
        $ShellVerifierText = $ShellVerifierText.Replace($ProductionFingerprint, $Fingerprint)
        [IO.File]::WriteAllText(
            $ShellVerifierFixture,
            $ShellVerifierText,
            [Text.UTF8Encoding]::new($false)
        )
        $WslRoot = "/mnt/" + $MultiLinux.Substring(0, 1).ToLowerInvariant() +
            $MultiLinux.Substring(2).Replace('\', '/')
        $WslVerifier = "/mnt/" + $ShellVerifierFixture.Substring(0, 1).ToLowerInvariant() +
            $ShellVerifierFixture.Substring(2).Replace('\', '/')
        & $Wsl.Source sh $WslVerifier $WslRoot
        Assert-CommandSucceeded -Action "Linux shell consumer verification"
        "unexpected" | Set-Content -LiteralPath (Join-Path $MultiLinux "unexpected.bin") -Encoding Ascii
        $PreviousErrorAction = $ErrorActionPreference
        try {
            $ErrorActionPreference = "Continue"
            $RejectedShellOutput = @(& $Wsl.Source sh $WslVerifier $WslRoot 2>&1)
            $RejectedShellExit = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $PreviousErrorAction
        }
        if ($RejectedShellExit -eq 0 -or -not ($RejectedShellOutput -match "inventory differs")) {
            throw "Linux shell verifier accepted an unsigned extra file"
        }
        Remove-Item -LiteralPath (Join-Path $MultiLinux "unexpected.bin")
    }

    New-Item -ItemType Directory -Path (Join-Path $GameDir "Alis\Content"), (Join-Path $GameDir "Engine\Content") -Force | Out-Null
    "game" | Set-Content -LiteralPath (Join-Path $GameDir "Alis.exe") -Encoding Ascii
    "alis data" | Set-Content -LiteralPath (Join-Path $GameDir "Alis\Content\fixture.bin") -Encoding Ascii
    "engine data" | Set-Content -LiteralPath (Join-Path $GameDir "Engine\Content\fixture.bin") -Encoding Ascii

    $RejectedUnrelatedGame = $false
    try {
        & $SignScript `
            -ReleaseDir $GameDir `
            -ApprovalDir $ReleaseDir `
            -Projection Game `
            -GpgPath (Join-Path $TestRoot "must-not-be-read-gpg.exe") `
            -GpgHome $GpgHome `
            -SigningKeyFingerprint $Fingerprint
    }
    catch {
        $RejectedUnrelatedGame = $_.Exception.Message -like "*same release workspace*"
    }
    if (-not $RejectedUnrelatedGame) {
        throw "Game signing accepted unrelated game and approval directories"
    }

    $GameDir = $CoordinatedGame
    $GameManifest = Join-Path $GameDir "Verification\SHA256SUMS.txt"
    if (-not ((Get-Content -LiteralPath $GameManifest) -match "\*Alis/Content/fixture\.bin") -or
        -not ((Get-Content -LiteralPath $GameManifest) -match "\*Engine/Content/fixture\.bin")) {
        throw "Game signature manifest does not preserve safe relative paths"
    }
    & $VerifyScript `
        -ReleaseDir $GameDir `
        -ManifestRelativePath "Verification\SHA256SUMS.txt" `
        -SignatureRelativePath "Verification\SHA256SUMS.txt.asc" `
        -BundledPublicKeyName "Verification\ALIS_PUBLIC_KEY.asc" `
        -AllowRelativeAssetPaths `
        -RequireExactInventory `
        -GpgPath $GpgPath `
        -ExpectedFingerprint $Fingerprint `
        -PublicKeyUrl ""
    if (-not $?) {
        throw "Signed game projection verification failed"
    }
    "mutated" | Set-Content -LiteralPath (Join-Path $GameDir "Alis\Content\fixture.bin") -Encoding Ascii
    $RejectedMutation = $false
    try {
        & $VerifyScript `
            -ReleaseDir $GameDir `
            -ManifestRelativePath "Verification\SHA256SUMS.txt" `
            -SignatureRelativePath "Verification\SHA256SUMS.txt.asc" `
            -BundledPublicKeyName "Verification\ALIS_PUBLIC_KEY.asc" `
            -AllowRelativeAssetPaths `
            -RequireExactInventory `
            -GpgPath $GpgPath `
            -ExpectedFingerprint $Fingerprint `
            -PublicKeyUrl ""
    }
    catch {
        $RejectedMutation = $_.Exception.Message -like "*Hash mismatch*"
    }
    if (-not $RejectedMutation) {
        throw "Game verification accepted a mutated packaged file"
    }
    "alis data" | Set-Content -LiteralPath (Join-Path $GameDir "Alis\Content\fixture.bin") -Encoding Ascii
    "unexpected" | Set-Content -LiteralPath (Join-Path $GameDir "unexpected.bin") -Encoding Ascii
    $RejectedExtra = $false
    try {
        & $VerifyScript `
            -ReleaseDir $GameDir `
            -ManifestRelativePath "Verification\SHA256SUMS.txt" `
            -SignatureRelativePath "Verification\SHA256SUMS.txt.asc" `
            -BundledPublicKeyName "Verification\ALIS_PUBLIC_KEY.asc" `
            -AllowRelativeAssetPaths `
            -RequireExactInventory `
            -GpgPath $GpgPath `
            -ExpectedFingerprint $Fingerprint `
            -PublicKeyUrl ""
    }
    catch {
        $RejectedExtra = $_.Exception.Message -like "*inventory mismatch*"
    }
    if (-not $RejectedExtra) {
        throw "Game verification accepted an unsigned extra file"
    }

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
    if ($Wsl) {
        $WslFlatRoot = "/mnt/" + $ReleaseDir.Substring(0, 1).ToLowerInvariant() +
            $ReleaseDir.Substring(2).Replace('\', '/')
        & $Wsl.Source sh $WslVerifier $WslFlatRoot
        Assert-CommandSucceeded -Action "Flat GitHub shell consumer verification"
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
