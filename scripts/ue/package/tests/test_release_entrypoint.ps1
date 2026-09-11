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

$ReleaseSource = Get-Content -LiteralPath $ReleaseScript -Raw
if ($ReleaseSource -match '-Mode\s+Accept' -or
    $ReleaseSource -match 'After the Shipping walkthrough') {
    throw "Release entrypoint must not stop for a separate Candidate approval."
}

function Write-PendingRelease {
    param(
        [string]$Directory,
        [string]$Status = "pending_owner_approval"
    )

    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $PayloadPath = Join-Path $Directory "fixture.bin"
    [IO.File]::WriteAllBytes($PayloadPath, [byte[]](1, 2, 3, 4))
    $Payload = Get-Item -LiteralPath $PayloadPath
    $Manifest = [ordered]@{
        schema = "alis-release-manifest-v1"
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
        Get-ChildItem -LiteralPath $Directory -File |
            Sort-Object Name |
            ForEach-Object {
                "{0}|{1}" -f $_.Name, (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
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

    Remove-Item -LiteralPath $ReleaseDir -Recurse -Force
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
