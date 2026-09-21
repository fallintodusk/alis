#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ReleaseDir,
    [Parameter(Mandatory = $true)][string]$PublicSourceRoot,
    [Parameter(Mandatory = $true)][string]$PlayerPackageRoot,
    [Parameter(Mandatory = $true)][string]$PlayerEvidence,
    [string]$LinuxPackageRoot,
    [string]$LinuxAcceptance,
    [Parameter(Mandatory = $true)][string]$DeveloperReleaseDir,
    [Parameter(Mandatory = $true)][string]$DeveloperPayloadManifest,
    [Parameter(Mandatory = $true)][string]$ComponentManifest,
    [Parameter(Mandatory = $true)][string]$DependencyReport,
    [Parameter(Mandatory = $true)][string]$PrivacyReport,
    [Parameter(Mandatory = $true)][string]$MapLoadReport,
    [Parameter(Mandatory = $true)][string]$AttributionNotice,
    [Parameter(Mandatory = $true)][string]$ProductTerms,
    [string]$ReleaseVersion = "2.0.0",
    [string]$ReleaseTag = "v2.0.0",
    [int]$SplitSizeMiB = 1900,
    [string]$SevenZip
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$Implementation = Join-Path $ScriptDir "prepare_release.py"
$WorkspaceTool = Join-Path $ScriptDir "release_workspace.py"
$Python = Get-Command python -ErrorAction SilentlyContinue
if (-not $Python) {
    throw "Python was not found on PATH."
}

