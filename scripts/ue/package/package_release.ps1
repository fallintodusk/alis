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

    Release signing is deliberately separate. The combined release transaction
    must validate and approve every player/developer/legal input before
    sign_release.ps1 will access a private key.
#>

param(
    [string]$OutputDir,
    [string]$ClientConfig = "Shipping",
    [string]$Platform = "Win64",
    [string]$EngineRoot,
    [switch]$SourceRelease,
    [string]$RequiredCookMap,
    [switch]$SkipBuild,
    [switch]$IncludeStagedDebugFiles,
    [switch]$EncryptContent,
    [switch]$CreateReleaseArchive,
    [int]$SplitSizeMB = 1900
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

$RunUAT = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path $RunUAT)) {
    throw "RunUAT.bat not found under engine root: $EngineRoot"
}

$IsInstalledEngine = Test-Path (Join-Path $EngineRoot "Engine\Build\InstalledBuild.txt")
if (-not $IsInstalledEngine -and -not $SourceRelease) {
    throw (
        "A non-installed source engine requires explicit -SourceRelease. " +
        "Use the default launcher engine for candidate/package iteration, or " +
        "run package_release_source.bat for the public release gate.")
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
    "-unattended",
    # Generated World Partition actors can replace external packages between
    # cooks. A release cook must scan the current tree, not a prior AR cache.
    '-AdditionalCookerOptions="-NoAssetRegistryCache"'
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

if ($RequiredCookMap) {
    if ($RequiredCookMap -notmatch '^/[A-Za-z0-9_/-]+$') {
        throw "RequiredCookMap is not a valid project package path: $RequiredCookMap"
    }
    $DefaultGame = Join-Path $ProjectRoot "Config\DefaultGame.ini"
    $CookMaps = @()
    foreach ($Line in Get-Content -LiteralPath $DefaultGame) {
        if ($Line -match '^\+MapsToCook=\(FilePath="(?<Map>/[A-Za-z0-9_/-]+)"\)\s*$') {
            if ($CookMaps -notcontains $Matches.Map) {
                $CookMaps += $Matches.Map
            }
        }
    }
    if ($CookMaps.Count -eq 0) {
        throw "RequiredCookMap cannot extend an empty configured release map set."
    }
    if ($CookMaps -notcontains $RequiredCookMap) {
        $CookMaps += $RequiredCookMap
    }
    # BuildCookRun cook-list parameter, verified against UE 5.8
    # AutomationTool ProjectParams.cs: -MapsToCook=<A>+<B> is parsed and
    # split into ProjectParams.MapsToCook; -map is the run-map parameter
    # and must not carry the cook list.
    $Args += "-MapsToCook=$($CookMaps -join '+')"
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " ALIS Release Packaging" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "UE_PATH      = $EngineRoot"
Write-Host "ENGINE_KIND  = $(if ($IsInstalledEngine) { 'installed' } else { 'source-release' })"
Write-Host "PROJECT_FILE = $ProjectFile"
Write-Host "PLATFORM     = $Platform"
Write-Host "CONFIG       = $ClientConfig"
Write-Host "OUTPUT_DIR   = $OutputDir"
Write-Host "LOG_FILE     = $LogFile"
Write-Host "SKIP_BUILD   = $SkipBuild"
Write-Host "NODEBUGINFO  = $(-not $IncludeStagedDebugFiles)"
Write-Host "ENCRYPTION   = $EncryptContent"
Write-Host "REQUIRED_MAP = $RequiredCookMap"
Write-Host "ZIP_RELEASE  = $CreateReleaseArchive"
Write-Host "SPLIT_SIZE   = $SplitSizeMB MiB"
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

# UE otherwise ties Common Zen to the cook process, which exits before stage.
# Scope the supported lifetime override to this UAT process tree only.
$PreviousZenLifetime = [Environment]::GetEnvironmentVariable("UE-ZenLimitProcessLifetime", "Process")
$PreviousNoProxy = [Environment]::GetEnvironmentVariable("NO_PROXY", "Process")
$HasBracketedLoopback = @($PreviousNoProxy -split ',' | ForEach-Object { $_.Trim() }) -contains '[::1]'
$UatNoProxy = if ([string]::IsNullOrWhiteSpace($PreviousNoProxy)) {
    '[::1]'
} elseif ($HasBracketedLoopback) {
    $PreviousNoProxy
} else {
    "$PreviousNoProxy,[::1]"
}
try {
    [Environment]::SetEnvironmentVariable("UE-ZenLimitProcessLifetime", "false", "Process")
    # .NET proxy matching requires brackets for Zen's IPv6 loopback URL.
    [Environment]::SetEnvironmentVariable("NO_PROXY", $UatNoProxy, "Process")
    & $RunUAT @Args 2>&1 | Tee-Object -FilePath $LogFile
    $ExitCode = $LASTEXITCODE
} finally {
    [Environment]::SetEnvironmentVariable("UE-ZenLimitProcessLifetime", $PreviousZenLifetime, "Process")
    [Environment]::SetEnvironmentVariable("NO_PROXY", $PreviousNoProxy, "Process")
}

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

# UAT can return success after SafeCopyFile exhausts its retries. Prove the
# archive contains every staged byte before any content-specific checks run.
$ArchiveIntegrityScript = Join-Path $ScriptDir "verify_staged_archive.py"
$StagedWindowsDir = Join-Path $ProjectRoot "Saved\StagedBuilds\Windows"
if (-not $pythonExe -or -not (Test-Path $ArchiveIntegrityScript)) {
    throw "Post-package archive integrity verification is unavailable."
}
& $pythonExe $ArchiveIntegrityScript --staged-root $StagedWindowsDir --archive-root $WindowsDir
if ($LASTEXITCODE -ne 0) {
    throw "Post-package archive integrity verification failed."
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

$AllFiles = @(Get-ChildItem $WindowsDir -Recurse -File)
$ReleaseFiles = @(if ($IncludeStagedDebugFiles) {
    $AllFiles
} else {
    $AllFiles | Where-Object { $_.Extension -ne ".pdb" }
})

$LargestFile = $ReleaseFiles | Sort-Object Length -Descending | Select-Object -First 1
$TotalBytes = ($ReleaseFiles | Measure-Object Length -Sum).Sum
$OverLimitFiles = @($ReleaseFiles | Where-Object { $_.Length -ge 2GB })

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

    $UnsafeArchiveOutputs = @($ArchiveOutputs | Where-Object { $_.Length -ge 2GB })
    if ($UnsafeArchiveOutputs.Count -gt 0) {
        $UnsafeNames = ($UnsafeArchiveOutputs | ForEach-Object { $_.Name }) -join ", "
        $ArchiveOutputs | Remove-Item -Force
        throw "Generated archive exceeded the GitHub release asset limit: $UnsafeNames"
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
    Write-Host "[i] Internal package files at or above 2 GiB (GitHub assets are checked after archive splitting):"
    foreach ($File in $OverLimitFiles) {
        Write-Host "  $($File.FullName) :: $(Format-Bytes -Bytes $File.Length)"
    }
}

if ($ArchiveOutputs.Count -gt 0) {
    Write-Host "Archive outputs:" -ForegroundColor Green
    foreach ($ArchiveFile in $ArchiveOutputs) {
        Write-Host "  $($ArchiveFile.Name) :: $(Format-Bytes -Bytes $ArchiveFile.Length)"
    }
}

Write-Host "Log file: $LogFile"
