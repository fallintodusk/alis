[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$MirrorArgs
)

$ErrorActionPreference = "Stop"

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
