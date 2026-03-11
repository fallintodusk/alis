<#
.SYNOPSIS
    Extracts crash analysis from Windows minidump files to text format.

.DESCRIPTION
    Uses cdb.exe (Console Debugger) to deserialize .dmp files and output
    human-readable crash analysis including callstacks, local variables,
    and exception info.

.PARAMETER DumpFile
    Path to the .dmp file to analyze.

.PARAMETER OutputFile
    Optional output path. Defaults to <DumpFile>.analysis.txt

.PARAMETER Quick
    Skip Microsoft symbol server (faster, but less engine detail).
    Use for quick triage. First full run caches symbols for future use.

.EXAMPLE
    .\analyze_dump.ps1 "Saved\Crashes\UECC-Windows-XXX\UEMinidump.dmp"

.EXAMPLE
    .\analyze_dump.ps1 -Quick "Saved\Crashes\UECC-Windows-XXX\UEMinidump.dmp"

.EXAMPLE
    .\analyze_dump.ps1 -DumpFile "crash.dmp" -OutputFile "report.txt"

.NOTES
    Requires: Windows SDK Debugging Tools (cdb.exe)

    Install: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
    Select ONLY "Debugging Tools for Windows" (~100MB)

    Note: WinDbg Preview (Store) does NOT work - it's sandboxed.

    Modes:
    - Default (full): Downloads MS symbols (~5-15 min first run, cached after)
    - Quick (-Quick): Local symbols only (~30 sec), less engine detail
#>

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$DumpFile,

    [Parameter(Mandatory=$false)]
    [string]$OutputFile,

    [Parameter(Mandatory=$false)]
    [switch]$Quick
)

$ErrorActionPreference = "Stop"

# Find cdb.exe - check SDK paths first, then WinDbg Preview (Store)
$cdbPaths = @(
    "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe",
    "<debugger-path>",
    "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\cdb.exe"
)

$cdb = $null
foreach ($path in $cdbPaths) {
    if (Test-Path $path) {
        $cdb = $path
        break
    }
}

# Check WinDbg Preview (Microsoft Store) if SDK not found
if (-not $cdb) {
    $winDbgPackage = Get-AppxPackage -Name "Microsoft.WinDbg" -ErrorAction SilentlyContinue
    if ($winDbgPackage) {
        $storeCdbPath = Join-Path $winDbgPackage.InstallLocation "amd64\cdb.exe"
        if (Test-Path $storeCdbPath) {
            $cdb = $storeCdbPath
        }
    }
}

if (-not $cdb) {
    Write-Error @"
cdb.exe not found. Install Windows Debugging Tools:

  Option 1: Microsoft Store -> WinDbg Preview
  Option 2: winget install Microsoft.WinDbg
  Option 3: Download Windows SDK, select 'Debugging Tools for Windows'
            https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
"@
    exit 2
}

# Validate dump file
if (-not (Test-Path $DumpFile)) {
    Write-Error "Dump file not found: $DumpFile"
    exit 2
}

$DumpFile = Resolve-Path $DumpFile

# Set output path
if (-not $OutputFile) {
    $OutputFile = "$DumpFile.analysis.txt"
}

# Get paths from config
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent (Split-Path -Parent $scriptRoot)
$uePathConf = Join-Path $projectRoot "scripts\config\ue_path.conf"

$uePath = "<ue-path>"
if (Test-Path $uePathConf) {
    $content = Get-Content $uePathConf -Raw
    if ($content -match 'UE_PATH\s*=\s*(.+)') {
        $uePath = $Matches[1].Trim()
    }
}

# Build symbol paths based on mode
if ($Quick) {
    # Quick mode: local symbols only (UE + project), no MS symbol server
    $symbolPaths = @(
        "<symbols-dir>",                              # Use cached symbols if available
        "$uePath\Engine\Binaries\Win64",
        "$projectRoot\Binaries\Win64"
    ) -join ";"
    Write-Host "Mode: QUICK (local symbols only)"
} else {
    # Full mode: includes MS symbol server (slow first run, cached after)
    $symbolPaths = @(
        "srv*<symbols-dir>*https://msdl.microsoft.com/download/symbols",
        "$uePath\Engine\Binaries\Win64",
        "$projectRoot\Binaries\Win64"
    ) -join ";"
    Write-Host "Mode: FULL (with MS symbol server - may be slow first run)"
}

