# Autonomous build script for Build Service (PowerShell on Windows)
# Ensures CMake and NASM are in PATH before running cargo

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$CargoArgs
)

$ErrorActionPreference = "Continue"

# Resolve BuildService root from script location
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildServiceRoot = Split-Path -Parent $scriptRoot
$cargoToml = Join-Path $buildServiceRoot "Cargo.toml"
if (-not (Test-Path $cargoToml)) {
    Write-Host "ERROR: Could not locate Cargo.toml at expected path: $cargoToml" -ForegroundColor Red
    exit 1
}

# Add CMake to PATH
$cmakePath = "C:\Program Files\CMake\bin"
if (Test-Path $cmakePath) {
    $env:PATH = "$cmakePath;$env:PATH"
}

# Add NASM to PATH if installed
$nasmPath = "C:\Program Files\NASM"
if (Test-Path $nasmPath) {
    $env:PATH = "$nasmPath;$env:PATH"
}

# Verify CMake is available
try {
    $cmakeVersion = & cmake --version 2>&1 | Select-Object -First 1
    Write-Host "Using CMake: $cmakeVersion" -ForegroundColor Green
} catch {
    Write-Host "ERROR: CMake not found. Please install CMake:" -ForegroundColor Red
    Write-Host "  choco install cmake" -ForegroundColor Yellow
    Write-Host "Or download from: https://cmake.org/download/" -ForegroundColor Yellow
    exit 1
}

# Set AWS_LC_SYS_PREBUILT_NASM to use prebuilt NASM objects
$env:AWS_LC_SYS_PREBUILT_NASM = "1"

# Ensure cargo is available (user-level rustup install by default)
$cargoHome = if ($env:CARGO_HOME) { $env:CARGO_HOME } else { Join-Path $HOME ".cargo" }
$cargoBin = Join-Path $cargoHome "bin"
if (Test-Path $cargoBin) {
    $pathEntries = $env:PATH -split ';'
    if ($pathEntries -notcontains $cargoBin) {
        $env:PATH = "$cargoBin;$env:PATH"
        Write-Host "Added cargo bin to PATH: $cargoBin" -ForegroundColor Green
    }
} else {
    Write-Host "WARNING: Cargo bin directory not found at $cargoBin" -ForegroundColor Yellow
}

# Run cargo command
Write-Host "Running: cargo $($CargoArgs -join " ")" -ForegroundColor Cyan
Push-Location $buildServiceRoot
Write-Host "Working directory: $buildServiceRoot" -ForegroundColor Green
try {
    & cargo @CargoArgs
    $exitCode = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($exitCode -ne 0) {
    exit $exitCode
}
