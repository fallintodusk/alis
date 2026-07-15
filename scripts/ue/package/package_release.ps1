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
    - optional signing via sign_release.ps1 after archive creation

    TODO(ALIS-Release): Wire the split 7-Zip outputs into the GitHub release publish
    workflow end-to-end so transport-size handling stays outside UE packaging settings.
    Do not reintroduce MaxChunkSize just to satisfy GitHub asset limits.
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
    [int]$SplitSizeMB = 1700,
    [switch]$SignRelease,
    [string]$GpgPath,
    [string]$GpgHome,
    [string]$SigningKeyFingerprint,
    [switch]$SkipSignVerify
)

$ErrorActionPreference = "Stop"


function Format-Bytes {
    param(
        [Parameter(Mandatory = $true)]
        [Int64]$Bytes
    )

    return "{0} bytes ({1:N3} GiB)" -f $Bytes, ($Bytes / 1GB)
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$ConfigDir = Join-Path $ProjectRoot "scripts\config"
$ProjectFile = Join-Path $ProjectRoot "Alis.uproject"

if (-not $EngineRoot) {
    . (Join-Path $ConfigDir "Resolve-UEConfig.ps1")
    $config = Resolve-UEConfig -ConfigDir $ConfigDir
    $EngineRoot = $config.UE_PATH
}

if (-not $EngineRoot) {
    throw "UE_PATH is not set. Create scripts/config/ue_path.local.conf or pass -EngineRoot."
}

# Materialize project-local UBT config. Saved/ is gitignored and gets cleaned
# periodically; this sync restores BuildConfiguration.xml from its committed
# SOT before UBT reads it. Required to avoid cold-build PCH OOM (C3859/C1076).
. (Join-Path $ConfigDir "Sync-UBTConfig.ps1")
Sync-UBTConfig -ProjectRoot $ProjectRoot

# Detect source vs installed engine for Target.cs diagnostics gating
$InstalledBuildMarker = Join-Path $EngineRoot "Engine\Build\InstalledBuild.txt"
if (Test-Path $InstalledBuildMarker) {
    Remove-Item Env:ENGINE_FROM_SOURCE -ErrorAction SilentlyContinue
} else {
    $env:ENGINE_FROM_SOURCE = "1"
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
Write-Host "SIGN_RELEASE = $SignRelease"
Write-Host ""

# --- Pre-package validation (fast, no editor) ---
Write-Host "Pre-package validation..." -ForegroundColor Cyan
$CheckDir = Join-Path $ProjectRoot "scripts\ue\check"

# 1. Shipping ini audit (pure Python, <1s)
$IniCheckScript = Join-Path $CheckDir "config\validate_shipping_ini.py"
if (Test-Path $IniCheckScript) {
    $pythonExe = $null
    $EnginePython = Join-Path $EngineRoot "Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
    if (Test-Path $EnginePython) {
        $pythonExe = $EnginePython
    } elseif (Get-Command python -ErrorAction SilentlyContinue) {
        $pythonExe = "python"
    }
    if ($pythonExe) {
        & $pythonExe $IniCheckScript --config-dir (Join-Path $ProjectRoot "Config")
        if ($LASTEXITCODE -ne 0) {
            throw "Pre-package validation failed: shipping ini check found unsafe settings. Fix before packaging."
        }
    } else {
        Write-Host "  [!] Python not found - skipping ini validation" -ForegroundColor Yellow
    }
}

# 2. Data cross-reference validation (<5s)
$DataCheckScript = Join-Path $CheckDir "data\validate_all.py"
if (Test-Path $DataCheckScript) {
    if ($pythonExe) {
        & $pythonExe $DataCheckScript
        if ($LASTEXITCODE -ne 0) {
            throw "Pre-package validation failed: data cross-reference errors found. Fix before packaging."
        }
    }
}

# 3. Plugin Data/ staging audit (<1s)
# Catches the silent class of bug where a plugin parses JSON via
# FProjectPaths::GetPluginDataDir at runtime but its Build.cs forgets to
# add RuntimeDependencies for Plugins/<X>/Data/ -- file ships only in
# Editor, Shipping falls back to defaults with a Warning log.
$StagingCheckScript = Join-Path $CheckDir "governance\validate_plugin_data_staging.py"
if (Test-Path $StagingCheckScript) {
    if ($pythonExe) {
        & $pythonExe $StagingCheckScript
        if ($LASTEXITCODE -ne 0) {
            throw "Pre-package validation failed: plugin data staging gap. Fix before packaging."
        }
    }
}

Write-Host "Pre-package validation passed." -ForegroundColor Green
Write-Host ""

& $RunUAT @Args 2>&1 | Tee-Object -FilePath $LogFile
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

# Post-package smoke check: confirm runtime-read JSON files survived the cook.
# Catches the case where staging is declared but the cook quietly dropped them
# (e.g. plugin disabled in target, glob mismatch, IoStore quirk).
if (Test-Path $StagingCheckScript) {
    if ($pythonExe) {
        Write-Host ""
        Write-Host "Post-package archive verification..." -ForegroundColor Cyan
        & $pythonExe $StagingCheckScript --archive-root $OutputDir
        if ($LASTEXITCODE -ne 0) {
            throw "Post-package verification failed: a plugin's runtime data is missing from the archive."
        }
    }
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
        $DefaultPath = Join-Path $env:ProgramFiles "7-Zip\7z.exe"
        if (Test-Path $DefaultPath) {
            $SevenZip = Get-Command $DefaultPath
        }
    }

    if (-not $SevenZip) {
        throw "7-Zip was not found in PATH or at '$env:ProgramFiles\7-Zip'. Install 7-Zip or omit -CreateReleaseArchive."
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

if ($SignRelease) {
    if ($ArchiveOutputs.Count -eq 0) {
        throw "Signing requires release archive files. Rerun with -CreateReleaseArchive, or sign an explicit release directory with sign_release.ps1."
    }

    $SignScript = Join-Path $ScriptDir "sign_release.ps1"
    if (-not (Test-Path $SignScript)) {
        throw "Signing script was not found: $SignScript"
    }

    $SignArgs = @{
        ReleaseDir = $OutputDir
    }
    if ($GpgPath) {
        $SignArgs.GpgPath = $GpgPath
    }
    if ($GpgHome) {
        $SignArgs.GpgHome = $GpgHome
    }
    if ($SigningKeyFingerprint) {
        $SignArgs.SigningKeyFingerprint = $SigningKeyFingerprint
    }
    if ($SkipSignVerify) {
        $SignArgs.SkipVerify = $true
    }

    Write-Host ""
    Write-Host "Signing release artifacts..." -ForegroundColor Cyan
    & $SignScript @SignArgs

    $ManifestPath = Join-Path $OutputDir "SHA256SUMS.txt"
    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        throw "Signing completed without writing SHA256SUMS.txt."
    }

    $RequiredSignedAssets = @(
        "ALIS_PUBLIC_KEY.asc",
        "INSTALL.txt",
        "VERIFY_RELEASE.bat",
        "VERIFY_RELEASE.ps1",
        "SHA256SUMS.txt",
        "SHA256SUMS.txt.asc"
    )
    $MissingSignedAssets = @($RequiredSignedAssets | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $OutputDir $_))
    })
    if ($MissingSignedAssets.Count -gt 0) {
        throw "Signing completed without required release assets: $($MissingSignedAssets -join ', ')"
    }

    $RequiredManifestAssets = @(
        "ALIS_PUBLIC_KEY.asc",
        "INSTALL.txt",
        "VERIFY_RELEASE.bat",
        "VERIFY_RELEASE.ps1"
    )
    foreach ($RequiredAsset in $RequiredManifestAssets) {
        $ManifestPattern = "^[0-9A-Fa-f]{64} \*$([regex]::Escape($RequiredAsset))$"
        if (-not (Select-String -Path $ManifestPath -Pattern $ManifestPattern)) {
            throw "SHA256SUMS.txt does not cover required release asset: $RequiredAsset"
        }
    }

    $ProtocolEnvelopeAssets = @(
        "SHA256SUMS.txt",
        "SHA256SUMS.txt.asc"
    )
    foreach ($ProtocolEnvelopeAsset in $ProtocolEnvelopeAssets) {
        $ManifestPattern = "^[0-9A-Fa-f]{64} \*$([regex]::Escape($ProtocolEnvelopeAsset))$"
        if (Select-String -Path $ManifestPath -Pattern $ManifestPattern) {
            throw "SHA256SUMS.txt must not hash itself or its detached signature: $ProtocolEnvelopeAsset"
        }
    }

    $BadManifestEntries = Select-String `
        -Path $ManifestPath `
        -Pattern '(^|[\\/])(Windows|debug)[\\/]|package_summary\.txt|sign_release_summary\.txt|verify_release_summary\.txt'

    if ($BadManifestEntries) {
        throw "SHA256SUMS.txt includes local/debug artifacts that are moved or not uploaded. Fix release signing exclusions before publishing."
    }

    $DebugDir = Join-Path $OutputDir "debug"
    New-Item -ItemType Directory -Force -Path $DebugDir | Out-Null

    @(
        "DO NOT UPLOAD THIS FOLDER",
        "",
        "This folder is local build/debug evidence only.",
        "It may contain absolute build-machine paths in summary files.",
        "Upload only the release-root files next to this folder."
    ) | Set-Content -Encoding Ascii (Join-Path $DebugDir "DO_NOT_UPLOAD.txt")

    foreach ($LocalItem in @("Windows", "package_summary.txt", "sign_release_summary.txt", "verify_release_summary.txt")) {
        $SourcePath = Join-Path $OutputDir $LocalItem
        if (-not (Test-Path -LiteralPath $SourcePath)) {
            continue
        }

        $TargetPath = Join-Path $DebugDir $LocalItem
        if (Test-Path -LiteralPath $TargetPath) {
            Remove-Item -LiteralPath $TargetPath -Recurse -Force
        }

        Move-Item -LiteralPath $SourcePath -Destination $DebugDir -Force
    }

    $SummaryPath = Join-Path $DebugDir "package_summary.txt"
}

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
