#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = 'Stop'
$MirrorRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MirrorRoot))
$TestRoot = Join-Path $ProjectRoot ('tmp\release\tests\mirror-linked-worktree\' + [guid]::NewGuid().ToString('N'))
$Source = Join-Path $TestRoot 'source'
$Linked = Join-Path $TestRoot 'linked'

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw 'Linked worktree mirror regression requires WSL.'
}
$bashSource = Get-Content -LiteralPath (Join-Path $MirrorRoot 'mirror_to_github.sh') -Raw
if ($bashSource -notmatch 'GIT_DIR="\$MIRROR_SOURCE_GIT_DIR" GIT_WORK_TREE="\$REPO_ROOT"\s+\\\s+python3') {
    throw 'Developer payload composition lost the linked source Git context.'
}

New-Item -ItemType Directory -Path $Source -Force | Out-Null
try {
    & git -C $Source init --quiet
    & git -C $Source config user.name test
    & git -C $Source config user.email test@localhost
    'fixture' | Set-Content -LiteralPath (Join-Path $Source 'file.txt') -Encoding Ascii
    & git -C $Source add file.txt
    & git -C $Source commit --quiet -m fixture
    & git -C $Source worktree add --quiet --detach $Linked HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot create linked worktree fixture.' }
    $scriptDir = Join-Path $Linked 'scripts\git\mirror'
    New-Item -ItemType Directory -Path $scriptDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $MirrorRoot 'mirror_to_github.ps1') -Destination $scriptDir
    Copy-Item -LiteralPath (Join-Path $MirrorRoot 'mirror_to_github.sh') -Destination $scriptDir
    $priorPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass `
            -File (Join-Path $scriptDir 'mirror_to_github.ps1') `
            --dry-run --ephemeral-preview --force 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $priorPreference }
    $text = $output -join "`n"
    if ($exitCode -eq 0 -or $text -notmatch 'Exclude file not found' -or
        $text -match 'Could not detect repository root|Linked worktree Git identity drift') {
        throw "Linked worktree mirror did not pass its Git source boundary: $text"
    }
    Write-Host '[OK] Windows linked worktree resolves in WSL through the mirror adapter.'
}
finally {
    if (Test-Path -LiteralPath $Linked) {
        & git -C $Source worktree remove --force $Linked | Out-Null
    }
    $tmpRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'tmp')).TrimEnd('\', '/')
    $resolvedTestRoot = [IO.Path]::GetFullPath($TestRoot)
    if (-not $resolvedTestRoot.StartsWith($tmpRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Linked worktree fixture escaped project tmp.'
    }
    if (Test-Path -LiteralPath $resolvedTestRoot) {
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }
}
