#Requires -Version 5.1
# License terms: see repository root LICENSE.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$')]
    [string]$ReleaseVersion,
    [Parameter(Mandatory = $true)]
    [string]$InputRoot,
    [string]$PublicAssetRoot
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$ReleaseTag = "v$ReleaseVersion"
$InputRoot = [IO.Path]::GetFullPath($InputRoot)
$TmpRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "tmp")).TrimEnd('\', '/')
$TmpPrefix = $TmpRoot + [IO.Path]::DirectorySeparatorChar
if (-not $InputRoot.StartsWith($TmpPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "InputRoot must remain under $TmpRoot`: $InputRoot"
}
if (Test-Path -LiteralPath $InputRoot) {
    throw "Automatic release InputRoot must not exist: $InputRoot"
}

$WorkParent = Join-Path $ProjectRoot "tmp\release\work\$ReleaseTag"
$WorkRoot = Join-Path $WorkParent ([guid]::NewGuid().ToString("N"))
$PublicSource = Join-Path $WorkRoot "public-source"
$Developer = Join-Path $WorkRoot "developer"
$Reports = Join-Path $WorkRoot "reports"
$ManifestProjection = Join-Path $WorkRoot "public-world-manifests"
$CleanCheckout = Join-Path (Join-Path $ProjectRoot "tmp\release\c") `
    ([guid]::NewGuid().ToString("N"))
$InputsPromoted = $false

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Description,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )
    & $Action
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

function Invoke-PublicReleaseComposition {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestRoot,
        [Parameter(Mandatory = $true)][string]$AssetRoot
    )

    Invoke-Checked "Public World manifest staging" {
        & python (Join-Path $ProjectRoot "scripts\git\mirror\stage_public_world_manifests.py") `
            --source-root $ManifestRoot --asset-root $AssetRoot --output-root $ManifestProjection
    }

    $PrivacyReport = Join-Path $Reports "public-source-privacy.json"
    $MirrorArguments = @(
        "--remote-url", "https://github.com/fallintodusk/alis.git",
        "--branch", "release/$ReleaseTag-source",
        "--dry-run", "--official-release", "--force",
        "--developer-release-dir", $Developer,
        "--developer-version", $ReleaseVersion,
        "--candidate-dir", $PublicSource,
        "--public-world-manifest-root", $ManifestProjection,
        "--developer-asset-root", $AssetRoot,
        "--report", $PrivacyReport
    )
    Invoke-Checked "Public source/developer projection" {
        & (Join-Path $ProjectRoot "scripts\git\mirror\mirror_to_github.ps1") @MirrorArguments
    }
}

