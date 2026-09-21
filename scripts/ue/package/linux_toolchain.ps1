#Requires -Version 5.1

function Test-LinuxToolchain {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$EngineRoot,
        [Parameter(Mandatory)][string]$ToolchainRoot
    )

    $SdkManifestPath = Join-Path $EngineRoot "Engine\Config\Linux\Linux_SDK.json"
    if (-not (Test-Path -LiteralPath $SdkManifestPath -PathType Leaf)) {
        throw "Linux SDK manifest was not found for the selected engine: $SdkManifestPath"
    }

    try {
        $SdkManifest = Get-Content -LiteralPath $SdkManifestPath -Raw | ConvertFrom-Json
    } catch {
        throw "Linux SDK manifest is invalid JSON: $SdkManifestPath"
    }
    $RequiredVersion = [string]$SdkManifest.MainVersion
    if ([string]::IsNullOrWhiteSpace($RequiredVersion)) {
        throw "Linux SDK manifest does not declare MainVersion: $SdkManifestPath"
    }

    if (-not (Test-Path -LiteralPath $ToolchainRoot -PathType Container)) {
        throw "Configured Linux cross-toolchain root does not exist: $ToolchainRoot"
    }
    $ResolvedRoot = (Resolve-Path -LiteralPath $ToolchainRoot).Path
    $MarkerPath = Join-Path $ResolvedRoot "ToolchainVersion.txt"
    if (-not (Test-Path -LiteralPath $MarkerPath -PathType Leaf)) {
        throw "Configured Linux cross-toolchain has no ToolchainVersion.txt: $ResolvedRoot"
    }
    $ActualVersion = (Get-Content -LiteralPath $MarkerPath -Raw).Trim()
    if ($ActualVersion -ne $RequiredVersion) {
        throw "Selected engine requires Linux toolchain '$RequiredVersion', found '$ActualVersion' at '$ResolvedRoot'."
    }

    $CompilerPath = Join-Path $ResolvedRoot "x86_64-unknown-linux-gnu\bin\clang++.exe"
    if (-not (Test-Path -LiteralPath $CompilerPath -PathType Leaf)) {
        throw "Configured Linux cross-toolchain is incomplete; clang++.exe was not found: $CompilerPath"
    }

    return [PSCustomObject]@{
        Root = $ResolvedRoot
        Version = $RequiredVersion
        Compiler = $CompilerPath
    }
}

function Invoke-WithLinuxToolchain {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ToolchainRoot,
        [Parameter()][object]$Context,
        [Parameter(Mandatory)][scriptblock]$Action
    )

    $PreviousRoot = [Environment]::GetEnvironmentVariable(
        "LINUX_MULTIARCH_ROOT", "Process")
    try {
        [Environment]::SetEnvironmentVariable(
            "LINUX_MULTIARCH_ROOT", $ToolchainRoot, "Process")
        & $Action $Context
    } finally {
        [Environment]::SetEnvironmentVariable(
            "LINUX_MULTIARCH_ROOT", $PreviousRoot, "Process")
    }
}
