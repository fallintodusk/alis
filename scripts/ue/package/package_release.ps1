#Requires -Version 5.1
<#
.SYNOPSIS
    Package a Win64 ALIS release build via RunUAT BuildCookRun.

.DESCRIPTION
    Reads UE_PATH from scripts/config/ue_path.conf by default.
    Uses release-safe defaults:
    - Shipping config
    - IoStore/Pak packaging
    - -nodebuginfo to keep staged PDBs out of the distributable payload
    - -skipencryption by default because current ALIS Shipping uses modular linking
      and encrypted startup containers fail before the game module can register the key
    - GitHub-safe split zip transport by default when creating release archives
#>

param(
    [string]$OutputDir,
    [string]$ClientConfig = "Shipping",
    [string]$Platform = "Win64",
    [string]$EngineRoot,
    [switch]$SkipBuild,
    [switch]$IncludeStagedDebugFiles,
    [switch]$EncryptContent,
    [switch]$CreateReleaseArchive,
    [int]$SplitSizeMB = 1700
)

$ErrorActionPreference = "Stop"

function Get-ConfigValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    if (-not (Test-Path $Path)) {
        return $null
    }

    foreach ($line in Get-Content $Path) {
        if ($line -match "^\s*$Key\s*(:=|=)\s*(.+)$") {
            return $Matches[2].Trim().Trim('"', "'")
        }
    }

    return $null
}

function Format-Bytes {
    param(
        [Parameter(Mandatory = $true)]
        [Int64]$Bytes
    )

    return "{0} bytes ({1:N3} GiB)" -f $Bytes, ($Bytes / 1GB)
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$ConfigFile = Join-Path $ProjectRoot "scripts\config\ue_path.conf"
$ProjectFile = Join-Path $ProjectRoot "Alis.uproject"

if (-not $EngineRoot) {
    $EngineRoot = Get-ConfigValue -Path $ConfigFile -Key "UE_PATH"
}

if (-not $EngineRoot) {
    throw "UE_PATH is not set. Configure scripts/config/ue_path.conf or pass -EngineRoot."
}

$RunUAT = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path $RunUAT)) {
    throw "RunUAT.bat not found under engine root: $EngineRoot"
}

if (-not (Test-Path $ProjectFile)) {
    throw "Project file not found: $ProjectFile"
}

if (-not $OutputDir) {
    $Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputDir = Join-Path $ProjectRoot "Saved\PackageRelease\ALIS_$Stamp"
}

$OutputDir = (New-Item -ItemType Directory -Force -Path $OutputDir).FullName
$LogDir = Join-Path $ProjectRoot "Saved\Logs"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$LogFile = Join-Path $LogDir ("package_release_{0}.log" -f (Get-Date -Format "yyyyMMdd_HHmmss"))

$Args = @(
    "BuildCookRun",
    "-project=$ProjectFile",
    "-platform=$Platform",
    "-clientconfig=$ClientConfig",
    "-cook",
    "-stage",
    "-pak",
    "-iostore",
    "-package",
    "-archive",
    "-archivedirectory=$OutputDir",
    "-NoP4",
    "-utf8output",
    "-unattended"
)

if (-not $SkipBuild) {
    $Args += "-build"
}

if (-not $IncludeStagedDebugFiles) {
    $Args += "-nodebuginfo"
}

