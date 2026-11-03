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

    function Invoke-LinkedMirror {
        $priorPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $output = @(& powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass `
                -File (Join-Path $scriptDir 'mirror_to_github.ps1') `
                --dry-run --ephemeral-preview --force 2>&1)
            return [pscustomobject]@{ ExitCode = $LASTEXITCODE; Text = $output -join "`n" }
        }
        finally { $ErrorActionPreference = $priorPreference }
    }

    $run = Invoke-LinkedMirror
    if ($run.ExitCode -eq 0 -or $run.Text -notmatch 'Exclude file not found' -or
        $run.Text -match 'Could not detect repository root|Linked worktree Git identity drift') {
        throw "Linked worktree mirror did not pass its Git source boundary: $($run.Text)"
    }
    Write-Host '[OK] Windows linked worktree resolves in WSL through the mirror adapter.'

    # The private patterns are gitignored, so a linked worktree never holds them.
    foreach ($carrier in 'mirror.exclude', 'forbidden_text_patterns.regex') {
        Copy-Item -LiteralPath (Join-Path $MirrorRoot $carrier) -Destination $scriptDir
    }
    $run = Invoke-LinkedMirror
    if ($run.ExitCode -eq 0 -or $run.Text -notmatch 'Private forbidden patterns file not found') {
        throw "Missing private patterns did not fail closed: $($run.Text)"
    }
    Write-Host '[OK] Mirror refuses to run without the private patterns.'

    $mainScriptDir = Join-Path $Source 'scripts\git\mirror'
    New-Item -ItemType Directory -Path $mainScriptDir -Force | Out-Null
    $mainPatterns = Join-Path $mainScriptDir 'forbidden_private_text_patterns.regex'
    '# comment only' | Set-Content -LiteralPath $mainPatterns -Encoding Ascii
    $run = Invoke-LinkedMirror
    if ($run.ExitCode -eq 0 -or $run.Text -notmatch 'has no active patterns') {
        throw "Empty private patterns did not fail closed: $($run.Text)"
    }
    Write-Host '[OK] Mirror refuses private patterns with no active pattern.'

    # Reaching the scan needs no project projection, so those two steps are stubbed.
    foreach ($stub in 'sanitize_public_text.py', 'prepare_public_source.py') {
        'import sys; sys.exit(0)' | Set-Content -LiteralPath (Join-Path $scriptDir $stub) -Encoding Ascii
    }
    # Set-Content writes CRLF on purpose: this gitignored file has no eol normalization.
    $marker = 'fixture-forbidden-marker'
    $marker | Set-Content -LiteralPath $mainPatterns -Encoding Ascii

    $run = Invoke-LinkedMirror
    if ($run.ExitCode -ne 0 -or $run.Text -notmatch 'Ephemeral preview complete') {
        throw "Safe candidate did not pass through the main worktree patterns: $($run.Text)"
    }
    Write-Host '[OK] Linked worktree reads the main worktree patterns and passes a safe candidate.'

    $failures = New-Object System.Collections.Generic.List[string]
    function Test-MarkerRejected {
        param([string]$Name, [string]$Relative)
        $path = Join-Path $Linked $Relative
        $original = if (Test-Path -LiteralPath $path) { [IO.File]::ReadAllBytes($path) } else { $null }
        New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
        # LF content: a CR kept in the compiled pattern would no longer match it.
        [IO.File]::AppendAllText($path, "# $marker`n")
        try { $result = Invoke-LinkedMirror }
        finally {
            if ($null -eq $original) { Remove-Item -LiteralPath $path } else { [IO.File]::WriteAllBytes($path, $original) }
        }
        $location = [regex]::Escape($Relative.Replace('\', '/')) + ':\d+'
        if ($result.ExitCode -eq 0 -or $result.Text -notmatch "Forbidden content matched at $location") {
            $failures.Add("$Name was not rejected at its path and line.")
        }
        if ($result.Text -match $marker) {
            $failures.Add("$Name printed the matched text.")
        }
    }

    Test-MarkerRejected 'Published file' 'leak.txt'
    Test-MarkerRejected 'Mirror test folder' 'scripts\git\mirror\tests\leak.txt'
    Test-MarkerRejected 'Mirror script' 'scripts\git\mirror\mirror_to_github.sh'
    Test-MarkerRejected 'Public pattern file' 'scripts\git\mirror\forbidden_text_patterns.regex'
    '# comment only' | Set-Content -LiteralPath (Join-Path $scriptDir 'forbidden_text_patterns.regex') -Encoding Ascii
    Test-MarkerRejected 'Comment-only public list' 'leak.txt'

    if ($failures.Count -gt 0) {
        throw ($failures -join "`n")
    }
    Write-Host '[OK] Private patterns reject every published path without printing the match.'
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
