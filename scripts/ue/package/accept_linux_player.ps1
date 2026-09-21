#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ArchiveReport,
    [Parameter(Mandatory = $true)][string]$OutputReceipt,
    [string]$Distribution = "Ubuntu",
    [int]$TimeoutSeconds = 720
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$Implementation = Join-Path $ScriptDir "accept_linux_player.py"
$KazanProfile = Join-Path $ProjectRoot `
    "Plugins\World\ProjectWorldData\Data\Runtime\kazan_territory_512_1536_v1.json"
$ManhattanProfile = Join-Path $ProjectRoot `
    "Plugins\World\ProjectWorldData\Data\Runtime\manhattan_showcase_512_1536_v1.json"

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw "wsl.exe is required for Linux player acceptance."
}
$ResolvedReport = [IO.Path]::GetFullPath($ArchiveReport)
$ResolvedOutput = [IO.Path]::GetFullPath($OutputReceipt)
$TmpRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "tmp")).TrimEnd('\', '/')
if (-not $ResolvedOutput.StartsWith(
        $TmpRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Linux acceptance output must remain under the project tmp directory."
}
if (Test-Path -LiteralPath $ResolvedOutput) {
    throw "Linux acceptance output already exists: $ResolvedOutput"
}

function Convert-ToWslPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    $Converted = & wsl.exe -d $Distribution -- wslpath -a "'$Path'"
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($Converted)) {
        throw "Unable to convert path for WSL: $Path"
    }
    return $Converted.Trim()
}

$WslImplementation = Convert-ToWslPath $Implementation
$WslReport = Convert-ToWslPath $ResolvedReport
$WslOutput = Convert-ToWslPath $ResolvedOutput
$KazanHash = (Get-FileHash -LiteralPath $KazanProfile -Algorithm SHA256).Hash.ToLowerInvariant()
$ManhattanHash = (Get-FileHash -LiteralPath $ManhattanProfile -Algorithm SHA256).Hash.ToLowerInvariant()

& wsl.exe -d $Distribution -- python3 $WslImplementation `
    --archive-report $WslReport `
    --output $WslOutput `
    --kazan-runtime-sha256 $KazanHash `
    --manhattan-runtime-sha256 $ManhattanHash `
    --timeout-seconds $TimeoutSeconds
if ($LASTEXITCODE -ne 0) {
    throw "Linux player acceptance failed."
}

Write-Host "Linux player acceptance passed: $ResolvedOutput" -ForegroundColor Green
