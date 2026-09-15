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
    [string]$MapLoadReport,
    [string]$ProductTerms,
    [string]$FinalPublicSourceRoot,
    [string]$PublicRemoteUrl = "https://github.com/fallintodusk/alis.git",
    [string]$PublicBranch = "main",
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

function Remove-ObsoleteUnsignedRelease {
    param(
        [string]$Directory,
        [string]$ExpectedDirectory,
        [string]$ReleaseRoot,
        [string]$ReleaseVersion,
        [string]$ReleaseTag,
        [object]$Manifest
    )

    $ResolvedDirectory = [IO.Path]::GetFullPath($Directory).TrimEnd('\', '/')
    $ResolvedExpected = [IO.Path]::GetFullPath($ExpectedDirectory).TrimEnd('\', '/')
    $ResolvedReleaseRoot = [IO.Path]::GetFullPath($ReleaseRoot).TrimEnd('\', '/')
    if (-not $ResolvedDirectory.Equals($ResolvedExpected, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Obsolete release cleanup is limited to the default version path: $ResolvedExpected"
    }
    Assert-PathUnderRoot -Path $ResolvedDirectory -Root $ResolvedReleaseRoot -Description "Obsolete release directory"
    if ($Manifest.schema -notin @("alis-release-manifest-v1", "alis-release-manifest-v2") -or
        $Manifest.status -ne "pending_owner_approval" -or
        $Manifest.release_version -ne $ReleaseVersion -or
        $Manifest.release_tag -ne $ReleaseTag) {
        throw "Obsolete release is not the matching unsigned pending release; it was left unchanged for inspection."
    }

    $OldSigningManifest = Join-Path $ResolvedDirectory "SHA256SUMS.txt"
    $OldSignature = Join-Path $ResolvedDirectory "SHA256SUMS.txt.asc"
    if ((Test-Path -LiteralPath $OldSigningManifest) -or (Test-Path -LiteralPath $OldSignature)) {
        throw "Obsolete release has signed or incomplete signing output; it was left unchanged for inspection."
    }
    $ReleaseItem = Get-Item -LiteralPath $ResolvedDirectory -Force
    if (($ReleaseItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Obsolete release path is a reparse point; it was left unchanged for inspection."
    }

    Remove-Item -LiteralPath $ResolvedDirectory -Recurse -Force
}

function Remove-AutomaticReleaseInputs {
    param(
        [string]$InputRoot,
        [string]$ProjectTmpRoot,
        [bool]$UsesExplicitInputs
    )

    if ($UsesExplicitInputs -or -not (Test-Path -LiteralPath $InputRoot)) {
        return
    }
    Assert-PathUnderRoot -Path $InputRoot -Root $ProjectTmpRoot -Description "Automatic release inputs"
    Remove-Item -LiteralPath $InputRoot -Recurse -Force
}

function Remove-AbandonedReleaseScratch {
    param([string]$ReleaseRoot)

    foreach ($Name in @("work", "c", "final-public-source")) {
        $ScratchRoot = [IO.Path]::GetFullPath((Join-Path $ReleaseRoot $Name))
        Assert-PathUnderRoot -Path $ScratchRoot -Root $ReleaseRoot -Description "Release scratch"
        if (Test-Path -LiteralPath $ScratchRoot) {
            Remove-Item -LiteralPath $ScratchRoot -Recurse -Force
        }
    }
}

$ReleaseTag = "v$ReleaseVersion"
$ResolvedInputRoot = Resolve-ProjectPath -Path $InputRoot -DefaultPath "tmp\release\inputs\$ReleaseTag"
$ResolvedReleaseDir = Resolve-ProjectPath -Path $ReleaseDir -DefaultPath "tmp\release\$ReleaseTag"
$ProjectTmpRoot = Join-Path $ProjectRoot "tmp"
$ReleaseRoot = Join-Path $ProjectTmpRoot "release"
Assert-PathUnderRoot -Path $ResolvedReleaseDir -Root $ProjectTmpRoot -Description "ReleaseDir"
Remove-AbandonedReleaseScratch -ReleaseRoot $ReleaseRoot
$UsesExplicitInputs = -not [string]::IsNullOrWhiteSpace($PublicSourceRoot) -or
    -not [string]::IsNullOrWhiteSpace($PlayerPackageRoot) -or
    -not [string]::IsNullOrWhiteSpace($PlayerEvidence) -or
    -not [string]::IsNullOrWhiteSpace($DeveloperReleaseDir) -or
    -not [string]::IsNullOrWhiteSpace($ComponentManifest) -or
    -not [string]::IsNullOrWhiteSpace($DependencyReport) -or
    -not [string]::IsNullOrWhiteSpace($PrivacyReport) -or
    -not [string]::IsNullOrWhiteSpace($MapLoadReport)
$SigningManifest = Join-Path $ResolvedReleaseDir "SHA256SUMS.txt"
$Signature = Join-Path $ResolvedReleaseDir "SHA256SUMS.txt.asc"
$PreparedThisRun = $false
$AutomaticPlayerEvidence = $null

if (Test-Path -LiteralPath $ResolvedReleaseDir -PathType Container) {
    $ExistingManifest = Read-ReleaseManifest -Directory $ResolvedReleaseDir
    if ($ExistingManifest.schema -ne "alis-release-manifest-v3") {
        if (-not $SkipSigning) {
            throw "Release directory uses an obsolete public layout. Run the unsigned release command first to remove and replace it."
        }
        $ExpectedReleaseDir = Join-Path $ReleaseRoot $ReleaseTag
        Remove-ObsoleteUnsignedRelease `
            -Directory $ResolvedReleaseDir `
            -ExpectedDirectory $ExpectedReleaseDir `
            -ReleaseRoot $ReleaseRoot `
            -ReleaseVersion $ReleaseVersion `
            -ReleaseTag $ReleaseTag `
            -Manifest $ExistingManifest
        Write-Host "[Release] Removed obsolete unsigned release: $ResolvedReleaseDir"
    }
}

if (-not (Test-Path -LiteralPath $ResolvedReleaseDir -PathType Container)) {
    if (-not $UsesExplicitInputs) {
        $InputsToReplace = if ([string]::IsNullOrWhiteSpace($InputRoot)) {
            Join-Path $ReleaseRoot "inputs"
        }
        else {
            $ResolvedInputRoot
        }
        if (Test-Path -LiteralPath $InputsToReplace) {
            Assert-PathUnderRoot -Path $InputsToReplace -Root $ProjectTmpRoot -Description "InputRoot"
            Remove-Item -LiteralPath $InputsToReplace -Recurse -Force
        }

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
    $ResolvedMapLoadReport = Resolve-ProjectPath -Path $MapLoadReport -DefaultPath (Join-Path $ResolvedInputRoot "reports\public-world-map-load.json")
    $ResolvedProductTerms = Resolve-ProjectPath -Path $ProductTerms -DefaultPath "PRODUCT_TERMS.txt"

    $RequiredDirectories = @($ResolvedPublicSourceRoot, $ResolvedPlayerPackageRoot, $ResolvedDeveloperReleaseDir)
    foreach ($RequiredDirectory in $RequiredDirectories) {
        if (-not (Test-Path -LiteralPath $RequiredDirectory -PathType Container)) {
            throw "Required release input directory is missing: $RequiredDirectory"
        }
    }
    $RequiredFiles = @($ResolvedPlayerEvidence, $ResolvedComponentManifest, $ResolvedDependencyReport, $ResolvedPrivacyReport, $ResolvedMapLoadReport, $ResolvedProductTerms)
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
        "-MapLoadReport", $ResolvedMapLoadReport,
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
if ($Manifest.schema -ne "alis-release-manifest-v3") {
    throw "Release directory uses an unsupported manifest schema."
}
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
    Remove-AutomaticReleaseInputs `
        -InputRoot $ResolvedInputRoot `
        -ProjectTmpRoot $ProjectTmpRoot `
        -UsesExplicitInputs $UsesExplicitInputs
    Write-Host "[OK] Existing signed release verified: $ResolvedReleaseDir" -ForegroundColor Green
    exit 0
}

Invoke-PythonReleaseTool -Arguments @("verify", "--release-dir", $ResolvedReleaseDir)

if ($PreparedThisRun -and -not $SkipSigning) {
    Write-Host "[CHECKPOINT] Unsigned release prepared and verified: $ResolvedReleaseDir" -ForegroundColor Yellow
    Write-Host "Review README.txt and the flat public asset set, then rerun:"
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

$OwnedFinalPublicSource = $null
$OwnedFinalPublicSourceParent = $null
try {
    if ($Manifest.status -eq "pending_owner_approval") {
        $ResolvedFinalPublicSource = if ([string]::IsNullOrWhiteSpace($FinalPublicSourceRoot)) {
            $FinalSourceParent = Join-Path $ProjectRoot "tmp\release\final-public-source"
            $FinalSource = Join-Path $FinalSourceParent $ReleaseTag
            $ResolvedFinal = [IO.Path]::GetFullPath($FinalSource)
            Assert-PathUnderRoot `
                -Path $ResolvedFinal `
                -Root $FinalSourceParent `
                -Description "Final public source checkout"
            $OwnedFinalPublicSource = $ResolvedFinal
            $OwnedFinalPublicSourceParent = $FinalSourceParent
            if (Test-Path -LiteralPath $ResolvedFinal) {
                Remove-Item -LiteralPath $ResolvedFinal -Recurse -Force
            }
            New-Item -ItemType Directory -Path $FinalSourceParent -Force | Out-Null
            & git -c core.longpaths=true clone --no-tags --depth 1 --branch $PublicBranch `
                $PublicRemoteUrl $ResolvedFinal
            if ($LASTEXITCODE -ne 0) {
                throw "Unable to resolve final public source branch $PublicBranch."
            }
            & git -C $ResolvedFinal config core.longpaths true
            if ($LASTEXITCODE -ne 0) {
                throw "Unable to persist long-path support in the final public source checkout."
            }
            $ResolvedFinal
        }
        else {
            Resolve-ProjectPath -Path $FinalPublicSourceRoot -DefaultPath ""
        }
        & $Python.Source (Join-Path $ScriptDir "finalize_release.py") `
            --release-dir $ResolvedReleaseDir `
            --final-public-source $ResolvedFinalPublicSource `
            --branch $PublicBranch
        if ($LASTEXITCODE -ne 0) {
            throw "Final public source binding failed."
        }
        $Manifest = Read-ReleaseManifest -Directory $ResolvedReleaseDir
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
}
finally {
    if ($OwnedFinalPublicSource -and (Test-Path -LiteralPath $OwnedFinalPublicSource)) {
        Assert-PathUnderRoot `
            -Path $OwnedFinalPublicSource `
            -Root $OwnedFinalPublicSourceParent `
            -Description "Final public source checkout"
        Remove-Item -LiteralPath $OwnedFinalPublicSource -Recurse -Force
    }
}

Remove-AutomaticReleaseInputs `
    -InputRoot $ResolvedInputRoot `
    -ProjectTmpRoot $ProjectTmpRoot `
    -UsesExplicitInputs $UsesExplicitInputs

Write-Host "[OK] Signed release is ready for upload: $ResolvedReleaseDir" -ForegroundColor Green
Write-Host "[OK] No remote write was performed." -ForegroundColor Green
