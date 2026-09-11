#Requires -Version 5.1
# License terms: see repository root LICENSE.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$')]
    [string]$ReleaseVersion,
    [switch]$SkipSigning,
    [string]$InputRoot,
    [string]$ReleaseDir,
    [string]$PublicSourceRoot,
    [string]$PlayerPackageRoot,
    [string]$PlayerEvidence,
    [string]$DeveloperReleaseDir,
    [string]$ComponentManifest,
    [string]$DependencyReport,
    [string]$PrivacyReport,
    [string]$ProductTerms,
    [string]$GpgPath,
    [string]$GpgHome
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$Python = Get-Command python -ErrorAction SilentlyContinue
if (-not $Python) {
    throw "Python was not found on PATH."
}

function Resolve-ProjectPath {
    param(
        [string]$Path,
        [string]$DefaultPath
    )

    $Candidate = if ([string]::IsNullOrWhiteSpace($Path)) { $DefaultPath } else { $Path }
    if (-not [IO.Path]::IsPathRooted($Candidate)) {
        $Candidate = Join-Path $ProjectRoot $Candidate
    }
    return [IO.Path]::GetFullPath($Candidate)
}

function Assert-PathUnderRoot {
    param(
        [string]$Path,
        [string]$Root,
        [string]$Description
    )

    $ResolvedRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    if (-not $Path.StartsWith(
            $ResolvedRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must remain under $ResolvedRoot`: $Path"
    }
}

function Invoke-PythonReleaseTool {
    param([string[]]$Arguments)

    & $Python.Source (Join-Path $ScriptDir "prepare_release.py") @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Release manifest command failed: $($Arguments -join ' ')"
    }
}

function Get-OneFile {
    param(
        [string]$Directory,
        [string]$Filter,
        [string]$Description
    )

    $Matches = @(Get-ChildItem -LiteralPath $Directory -Filter $Filter -File -ErrorAction Stop)
    if ($Matches.Count -ne 1) {
        throw "Expected exactly one $Description in $Directory; found $($Matches.Count)."
    }
    return $Matches[0].FullName
}

function Read-ReleaseManifest {
    param([string]$Directory)

    $ManifestPath = Join-Path $Directory "release_manifest.json"
    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        throw "Release manifest is missing: $ManifestPath"
    }
    return Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
}

$ReleaseTag = "v$ReleaseVersion"
$ResolvedInputRoot = Resolve-ProjectPath -Path $InputRoot -DefaultPath "tmp\release\inputs\$ReleaseTag"
$ResolvedReleaseDir = Resolve-ProjectPath -Path $ReleaseDir -DefaultPath "tmp\release\$ReleaseTag"
$ProjectTmpRoot = Join-Path $ProjectRoot "tmp"
Assert-PathUnderRoot -Path $ResolvedReleaseDir -Root $ProjectTmpRoot -Description "ReleaseDir"
$UsesExplicitInputs = -not [string]::IsNullOrWhiteSpace($PublicSourceRoot) -or
    -not [string]::IsNullOrWhiteSpace($PlayerPackageRoot) -or
    -not [string]::IsNullOrWhiteSpace($PlayerEvidence) -or
    -not [string]::IsNullOrWhiteSpace($DeveloperReleaseDir) -or
    -not [string]::IsNullOrWhiteSpace($ComponentManifest) -or
    -not [string]::IsNullOrWhiteSpace($DependencyReport) -or
    -not [string]::IsNullOrWhiteSpace($PrivacyReport)
$SigningManifest = Join-Path $ResolvedReleaseDir "SHA256SUMS.txt"
$Signature = Join-Path $ResolvedReleaseDir "SHA256SUMS.txt.asc"
$PreparedThisRun = $false
$AutomaticPlayerEvidence = $null

if (-not (Test-Path -LiteralPath $ResolvedReleaseDir -PathType Container)) {
    if (-not $UsesExplicitInputs) {
        $AcceptanceScript = Join-Path $ProjectRoot "scripts\ue\world\accept_playable_tour_candidate.ps1"
        $StatusJson = @(& $AcceptanceScript -Mode Status -ReleaseVersion $ReleaseVersion)
        if ($LASTEXITCODE -ne 0 -or $StatusJson.Count -ne 1) {
            throw "Unable to resolve the player Candidate acceptance state."
        }
        $AcceptanceStatus = $StatusJson[0] | ConvertFrom-Json
        if ($AcceptanceStatus.state -in @("candidate_missing", "candidate_stale")) {
            Write-Host "[Release] Building and machine-verifying the exact player Candidate."
            & (Join-Path $ProjectRoot "scripts\ue\world\test\performance\run_kazan_playable_tour.ps1")
            if ($LASTEXITCODE -ne 0) {
                throw "Player Candidate machine acceptance failed."
            }
            $StatusJson = @(& $AcceptanceScript -Mode Status -ReleaseVersion $ReleaseVersion)
            if ($LASTEXITCODE -ne 0 -or $StatusJson.Count -ne 1) {
                throw "Unable to recheck the player Candidate acceptance state."
            }
            $AcceptanceStatus = $StatusJson[0] | ConvertFrom-Json
            if ($AcceptanceStatus.state -notin @("awaiting_operator", "accepted")) {
                throw "The machine gate did not produce a reviewable Candidate."
            }
        }
        if ($AcceptanceStatus.state -notin @("awaiting_operator", "accepted") -or
            [string]::IsNullOrWhiteSpace([string]$AcceptanceStatus.composite)) {
            throw "The player Candidate has no current machine-acceptance evidence."
        }
        $AutomaticPlayerEvidence = [string]$AcceptanceStatus.composite

        if (Test-Path -LiteralPath $ResolvedInputRoot) {
            Assert-PathUnderRoot -Path $ResolvedInputRoot -Root $ProjectTmpRoot -Description "InputRoot"
            Remove-Item -LiteralPath $ResolvedInputRoot -Recurse -Force
        }
        & (Join-Path $ScriptDir "prepare_release_inputs.ps1") `
            -ReleaseVersion $ReleaseVersion -InputRoot $ResolvedInputRoot
        if ($LASTEXITCODE -ne 0) {
            throw "Automatic release input preparation failed."
        }
    }

    $ResolvedPublicSourceRoot = Resolve-ProjectPath -Path $PublicSourceRoot -DefaultPath (Join-Path $ResolvedInputRoot "public-source")
    $ResolvedPlayerPackageRoot = Resolve-ProjectPath -Path $PlayerPackageRoot -DefaultPath "Saved\PackageRelease\KazanPlayableTour\Candidate"
    $ResolvedPlayerEvidence = if ($AutomaticPlayerEvidence) {
        [IO.Path]::GetFullPath($AutomaticPlayerEvidence)
    }
    else {
        Resolve-ProjectPath -Path $PlayerEvidence -DefaultPath "Saved\Validation\WorldRealization\playable-tour\Candidate\operator-acceptance.json"
    }
    $ResolvedDeveloperReleaseDir = Resolve-ProjectPath -Path $DeveloperReleaseDir -DefaultPath (Join-Path $ResolvedInputRoot "developer")
    $ResolvedComponentManifest = Resolve-ProjectPath -Path $ComponentManifest -DefaultPath (Join-Path $ResolvedInputRoot "reports\effective-component-manifest.json")
    $ResolvedDependencyReport = Resolve-ProjectPath -Path $DependencyReport -DefaultPath (Join-Path $ResolvedInputRoot "reports\developer-dependency-report.json")
    $ResolvedPrivacyReport = Resolve-ProjectPath -Path $PrivacyReport -DefaultPath (Join-Path $ResolvedInputRoot "reports\public-source-privacy.json")
    $ResolvedProductTerms = Resolve-ProjectPath -Path $ProductTerms -DefaultPath "PRODUCT_TERMS.txt"

    $RequiredDirectories = @($ResolvedPublicSourceRoot, $ResolvedPlayerPackageRoot, $ResolvedDeveloperReleaseDir)
    foreach ($RequiredDirectory in $RequiredDirectories) {
        if (-not (Test-Path -LiteralPath $RequiredDirectory -PathType Container)) {
            throw "Required release input directory is missing: $RequiredDirectory"
        }
    }
    $RequiredFiles = @($ResolvedPlayerEvidence, $ResolvedComponentManifest, $ResolvedDependencyReport, $ResolvedPrivacyReport, $ResolvedProductTerms)
    foreach ($RequiredFile in $RequiredFiles) {
        if (-not (Test-Path -LiteralPath $RequiredFile -PathType Leaf)) {
            throw "Required release input file is missing: $RequiredFile"
        }
    }

    $DeveloperPayloadManifest = Get-OneFile -Directory $ResolvedDeveloperReleaseDir -Filter "*.developer-payload.json" -Description "developer payload manifest"
    $AttributionNotice = Get-OneFile -Directory $ResolvedDeveloperReleaseDir -Filter "*.notices.json" -Description "developer attribution notice"
    $PrepareArguments = @(
        "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass",
        "-File", (Join-Path $ScriptDir "prepare_release.ps1"),
        "-ReleaseDir", $ResolvedReleaseDir,
        "-PublicSourceRoot", $ResolvedPublicSourceRoot,
        "-PlayerPackageRoot", $ResolvedPlayerPackageRoot,
        "-PlayerEvidence", $ResolvedPlayerEvidence,
        "-DeveloperReleaseDir", $ResolvedDeveloperReleaseDir,
        "-DeveloperPayloadManifest", $DeveloperPayloadManifest,
        "-ComponentManifest", $ResolvedComponentManifest,
        "-DependencyReport", $ResolvedDependencyReport,
        "-PrivacyReport", $ResolvedPrivacyReport,
        "-AttributionNotice", $AttributionNotice,
        "-ProductTerms", $ResolvedProductTerms,
        "-ReleaseVersion", $ReleaseVersion,
        "-ReleaseTag", $ReleaseTag
    )
    & powershell.exe @PrepareArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Release preparation failed."
    }
    $PreparedThisRun = $true
}