if (-not $EncryptContent) {
    $Args += "-skipencryption"
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " ALIS Release Packaging" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "UE_PATH      = $EngineRoot"
Write-Host "PROJECT_FILE = $ProjectFile"
Write-Host "PLATFORM     = $Platform"
Write-Host "CONFIG       = $ClientConfig"
Write-Host "OUTPUT_DIR   = $OutputDir"
Write-Host "LOG_FILE     = $LogFile"
Write-Host "SKIP_BUILD   = $SkipBuild"
Write-Host "NODEBUGINFO  = $(-not $IncludeStagedDebugFiles)"
Write-Host "ENCRYPTION   = $EncryptContent"
Write-Host "ZIP_RELEASE  = $CreateReleaseArchive"
Write-Host "SPLIT_SIZE   = $SplitSizeMB MiB"
Write-Host ""

& $RunUAT @Args *> $LogFile
$ExitCode = $LASTEXITCODE

if ($ExitCode -ne 0) {
    Write-Host "[ERROR] Packaging failed with exit code $ExitCode" -ForegroundColor Red
    Write-Host "See log: $LogFile" -ForegroundColor Yellow
    Get-Content $LogFile -Tail 120
    exit $ExitCode
}

$WindowsDir = Join-Path $OutputDir "Windows"
if (-not (Test-Path $WindowsDir)) {
    throw "Packaging succeeded but output folder was not found: $WindowsDir"
}

$AllFiles = Get-ChildItem $WindowsDir -Recurse -File
$ReleaseFiles = if ($IncludeStagedDebugFiles) {
    $AllFiles
} else {
    $AllFiles | Where-Object { $_.Extension -ne ".pdb" }
}

$LargestFile = $ReleaseFiles | Sort-Object Length -Descending | Select-Object -First 1
$TotalBytes = ($ReleaseFiles | Measure-Object Length -Sum).Sum
$OverLimitFiles = $ReleaseFiles | Where-Object { $_.Length -ge 2GB }

$SummaryLines = @(
    "ALIS Release Packaging Summary",
    "OutputDir=$OutputDir",
    "WindowsDir=$WindowsDir",
    "EncryptContent=$EncryptContent",
    "FileCount=$($ReleaseFiles.Count)",
    "Total=$([string](Format-Bytes -Bytes $TotalBytes))",
    "LargestFile=$($LargestFile.FullName)",
    "LargestFileSize=$([string](Format-Bytes -Bytes $LargestFile.Length))",
    "FilesOver2GiB=$($OverLimitFiles.Count)"
)

$ArchiveOutputs = @()

if ($CreateReleaseArchive) {
    $SevenZip = Get-Command "7z.exe" -ErrorAction SilentlyContinue
    if (-not $SevenZip) {
        $SevenZip = Get-Command "7z" -ErrorAction SilentlyContinue
    }

    if (-not $SevenZip) {
        throw "7-Zip was not found in PATH. Install 7-Zip or omit -CreateReleaseArchive."
    }

    $ArchiveBase = Join-Path $OutputDir ("ALIS_Win64_{0}.zip" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
    $ArchiveInput = Join-Path $WindowsDir "*"
    $ZipArgs = @("a", "-tzip", $ArchiveBase, $ArchiveInput)

    & $SevenZip.Source @ZipArgs
    if ($LASTEXITCODE -ne 0) {
        throw "7-Zip archive creation failed with exit code $LASTEXITCODE"
    }

    $ArchiveOutputs = @(
        Get-ChildItem $OutputDir -File |
            Where-Object { $_.Name -like ([System.IO.Path]::GetFileName($ArchiveBase) + "*") } |
            Sort-Object Name
    )

    $SplitThresholdBytes = [Int64]$SplitSizeMB * 1MB
    if ($SplitSizeMB -gt 0 -and $ArchiveOutputs.Count -eq 1 -and $ArchiveOutputs[0].Length -gt $SplitThresholdBytes) {
        Remove-Item $ArchiveOutputs[0].FullName -Force

        $ZipArgs = @("a", "-tzip", ("-v{0}m" -f $SplitSizeMB), $ArchiveBase, $ArchiveInput)
        & $SevenZip.Source @ZipArgs
        if ($LASTEXITCODE -ne 0) {
            throw "7-Zip split archive creation failed with exit code $LASTEXITCODE"
        }

        $ArchiveOutputs = @(
            Get-ChildItem $OutputDir -File |
                Where-Object { $_.Name -like ([System.IO.Path]::GetFileName($ArchiveBase) + "*") } |
                Sort-Object Name
        )
    }

    $SummaryLines += "ArchiveParts=$($ArchiveOutputs.Count)"
    foreach ($ArchiveFile in $ArchiveOutputs) {
        $SummaryLines += "ArchivePart=$($ArchiveFile.Name) :: $([string](Format-Bytes -Bytes $ArchiveFile.Length))"
    }
}

$SummaryPath = Join-Path $OutputDir "package_summary.txt"
$SummaryLines | Set-Content -Encoding Ascii $SummaryPath

Write-Host ""
Write-Host "Packaging completed successfully." -ForegroundColor Green
Write-Host "Summary: $SummaryPath"
Write-Host "Largest release file: $($LargestFile.Name) :: $(Format-Bytes -Bytes $LargestFile.Length)"
Write-Host "Total release payload: $(Format-Bytes -Bytes $TotalBytes)"

if ($OverLimitFiles.Count -gt 0) {
    Write-Host "[WARNING] Files at or above 2 GiB were detected:" -ForegroundColor Yellow
    foreach ($File in $OverLimitFiles) {
        Write-Host "  $($File.FullName) :: $(Format-Bytes -Bytes $File.Length)" -ForegroundColor Yellow
    }
}

if ($ArchiveOutputs.Count -gt 0) {
    Write-Host "Archive outputs:" -ForegroundColor Green
    foreach ($ArchiveFile in $ArchiveOutputs) {
        Write-Host "  $($ArchiveFile.Name) :: $(Format-Bytes -Bytes $ArchiveFile.Length)"
    }
}

Write-Host "Log file: $LogFile"
