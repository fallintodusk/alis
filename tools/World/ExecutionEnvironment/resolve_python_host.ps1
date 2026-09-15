#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = "Stop"
$LockPath = Join-Path $PSScriptRoot "requirements.lock.txt"
$VersionLines = @(
    Get-Content -LiteralPath $LockPath |
        Where-Object { $_ -match '^#\s*python-version:\s*([0-9]+\.[0-9]+)\s*$' }
)
if ($VersionLines.Count -ne 1) {
    throw "World Python dependency lock must declare exactly one python-version."
}
$ExpectedVersion = [regex]::Match(
    $VersionLines[0],
    '^#\s*python-version:\s*([0-9]+\.[0-9]+)\s*$'
).Groups[1].Value

function Resolve-CompatiblePython {
    param(
        [string]$Executable,
        [string[]]$PrefixArguments = @()
    )

    $Probe = "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}|{sys.executable}')"
    $Output = @(& $Executable @PrefixArguments -S -c $Probe 2>$null)
    if ($LASTEXITCODE -ne 0 -or $Output.Count -ne 1) {
        return $null
    }
    $Parts = $Output[0] -split '\|', 2
    if ($Parts.Count -ne 2 -or $Parts[0] -cne $ExpectedVersion -or
        -not (Test-Path -LiteralPath $Parts[1] -PathType Leaf)) {
        return $null
    }
    return [IO.Path]::GetFullPath($Parts[1])
}

$CurrentPython = Get-Command python -ErrorAction SilentlyContinue
if ($CurrentPython) {
    $Resolved = Resolve-CompatiblePython -Executable $CurrentPython.Source
    if ($Resolved) {
        Write-Output $Resolved
        exit 0
    }
}

$PythonLauncher = Get-Command py -ErrorAction SilentlyContinue
if ($PythonLauncher) {
    $Resolved = Resolve-CompatiblePython -Executable $PythonLauncher.Source `
        -PrefixArguments @("-$ExpectedVersion")
    if ($Resolved) {
        Write-Output $Resolved
        exit 0
    }
}

throw "World tooling requires CPython $ExpectedVersion on Windows; no compatible host was found."