$Manifest = Read-ReleaseManifest -Directory $ResolvedReleaseDir
if ($Manifest.release_version -ne $ReleaseVersion -or $Manifest.release_tag -ne $ReleaseTag) {
    throw "Release directory identity does not match requested version $ReleaseVersion."
}

$HasSigningManifest = Test-Path -LiteralPath $SigningManifest -PathType Leaf
$HasSignature = Test-Path -LiteralPath $Signature -PathType Leaf
if ($HasSigningManifest -ne $HasSignature) {
    throw "Release directory contains an incomplete signing result; inspect it before retrying."
}

if ($HasSigningManifest) {
    if ($SkipSigning) {
        throw "Unsigned rehearsal requires an unsigned release directory: $ResolvedReleaseDir"
    }
    & (Join-Path $ScriptDir "verify_release.ps1") -ReleaseDir $ResolvedReleaseDir -GpgPath $GpgPath
    if (-not $?) {
        throw "Consumer-side release verification failed."
    }
    Write-Host "[OK] Existing signed release verified: $ResolvedReleaseDir" -ForegroundColor Green
    exit 0
}

Invoke-PythonReleaseTool -Arguments @("verify", "--release-dir", $ResolvedReleaseDir)

if ($PreparedThisRun -and -not $SkipSigning) {
    Write-Host "[CHECKPOINT] Unsigned release prepared and verified: $ResolvedReleaseDir" -ForegroundColor Yellow
    Write-Host "Review the exact Product build, release files, and PRODUCT_TERMS.txt, then rerun:"
    Write-Host "  make release $ReleaseVersion"
    exit 0
}

