#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = "Stop"
$EnvironmentRoot = Split-Path -Parent $PSScriptRoot
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $EnvironmentRoot))
$Resolver = Join-Path $EnvironmentRoot "resolve_python_host.ps1"
$TestRoot = Join-Path $ProjectRoot "tmp\world\execution_environment\resolver-test"
$FakeBin = Join-Path $TestRoot "wrong-python"
$OriginalPath = $env:PATH

if (Test-Path -LiteralPath $TestRoot) {
    throw "Python resolver fixture already exists: $TestRoot"
}

try {
    New-Item -ItemType Directory -Path $FakeBin -Force | Out-Null
    @(
        "@echo off"
        "echo Python 3.11.0"
        "exit /b 0"
    ) | Set-Content -LiteralPath (Join-Path $FakeBin "python.cmd") -Encoding Ascii
    $env:PATH = "$FakeBin;$OriginalPath"

    if (-not (Test-Path -LiteralPath $Resolver -PathType Leaf)) {
        throw "Pinned Python host resolver is missing: $Resolver"
    }
    $ResolvedPython = @(& $Resolver)
    if ($ResolvedPython.Count -ne 1 -or
        -not (Test-Path -LiteralPath $ResolvedPython[0] -PathType Leaf)) {
        throw "Pinned Python host resolver returned an invalid executable."
    }
    $Version = @(& $ResolvedPython[0] -S -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
    if ($LASTEXITCODE -ne 0 -or $Version.Count -ne 1 -or $Version[0] -cne "3.14") {
        throw "Pinned Python host resolver selected the active unsupported Python."
    }
    Write-Host "[OK] World Python resolver ignored an unsupported active environment"
}
finally {
    $env:PATH = $OriginalPath
    if (Test-Path -LiteralPath $TestRoot) {
        $ResolvedOwner = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "tmp\world\execution_environment")).TrimEnd('\', '/')
        $ResolvedTestRoot = (Resolve-Path -LiteralPath $TestRoot).Path
        if (-not $ResolvedTestRoot.StartsWith(
                $ResolvedOwner + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected resolver test path: $ResolvedTestRoot"
        }
        Remove-Item -LiteralPath $ResolvedTestRoot -Recurse -Force
    }
}
