# Resolve-UEConfig.ps1 - Single source of truth for UE config resolution (PS)
#
# Dot-source this file to get the resolution functions.
# Grammar + authority model: see the header of ue_path.conf.
#
# Key-level merge: ue_path.local.conf overrides ue_path.conf PER KEY.
# This is the PURE conf reader: it never consults env UE_PATH.
# The stale-env HARD FAIL lives in the resolver wrappers (env.ps1, bat,
# sh, mk) - never here, so setup_ue_env.ps1 can always repair a stale
# cache (bootstrap-deadlock avoidance).
#
# Usage:
#   . (Join-Path $configDir "Resolve-UEConfig.ps1")
#   $config = Resolve-UEConfig -ConfigDir $configDir
#   $config.UE_PATH / $config.UE_SOURCE_PATH / $config.BUILD_TARGET ...
#   $config.ConfigFile   # last file that contributed (compat)
#   $config.ConfigFiles  # all files used, tracked first

$script:UEConfKnownKeys = @(
    "UE_PATH", "UE_SOURCE_PATH", "LINUX_MULTIARCH_ROOT",
    "BUILD_TARGET", "BUILD_CONFIG", "BUILD_PLATFORM"
)

function Read-UEConfFile {
    # Strict single-file parse. Throws on any grammar violation.
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    $values = @{}
    $lineno = 0
    foreach ($raw in (Get-Content -LiteralPath $Path)) {
        $lineno++
        $line = $raw.TrimEnd("`r")
        $stripped = $line.Trim()
        if (-not $stripped -or $stripped.StartsWith("#")) { continue }
        if ($line -notmatch '^([A-Z_][A-Z0-9_]*)=(.*)$') {
            throw ("{0}:{1}: malformed line (grammar: KEY=VALUE, no spaces " +
                   "around '='): '{2}'" -f $Path, $lineno, $line)
        }
        $key = $Matches[1]; $value = $Matches[2]
        if ($script:UEConfKnownKeys -notcontains $key) {
            throw ("{0}:{1}: unknown key '{2}'" -f $Path, $lineno, $key)
        }
        if ($values.ContainsKey($key)) {
            throw ("{0}:{1}: duplicate key '{2}'" -f $Path, $lineno, $key)
        }
        if (-not $value.Trim()) {
            throw ("{0}:{1}: empty value for '{2}'" -f $Path, $lineno, $key)
        }
        if ($value -match '[#$"' + "'" + ']') {
            throw ("{0}:{1}: forbidden character in value for '{2}' (no " +
                   "inline comments, '$', or quotes)" -f $Path, $lineno, $key)
        }
        $values[$key] = $value.Trim()
    }
    return $values
}

function Resolve-UEConfig {
    [CmdletBinding()]
    param([Parameter()][string]$ConfigDir)

    if (-not $ConfigDir) { $ConfigDir = $PSScriptRoot }

    $result = @{
        ConfigFile     = ""
        ConfigFiles    = @()
        UE_PATH        = ""
        UE_SOURCE_PATH = ""
        LINUX_MULTIARCH_ROOT = ""
        BUILD_TARGET   = "AlisEditor"
        BUILD_CONFIG   = "Development"
        BUILD_PLATFORM = "Win64"
    }

    $tracked = Join-Path $ConfigDir "ue_path.conf"
    $local   = Join-Path $ConfigDir "ue_path.local.conf"

    $merged = @{}
    foreach ($file in @($tracked, $local)) {
        if (Test-Path -LiteralPath $file) {
            $parsed = Read-UEConfFile -Path $file
            foreach ($k in $parsed.Keys) { $merged[$k] = $parsed[$k] }
            $result.ConfigFiles += $file
            $result.ConfigFile = $file
        }
    }

    foreach ($k in $merged.Keys) { $result[$k] = $merged[$k] }
    return $result
}

function Get-UENormalizedPath {
    # Canonical comparable form: forward slashes, MSYS /c/ -> C:/,
    # no trailing slash, lowercase.
    [CmdletBinding()]
    param([Parameter()][string]$Path)

    if (-not $Path) { return "" }
    $p = $Path.Trim() -replace '\\', '/'
    if ($p -match '^/([A-Za-z])(/.*)?$') {
        $p = $Matches[1].ToUpper() + ":" + $(if ($Matches[2]) { $Matches[2] } else { "" })
    }
    $p = $p.TrimEnd('/')
    return $p.ToLower()
}

function Test-UEEngineRoot {
    # A valid launcher/installed engine root for fallback purposes.
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)
    return (Test-Path -LiteralPath (
        Join-Path $Path "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"))
}

function Resolve-UELauncherFallback {
    # Numeric-highest valid UE_<major>.<minor> install. Used ONLY when
    # no conf declares UE_PATH. 5.10 must beat 5.9 (numeric, not lexical).
    [CmdletBinding()]
    param(
        # Overridable for conformance tests; production callers use default.
        [Parameter()][string[]]$Roots = @(
            "C:\UnrealEngine", "C:\Program Files\Epic Games")
    )

    $roots = $Roots
    $best = $null
    $bestVer = @(-1, -1)
    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        foreach ($dir in (Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue)) {
            if ($dir.Name -notmatch '^UE_(\d+)\.(\d+)$') { continue }
            if (-not (Test-UEEngineRoot -Path $dir.FullName)) { continue }
            $ver = @([int]$Matches[1], [int]$Matches[2])
            if (($ver[0] -gt $bestVer[0]) -or
                (($ver[0] -eq $bestVer[0]) -and ($ver[1] -gt $bestVer[1]))) {
                $bestVer = $ver
                $best = $dir.FullName
            }
        }
    }
    return $best
}

function Test-UEStaleEnv {
    # Returns an error string when env UE_PATH exists and mismatches the
    # resolved conf value; $null when consistent or env unset.
    [CmdletBinding()]
    param([Parameter()][string]$ResolvedUEPath)

    $envVal = $Env:UE_PATH
    if (-not $envVal -or -not $ResolvedUEPath) { return $null }
    if ((Get-UENormalizedPath $envVal) -ne (Get-UENormalizedPath $ResolvedUEPath)) {
        return ("stale UE_PATH env ({0}) does not match resolved conf value " +
                "({1}) - rerun scripts/setup/setup_ue_env.ps1" -f
                $envVal, $ResolvedUEPath)
    }
    return $null
}
