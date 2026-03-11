[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$MirrorArgs
)

$ErrorActionPreference = "Stop"

function Test-ArgPresent {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Args,
        [Parameter(Mandatory = $true)]
        [string[]]$Needles
    )

    foreach ($Needle in $Needles) {
        if ($Args -contains $Needle) {
            return $true
        }
    }

    return $false
}

function Assert-CleanWorkingTree {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $StatusLines = @(& git -c core.fsmonitor=false -C $RepoRoot status --porcelain=v1 --untracked-files=all 2>$null)
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to inspect working tree state with git status."
    }

    if (-not $StatusLines -or ($StatusLines.Count -eq 1 -and [string]::IsNullOrWhiteSpace($StatusLines[0]))) {
        return
    }

    $Staged = New-Object System.Collections.Generic.List[string]
    $Unstaged = New-Object System.Collections.Generic.List[string]
    $Untracked = New-Object System.Collections.Generic.List[string]

    foreach ($Line in $StatusLines) {
        if ([string]::IsNullOrWhiteSpace($Line) -or $Line.Length -lt 3) {
            continue
        }

        $IndexStatus = $Line[0]
        $WorktreeStatus = $Line[1]
        $Path = $Line.Substring(3)

        if ($IndexStatus -eq '?') {
            $Untracked.Add($Path) | Out-Null
            continue
        }

        if ($IndexStatus -ne ' ') {
            $Staged.Add($Path) | Out-Null
        }

        if ($WorktreeStatus -ne ' ') {
            $Unstaged.Add($Path) | Out-Null
        }
    }

    if ($Staged.Count -eq 0 -and $Unstaged.Count -eq 0 -and $Untracked.Count -eq 0) {
        return
    }

    if ($Staged.Count -gt 0) {
        Write-Error ("[FAIL] Staged changes detected:`n" + ($Staged -join "`n"))
    }
    if ($Unstaged.Count -gt 0) {
        Write-Error ("[FAIL] Unstaged changes detected:`n" + ($Unstaged -join "`n"))
    }
    if ($Untracked.Count -gt 0) {
        Write-Error ("[FAIL] Untracked files detected:`n" + ($Untracked -join "`n"))
    }

    throw "[FAIL] Source repository has local changes. Commit/stash changes first, or re-run with --force."
}

function Convert-ToWslPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $Converted = & wsl.exe wslpath -a $Path
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to convert path to WSL form: $Path"
    }

    return ($Converted | Out-String).Trim()
}

function Convert-ToBashSingleQuoted {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $QuoteEscape = [string]::Concat("'", '"', "'", '"', "'")
    return "'" + $Value.Replace("'", $QuoteEscape) + "'"
}

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw "wsl.exe is required for mirror_to_github.ps1."
}

$RepoRoot = (& git -C $PSScriptRoot rev-parse --show-toplevel 2>$null | Out-String).Trim()
if (-not $RepoRoot) {
    throw "Could not detect repository root from $PSScriptRoot."
}

$RepoRootWsl = Convert-ToWslPath -Path $RepoRoot
$ScriptPathWsl = "$RepoRootWsl/scripts/git/mirror/mirror_to_github.sh"

$BypassDirtyCheck = Test-ArgPresent -Args $MirrorArgs -Needles @("--force", "--allow-dirty")
if (-not $BypassDirtyCheck) {
    Assert-CleanWorkingTree -RepoRoot $RepoRoot
}

$ForwardArgs = New-Object System.Collections.Generic.List[string]
for ($Index = 0; $Index -lt $MirrorArgs.Count; $Index++) {
    $Arg = $MirrorArgs[$Index]
    if ($Arg -eq "--exclude-file" -or $Arg -eq "--forbidden-patterns-file") {
        $ForwardArgs.Add($Arg) | Out-Null
        if ($Index + 1 -ge $MirrorArgs.Count) {
            throw "Missing value for $Arg"
        }
        $Value = $MirrorArgs[$Index + 1]
        if ([System.IO.Path]::IsPathRooted($Value)) {
            $Value = Convert-ToWslPath -Path $Value
        }
        $ForwardArgs.Add($Value) | Out-Null
        $Index++
        continue
    }

    $ForwardArgs.Add($Arg) | Out-Null
}

if (-not $BypassDirtyCheck) {
    # Native Windows git status is much faster and avoids WSL/LFS dirty-check hangs.
    $ForwardArgs.Add("--force") | Out-Null
}

$RepoRootWslQuoted = Convert-ToBashSingleQuoted -Value $RepoRootWsl
$ScriptPathWslQuoted = Convert-ToBashSingleQuoted -Value $ScriptPathWsl
$ArgString = ($ForwardArgs | ForEach-Object { Convert-ToBashSingleQuoted -Value $_ }) -join " "
$Prefix = ""
if ($env:MIRROR_GIT_SSH_COMMAND) {
    $Prefix = "export MIRROR_GIT_SSH_COMMAND=" + (Convert-ToBashSingleQuoted -Value $env:MIRROR_GIT_SSH_COMMAND) + "; "
}

$BashCommand = "$Prefix" + "cd $RepoRootWslQuoted && exec bash $ScriptPathWslQuoted"
if ($ArgString) {
    $BashCommand += " $ArgString"
}

& wsl.exe bash -lc $BashCommand
exit $LASTEXITCODE