Write-Host "Analyzing dump: $DumpFile"
Write-Host "Using symbols: $symbolPaths"
Write-Host ""

# CDB commands to execute - comprehensive crash analysis
# Setup commands differ by mode
if ($Quick) {
    $setupCommands = @(
        ".reload"                        # Lazy reload (no MS server, faster)
    )
} else {
    $setupCommands = @(
        ".symfix+ <symbols-dir>",           # Add MS symbol server
        ".reload /f"                     # Force full reload
    )
}

$commands = $setupCommands + @(

    # Section: CRASH SUMMARY
    ".echo === CRASH SUMMARY ===",
    "!analyze -v",

    # Section: EXCEPTION DETAILS
    ".echo === EXCEPTION DETAILS ===",
    ".exr -1",                       # Exception record
    ".ecxr",                         # Set context to exception

    # Section: REGISTERS AT CRASH
    ".echo === REGISTERS AT CRASH ===",
    "r",                             # All registers
    "rM 0xff",                       # Extended registers (SSE/AVX if available)

    # Section: CRASH CALLSTACK (detailed)
    ".echo === CRASH CALLSTACK (with params) ===",
    "kbn 100",                       # 100 frames with frame numbers
    "kvn 50",                        # 50 frames with FPO data and params

    # Section: LOCAL VARIABLES AT CRASH
    ".echo === LOCAL VARIABLES (crash frame) ===",
    "dv /V /t",                      # Verbose with types

    # Section: THIS POINTER (if available)
    ".echo === THIS POINTER INSPECTION ===",
    ".catch",                        # Catch errors for optional commands
    "dt this",                       # Dump 'this' object type
    "!address @rcx",                 # Memory info for typical 'this' register
    ".catch",

    # Section: MEMORY AROUND CRASH ADDRESS
    ".echo === MEMORY AT FAULTING ADDRESS ===",
    ".catch",
    "!address @rax",                 # Memory region info
    "dps @rsp L20",                  # Stack dump (pointers)
    ".catch",

    # Section: STACK FRAMES DETAIL (first 5 frames)
    ".echo === STACK FRAME 0 (crash point) ===",
    ".frame 0",
    "dv /V /t",
    ".echo === STACK FRAME 1 ===",
    ".frame 1",
    "dv /V /t",
    ".echo === STACK FRAME 2 ===",
    ".frame 2",
    "dv /V /t",
    ".echo === STACK FRAME 3 ===",
    ".frame 3",
    "dv /V /t",
    ".echo === STACK FRAME 4 ===",
    ".frame 4",
    "dv /V /t",

    # Section: ALL THREADS
    ".echo === ALL THREADS SUMMARY ===",
    "~*e .echo --- Thread; ~.; kb 20",  # Each thread's stack

    # Section: LOADED MODULES
    ".echo === LOADED MODULES (project only) ===",
    "lm m Unreal*Project*",          # Project modules
    "lm m Unreal*Alis*",             # Alis modules

    # Section: CRITICAL OBJECTS
    ".echo === CRITICAL MEMORY REGIONS ===",
    "!address -summary",             # Memory summary

    # Section: RECENT HEAP ACTIVITY
    ".echo === HEAP SUMMARY ===",
    "!heap -s",                      # Heap summary (quick)

    # Quit
    "q"
) -join ";"

# Run cdb
Write-Host "Running cdb.exe..."
& $cdb -z $DumpFile -y $symbolPaths -logo $OutputFile -lines -c $commands 2>&1 | Out-Null

if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $null) {
    Write-Warning "cdb.exe returned exit code: $LASTEXITCODE"
}

if (Test-Path $OutputFile) {
    $size = (Get-Item $OutputFile).Length
    Write-Host ""
    Write-Host "Analysis complete: $OutputFile ($size bytes)"
    Write-Host ""
    Write-Host "Quick view (first 50 lines):"
    Write-Host "----------------------------"
    Get-Content $OutputFile -Head 50
} else {
    Write-Error "Failed to create output file"
    exit 1
}
