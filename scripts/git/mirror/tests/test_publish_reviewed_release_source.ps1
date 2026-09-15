#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$MirrorRoot = Split-Path -Parent $ScriptDir
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MirrorRoot))
$TestParent = Join-Path $ProjectRoot 'tmp\release\tests\publish-reviewed-source'
$TestRoot = Join-Path $TestParent ([Guid]::NewGuid().ToString('N'))
$ReleaseRoot = Join-Path $TestRoot 'release'
$EmptyReleaseRoot = Join-Path $TestRoot 'empty-release'
$Candidate = Join-Path $ReleaseRoot 'inputs\v9.8.5\public-source'
$Pending = Join-Path $ReleaseRoot 'v9.8.5'
$Remote = Join-Path $TestRoot 'remote.git'

function Invoke-Git {
    param([string]$WorkingDirectory, [Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
    & git -C $WorkingDirectory @Arguments | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Git command failed in $WorkingDirectory`: $($Arguments -join ' ')"
    }
}

function Assert-PublisherFails {
    param(
        [string]$ReleaseTag,
        [string]$ExpectedMessage,
        [string]$PublisherReleaseRoot = $ReleaseRoot
    )

    $Failed = $false
    try {
        & (Join-Path $MirrorRoot 'publish_reviewed_release_source.ps1') `
            -RemoteUrl $Remote -Branch main -ReleaseRoot $PublisherReleaseRoot -ReleaseTag $ReleaseTag
    }
    catch {
        $Failed = $true
        if ($_.Exception.Message -notlike "*$ExpectedMessage*") {
            throw "Expected failure containing '$ExpectedMessage', got: $($_.Exception.Message)"
        }
    }

    if (-not $Failed) {
        throw "Expected reviewed publication to reject release tag '$ReleaseTag'."
    }
}

try {
    $BareMake = @(& make -n -C $ProjectRoot mirror FORCE_WSL=0 2>&1)
    $BareMakeText = $BareMake -join "`n"
    if ($LASTEXITCODE -ne 0 -or
        $BareMakeText -notmatch 'mirror mode: generic' -or
        $BareMakeText -notmatch 'mirror_to_github\.ps1' -or
        $BareMakeText -match 'publish_reviewed_release_source\.ps1') {
        throw 'Bare make mirror must invoke the generic mirror publisher.'
    }
    $TaggedMake = @(& make -n -C $ProjectRoot mirror FORCE_WSL=0 RTAG=v9.8.5 2>&1)
    $TaggedMakeText = $TaggedMake -join "`n"
    if ($LASTEXITCODE -ne 0 -or
        $TaggedMakeText -notmatch 'mirror mode: reviewed release publication' -or
        $TaggedMakeText -notmatch 'publish_reviewed_release_source\.ps1' -or
        $TaggedMakeText -notmatch '-ReleaseTag\s+"v9\.8\.5"') {
        throw 'Make mirror did not pass the explicit RTAG to the reviewed publisher.'
    }

    New-Item -ItemType Directory -Path $Candidate, $Pending, $EmptyReleaseRoot -Force | Out-Null
    Invoke-Git $Candidate init -q
    Invoke-Git $Candidate config user.name test
    Invoke-Git $Candidate config user.email test@localhost
    'base' | Set-Content -LiteralPath (Join-Path $Candidate 'tracked.txt') -Encoding Ascii
    Invoke-Git $Candidate add tracked.txt
    Invoke-Git $Candidate commit -q -m base
    $BaseRevision = (& git -C $Candidate rev-parse HEAD).Trim()
    & git init --bare -q $Remote
    if ($LASTEXITCODE -ne 0) { throw 'Unable to create local publication remote.' }
    Invoke-Git $Candidate push $Remote 'HEAD:refs/heads/main'

    'reviewed' | Set-Content -LiteralPath (Join-Path $Candidate 'tracked.txt') -Encoding Ascii
    Invoke-Git $Candidate add tracked.txt
    Invoke-Git $Candidate commit -q -m reviewed
    Invoke-Git $Candidate tag v9.8.5
    $Revision = (& git -C $Candidate rev-parse HEAD).Trim()
    $Tree = (& git -C $Candidate rev-parse 'HEAD^{tree}').Trim()
    New-Item -ItemType Directory -Path $Pending -Force | Out-Null
    $Artifact = Join-Path $Pending 'fixture.bin'
    [IO.File]::WriteAllBytes($Artifact, [byte[]](1, 2, 3))
    $Manifest = [ordered]@{
        schema = 'alis-release-manifest-v3'
        status = 'pending_owner_approval'
        release_version = '9.8.5'
        release_tag = 'v9.8.5'
        public_source = @{ revision = $Revision; tree = $Tree }
        unresolved_count = 0
        artifacts = @([ordered]@{
            name = 'fixture.bin'
            byte_size = 3
            sha256 = (Get-FileHash -LiteralPath $Artifact -Algorithm SHA256).Hash.ToLowerInvariant()
        })
    }
    $Manifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $Pending 'release_manifest.json') -Encoding Ascii

    Assert-PublisherFails -ReleaseTag '' -ExpectedMessage 'ReleaseTag is required'
    Assert-PublisherFails -ReleaseTag 'v9.8.5' `
        -ExpectedMessage 'requires one pending reviewed release source' `
        -PublisherReleaseRoot $EmptyReleaseRoot

    Assert-PublisherFails -ReleaseTag '9.8.5' -ExpectedMessage 'valid vX.Y.Z'
    Assert-PublisherFails -ReleaseTag 'v9.8.6' -ExpectedMessage 'does not match reviewed release tag'

    & (Join-Path $MirrorRoot 'publish_reviewed_release_source.ps1') `
        -RemoteUrl $Remote -Branch main -ReleaseRoot $ReleaseRoot -ReleaseTag v9.8.5
    if ($LASTEXITCODE -ne 0) { throw 'Reviewed branch-and-tag publication test failed.' }
    $Published = ((& git --git-dir=$Remote rev-parse refs/heads/main) | Select-Object -Last 1).Trim()
    if ($Published -cne $Revision) {
        throw 'Local remote does not contain the exact reviewed source revision.'
    }
    $PublishedTag = ((& git --git-dir=$Remote rev-parse 'refs/tags/v9.8.5^{}') |
        Select-Object -Last 1).Trim()
    if ($PublishedTag -cne $Revision) {
        throw 'Local remote release tag does not contain the exact reviewed source revision.'
    }

    & (Join-Path $MirrorRoot 'publish_reviewed_release_source.ps1') `
        -RemoteUrl $Remote -Branch main -ReleaseRoot $ReleaseRoot -ReleaseTag v9.8.5
    if ($LASTEXITCODE -ne 0) { throw 'Idempotent reviewed tag publication failed.' }

    Invoke-Git $Remote update-ref refs/heads/main $BaseRevision
    Invoke-Git $Remote update-ref refs/tags/v9.8.5 $BaseRevision
    Assert-PublisherFails -ReleaseTag 'v9.8.5' -ExpectedMessage 'already points to a different commit'
    $UnchangedBranch = ((& git --git-dir=$Remote rev-parse refs/heads/main) |
        Select-Object -Last 1).Trim()
    if ($UnchangedBranch -cne $BaseRevision) {
        throw 'Conflicting remote release tag did not prevent branch publication.'
    }
    $ConflictingTag = ((& git --git-dir=$Remote rev-parse refs/tags/v9.8.5) |
        Select-Object -Last 1).Trim()
    if ($ConflictingTag -cne $BaseRevision) {
        throw 'Conflicting remote release tag was moved.'
    }

    Write-Host '[OK] Generic mirror dispatch and explicit reviewed tag publication passed.'
}
finally {
    if (Test-Path -LiteralPath $TestRoot) {
        $resolvedParent = [IO.Path]::GetFullPath($TestParent).TrimEnd('\', '/')
        $resolvedRoot = [IO.Path]::GetFullPath($TestRoot)
        if (-not $resolvedRoot.StartsWith(
                $resolvedParent + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected test root: $resolvedRoot"
        }
        Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
    }
}
