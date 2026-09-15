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

New-Item -ItemType Directory -Path (Join-Path $Seed "scripts\git\mirror"), $DeveloperRoot -Force | Out-Null

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

    & (Join-Path $DeveloperRoot "INSTALL_ALIS_DEVELOPER.ps1") -Destination $Destination -RepositoryUrl $Seed
    $Invocation = Join-Path $Destination "bootstrap-invocation.txt"
    if (-not (Test-Path -LiteralPath $Invocation -PathType Leaf)) {
        throw "Developer bootstrap did not delegate to the checkout-local installer."
    }
    $ExpectedInvocation = "$ReleaseRoot|$(Join-Path $DeveloperRoot 'fixture.developer-payload.json')"
    if ((Get-Content -LiteralPath $Invocation -Raw).Trim() -cne $ExpectedInvocation) {
        throw "Developer bootstrap passed the wrong release boundary to the trusted installer."
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
