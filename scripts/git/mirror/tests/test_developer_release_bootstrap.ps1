#Requires -Version 5.1

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$MirrorDir = Split-Path -Parent $ScriptDir
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MirrorDir))
$TestParent = Join-Path $ProjectRoot "tmp\release\developer-bootstrap-test"
$TestRoot = Join-Path $TestParent ([guid]::NewGuid().ToString("N"))
$Seed = Join-Path $TestRoot "seed"
$ReleaseRoot = Join-Path $TestRoot "release"
$DeveloperRoot = Join-Path $ReleaseRoot "developer"
$Destination = Join-Path $TestRoot "installed"
$DefaultDestination = Join-Path $TestRoot "Alis-v9.8.7"

New-Item -ItemType Directory -Path (Join-Path $Seed "scripts\git\mirror"), $DeveloperRoot -Force | Out-Null

function Read-Host {
    param([string]$Prompt)

    return ""
}

try {
    $BootstrapSource = Get-Content -LiteralPath (Join-Path $MirrorDir "bootstrap_developer_release.ps1") -Raw
    if (-not $BootstrapSource.Contains(
            'Read-Host "Developer checkout location [$DefaultDestination] (press Enter to accept)"')) {
        throw "Developer bootstrap prompt does not explain how to accept the default location."
    }
    "fixture" | Set-Content -LiteralPath (Join-Path $Seed "Alis.uproject") -Encoding Ascii
    @'
param(
    [string]$ProjectRoot,
    [string]$ReleaseDir,
    [string]$ManifestPath,
    [switch]$RequireReleaseSignature
)
if (-not $RequireReleaseSignature) { throw "signature was not required" }
if ($env:ALIS_TEST_BOOTSTRAP_FAIL -eq "1") { throw "fixture install failure" }
"$ReleaseDir|$ManifestPath" | Set-Content -LiteralPath (Join-Path $ProjectRoot "bootstrap-invocation.txt") -Encoding Ascii
'@ | Set-Content -LiteralPath (Join-Path $Seed "scripts\git\mirror\install_developer_payload.ps1") -Encoding Ascii
    & git -C $Seed init -q
    & git -C $Seed config user.name test
    & git -C $Seed config user.email test@localhost
    & git -C $Seed add .
    & git -C $Seed commit -q -m fixture
    & git -C $Seed tag v9.8.7
    $Revision = (& git -C $Seed rev-parse HEAD | Out-String).Trim()

    [ordered]@{
        schema_version = 2
        public_source = [ordered]@{
            tag = "v9.8.7"
            revision = $Revision
        }
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $DeveloperRoot "fixture.developer-payload.json") -Encoding Ascii
    "fixture" | Set-Content -LiteralPath (Join-Path $ReleaseRoot "SHA256SUMS.txt") -Encoding Ascii
    "fixture" | Set-Content -LiteralPath (Join-Path $ReleaseRoot "SHA256SUMS.txt.asc") -Encoding Ascii
    Copy-Item -LiteralPath (Join-Path $MirrorDir "bootstrap_developer_release.ps1") -Destination (Join-Path $DeveloperRoot "INSTALL_ALIS_DEVELOPER.ps1")

    & (Join-Path $DeveloperRoot "INSTALL_ALIS_DEVELOPER.ps1") -RepositoryUrl $Seed
    if (-not (Test-Path -LiteralPath (Join-Path $DefaultDestination "bootstrap-invocation.txt") -PathType Leaf)) {
        throw "Developer bootstrap default destination must be beside, not inside, the release directory."
    }
    if (Test-Path -LiteralPath (Join-Path $ReleaseRoot "Alis-v9.8.7")) {
        throw "Developer bootstrap contaminated its release directory."
    }

    & (Join-Path $DeveloperRoot "INSTALL_ALIS_DEVELOPER.ps1") -Destination $Destination -RepositoryUrl $Seed
    $Invocation = Join-Path $Destination "bootstrap-invocation.txt"
    if (-not (Test-Path -LiteralPath $Invocation -PathType Leaf)) {
        throw "Developer bootstrap did not delegate to the checkout-local installer."
    }
    $ExpectedInvocation = "$ReleaseRoot|$(Join-Path $DeveloperRoot 'fixture.developer-payload.json')"
    if ((Get-Content -LiteralPath $Invocation -Raw).Trim() -cne $ExpectedInvocation) {
        throw "Developer bootstrap passed the wrong release boundary to the trusted installer."
    }

    $FailedDestination = Join-Path $TestRoot "failed-install"
    $InstallFailureRejected = $false
    $env:ALIS_TEST_BOOTSTRAP_FAIL = "1"
    try {
        & (Join-Path $DeveloperRoot "INSTALL_ALIS_DEVELOPER.ps1") `
            -Destination $FailedDestination -RepositoryUrl $Seed
    } catch {
        $InstallFailureRejected = $_.Exception.Message -like "*fixture install failure*"
    } finally {
        Remove-Item Env:\ALIS_TEST_BOOTSTRAP_FAIL -ErrorAction SilentlyContinue
    }
    if (-not $InstallFailureRejected -or (Test-Path -LiteralPath $FailedDestination)) {
        throw "Developer bootstrap did not remove its failed checkout."
    }

    Remove-Item -LiteralPath (Join-Path $ReleaseRoot "SHA256SUMS.txt.asc") -Force
    $RejectedDestination = Join-Path $TestRoot "unsigned"
    $Rejected = $false
    try {
        & (Join-Path $DeveloperRoot "INSTALL_ALIS_DEVELOPER.ps1") -Destination $RejectedDestination -RepositoryUrl $Seed
    } catch {
        $Rejected = $_.Exception.Message -like "*incomplete*"
    }
    if (-not $Rejected -or (Test-Path -LiteralPath $RejectedDestination)) {
        throw "Developer bootstrap did not reject incomplete signing files before cloning."
    }

    Write-Host "[OK] Developer bootstrap cloned the exact tag and delegated to its trusted installer"
} finally {
    if (Test-Path -LiteralPath $TestRoot) {
        $ResolvedParent = [IO.Path]::GetFullPath($TestParent).TrimEnd('\', '/')
        $ResolvedTestRoot = (Resolve-Path -LiteralPath $TestRoot).Path
        if (-not $ResolvedTestRoot.StartsWith(
                $ResolvedParent + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected test path: $ResolvedTestRoot"
        }
        Remove-Item -LiteralPath $ResolvedTestRoot -Recurse -Force
    }
}
