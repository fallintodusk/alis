#Requires -Version 5.1
# License terms: see repository root LICENSE.

[CmdletBinding()]
param(
    [string]$RemoteUrl = "git@github.com:fallintodusk/alis.git",
    [string]$Branch = "main",
    [string]$ReleaseRoot,
    [string]$ReleaseTag
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$ReleaseRoot = if ([string]::IsNullOrWhiteSpace($ReleaseRoot)) {
    Join-Path $ProjectRoot "tmp\release"
}
else {
    [IO.Path]::GetFullPath($ReleaseRoot)
}

function Get-RemoteTagCommit {
    param(
        [string]$Url,
        [string]$Tag
    )

    $DirectRef = "refs/tags/$Tag"
    $PeeledRef = "$DirectRef^{}"
    $Lines = @(& git ls-remote --exit-code $Url $DirectRef $PeeledRef)
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -eq 2) {
        return $null
    }
    if ($ExitCode -ne 0) {
        throw "Unable to resolve public release tag $Tag."
    }

    $Direct = @($Lines | Where-Object { ($_ -split '\s+', 2)[1] -ceq $DirectRef })
    $Peeled = @($Lines | Where-Object { ($_ -split '\s+', 2)[1] -ceq $PeeledRef })
    if ($Direct.Count -ne 1 -or $Peeled.Count -gt 1) {
        throw "Public release tag $Tag did not resolve unambiguously."
    }
    if ($Peeled.Count -eq 1) {
        return ($Peeled[0] -split '\s+', 2)[0]
    }
    return ($Direct[0] -split '\s+', 2)[0]
}

if ([string]::IsNullOrWhiteSpace($ReleaseTag)) {
    throw "ReleaseTag is required for reviewed release publication."
}
if ($ReleaseTag -cnotmatch '^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
    throw "ReleaseTag must be a valid vX.Y.Z tag."
}

$Pending = @(
    Get-ChildItem -LiteralPath $ReleaseRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^v[0-9]+\.[0-9]+\.[0-9]+$' } |
        ForEach-Object {
            $ManifestPath = Join-Path $_.FullName "release_manifest.json"
            if (Test-Path -LiteralPath $ManifestPath -PathType Leaf) {
                $Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
                if ($Manifest.status -ceq "pending_owner_approval") {
                    [pscustomobject]@{ Directory = $_.FullName; Manifest = $Manifest }
                }
            }
        }
)

if ($Pending.Count -eq 0) {
    throw "ReleaseTag requires one pending reviewed release source."
}
if ($Pending.Count -ne 1) {
    throw "Expected one pending reviewed release source; found $($Pending.Count)."
}

$Item = $Pending[0]
$Manifest = $Item.Manifest
$Version = [string]$Manifest.release_version
$Tag = [string]$Manifest.release_tag
if ($ReleaseTag -cne $Tag) {
    throw "Requested release tag $ReleaseTag does not match reviewed release tag $Tag."
}
$Candidate = Join-Path $ReleaseRoot "inputs\$Tag\public-source"
if (-not (Test-Path -LiteralPath $Candidate -PathType Container)) {
    throw "Reviewed public source candidate is missing: $Candidate"
}

& python (Join-Path $ProjectRoot "scripts\ue\package\prepare_release.py") verify --release-dir $Item.Directory
if ($LASTEXITCODE -ne 0) {
    throw "Reviewed unsigned release verification failed."
}
$Revision = @(& git -C $Candidate rev-parse HEAD)
$Tree = @(& git -C $Candidate rev-parse "HEAD^{tree}")
$Tagged = @(& git -C $Candidate rev-parse "refs/tags/$Tag^{commit}")
if ($LASTEXITCODE -ne 0 -or $Revision.Count -ne 1 -or $Tree.Count -ne 1 -or $Tagged.Count -ne 1) {
    throw "Reviewed public source Git identity is incomplete."
}
if ($Revision[0] -cne [string]$Manifest.public_source.revision -or
    $Tree[0] -cne [string]$Manifest.public_source.tree -or
    $Tagged[0] -cne $Revision[0]) {
    throw "Reviewed public source no longer matches the unsigned release manifest."
}

$RemoteTagCommit = Get-RemoteTagCommit -Url $RemoteUrl -Tag $Tag
if ($null -ne $RemoteTagCommit -and $RemoteTagCommit -cne $Revision[0]) {
    throw "Public release tag $Tag already points to a different commit; refusing to move it."
}

$RemoteRevision = @(& git ls-remote --exit-code $RemoteUrl "refs/heads/$Branch")
if ($LASTEXITCODE -eq 0) {
    $RemoteSha = ($RemoteRevision[0] -split '\s+')[0]
    & git -C $Candidate fetch --quiet $RemoteUrl "refs/heads/$Branch`:refs/remotes/release-publication/$Branch"
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to fetch public branch $Branch before publication."
    }
    & git -C $Candidate merge-base --is-ancestor $RemoteSha $Revision[0]
    if ($LASTEXITCODE -ne 0) {
        throw "Reviewed public source is not a fast-forward of $Branch; prepare a fresh unsigned release."
    }
}
elseif ($LASTEXITCODE -ne 2) {
    throw "Unable to resolve public branch $Branch."
}

$PushArguments = @('-C', $Candidate, 'push')
if ($null -eq $RemoteTagCommit) {
    $PushArguments += '--atomic'
}
$PushArguments += $RemoteUrl
$PushArguments += "$($Revision[0]):refs/heads/$Branch"
if ($null -eq $RemoteTagCommit) {
    $PushArguments += "refs/tags/$($Tag):refs/tags/$($Tag)"
}
& git @PushArguments
if ($LASTEXITCODE -ne 0) {
    throw "Reviewed public source publication failed."
}
$Published = @(& git ls-remote --exit-code $RemoteUrl "refs/heads/$Branch")
if ($LASTEXITCODE -ne 0 -or $Published.Count -ne 1 -or ($Published[0] -split '\s+')[0] -cne $Revision[0]) {
    throw "Published public source does not match the reviewed commit."
}
$PublishedTag = Get-RemoteTagCommit -Url $RemoteUrl -Tag $Tag
if ($null -eq $PublishedTag -or $PublishedTag -cne $Revision[0]) {
    throw "Published release tag $Tag does not match the reviewed commit."
}
Write-Host "[OK] Published reviewed public source $($Revision[0]) on $Branch and $Tag for $Version" -ForegroundColor Green
