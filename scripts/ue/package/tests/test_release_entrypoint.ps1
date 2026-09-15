#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackageDir = Split-Path -Parent $ScriptDir
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PackageDir))
$ReleaseScript = Join-Path $PackageDir "release.ps1"
$TestParent = Join-Path $ProjectRoot "tmp\release"
$TestRoot = Join-Path $TestParent "v9.8.7"
$ReleaseDir = $TestRoot
$FinalRemote = Join-Path $TestParent ("final-source-fixture-" + [Guid]::NewGuid().ToString("N"))
$FinalCheckout = Join-Path $TestParent "final-public-source\v9.8.7"
$EphemeralRoots = @(
    (Join-Path $TestParent "work"),
    (Join-Path $TestParent "c"),
    (Join-Path $TestParent "final-public-source")
)

$ReleaseSource = Get-Content -LiteralPath $ReleaseScript -Raw
if ($ReleaseSource -match '-Mode\s+Accept' -or
    $ReleaseSource -match 'After the Shipping walkthrough') {
    throw "Release entrypoint must not stop for a separate Candidate approval."
}
if ($ReleaseSource -match '\bRead-Host\b' -or
    $ReleaseSource -match 'Type\s+APPROVE') {
    throw "The signed release command is the approval action; only GPG may prompt."
}
if ($ReleaseSource -notmatch '(?s)Invoke-PythonReleaseTool\s+-Arguments\s+@\(\s*"approve".*?"--approve-product-terms-and-rights"') {
    throw "The signed release command must record Product, terms, and rights approval automatically."
}
if ($ReleaseSource -notmatch 'git\s+-c\s+core\.longpaths=true\s+clone' -or
    $ReleaseSource -notmatch 'git\s+-C\s+\$ResolvedFinal\s+config\s+core\.longpaths\s+true') {
    throw "Final public checkout must enable and persist Windows long-path support."
}
if ($ReleaseSource -notmatch '(?s)finally\s*\{.*Remove-Item\s+-LiteralPath\s+\$OwnedFinalPublicSource\s+-Recurse\s+-Force') {
    throw "Final public checkout must be removed after final source validation."
}
if ($ReleaseSource -notmatch '(?s)if \(\$HasSigningManifest\).*Consumer-side release verification failed.*Remove-AutomaticReleaseInputs' -or
    $ReleaseSource -notmatch '(?s)sign_release\.ps1.*Consumer-side release verification failed.*Remove-AutomaticReleaseInputs') {
    throw "Verified signed releases must remove their superseded automatic inputs."
}
$InputsCleanupIndex = $ReleaseSource.IndexOf('$InputsToReplace')
$PlayerGateIndex = $ReleaseSource.IndexOf('$AcceptanceScript')
if ($InputsCleanupIndex -lt 0 -or $PlayerGateIndex -lt 0 -or $InputsCleanupIndex -gt $PlayerGateIndex) {
    throw "Fresh automatic preparation must remove stale inputs before the player gate can fail."
}

