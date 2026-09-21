#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackageDir = Split-Path -Parent $ScriptDir
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PackageDir))
$ReleaseScript = Join-Path $PackageDir "release.ps1"
$TestParent = Join-Path $ProjectRoot "tmp\release"
$TestRoot = Join-Path $TestParent "v2.0.97"
$ReleaseDir = $TestRoot
$WorkspaceRoot = Join-Path $TestParent "v2.0.98"
$SchemaMismatchRoot = Join-Path $TestParent ("schema-mismatch-" + [Guid]::NewGuid().ToString("N"))
$FinalRemote = Join-Path $TestParent ("final-source-fixture-" + [Guid]::NewGuid().ToString("N"))
$FinalCheckout = Join-Path $TestParent "final-public-source\v2.0.97"
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
        [string]$Schema = "alis-release-manifest-v3",
        [string]$ReleaseVersion = "2.0.97"
    )

    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $ManifestDirectory = $Directory
    $PlayerSource = $null
    if ($Schema -eq "alis-release-manifest-v3") {
        $Game = Join-Path $Directory "game"
        $ManifestDirectory = Join-Path $Directory "github"
        $Shipping = Join-Path $Game "Alis\Binaries\Win64\Alis-Win64-Shipping.exe"
        New-Item -ItemType Directory -Path (Split-Path -Parent $Shipping), $ManifestDirectory -Force | Out-Null
        Set-Content -LiteralPath $Shipping -Value "shipping" -Encoding Ascii
        Set-Content -LiteralPath (Join-Path $Game "Alis.exe") -Value "game" -Encoding Ascii
        Set-Content -LiteralPath (Join-Path $Directory "package_summary.txt") -Value "accepted" -Encoding Ascii
        $PackageTree = & python (Join-Path $PackageDir "release_workspace.py") package-tree --workspace-root $Directory
        if ($LASTEXITCODE -ne 0) {
            throw "Could not prepare release workspace package identity."
        }
        $PlayerSource = @{
            package_tree_sha256 = [string]$PackageTree
            shipping_executable_sha256 = (Get-FileHash -LiteralPath $Shipping -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        [ordered]@{
            schema = "alis-release-workspace-v1"
            status = "complete"
            release_version = $ReleaseVersion
            release_tag = "v$ReleaseVersion"
        } | ConvertTo-Json -Depth 4 |
            Set-Content -LiteralPath (Join-Path $Directory "release-workspace.json") -Encoding Ascii
    }
    $PayloadPath = Join-Path $ManifestDirectory "fixture.bin"
    [IO.File]::WriteAllBytes($PayloadPath, [byte[]](1, 2, 3, 4))
    $Payload = Get-Item -LiteralPath $PayloadPath
    $Manifest = [ordered]@{
        schema = $Schema
        status = $Status
        release_version = $ReleaseVersion
        release_tag = "v$ReleaseVersion"
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
        player_source = $PlayerSource
        artifacts = @(
            [ordered]@{
                name = $Payload.Name
                byte_size = $Payload.Length
                sha256 = (Get-FileHash -LiteralPath $PayloadPath -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        )
    }
    $Manifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $ManifestDirectory "release_manifest.json") -Encoding Ascii
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
if (Test-Path -LiteralPath $WorkspaceRoot) {
    throw "Release workspace fixture already exists: $WorkspaceRoot"
}
try {
    Write-PendingRelease -Directory $SchemaMismatchRoot -ReleaseVersion "2.1.0"
    Assert-Fails -MessagePattern "*requires workspace schema*" -Action {
        & $ReleaseScript -ReleaseVersion "2.1.0" -ReleaseDir $SchemaMismatchRoot -SkipSigning
    }

    Write-PendingRelease -Directory $WorkspaceRoot -ReleaseVersion "2.0.98"
    $WorkspaceBefore = Get-InventoryDigest -Directory $WorkspaceRoot
    & $ReleaseScript -ReleaseVersion "2.0.98" -ReleaseDir $WorkspaceRoot -SkipSigning
    if (-not $?) {
        throw "Unsigned release did not accept the game/github workspace."
    }
    $WorkspaceAfter = Get-InventoryDigest -Directory $WorkspaceRoot
    if ($WorkspaceBefore -cne $WorkspaceAfter) {
        throw "Unsigned release mutated the game/github workspace."
    }

    Write-PendingRelease -Directory $ReleaseDir
    foreach ($EphemeralRoot in $EphemeralRoots) {
        New-Item -ItemType Directory -Path (Join-Path $EphemeralRoot "stale\.git") -Force | Out-Null
    }
    $Before = Get-InventoryDigest -Directory $ReleaseDir
    Push-Location $ProjectRoot
    try {
        & make release 2.0.97 RELEASE_SIGN=0
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
    if ((Test-Path -LiteralPath (Join-Path $ReleaseDir "github\SHA256SUMS.txt")) -or
        (Test-Path -LiteralPath (Join-Path $ReleaseDir "github\SHA256SUMS.txt.asc")) -or
        (Test-Path -LiteralPath (Join-Path $ReleaseDir "game\Verification\SHA256SUMS.txt"))) {
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
            -ReleaseVersion "2.0.97" `
            -PublicRemoteUrl $FinalRemote
    }
    if (Test-Path -LiteralPath $FinalCheckout) {
        throw "Failed final public source validation left its checkout behind."
    }

    Remove-Item -LiteralPath $ReleaseDir -Recurse -Force
    Write-PendingRelease -Directory $ReleaseDir -Schema "alis-release-manifest-v2"
    $ArchiveRoot = Join-Path $TestParent "archive"
    $ArchivesBefore = @(
        Get-ChildItem -LiteralPath $ArchiveRoot -Directory -Filter "v2.0.97-flat-*" -ErrorAction SilentlyContinue |
            ForEach-Object FullName
    )
    Assert-Fails -MessagePattern "*Required release input directory is missing*" -Action {
        & $ReleaseScript `
            -ReleaseVersion "2.0.97" `
            -ReleaseDir $ReleaseDir `
            -PublicSourceRoot (Join-Path $TestParent "missing-public-source") `
            -SkipSigning
    }
    if (Test-Path -LiteralPath $ReleaseDir) {
        throw "Obsolete unsigned release was not deleted before fresh preparation."
    }
    $ArchivesAfter = @(
        Get-ChildItem -LiteralPath $ArchiveRoot -Directory -Filter "v2.0.97-flat-*" -ErrorAction SilentlyContinue |
            ForEach-Object FullName
    )
    if (($ArchivesBefore -join "`n") -cne ($ArchivesAfter -join "`n")) {
        throw "Obsolete unsigned release cleanup must not create an archive copy."
    }

    Write-PendingRelease -Directory $ReleaseDir -Schema "alis-release-manifest-v2"
    Set-Content -LiteralPath (Join-Path $ReleaseDir "SHA256SUMS.txt") -Value "fixture" -Encoding Ascii
    Assert-Fails -MessagePattern "*signed or incomplete signing output*" -Action {
        & $ReleaseScript -ReleaseVersion "2.0.97" -ReleaseDir $ReleaseDir -SkipSigning
    }
    if (-not (Test-Path -LiteralPath $ReleaseDir -PathType Container)) {
        throw "Ambiguous obsolete release must remain unchanged for inspection."
    }

    if (Test-Path -LiteralPath $ReleaseDir) {
        Remove-Item -LiteralPath $ReleaseDir -Recurse -Force
    }
    Write-PendingRelease -Directory $ReleaseDir -Status "ready_for_signature"
    $ReadyManifestPath = Join-Path $ReleaseDir "github\release_manifest.json"
    $ReadyManifest = Get-Content -LiteralPath $ReadyManifestPath -Raw | ConvertFrom-Json
    $ReadyManifest | Add-Member -NotePropertyName approval_scope -NotePropertyValue "game"
    $ReadyManifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $ReadyManifestPath -Encoding Ascii
    Assert-Fails -MessagePattern "*Game-only approval cannot be promoted*" -Action {
        & $ReleaseScript -ReleaseVersion "2.0.97" -ReleaseDir $ReleaseDir
    }
    $ReadyManifest.approval_scope = "full"
    $ReadyManifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $ReadyManifestPath -Encoding Ascii
    Assert-Fails -MessagePattern "*expected pending_owner_approval*" -Action {
        & $ReleaseScript -ReleaseVersion "2.0.97" -ReleaseDir $ReleaseDir -SkipSigning
    }

    Assert-Fails -MessagePattern "*does not match*pattern*" -Action {
        & $ReleaseScript -ReleaseVersion "2.0" -ReleaseDir $ReleaseDir -SkipSigning
    }

    Assert-Fails -MessagePattern "*ReleaseDir must remain under*tmp*" -Action {
        & $ReleaseScript `
            -ReleaseVersion "2.0.97" `
            -ReleaseDir (Join-Path $ProjectRoot "Saved\ReleaseOutsideTmp") `
            -SkipSigning
    }

    Assert-Fails -MessagePattern "*Required release input directory is missing*" -Action {
        & $ReleaseScript `
            -ReleaseVersion "2.0.96" `
            -InputRoot (Join-Path $TestParent "missing-inputs") `
            -ReleaseDir (Join-Path $TestParent "v2.0.96") `
            -PublicSourceRoot (Join-Path $TestParent "missing-public-source") `
            -SkipSigning
    }

    Assert-Fails -MessagePattern "*requires an existing reviewed release workspace*" -Action {
        & $ReleaseScript `
            -ReleaseVersion "2.0.96" `
            -ReleaseDir (Join-Path $TestParent "v2.0.96") `
            -Target Game `
            -PublicRemoteUrl (Join-Path $TestParent "must-not-be-read-public-remote")
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
    if (Test-Path -LiteralPath $WorkspaceRoot) {
        $ResolvedWorkspaceRoot = (Resolve-Path -LiteralPath $WorkspaceRoot).Path
        Remove-Item -LiteralPath $ResolvedWorkspaceRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $SchemaMismatchRoot) {
        $ResolvedSchemaMismatchRoot = (Resolve-Path -LiteralPath $SchemaMismatchRoot).Path
        Remove-Item -LiteralPath $ResolvedSchemaMismatchRoot -Recurse -Force
    }
}
