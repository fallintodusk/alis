#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackageDir = Split-Path -Parent $ScriptDir
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PackageDir))
$PackageScript = Join-Path $PackageDir "package_release.ps1"
$SourceWrapper = Join-Path $PackageDir "package_release_source.bat"
$OwnerRoot = Join-Path $ProjectRoot "tmp\package\engine_route_admission"
$TestRoot = Join-Path $OwnerRoot ([Guid]::NewGuid().ToString("N"))
$FakeEngine = Join-Path $TestRoot "source_engine"
$FakeRunUat = Join-Path $FakeEngine "Engine\Build\BatchFiles\RunUAT.bat"
$OutputDir = Join-Path $TestRoot "output"

try {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $FakeRunUat) | Out-Null
    @(
        "@echo off",
        "echo FAKE_UAT_REACHED",
        "exit /b 37"
    ) | Set-Content -LiteralPath $FakeRunUat -Encoding Ascii

    $PreviousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $Output = & powershell.exe `
            -NoProfile `
            -ExecutionPolicy Bypass `
            -File $PackageScript `
            -EngineRoot $FakeEngine `
            -OutputDir $OutputDir `
            -SkipBuild 2>&1 | Out-String
        $ExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $PreviousErrorAction
    }

    if ($ExitCode -eq 0) {
        throw "Non-installed engine was admitted without -SourceRelease."
    }
    if ($Output -notmatch "requires explicit -SourceRelease") {
        throw "Source-engine rejection did not explain the explicit release route."
    }
    if ($Output -match "FAKE_UAT_REACHED") {
        throw "Source-engine rejection happened after UAT was launched."
    }

    $WrapperText = Get-Content -LiteralPath $SourceWrapper -Raw
    if ($WrapperText -notmatch '(?i)package_release\.ps1" -EngineRoot "%UE_SOURCE_PATH%" -SourceRelease') {
        throw "Source release wrapper does not supply -SourceRelease."
    }

    Write-Host "[OK] Candidate packaging rejects source-engine cache switching before UAT."
}
finally {
    if (Test-Path -LiteralPath $TestRoot) {
        $ResolvedOwnerRoot = (Resolve-Path -LiteralPath $OwnerRoot).Path
        $ResolvedTestRoot = (Resolve-Path -LiteralPath $TestRoot).Path
        if (-not $ResolvedTestRoot.StartsWith(
            $ResolvedOwnerRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean unexpected test path: $ResolvedTestRoot"
        }
        Remove-Item -LiteralPath $ResolvedTestRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $OwnerRoot) {
        $Remaining = @(Get-ChildItem -LiteralPath $OwnerRoot -Force)
        if ($Remaining.Count -eq 0) {
            Remove-Item -LiteralPath $OwnerRoot -Force
        }
    }
}