if ($SkipSigning) {
    if ($Manifest.status -ne "pending_owner_approval") {
        throw "Unsigned rehearsal expected pending_owner_approval, found $($Manifest.status)."
    }
    Write-Host "[OK] Unsigned release prepared and verified: $ResolvedReleaseDir" -ForegroundColor Green
    Write-Host "[OK] Private-key access and signing were skipped." -ForegroundColor Green
    exit 0
}

if ($Manifest.status -eq "pending_owner_approval") {
    Write-Host "Review the exact bundled PRODUCT_TERMS.txt and release artifacts before approval."
    $Confirmation = Read-Host "Type APPROVE $ReleaseVersion to approve the exact Product, terms, and rights"
    if ($Confirmation -cne "APPROVE $ReleaseVersion") {
        throw "Release owner approval was not granted; signing was not started."
    }
    Invoke-PythonReleaseTool -Arguments @(
        "approve", "--release-dir", $ResolvedReleaseDir,
        "--approve-product-terms-and-rights"
    )
}

Invoke-PythonReleaseTool -Arguments @("verify", "--release-dir", $ResolvedReleaseDir, "--require-ready")
$SignArguments = @{ ReleaseDir = $ResolvedReleaseDir }
if ($GpgPath) { $SignArguments.GpgPath = $GpgPath }
if ($GpgHome) { $SignArguments.GpgHome = $GpgHome }
& (Join-Path $ScriptDir "sign_release.ps1") @SignArguments
if (-not $?) {
    throw "Release signing failed."
}
& (Join-Path $ScriptDir "verify_release.ps1") -ReleaseDir $ResolvedReleaseDir -GpgPath $GpgPath
if (-not $?) {
    throw "Consumer-side release verification failed."
}

Write-Host "[OK] Signed release is ready for upload: $ResolvedReleaseDir" -ForegroundColor Green
Write-Host "[OK] No remote write was performed." -ForegroundColor Green