try {
    New-Item -ItemType Directory -Path $Reports -Force | Out-Null
    if ($PublicAssetRoot) {
        $ResolvedAssetRoot = [IO.Path]::GetFullPath($PublicAssetRoot)
        $ResolvedManifestRoot = Join-Path $ResolvedAssetRoot `
            'Plugins\World\ProjectWorldData\Data\Manifests'
        Invoke-PublicReleaseComposition -ManifestRoot $ResolvedManifestRoot `
            -AssetRoot $ResolvedAssetRoot
    }
    else {
        . (Join-Path $ScriptDir 'public_world_projection.ps1')
        Invoke-WithProjectWorldPublicProjection -ProjectRoot $ProjectRoot -WorkRoot $WorkRoot `
            -Action {
                param($PublicManifestRoot, $LiveProjectRoot)
                Invoke-PublicReleaseComposition -ManifestRoot $PublicManifestRoot `
                    -AssetRoot $LiveProjectRoot
            }
    }

    $ComponentReport = Join-Path $Reports "effective-component-manifest.json"
    Invoke-Checked "Component-license manifest" {
        & python (Join-Path $PublicSource "scripts\ue\check\governance\generate_component_manifest.py") `
            --repo-root $PublicSource --tag $ReleaseTag --output $ComponentReport
    }

    Invoke-Checked "Isolated public checkout" {
        & git -c core.longpaths=true -c advice.detachedHead=false clone --quiet --no-local --branch $ReleaseTag `
            $PublicSource $CleanCheckout
    }
    Invoke-Checked "Public checkout long-path configuration" {
        & git -C $CleanCheckout config core.longpaths true
    }
    $Installer = Join-Path $CleanCheckout "scripts\git\mirror\install_developer_payload.ps1"
    Invoke-Checked "Developer payload installation" {
        & $Installer -ProjectRoot $CleanCheckout -ReleaseDir $Developer
    }
    Invoke-Checked "Developer payload no-op reinstall" {
        & $Installer -ProjectRoot $CleanCheckout -ReleaseDir $Developer
    }

    . (Join-Path $ProjectRoot "scripts\config\Resolve-UEConfig.ps1")
    $PrivateConfig = Resolve-UEConfig -ConfigDir (Join-Path $ProjectRoot "scripts\config")
    $LocalConfig = Join-Path $CleanCheckout "scripts\config\ue_path.local.conf"
    "UE_PATH=$($PrivateConfig.UE_PATH.Replace('\', '/'))" | Set-Content -LiteralPath $LocalConfig -Encoding Ascii
    Invoke-Checked "Public developer build" {
        & (Join-Path $CleanCheckout "scripts\ue\standalone\build.ps1")
    }
    Invoke-Checked "Public skill projection" {
        & (Join-Path $CleanCheckout "scripts\agents\link_codex_skills.ps1")
        & (Join-Path $CleanCheckout "scripts\agents\link_codex_skills.ps1") -Verify
    }

    $DependencyReport = Join-Path $Reports "developer-dependency-report.json"
    Invoke-Checked "Developer dependency audit" {
        & (Join-Path $CleanCheckout "scripts\git\mirror\audit_developer_payload.ps1") `
            -OutputPath $DependencyReport
    }
    $MapReceipt = Join-Path $Reports "public-world-map-load.json"
    $env:ALIS_PUBLIC_MAP_RECEIPT = $MapReceipt
    try {
        $Editor = Join-Path $PrivateConfig.UE_PATH "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
        $MapVerifier = Join-Path $CleanCheckout "scripts\ue\editor\level\verify_public_world_maps.py"
        Invoke-Checked "Kazan/Manhattan map load" {
            & $Editor (Join-Path $CleanCheckout "Alis.uproject") -run=pythonscript `
                "-script=$MapVerifier" -unattended -nop4 -NoSound -NullRHI
        }
    }
    finally {
        Remove-Item Env:ALIS_PUBLIC_MAP_RECEIPT -ErrorAction SilentlyContinue
    }
    $MapResult = Get-Content -LiteralPath $MapReceipt -Raw | ConvertFrom-Json
    if ($MapResult.status -cne "accepted" -or @($MapResult.maps).Count -ne 2) {
        throw "Public map-load receipt was not accepted."
    }
    $Status = @(& git -C $CleanCheckout status --porcelain=v1 --untracked-files=all)
    if ($LASTEXITCODE -ne 0 -or ($Status | Where-Object { $_ -notmatch '^!!' -and -not [string]::IsNullOrWhiteSpace($_) })) {
        throw "Public verification checkout did not remain clean."
    }

    Remove-Item -LiteralPath $ManifestProjection -Recurse -Force
    New-Item -ItemType Directory -Path (Split-Path -Parent $InputRoot) -Force | Out-Null
    Move-Item -LiteralPath $WorkRoot -Destination $InputRoot
    $InputsPromoted = $true
    Write-Host "[OK] Release inputs prepared automatically: $InputRoot" -ForegroundColor Green
}
finally {
    if (Test-Path -LiteralPath $CleanCheckout) {
        Remove-Item -LiteralPath $CleanCheckout -Recurse -Force
    }
    if (-not $InputsPromoted -and (Test-Path -LiteralPath $WorkRoot)) {
        Remove-Item -LiteralPath $WorkRoot -Recurse -Force
    }
    if ((Test-Path -LiteralPath $WorkParent) -and
        @(Get-ChildItem -LiteralPath $WorkParent -Force).Count -eq 0) {
        Remove-Item -LiteralPath $WorkParent -Force
    }
}
