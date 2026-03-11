# scripts/ide/sync_claude_settings.ps1
# Sync .claude/settings.local.json engine paths to UE_PATH from scripts/config/ue_path.conf
#
# STATUS: DISABLED - regex approach is too fragile
#
# PROBLEMS:
# 1. Greedy regex patterns truncate paths after "UnrealEngine":
#    - Read(//G/UnrealEngine-5.7/Engine/Source/**) -> Read(//G/UnrealEngine-5.7)
#    - Bash("<ue-path>/Engine/Binaries/...) -> Bash("<ue-path>"
#    The [^")]*  character class eats the path suffix we need to preserve.
#
# 2. Permission entries have varied formats (forward/back slashes, with/without quotes,
#    UNC paths //G/..., Windows paths G:\..., etc.) - single regex can't handle all.
#
# 3. PowerShell -replace with script blocks doesn't work as expected - it stringifies
#    the block literal into the output instead of executing it.
#
# SOLUTION NEEDED:
# - Parse JSON properly, iterate permission entries, replace only the engine root portion
# - Or: maintain a template and regenerate settings.local.json
# - Or: just manually edit when engine path changes (rare)

Write-Host "sync_claude_settings.ps1: DISABLED - see comments in script for details"
exit 0

# =============================================================================
# CONFIGURATION - Edit these patterns when engine location/naming changes
# =============================================================================

# Known engine root patterns to search for (regex, will match any version number)
$EnginePatterns = @(
    'G:\\UnrealEngine-[0-9.]+',           # Source build on G: drive
    'G:/UnrealEngine-[0-9.]+',            # Same, forward slashes
    'C:\\UnrealEngine\\UE_[0-9.]+',       # Prebuilt on C: drive
    'C:/UnrealEngine/UE_[0-9.]+',         # Same, forward slashes
    'C:\\Program Files\\Epic Games\\UE_[0-9.]+',  # Epic launcher install
    'C:/Program Files/Epic Games/UE_[0-9.]+'      # Same, forward slashes
)

# UNC path pattern for Read(//G/...) style entries
$UncPattern = '//[A-Z]/UnrealEngine[^"/)]*'

# =============================================================================

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ScriptsRoot = Resolve-Path (Join-Path $ScriptDir "..")
$ProjectRoot = Split-Path -Parent $ScriptsRoot

$EnvScript = Join-Path $ProjectRoot "scripts\config\env.ps1"
if (-not (Test-Path $EnvScript)) {
    Write-Error "Missing env.ps1: $EnvScript"
    exit 1
}

. $EnvScript

$SettingsPath = Join-Path $ProjectRoot ".claude\settings.local.json"
if (-not (Test-Path $SettingsPath)) {
    Write-Error "Missing settings.local.json: $SettingsPath"
    exit 1
}

$UEPath = $Env:UE_PATH.Trim('"')
if (-not $UEPath) {
    Write-Error "UE_PATH not set. Configure scripts/config/ue_path.conf."
    exit 1
}

# Build //Drive/path form for Read(//G/...) entries
if ($UEPath.Length -lt 3 -or $UEPath[1] -ne ':') {
    Write-Error "UE_PATH must be a Windows path like G:\\UnrealEngine"
    exit 1
}

$Drive = $UEPath.Substring(0, 1)
$Rest = $UEPath.Substring(2) -replace '\\', '/'
$UEPathUnc = "//${Drive}${Rest}"

$content = Get-Content -Raw $SettingsPath

# Update env vars
$content = $content -replace '"UE_INSTALL_LOCATION"\s*:\s*"[^"]*"', ('"UE_INSTALL_LOCATION": "' + $UEPath + '"')

# Replace known engine paths using patterns from config
$UEPathFwd = $UEPath -replace '\\', '/'
foreach ($pattern in $EnginePatterns) {
    if ($pattern -match '\\\\') {
        # Backslash pattern -> replace with backslash path
        $content = $content -replace $pattern, $UEPath
    } else {
        # Forward slash pattern -> replace with forward slash path
        $content = $content -replace $pattern, $UEPathFwd
    }
}

# Replace Read(//Drive/...UnrealEngine...) entries with new UNC path
$content = $content -replace $UncPattern, $UEPathUnc

# Replace additionalDirectories engine path entries
$content = $content -replace '"[A-Z]:\\[^\"]*UnrealEngine[^"]*"', ('"' + $UEPath + '"')
$content = $content -replace '"[A-Z]:/[^\"]*UnrealEngine[^"]*"', ('"' + ($UEPath -replace '\\','/') + '"')

Set-Content -Path $SettingsPath -Value $content