function Write-PendingRelease {
    param(
        [string]$Directory,
        [string]$Status = "pending_owner_approval",
        [string]$Schema = "alis-release-manifest-v3"
    )

    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $PayloadPath = Join-Path $Directory "fixture.bin"
    [IO.File]::WriteAllBytes($PayloadPath, [byte[]](1, 2, 3, 4))
    $Payload = Get-Item -LiteralPath $PayloadPath
    $Manifest = [ordered]@{
        schema = $Schema
        status = $Status
        release_version = "9.8.7"
        release_tag = "v9.8.7"
        unresolved_count = 0
        product_review = if ($Status -eq "ready_for_signature") {
            @{ status = "accepted" }
        } else {
            @{ status = "pending_owner_approval" }
        }
        rights_review = if ($Status -eq "ready_for_signature") {
            @{ status = "accepted" }
        } else {
            @{ status = "pending_owner_approval" }
        }
        artifacts = @(
            [ordered]@{
                name = $Payload.Name
                byte_size = $Payload.Length
                sha256 = (Get-FileHash -LiteralPath $PayloadPath -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        )
    }
    $Manifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $Directory "release_manifest.json") -Encoding Ascii
}

function Get-InventoryDigest {
    param([string]$Directory)

    return @(
        Get-ChildItem -LiteralPath $Directory -File -Recurse |
            Sort-Object FullName |
            ForEach-Object {
                $Relative = $_.FullName.Substring($Directory.TrimEnd('\', '/').Length + 1)
                "{0}|{1}" -f $Relative, (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            }
    ) -join "`n"
}

function Assert-Fails {
    param(
        [scriptblock]$Action,
        [string]$MessagePattern
    )

    $Failed = $false
    try {
        & $Action
    }
    catch {
        $Failed = $_.Exception.Message -like $MessagePattern
    }
    if (-not $Failed) {
        throw "Expected failure matching: $MessagePattern"
    }
}

if (Test-Path -LiteralPath $TestRoot) {
    throw "Release entrypoint fixture already exists: $TestRoot"
}
try {
    Write-PendingRelease -Directory $ReleaseDir
    foreach ($EphemeralRoot in $EphemeralRoots) {
        New-Item -ItemType Directory -Path (Join-Path $EphemeralRoot "stale\.git") -Force | Out-Null
    }
    $Before = Get-InventoryDigest -Directory $ReleaseDir
    Push-Location $ProjectRoot
    try {
        & make release 9.8.7 RELEASE_SIGN=0
        if ($LASTEXITCODE -ne 0) {
            throw "Unsigned make release entrypoint failed."
        }
    }
    finally {
        Pop-Location
    }
    $After = Get-InventoryDigest -Directory $ReleaseDir
    if ($Before -cne $After) {
        throw "Unsigned release entrypoint mutated the prepared release."
    }
    if ((Test-Path -LiteralPath (Join-Path $ReleaseDir "SHA256SUMS.txt")) -or
        (Test-Path -LiteralPath (Join-Path $ReleaseDir "SHA256SUMS.txt.asc"))) {
        throw "Unsigned release entrypoint created signing outputs."
    }
    foreach ($EphemeralRoot in $EphemeralRoots) {
        if (Test-Path -LiteralPath $EphemeralRoot) {
            throw "Release entrypoint left abandoned scratch behind: $EphemeralRoot"
        }
    }

    New-Item -ItemType Directory -Path $FinalRemote -Force | Out-Null
    & git -c init.defaultBranch=main init -q $FinalRemote
    & git -C $FinalRemote config user.name "release-test"
    & git -C $FinalRemote config user.email "release-test@localhost"
    Set-Content -LiteralPath (Join-Path $FinalRemote "README.md") -Value "fixture" -Encoding Ascii
    & git -C $FinalRemote add README.md
    & git -C $FinalRemote commit -q -m "fixture"
    if ($LASTEXITCODE -ne 0) {
        throw "Could not prepare final source cleanup fixture."
    }
    Assert-Fails -MessagePattern "*Final public source binding failed*" -Action {
        & $ReleaseScript `
            -ReleaseVersion "9.8.7" `
            -PublicRemoteUrl $FinalRemote
    }
    if (Test-Path -LiteralPath $FinalCheckout) {
        throw "Failed final public source validation left its checkout behind."
    }

    Remove-Item -LiteralPath $ReleaseDir -Recurse -Force
    Write-PendingRelease -Directory $ReleaseDir -Schema "alis-release-manifest-v2"
    $ArchiveRoot = Join-Path $TestParent "archive"
    $ArchivesBefore = @(
        Get-ChildItem -LiteralPath $ArchiveRoot -Directory -Filter "v9.8.7-flat-*" -ErrorAction SilentlyContinue |
            ForEach-Object FullName
    )
    Assert-Fails -MessagePattern "*Required release input directory is missing*" -Action {
        & $ReleaseScript `
            -ReleaseVersion "9.8.7" `
            -ReleaseDir $ReleaseDir `
            -PublicSourceRoot (Join-Path $TestParent "missing-public-source") `
            -SkipSigning
    }
    if (Test-Path -LiteralPath $ReleaseDir) {
        throw "Obsolete unsigned release was not deleted before fresh preparation."
    }
    $ArchivesAfter = @(
        Get-ChildItem -LiteralPath $ArchiveRoot -Directory -Filter "v9.8.7-flat-*" -ErrorAction SilentlyContinue |
            ForEach-Object FullName
    )
    if (($ArchivesBefore -join "`n") -cne ($ArchivesAfter -join "`n")) {
        throw "Obsolete unsigned release cleanup must not create an archive copy."
    }

    Write-PendingRelease -Directory $ReleaseDir -Schema "alis-release-manifest-v2"
    Set-Content -LiteralPath (Join-Path $ReleaseDir "SHA256SUMS.txt") -Value "fixture" -Encoding Ascii
    Assert-Fails -MessagePattern "*signed or incomplete signing output*" -Action {
        & $ReleaseScript -ReleaseVersion "9.8.7" -ReleaseDir $ReleaseDir -SkipSigning
    }
    if (-not (Test-Path -LiteralPath $ReleaseDir -PathType Container)) {
        throw "Ambiguous obsolete release must remain unchanged for inspection."
    }

    if (Test-Path -LiteralPath $ReleaseDir) {
        Remove-Item -LiteralPath $ReleaseDir -Recurse -Force
    }
    Write-PendingRelease -Directory $ReleaseDir -Status "ready_for_signature"
    Assert-Fails -MessagePattern "*expected pending_owner_approval*" -Action {
        & $ReleaseScript -ReleaseVersion "9.8.7" -ReleaseDir $ReleaseDir -SkipSigning
    }

    Assert-Fails -MessagePattern "*does not match*pattern*" -Action {
        & $ReleaseScript -ReleaseVersion "9.8" -ReleaseDir $ReleaseDir -SkipSigning
    }

    Assert-Fails -MessagePattern "*ReleaseDir must remain under*tmp*" -Action {
        & $ReleaseScript `
            -ReleaseVersion "9.8.7" `
            -ReleaseDir (Join-Path $ProjectRoot "Saved\ReleaseOutsideTmp") `
            -SkipSigning
    }

    Assert-Fails -MessagePattern "*Required release input directory is missing*" -Action {
        & $ReleaseScript `
            -ReleaseVersion "9.8.6" `
            -InputRoot (Join-Path $TestParent "missing-inputs") `
            -ReleaseDir (Join-Path $TestParent "v9.8.6") `
            -PublicSourceRoot (Join-Path $TestParent "missing-public-source") `
            -SkipSigning
    }

    Write-Host "[OK] Release entrypoint unsigned contract passed"
}
finally {
    foreach ($EphemeralRoot in $EphemeralRoots) {
        if (Test-Path -LiteralPath $EphemeralRoot) {
            Remove-Item -LiteralPath $EphemeralRoot -Recurse -Force
        }
    }
    if (Test-Path -LiteralPath $FinalRemote) {
        Remove-Item -LiteralPath $FinalRemote -Recurse -Force
    }
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
