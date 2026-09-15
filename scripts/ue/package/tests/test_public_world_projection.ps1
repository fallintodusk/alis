#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackageRoot = Split-Path -Parent $ScriptDir
. (Join-Path $PackageRoot 'public_world_projection.ps1')
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PackageRoot))
$TestParent = Join-Path $ProjectRoot 'tmp\release\tests\public-world-projection'
$TestRoot = Join-Path $TestParent ([Guid]::NewGuid().ToString('N'))
$CleanupWorkParent = $null

try {
    New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null
    $file = Join-Path $TestRoot 'asset.uasset'
    [IO.File]::WriteAllBytes($file, [byte[]](1, 2, 3))
    $records = @([pscustomobject]@{ Source = $file })
    $before = Get-ProjectWorldProjectionDigest -Records $records
    [IO.File]::WriteAllBytes($file, [byte[]](1, 2, 4))
    $after = Get-ProjectWorldProjectionDigest -Records $records
    if ($before -ceq $after) {
        throw 'Projection restoration digest did not detect changed bytes.'
    }
    $resolved = Convert-ProjectWorldPackageRootToContentPath `
        -ContentRoot $TestRoot -PackageRoot '/ProjectWorldData/Generated/Territory/Water/'
    if (-not $resolved.EndsWith('Generated\Territory\Water')) {
        throw "Public package root resolved incorrectly: $resolved"
    }
    $rejected = $false
    try {
        Convert-ProjectWorldPackageRootToContentPath `
            -ContentRoot $TestRoot -PackageRoot '/Other/Generated/Territory/Water/' | Out-Null
    }
    catch {
        $rejected = $_.Exception.Message -like '*Unsupported public World artifact root*'
    }
    if (-not $rejected) {
        throw 'Foreign public World asset owner was not rejected.'
    }
    $inputSource = Get-Content -LiteralPath (Join-Path $PackageRoot 'prepare_release_inputs.ps1') -Raw
    if ($inputSource -match 'Find-PublicAssetRoot' -or
        $inputSource -match 'Sort-Object\s+LastWriteTimeUtc') {
        throw 'Release inputs still discover a historical public World projection.'
    }
    if ($inputSource -notmatch 'Invoke-WithProjectWorldPublicProjection') {
        throw 'Release inputs do not use the current ProjectWorldData projection owner.'
    }
    if ($inputSource -notmatch 'git\s+-c\s+core\.longpaths=true\s+-c\s+advice\.detachedHead=false\s+clone\s+--quiet') {
        throw 'Public clean checkout does not enable long paths and suppress expected clone progress and detached-tag advice.'
    }
    if ($inputSource -notmatch 'git\s+-C\s+\$CleanCheckout\s+config\s+core\.longpaths\s+true') {
        throw 'Public clean checkout does not persist Windows long-path support for consumer scripts.'
    }
    $mirrorSource = Get-Content -LiteralPath `
        (Join-Path $ProjectRoot 'scripts\git\mirror\mirror_to_github.sh') -Raw
    if ($mirrorSource -notmatch 'config core\.longpaths true') {
        throw 'Reviewed public source repositories do not persist Windows long-path support.'
    }
    if ($inputSource -notmatch 'Join-Path \$ProjectRoot "tmp\\release\\c"' -or
        $inputSource -match '\$CleanCheckout\s*=\s*Join-Path \$WorkRoot') {
        throw 'Public clean checkout is not isolated under the short release-owned path.'
    }
    if ($inputSource -notmatch '(?s)finally\s*\{.*Remove-Item\s+-LiteralPath\s+\$CleanCheckout\s+-Recurse\s+-Force' -or
        $inputSource -notmatch '(?s)finally\s*\{.*-not\s+\$InputsPromoted.*Remove-Item\s+-LiteralPath\s+\$WorkRoot\s+-Recurse\s+-Force') {
        throw 'Release input preparation does not clean ephemeral checkouts and failed work trees.'
    }
    $CleanupVersion = "9.8.$([DateTime]::UtcNow.Ticks)"
    $CleanupTag = "v$CleanupVersion"
    $CleanupWorkParent = Join-Path $ProjectRoot "tmp\release\work\$CleanupTag"
    $CleanupInputRoot = Join-Path $TestRoot 'failed-inputs'
    $PreparationFailed = $false
    try {
        & (Join-Path $PackageRoot 'prepare_release_inputs.ps1') `
            -ReleaseVersion $CleanupVersion `
            -InputRoot $CleanupInputRoot `
            -PublicAssetRoot $TestRoot *> $null
    }
    catch {
        $PreparationFailed = $true
    }
    if (-not $PreparationFailed) {
        throw 'Invalid release inputs unexpectedly passed preparation.'
    }
    if (Test-Path -LiteralPath $CleanupWorkParent) {
        throw 'Failed release input preparation left its work tree behind.'
    }
    Write-Host '[OK] Public World release projection is current-owner-bound and drift-sensitive.'
}
finally {
    if ($CleanupWorkParent -and (Test-Path -LiteralPath $CleanupWorkParent)) {
        Remove-Item -LiteralPath $CleanupWorkParent -Recurse -Force
    }
    if (Test-Path -LiteralPath $TestRoot) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force
    }
}
