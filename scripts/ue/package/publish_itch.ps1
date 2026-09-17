#Requires -Version 5.1
# License terms: see repository root LICENSE.

[CmdletBinding()]
param(
    [ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$')]
    [string]$ReleaseVersion,
    [string]$Target,
    [string]$Channel = "windows",
    [string]$ReleaseRoot,
    [string]$ButlerPath,
    [string]$VerifierScript
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$WorkspaceTool = Join-Path $ScriptDir "release_workspace.py"
$MinimumButlerVersion = [Version]"15.27.0"

function Get-NormalizedPath {
    param([string]$Path)

    return [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
}

function Assert-DirectChildPath {
    param(
        [string]$Path,
        [string]$Parent,
        [string]$Description
    )

    $ResolvedPath = Get-NormalizedPath -Path $Path
    $ResolvedParent = Get-NormalizedPath -Path $Parent
    $ActualParent = Get-NormalizedPath -Path (Split-Path -Parent $ResolvedPath)
    if (-not [string]::Equals(
            $ActualParent,
            $ResolvedParent,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must be a direct child of $ResolvedParent`: $ResolvedPath"
    }
}

function Resolve-ButlerExecutable {
    param([string]$RequestedPath)

    if ($RequestedPath) {
        if (-not (Test-Path -LiteralPath $RequestedPath -PathType Leaf)) {
            throw "Butler executable was not found: $RequestedPath"
        }
        return (Resolve-Path -LiteralPath $RequestedPath).Path
    }

    $Command = Get-Command "butler.exe" -ErrorAction SilentlyContinue
    if (-not $Command) {
        $Command = Get-Command "butler" -ErrorAction SilentlyContinue
    }
    if ($Command) {
        return $Command.Source
    }

    $Candidates = @()
    if ($env:LOCALAPPDATA) {
        $Candidates += Join-Path $env:LOCALAPPDATA "Programs\butler\butler.exe"
    }
    $UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($UserPath) {
        $Candidates += @($UserPath -split ";" | Where-Object { $_ } | ForEach-Object {
            Join-Path $_ "butler.exe"
        })
    }
    foreach ($Candidate in $Candidates | Select-Object -Unique) {
        if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $Candidate).Path
        }
    }

    throw "Butler was not found. Install it from https://itch.io/docs/butler/installing.html and open a new terminal."
}

function Invoke-ButlerJson {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$Description
    )

    $PreviousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $Output = @(& $Executable @Arguments 2>&1)
        $ExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $PreviousErrorAction
    }
    if ($ExitCode -ne 0) {
        $Detail = ($Output | Select-Object -Last 1)
        throw "$Description failed with exit code $ExitCode$(if ($Detail) { ": $Detail" })."
    }
    $Results = @()
    foreach ($Line in $Output) {
        try {
            $Event = ([string]$Line) | ConvertFrom-Json
            if ($Event.type -eq "result") {
                $Results += $Event.value
            }
        }
        catch {
        }
    }
    if ($Results.Count -ne 1) {
        throw "$Description returned $($Results.Count) structured result events; expected exactly one."
    }
    return $Results[0]
}

function Get-RemoteChannel {
    param(
        [string]$Executable,
        [string]$ProjectTarget,
        [string]$ChannelName
    )

    $Status = Invoke-ButlerJson `
        -Executable $Executable `
        -Arguments @("status", "--json", $ProjectTarget) `
        -Description "Butler status"
    if (-not [string]::Equals(
            [string]$Status.target,
            $ProjectTarget,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Butler status target mismatch. Expected $ProjectTarget but found $($Status.target)."
    }
    $Matches = @($Status.channels | Where-Object { $_.name -eq $ChannelName })
    if ($Matches.Count -gt 1) {
        throw "Butler returned duplicate channel state for $ChannelName."
    }
    if ($Matches.Count -eq 0) {
        return $null
    }
    return $Matches[0]
}

function ConvertTo-StableVersion {
    param(
        [string]$Value,
        [string]$Description
    )

    if ($Value -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
        throw "$Description is not a stable X.Y.Z version: $Value"
    }
    return [Version]$Value
}

function Get-ComparisonChangeCount {
    param([object]$Comparison)

    if (-not $Comparison) {
        throw "Butler preview has no comparison counts."
    }
    $Total = 0L
    foreach ($Name in @("new", "modified", "deleted")) {
        $Property = $Comparison.PSObject.Properties[$Name]
        if (-not $Property -or [int64]$Property.Value -lt 0) {
            throw "Butler preview has an invalid $Name count."
        }
        $Total += [int64]$Property.Value
    }
    return $Total
}

if ([string]::IsNullOrWhiteSpace($Target)) {
    throw "The itch.io user/game target is required. Use make publish itch for the ALIS default or pass -Target user/game."
}
if ($Target -notmatch '^[A-Za-z0-9._-]+/[A-Za-z0-9._-]+$') {
    throw "Itch target must use the exact user/game form: $Target"
}
if ($Channel -notmatch '^[a-z0-9][a-z0-9._-]*$') {
    throw "Itch channel contains unsupported characters: $Channel"
}

$ResolvedReleaseRoot = if ($ReleaseRoot) {
    Get-NormalizedPath -Path $ReleaseRoot
}
else {
    Get-NormalizedPath -Path (Join-Path $ProjectRoot "tmp\release")
}
if (-not (Test-Path -LiteralPath $ResolvedReleaseRoot -PathType Container)) {
    throw "Release root does not exist: $ResolvedReleaseRoot"
}

if ($ReleaseVersion) {
    $SelectedVersion = $ReleaseVersion
    $WorkspaceRoot = Join-Path $ResolvedReleaseRoot "v$SelectedVersion"
}
else {
    $Candidates = @(Get-ChildItem -LiteralPath $ResolvedReleaseRoot -Directory | ForEach-Object {
        if ($_.Name -match '^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
            [PSCustomObject]@{
                Path = $_.FullName
                ReleaseVersion = $Matches[0].Substring(1)
                ParsedVersion = [Version]$Matches[0].Substring(1)
            }
        }
    })
    if ($Candidates.Count -eq 0) {
        throw "No stable SemVer release workspaces exist directly under $ResolvedReleaseRoot."
    }
    $Selected = $Candidates | Sort-Object ParsedVersion -Descending | Select-Object -First 1
    $SelectedVersion = $Selected.ReleaseVersion
    $WorkspaceRoot = $Selected.Path
}

Assert-DirectChildPath -Path $WorkspaceRoot -Parent $ResolvedReleaseRoot -Description "Selected release workspace"
if (-not (Test-Path -LiteralPath $WorkspaceRoot -PathType Container)) {
    throw "The selected release workspace is missing: $WorkspaceRoot"
}

$Python = Get-Command "python" -ErrorAction SilentlyContinue
if (-not $Python -or -not (Test-Path -LiteralPath $WorkspaceTool -PathType Leaf)) {
    throw "Python and release_workspace.py are required to verify the selected release workspace."
}
$PreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$WorkspaceVerification = @(& $Python.Source $WorkspaceTool verify --workspace-root $WorkspaceRoot 2>&1)
$WorkspaceExitCode = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
if ($WorkspaceExitCode -ne 0) {
    $VerificationDetail = $WorkspaceVerification | Select-Object -Last 1
    throw "The selected release workspace failed identity verification: $WorkspaceRoot$(if ($VerificationDetail) { ": $VerificationDetail" })"
}
$WorkspaceVerification | ForEach-Object { Write-Host $_ }

$GameRoot = Join-Path $WorkspaceRoot "game"
$RequiredGameFiles = @(
    "Alis.exe",
    "Alis\Binaries\Win64\Alis-Win64-Shipping.exe",
    "VERIFY_ALIS.bat",
    "Verification\VERIFY_ALIS.ps1",
    "Verification\ALIS_PUBLIC_KEY.asc",
    "Verification\SHA256SUMS.txt",
    "Verification\SHA256SUMS.txt.asc"
)
foreach ($RelativePath in $RequiredGameFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $GameRoot $RelativePath) -PathType Leaf)) {
        throw "Signed game projection is incomplete: $RelativePath"
    }
}
$ReparsePoints = @(Get-ChildItem -LiteralPath $GameRoot -Recurse -Force -Attributes ReparsePoint)
if ($ReparsePoints.Count -gt 0) {
    throw "Signed game projection contains reparse points and cannot be published: $($ReparsePoints[0].FullName)"
}

$ResolvedVerifier = if ($VerifierScript) {
    (Resolve-Path -LiteralPath $VerifierScript).Path
}
else {
    Join-Path $ScriptDir "verify_release.ps1"
}
& $ResolvedVerifier `
    -ReleaseDir $GameRoot `
    -ManifestRelativePath "Verification\SHA256SUMS.txt" `
    -SignatureRelativePath "Verification\SHA256SUMS.txt.asc" `
    -BundledPublicKeyName "Verification\ALIS_PUBLIC_KEY.asc" `
    -AllowRelativeAssetPaths `
    -RequireExactInventory `
    -PublicKeyUrl ""
if (-not $?) {
    throw "Signed game projection verification failed."
}

$ResolvedButler = Resolve-ButlerExecutable -RequestedPath $ButlerPath
$PreviousErrorAction = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    $VersionOutput = @(& $ResolvedButler -V 2>&1)
    $VersionExitCode = $LASTEXITCODE
}
finally {
    $ErrorActionPreference = $PreviousErrorAction
}
if ($VersionExitCode -ne 0 -or -not (($VersionOutput -join " ") -match 'v([0-9]+\.[0-9]+\.[0-9]+)')) {
    throw "Unable to determine the installed Butler version."
}
$InstalledButlerVersion = [Version]$Matches[1]
if ($InstalledButlerVersion -lt $MinimumButlerVersion) {
    throw "Butler $InstalledButlerVersion is unsupported; install $MinimumButlerVersion or newer."
}

$RemoteChannel = Get-RemoteChannel `
    -Executable $ResolvedButler `
    -ProjectTarget $Target `
    -ChannelName $Channel
if ($RemoteChannel -and $RemoteChannel.pending) {
    throw "Itch channel $Channel has a pending build; wait for it to finish before publishing."
}

$TargetWithChannel = "${Target}:$Channel"
if ($RemoteChannel -and $RemoteChannel.head) {
    $RemoteVersion = ConvertTo-StableVersion `
        -Value ([string]$RemoteChannel.head.userVersion) `
        -Description "Itch channel head userVersion"
    $LocalVersion = [Version]$SelectedVersion
    if ($RemoteVersion -gt $LocalVersion) {
        throw "Itch channel $Channel already contains newer version $RemoteVersion; refusing downgrade to $SelectedVersion."
    }
    if ($RemoteVersion -eq $LocalVersion) {
        $Preview = Invoke-ButlerJson `
            -Executable $ResolvedButler `
            -Arguments @("push-preview", "--json", $GameRoot, $TargetWithChannel) `
            -Description "Butler push preview"
        if ((Get-ComparisonChangeCount -Comparison $Preview.comparison) -eq 0) {
            Write-Host "[OK] Itch $TargetWithChannel already contains ALIS $SelectedVersion with identical content." -ForegroundColor Green
            exit 0
        }
        throw "Itch channel has different same-version content for ALIS $SelectedVersion; refusing replacement."
    }
}

Write-Host "[Publish] Uploading signed ALIS $SelectedVersion game to $TargetWithChannel."
& $ResolvedButler push $GameRoot $TargetWithChannel --userversion $SelectedVersion --if-changed
if ($LASTEXITCODE -ne 0) {
    throw "Butler push failed with exit code $LASTEXITCODE."
}

$PublishedChannel = Get-RemoteChannel `
    -Executable $ResolvedButler `
    -ProjectTarget $Target `
    -ChannelName $Channel
if (-not $PublishedChannel) {
    throw "Butler push returned success but channel $Channel was absent during read-back."
}
$PublishedBuild = @($PublishedChannel.pending, $PublishedChannel.head) | Where-Object {
    $_ -and $_.userVersion -eq $SelectedVersion -and $_.state -in @("queued", "processing", "completed")
} | Select-Object -First 1
if (-not $PublishedBuild) {
    throw "Butler push returned success but read-back did not bind channel $Channel to ALIS $SelectedVersion."
}

Write-Host "[OK] Itch publication accepted: $TargetWithChannel ALIS $SelectedVersion (build $($PublishedBuild.id), state $($PublishedBuild.state))." -ForegroundColor Green
