#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackageDir = Split-Path -Parent $ScriptDir
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PackageDir))
$PublishScript = Join-Path $PackageDir "publish_itch.ps1"
$WorkspaceTool = Join-Path $PackageDir "release_workspace.py"
$TestParent = Join-Path $ProjectRoot "tmp\release\tests\publish-itch"
$TestRoot = Join-Path $TestParent ([Guid]::NewGuid().ToString("N"))
$ReleaseRoot = Join-Path $TestRoot "release"
$FakeButler = Join-Path $TestRoot "fake_butler.ps1"
$FakeVerifier = Join-Path $TestRoot "fake_verifier.ps1"
$ButlerLog = Join-Path $TestRoot "butler.log"
$VerifierLog = Join-Path $TestRoot "verifier.log"
$StatusCount = Join-Path $TestRoot "status-count.txt"

function Write-ReleaseWorkspace {
    param(
        [string]$Version
    )

    $Workspace = Join-Path $ReleaseRoot "v$Version"
    $Game = Join-Path $Workspace "game"
    $GitHub = Join-Path $Workspace "github"
    New-Item -ItemType Directory -Path `
        (Join-Path $Game "Alis\Binaries\Win64"), `
        (Join-Path $Game "Verification"), `
        $GitHub -Force | Out-Null
    "launcher" | Set-Content -LiteralPath (Join-Path $Game "Alis.exe") -Encoding Ascii
    "shipping" | Set-Content -LiteralPath (Join-Path $Game "Alis\Binaries\Win64\Alis-Win64-Shipping.exe") -Encoding Ascii
    "accepted" | Set-Content -LiteralPath (Join-Path $Workspace "package_summary.txt") -Encoding Ascii
    "verify" | Set-Content -LiteralPath (Join-Path $Game "VERIFY_ALIS.bat") -Encoding Ascii
    "verify" | Set-Content -LiteralPath (Join-Path $Game "Verification\VERIFY_ALIS.ps1") -Encoding Ascii
    "key" | Set-Content -LiteralPath (Join-Path $Game "Verification\ALIS_PUBLIC_KEY.asc") -Encoding Ascii
    "manifest" | Set-Content -LiteralPath (Join-Path $Game "Verification\SHA256SUMS.txt") -Encoding Ascii
    "signature" | Set-Content -LiteralPath (Join-Path $Game "Verification\SHA256SUMS.txt.asc") -Encoding Ascii
    "release" | Set-Content -LiteralPath (Join-Path $GitHub "README.txt") -Encoding Ascii

    $PackageDigest = [string](& python $WorkspaceTool package-tree --workspace-root $Workspace)
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to create workspace package identity"
    }
    $Shipping = Join-Path $Game "Alis\Binaries\Win64\Alis-Win64-Shipping.exe"
    $Readme = Join-Path $GitHub "README.txt"
    @{
        schema = "alis-release-manifest-v3"
        status = "ready_for_signature"
        approval_scope = "game"
        release_version = $Version
        release_tag = "v$Version"
        unresolved_count = 0
        product_review = @{ status = "accepted" }
        rights_review = @{ status = "accepted" }
        player_source = @{
            package_tree_sha256 = $PackageDigest
            shipping_executable_sha256 = (Get-FileHash -LiteralPath $Shipping -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        artifacts = @(@{
            name = "README.txt"
            byte_size = (Get-Item -LiteralPath $Readme).Length
            sha256 = (Get-FileHash -LiteralPath $Readme -Algorithm SHA256).Hash.ToLowerInvariant()
        })
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $GitHub "release_manifest.json") -Encoding Ascii
    @{
        schema = "alis-release-workspace-v1"
        status = "complete"
        release_version = $Version
        release_tag = "v$Version"
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Workspace "release-workspace.json") -Encoding Ascii
    return $Workspace
}

function Write-MultiPlatformReleaseWorkspace {
    param(
        [string]$Version
    )

    $Workspace = Join-Path $ReleaseRoot "v$Version"
    $WindowsGame = Join-Path $Workspace "game\windows-x86_64"
    $LinuxGame = Join-Path $Workspace "game\linux-x86_64"
    $GitHub = Join-Path $Workspace "github"
    New-Item -ItemType Directory -Path `
        (Join-Path $WindowsGame "Alis\Binaries\Win64"), `
        (Join-Path $WindowsGame "Verification"), `
        (Join-Path $LinuxGame "Alis\Binaries\Linux"), `
        (Join-Path $LinuxGame "Verification"), `
        $GitHub -Force | Out-Null
    "launcher" | Set-Content -LiteralPath (Join-Path $WindowsGame "Alis.exe") -Encoding Ascii
    "shipping" | Set-Content -LiteralPath (Join-Path $WindowsGame "Alis\Binaries\Win64\Alis-Win64-Shipping.exe") -Encoding Ascii
    [IO.File]::WriteAllBytes((Join-Path $LinuxGame "Alis\Binaries\Linux\Alis-Linux-Shipping"), [byte[]](0x7f, 0x45, 0x4c, 0x46, 0x01))
    "#!/bin/sh" | Set-Content -LiteralPath (Join-Path $LinuxGame "Alis.sh") -Encoding Ascii
    foreach ($Game in @($WindowsGame, $LinuxGame)) {
        "verify" | Set-Content -LiteralPath (Join-Path $Game "Verification\VERIFY_ALIS.ps1") -Encoding Ascii
        "key" | Set-Content -LiteralPath (Join-Path $Game "Verification\ALIS_PUBLIC_KEY.asc") -Encoding Ascii
        "manifest" | Set-Content -LiteralPath (Join-Path $Game "Verification\SHA256SUMS.txt") -Encoding Ascii
        "signature" | Set-Content -LiteralPath (Join-Path $Game "Verification\SHA256SUMS.txt.asc") -Encoding Ascii
    }
    "verify" | Set-Content -LiteralPath (Join-Path $WindowsGame "VERIFY_ALIS.bat") -Encoding Ascii
    "verify" | Set-Content -LiteralPath (Join-Path $LinuxGame "VERIFY_ALIS.sh") -Encoding Ascii
    "release" | Set-Content -LiteralPath (Join-Path $GitHub "README.txt") -Encoding Ascii

    $env:ALIS_TEST_PACKAGE_DIR = $PackageDir
    $env:ALIS_TEST_WINDOWS_GAME = $WindowsGame
    $env:ALIS_TEST_LINUX_GAME = $LinuxGame
    try {
        $Digests = & python -c "import os,sys; from pathlib import Path; sys.path.insert(0,os.environ['ALIS_TEST_PACKAGE_DIR']); import release_workspace as w; print(w.platform_game_tree_digest(Path(os.environ['ALIS_TEST_WINDOWS_GAME']),'windows-x86_64')); print(w.platform_game_tree_digest(Path(os.environ['ALIS_TEST_LINUX_GAME']),'linux-x86_64'))"
        if ($LASTEXITCODE -ne 0 -or @($Digests).Count -ne 2) {
            throw "Unable to create multi-platform workspace identities"
        }
    }
    finally {
        Remove-Item Env:ALIS_TEST_PACKAGE_DIR, Env:ALIS_TEST_WINDOWS_GAME, Env:ALIS_TEST_LINUX_GAME -ErrorAction SilentlyContinue
    }
    $WindowsShipping = Join-Path $WindowsGame "Alis\Binaries\Win64\Alis-Win64-Shipping.exe"
    $LinuxShipping = Join-Path $LinuxGame "Alis\Binaries\Linux\Alis-Linux-Shipping"
    $Readme = Join-Path $GitHub "README.txt"
    @{
        schema = "alis-release-manifest-v4"
        status = "ready_for_signature"
        approval_scope = "game"
        release_version = $Version
        release_tag = "v$Version"
        unresolved_count = 0
        product_review = @{ status = "accepted" }
        rights_review = @{ status = "accepted" }
        player_sources = @{
            "windows-x86_64" = @{
                revision = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                source_state_sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                runtime_payload_tree_sha256 = [string]$Digests[0]
                shipping_executable = "Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
                shipping_executable_sha256 = (Get-FileHash -LiteralPath $WindowsShipping -Algorithm SHA256).Hash.ToLowerInvariant()
            }
            "linux-x86_64" = @{
                revision = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                source_state_sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                runtime_payload_tree_sha256 = [string]$Digests[1]
                shipping_executable = "Alis/Binaries/Linux/Alis-Linux-Shipping"
                shipping_executable_sha256 = (Get-FileHash -LiteralPath $LinuxShipping -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
        artifacts = @(@{
            name = "README.txt"
            byte_size = (Get-Item -LiteralPath $Readme).Length
            sha256 = (Get-FileHash -LiteralPath $Readme -Algorithm SHA256).Hash.ToLowerInvariant()
        })
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $GitHub "release_manifest.json") -Encoding Ascii
    @{
        schema = "alis-release-workspace-v2"
        status = "complete"
        release_version = $Version
        release_tag = "v$Version"
        platforms = @("windows-x86_64", "linux-x86_64")
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Workspace "release-workspace.json") -Encoding Ascii
    return $Workspace
}

function Reset-FakeState {
    Remove-Item -LiteralPath $ButlerLog, $VerifierLog, $StatusCount -Force -ErrorAction SilentlyContinue
    $env:FAKE_BUTLER_STATUS_BEFORE = '{"time":1789635167,"type":"result","value":{"channels":[],"target":"example/game"}}'
    $env:FAKE_BUTLER_STATUS_AFTER = '{"type":"result","value":{"target":"example/game","channels":[{"name":"windows","head":{"id":10,"state":"completed","userVersion":"2.0.0"}}]}}'
    $env:FAKE_BUTLER_PREVIEW = '{"time":1789635168,"type":"result","value":{"channel":"windows","comparison":{"new":0,"modified":0,"deleted":0,"same":102,"newBytes":0,"modifiedBytes":0,"deletedBytes":0,"sameBytes":5102176124},"hasParent":true,"sourceSize":5102176124,"topChangedFiles":{"new":[],"modified":[],"deleted":[]}}}'
    $env:FAKE_BUTLER_PUSH_EXIT = "0"
    $env:FAKE_VERIFIER_FAIL = "0"
}

function Invoke-Publisher {
    param(
        [string]$Version
    )

    $Arguments = @{
        ReleaseRoot = $ReleaseRoot
        Target = "example/game"
        Channel = "windows"
        ButlerPath = $FakeButler
        VerifierScript = $FakeVerifier
    }
    if ($Version) {
        $Arguments.ReleaseVersion = $Version
    }
    & $PublishScript @Arguments
}

New-Item -ItemType Directory -Path $TestRoot, $ReleaseRoot -Force | Out-Null
@'
$argsList = @($args)
Add-Content -LiteralPath $env:FAKE_BUTLER_LOG -Value ($argsList -join "|") -Encoding Ascii
if ($argsList[0] -in @("-V", "version")) {
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & cmd.exe /d /c "1>&2 echo v15.31.0, fixture"
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorAction
    exit $exitCode
}
if ($argsList[0] -eq "status") {
    $count = 0
    if (Test-Path -LiteralPath $env:FAKE_BUTLER_STATUS_COUNT) {
        $count = [int](Get-Content -LiteralPath $env:FAKE_BUTLER_STATUS_COUNT -Raw)
    }
    $count++
    Set-Content -LiteralPath $env:FAKE_BUTLER_STATUS_COUNT -Value $count -Encoding Ascii
    if ($count -eq 1) { Write-Output $env:FAKE_BUTLER_STATUS_BEFORE }
    else { Write-Output $env:FAKE_BUTLER_STATUS_AFTER }
    exit 0
}
if ($argsList[0] -eq "push-preview") {
    Write-Output $env:FAKE_BUTLER_PREVIEW
    exit 0
}
if ($argsList[0] -eq "push") {
    if ([int]$env:FAKE_BUTLER_PUSH_EXIT -ne 0) { exit [int]$env:FAKE_BUTLER_PUSH_EXIT }
    Write-Output "fixture push accepted"
    exit 0
}
throw "Unexpected fake Butler command: $($argsList -join ' ')"
'@ | Set-Content -LiteralPath $FakeButler -Encoding Ascii
@'
param(
    [string]$ReleaseDir,
    [string]$ManifestRelativePath,
    [string]$SignatureRelativePath,
    [string]$BundledPublicKeyName,
    [switch]$AllowRelativeAssetPaths,
    [switch]$RequireExactInventory,
    [string]$PublicKeyUrl
)
if ($env:FAKE_VERIFIER_FAIL -eq "1") {
    throw "Fixture verification rejected the game"
}
Add-Content -LiteralPath $env:FAKE_VERIFIER_LOG -Value "$ReleaseDir|$ManifestRelativePath|$SignatureRelativePath|$BundledPublicKeyName|$AllowRelativeAssetPaths|$RequireExactInventory|$PublicKeyUrl" -Encoding Ascii
'@ | Set-Content -LiteralPath $FakeVerifier -Encoding Ascii

$env:FAKE_BUTLER_LOG = $ButlerLog
$env:FAKE_VERIFIER_LOG = $VerifierLog
$env:FAKE_BUTLER_STATUS_COUNT = $StatusCount

try {
    Write-ReleaseWorkspace -Version "1.9.0" | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $ReleaseRoot "v2.0.0") | Out-Null
    Reset-FakeState
    $RejectedInvalidHighest = $false
    try {
        Invoke-Publisher
    }
    catch {
        $RejectedInvalidHighest = $_.Exception.Message -like "*selected release workspace*"
    }
    if (-not $RejectedInvalidHighest -or (Test-Path -LiteralPath $ButlerLog)) {
        throw "Publisher did not fail closed on the invalid highest SemVer workspace"
    }

    Remove-Item -LiteralPath (Join-Path $ReleaseRoot "v2.0.0") -Recurse -Force
    $Workspace = Write-ReleaseWorkspace -Version "2.0.0"
    Reset-FakeState
    $env:FAKE_VERIFIER_FAIL = "1"
    $RejectedVerification = $false
    try {
        Invoke-Publisher -Version "2.0.0"
    }
    catch {
        $RejectedVerification = $_.Exception.Message -like "*verification rejected*"
    }
    if (-not $RejectedVerification -or (Test-Path -LiteralPath $ButlerLog)) {
        throw "Publisher invoked Butler after signed game verification failed"
    }

    Reset-FakeState
    Invoke-Publisher
    $Calls = @(Get-Content -LiteralPath $ButlerLog)
    if (-not ($Calls -match '^push\|') -or @($Calls -match '^status\|').Count -ne 2) {
        throw "Publisher did not perform one push with preflight and postflight status"
    }
    if (-not ((Get-Content -LiteralPath $VerifierLog -Raw) -like "*$Workspace\game|Verification\SHA256SUMS.txt|Verification\SHA256SUMS.txt.asc*")) {
        throw "Publisher did not verify the selected signed game projection"
    }

    Reset-FakeState
    $env:FAKE_BUTLER_STATUS_BEFORE = $env:FAKE_BUTLER_STATUS_AFTER
    Invoke-Publisher -Version "2.0.0"
    $Calls = @(Get-Content -LiteralPath $ButlerLog)
    if (-not ($Calls -match '^push-preview\|') -or ($Calls -match '^push\|')) {
        throw "Publisher did not treat identical same-version content as idempotent"
    }

    Reset-FakeState
    $env:FAKE_BUTLER_STATUS_BEFORE = $env:FAKE_BUTLER_STATUS_AFTER
    $env:FAKE_BUTLER_PREVIEW = '{"time":1789635168,"type":"result","value":{"channel":"windows","comparison":{"new":1,"modified":0,"deleted":0,"same":101,"newBytes":1,"modifiedBytes":0,"deletedBytes":0,"sameBytes":5102176123},"hasParent":true,"sourceSize":5102176124,"topChangedFiles":{"new":[{"path":"changed.bin","status":"new","size":1}],"modified":[],"deleted":[]}}}'
    $RejectedConflict = $false
    try {
        Invoke-Publisher -Version "2.0.0"
    }
    catch {
        $RejectedConflict = $_.Exception.Message -like "*same-version*"
    }
    if (-not $RejectedConflict -or (@(Get-Content -LiteralPath $ButlerLog) -match '^push\|')) {
        throw "Publisher did not reject changed same-version content before push"
    }

    Reset-FakeState
    $env:FAKE_BUTLER_STATUS_BEFORE = '{"type":"result","value":{"target":"example/game","channels":[{"name":"windows","head":{"id":11,"state":"completed","userVersion":"2.1.0"}}]}}'
    $RejectedDowngrade = $false
    try {
        Invoke-Publisher -Version "2.0.0"
    }
    catch {
        $RejectedDowngrade = $_.Exception.Message -like "*newer version*"
    }
    if (-not $RejectedDowngrade -or (@(Get-Content -LiteralPath $ButlerLog) -match '^push\|')) {
        throw "Publisher did not reject a remote-version downgrade before push"
    }

    Reset-FakeState
    $env:FAKE_BUTLER_STATUS_BEFORE = '{"type":"result","value":{"target":"example/game","channels":[{"name":"windows","pending":{"id":12,"state":"processing","userVersion":"2.1.0"}}]}}'
    $RejectedPending = $false
    try {
        Invoke-Publisher -Version "2.0.0"
    }
    catch {
        $RejectedPending = $_.Exception.Message -like "*pending build*"
    }
    if (-not $RejectedPending -or (@(Get-Content -LiteralPath $ButlerLog) -match '^push\|')) {
        throw "Publisher did not reject a pending remote build before push"
    }

    Reset-FakeState
    $env:FAKE_BUTLER_PUSH_EXIT = "9"
    $RejectedPush = $false
    try {
        Invoke-Publisher -Version "2.0.0"
    }
    catch {
        $RejectedPush = $_.Exception.Message -like "*Butler push failed*"
    }
    if (-not $RejectedPush -or @((Get-Content -LiteralPath $ButlerLog) -match '^status\|').Count -ne 1) {
        throw "Publisher did not propagate Butler push failure without a false read-back"
    }

    $MultiPlatformWorkspace = Write-MultiPlatformReleaseWorkspace -Version "2.1.0"
    Reset-FakeState
    $env:FAKE_BUTLER_STATUS_AFTER = '{"type":"result","value":{"target":"example/game","channels":[{"name":"windows","head":{"id":13,"state":"completed","userVersion":"2.1.0"}}]}}'
    Invoke-Publisher -Version "2.1.0"
    if (-not ((Get-Content -LiteralPath $VerifierLog -Raw) -like "*$MultiPlatformWorkspace\game\windows-x86_64|Verification\SHA256SUMS.txt*")) {
        throw "Publisher did not select the Windows game projection from workspace v2"
    }
    if (-not ((Get-Content -LiteralPath $ButlerLog -Raw) -like "*push|$MultiPlatformWorkspace\game\windows-x86_64|*")) {
        throw "Publisher did not upload the Windows game projection from workspace v2"
    }

    $DryRun = & make -n publish itch PUBLISH_VERSION=2.0.0 2>&1
    if ($LASTEXITCODE -ne 0 -or
        -not ($DryRun -match "publish_itch.ps1") -or
        -not ($DryRun -match '-Target "fallalis/fallalis"')) {
        throw "make publish itch does not route the default ALIS destination to the itch publisher"
    }
    $OverrideDryRun = & make -n publish itch ITCH_TARGET=example/game PUBLISH_VERSION=2.0.0 2>&1
    if ($LASTEXITCODE -ne 0 -or -not ($OverrideDryRun -match '-Target "example/game"')) {
        throw "make publish itch does not honor the explicit destination override"
    }
    $Unknown = & cmd.exe /d /c "make publish steam 2>&1"
    if ($LASTEXITCODE -eq 0 -or -not ($Unknown -match "Unknown publish destination")) {
        throw "make publish accepted an unknown destination"
    }

    Write-Host "[OK] itch publisher selection, verification, refusal, idempotence, and dispatch passed"
}
finally {
    Remove-Item Env:FAKE_BUTLER_LOG, Env:FAKE_VERIFIER_LOG, Env:FAKE_BUTLER_STATUS_COUNT, `
        Env:FAKE_BUTLER_STATUS_BEFORE, Env:FAKE_BUTLER_STATUS_AFTER, `
        Env:FAKE_BUTLER_PREVIEW, Env:FAKE_BUTLER_PUSH_EXIT, `
        Env:FAKE_VERIFIER_FAIL -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $TestRoot) {
        $ResolvedParent = [IO.Path]::GetFullPath($TestParent).TrimEnd('\', '/')
        $ResolvedTest = (Resolve-Path -LiteralPath $TestRoot).Path
        if (-not $ResolvedTest.StartsWith(
                $ResolvedParent + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected publisher test path: $ResolvedTest"
        }
        Remove-Item -LiteralPath $ResolvedTest -Recurse -Force
    }
}