$ResolvedReleaseDir = [IO.Path]::GetFullPath($ReleaseDir)
$TmpRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "tmp")).TrimEnd('\', '/')
if (-not $ResolvedReleaseDir.StartsWith(
        $TmpRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "ReleaseDir must remain under the project tmp directory: $ResolvedReleaseDir"
}
if (Test-Path -LiteralPath $ResolvedReleaseDir) {
    throw "ReleaseDir already exists: $ResolvedReleaseDir"
}

$ReleaseRoot = Join-Path $TmpRoot "release"
$WorkParent = Join-Path $ReleaseRoot "work"
$WorkRoot = Join-Path $WorkParent ("{0}-workspace-{1}" -f $ReleaseTag, [Guid]::NewGuid().ToString("N"))
$PreparedGitHubDir = Join-Path $WorkRoot "github"
$LinuxArchiveDir = Join-Path $WorkParent ("{0}-linux-archive-{1}" -f $ReleaseTag, [Guid]::NewGuid().ToString("N"))
$GeneratedLinuxAcceptance = Join-Path $WorkParent ("{0}-linux-acceptance-{1}.json" -f $ReleaseTag, [Guid]::NewGuid().ToString("N"))
$PreserveWorkRoot = $false
$UseMultiPlatformSchema = [version]$ReleaseVersion -ge [version]"2.1.0"

function Get-PackageSummaryValue {
    param(
        [string[]]$Lines,
        [string]$Name
    )

    $Matches = @($Lines | Where-Object { $_ -like "$Name=*" })
    if ($Matches.Count -ne 1) {
        throw "Linux package summary must contain exactly one $Name entry."
    }
    return $Matches[0].Substring($Name.Length + 1)
}

$ArchiveArgs = @(
    $Implementation, "archive-player",
    "--package-root", $PlayerPackageRoot,
    "--output-dir", $PreparedGitHubDir,
    "--release-version", $ReleaseVersion,
    "--split-size-mib", $SplitSizeMiB
)
if ($SevenZip) {
    $ArchiveArgs += @("--seven-zip", $SevenZip)
}
try {
    & $Python.Source @ArchiveArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Player archive preparation failed."
    }

    $ArchiveReport = Join-Path $PreparedGitHubDir "player-archive.json"
    if ($UseMultiPlatformSchema) {
        if ([string]::IsNullOrWhiteSpace($LinuxPackageRoot) -or
            -not (Test-Path -LiteralPath $LinuxPackageRoot -PathType Container)) {
            throw "LinuxPackageRoot is required for release $ReleaseVersion."
        }
        $LinuxSummaryPath = Join-Path $LinuxPackageRoot "package_summary.txt"
        if (-not (Test-Path -LiteralPath $LinuxSummaryPath -PathType Leaf)) {
            throw "Linux package summary is missing: $LinuxSummaryPath"
        }
        $LinuxSummary = @(Get-Content -LiteralPath $LinuxSummaryPath)
        $LinuxSummaryPlatform = Get-PackageSummaryValue -Lines $LinuxSummary -Name "Platform"
        $SourceRevision = Get-PackageSummaryValue -Lines $LinuxSummary -Name "SourceRevision"
        $SourceState = Get-PackageSummaryValue -Lines $LinuxSummary -Name "SourceStateSha256"
        if ($LinuxSummaryPlatform -cne "Linux" -or
            $SourceRevision -notmatch '^[0-9a-f]{40}$' -or
            $SourceState -notmatch '^[0-9a-f]{64}$') {
            throw "Linux package summary contains an invalid platform or source identity."
        }
        & $Python.Source (Join-Path $ScriptDir "release_platforms.py") archive-linux `
            --game-root $LinuxPackageRoot `
            --output-dir $LinuxArchiveDir `
            --release-version $ReleaseVersion `
            --split-size-mib $SplitSizeMiB `
            --source-revision $SourceRevision `
            --source-state-sha256 $SourceState
        if ($LASTEXITCODE -ne 0) {
            throw "Linux player archive preparation failed."
        }
        $LinuxArchiveReport = Join-Path $LinuxArchiveDir "linux-player-archive.json"
        $ResolvedLinuxAcceptance = if ([string]::IsNullOrWhiteSpace($LinuxAcceptance)) {
            & (Join-Path $ScriptDir "accept_linux_player.ps1") `
                -ArchiveReport $LinuxArchiveReport `
                -OutputReceipt $GeneratedLinuxAcceptance
            if ($LASTEXITCODE -ne 0) {
                throw "Linux player acceptance failed."
            }
            $GeneratedLinuxAcceptance
        }
        else {
            [IO.Path]::GetFullPath($LinuxAcceptance)
        }
        $PrepareArgs = @(
            (Join-Path $ScriptDir "prepare_release_v4.py"),
            "--release-version", $ReleaseVersion,
            "--release-tag", $ReleaseTag,
            "--private-source-root", $ProjectRoot,
            "--public-source-root", $PublicSourceRoot,
            "--windows-package-root", $PlayerPackageRoot,
            "--windows-evidence", $PlayerEvidence,
            "--windows-archive-report", $ArchiveReport,
            "--linux-game-root", $LinuxPackageRoot,
            "--linux-archive-report", $LinuxArchiveReport,
            "--linux-acceptance", $ResolvedLinuxAcceptance,
            "--developer-release-dir", $DeveloperReleaseDir,
            "--developer-payload-manifest", $DeveloperPayloadManifest,
            "--component-manifest", $ComponentManifest,
            "--dependency-report", $DependencyReport,
            "--privacy-report", $PrivacyReport,
            "--map-load-report", $MapLoadReport,
            "--attribution-notice", $AttributionNotice,
            "--product-terms", $ProductTerms,
            "--output-dir", $PreparedGitHubDir
        )
    }
    else {
        $PrepareArgs = @(
        $Implementation, "prepare",
        "--release-version", $ReleaseVersion,
        "--release-tag", $ReleaseTag,
        "--private-source-root", $ProjectRoot,
        "--public-source-root", $PublicSourceRoot,
        "--player-package-root", $PlayerPackageRoot,
        "--player-evidence", $PlayerEvidence,
        "--player-archive-report", $ArchiveReport,
        "--developer-release-dir", $DeveloperReleaseDir,
        "--developer-payload-manifest", $DeveloperPayloadManifest,
        "--component-manifest", $ComponentManifest,
        "--dependency-report", $DependencyReport,
        "--privacy-report", $PrivacyReport,
        "--map-load-report", $MapLoadReport,
        "--attribution-notice", $AttributionNotice,
        "--product-terms", $ProductTerms,
        "--output-dir", $PreparedGitHubDir
        )
    }
    & $Python.Source @PrepareArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Release manifest preparation failed."
    }

    if ($UseMultiPlatformSchema) {
        & $Python.Source $WorkspaceTool initialize-v2 `
            --workspace-root $WorkRoot `
            --windows-candidate-root (Join-Path $PlayerPackageRoot "Windows") `
            --linux-candidate-root $LinuxPackageRoot `
            --release-version $ReleaseVersion
    }
    else {
        & $Python.Source $WorkspaceTool initialize `
            --workspace-root $WorkRoot `
            --candidate-root $PlayerPackageRoot `
            --release-version $ReleaseVersion
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Release workspace initialization failed."
    }

    $PromotionAttempts = 6
    for ($Attempt = 1; $Attempt -le $PromotionAttempts; $Attempt++) {
        try {
            [IO.Directory]::Move($WorkRoot, $ResolvedReleaseDir)
            break
        }
        catch {
            $PromotionFailure = $_.Exception
            while ($PromotionFailure.InnerException) {
                $PromotionFailure = $PromotionFailure.InnerException
            }
            if (($PromotionFailure -isnot [IO.IOException]) -and
                ($PromotionFailure -isnot [UnauthorizedAccessException])) {
                throw
            }
            if ($Attempt -eq $PromotionAttempts) {
                $PreserveWorkRoot = $true
                throw "Release workspace promotion failed after $PromotionAttempts attempts. Prepared workspace preserved at: $WorkRoot"
            }
            Write-Warning "Release workspace promotion attempt $Attempt/$PromotionAttempts was blocked; retrying."
            Start-Sleep -Milliseconds (250 * $Attempt)
        }
    }
    if ($UseMultiPlatformSchema) {
        & $Python.Source $WorkspaceTool adopt-v2 `
            --workspace-root $ResolvedReleaseDir `
            --windows-candidate-root (Join-Path $PlayerPackageRoot "Windows") `
            --linux-candidate-root $LinuxPackageRoot
    }
    else {
        & $Python.Source $WorkspaceTool adopt `
            --workspace-root $ResolvedReleaseDir `
            --candidate-root $PlayerPackageRoot
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Release workspace game adoption failed. Rerun the release command to resume it."
    }
}
finally {
    if ((-not $PreserveWorkRoot) -and (Test-Path -LiteralPath $WorkRoot)) {
        Remove-Item -LiteralPath $WorkRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $LinuxArchiveDir) {
        Remove-Item -LiteralPath $LinuxArchiveDir -Recurse -Force
    }
    if (Test-Path -LiteralPath $GeneratedLinuxAcceptance) {
        Remove-Item -LiteralPath $GeneratedLinuxAcceptance -Force
    }
}

Write-Host "Release workspace prepared for owner review: $ResolvedReleaseDir" -ForegroundColor Green
