# scripts/setup/setup_ue_env.ps1
# Synchronizes ALL machine-local derived state from the conf SOT:
#   - persistent env UE_PATH (derived cache; resolvers hard-fail on drift)
#   - .vscode configs (${env:UE_PATH} + compileCommands literal paths)
#   - .claude/settings.local.json (env block + engine-path grants)
#   - .mcp.json (engine path plus absolute project root for commandlets)
# Run once after cloning and after every engine change (the engine-update
# orchestrator invokes this automatically).
#
# Reads the conf via the PURE reader (Resolve-UEConfig never consults
# env), so a STALE env cache can always be repaired here - resolvers
# hard-fail on the mismatch, this script fixes it.

param(
    [switch]$Force,
    # Test/orchestrator hooks; defaults are the real machine-local files.
    [ValidateSet("User", "Process")][string]$EnvScope = "User",
    [string]$ConfigDir,
    [string]$SettingsPath,
    [string]$McpPath,
    [string]$PreviousLauncherRoot,
    [string]$PreviousSourceRoot,
    [string]$ProjectRoot,
    [switch]$SkipVsCode
)

$ErrorActionPreference = "Stop"

if (-not $ConfigDir) { $ConfigDir = Join-Path $PSScriptRoot "..\config" }
. (Join-Path $ConfigDir "Resolve-UEConfig.ps1")
Import-Module (Join-Path $PSScriptRoot "UEEnvSync.psm1") -Force

$config = Resolve-UEConfig -ConfigDir $ConfigDir
$UePath = $config.UE_PATH

if (-not $UePath) {
    Write-Error "UE_PATH not found. Set it in scripts/config/ue_path.conf (or ue_path.local.conf)."
    exit 1
}

Write-Host "Config file(s): $($config.ConfigFiles -join ', ')"
Write-Host "UE_PATH from config: $UePath"
if ($config.UE_SOURCE_PATH) {
    Write-Host "UE_SOURCE_PATH from config: $($config.UE_SOURCE_PATH)"
}

if (-not $PreviousLauncherRoot) {
    $cachedLauncher = if ($EnvScope -eq "User") {
        [Environment]::GetEnvironmentVariable("UE_PATH", "User")
    } else {
        $Env:UE_PATH
    }
    if ($cachedLauncher -and $cachedLauncher -ne $UePath) {
        $PreviousLauncherRoot = $cachedLauncher
    }
}
if (-not $PreviousSourceRoot -and $Env:UE_SOURCE_PATH -and
    $Env:UE_SOURCE_PATH -ne $config.UE_SOURCE_PATH) {
    $PreviousSourceRoot = $Env:UE_SOURCE_PATH
}

$MachineValue = [Environment]::GetEnvironmentVariable("UE_PATH", "Machine")
if ($MachineValue -and $EnvScope -eq "User") {
    $winRoot = ($UePath -replace '/', '\')
    if ($MachineValue -ne $winRoot) {
        Write-Host "UE_PATH is set at Machine level: $MachineValue" -ForegroundColor Yellow
        Write-Host "To change, edit System Environment Variables or run as Admin."
        exit 1
    }
}

if (-not $ProjectRoot) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}
if (-not $SettingsPath) { $SettingsPath = Join-Path $ProjectRoot ".claude\settings.local.json" }
if (-not $McpPath) { $McpPath = Join-Path $ProjectRoot ".mcp.json" }

# --- 1. Prepare every machine-local file mutation ---
$jsonPlan = Get-UEMachineLocalJsonPlan -NewLauncherRoot $UePath `
    -NewSourceRoot $config.UE_SOURCE_PATH `
    -PreviousLauncherRoot $PreviousLauncherRoot `
    -PreviousSourceRoot $PreviousSourceRoot `
    -SettingsPath $SettingsPath -McpPath $McpPath
$blockers = @($jsonPlan.Blockers)
$mutations = @($jsonPlan.Mutations)

if (-not $SkipVsCode) {
    $VsCodeDir = Join-Path $ProjectRoot ".vscode"
    if (Test-Path $VsCodeDir) {
        $vsCodeFiles = @(
            @{ Name = "launch.json"; Root = '${env:UE_PATH}' },
            @{ Name = "tasks.json"; Root = '${env:UE_PATH}' },
            @{ Name = "settings.json"; Root = '${env:UE_PATH}' }
        )
        $vsCodeFiles += @(Get-ChildItem $VsCodeDir -Filter "compileCommands*.json" |
            ForEach-Object { @{ Name = $_.Name; Root = $UePath } })
        foreach ($item in $vsCodeFiles) {
            $filePath = Join-Path $VsCodeDir $item.Name
            if (-not (Test-Path -LiteralPath $filePath)) { continue }
            $content = Get-Content -LiteralPath $filePath -Raw
            try { $null = $content | ConvertFrom-Json }
            catch {
                $blockers += "$filePath`: invalid JSON - $($_.Exception.Message)"
                continue
            }
            $r = Update-EngineRootsInString -Text $content `
                -NewLauncherRoot $item.Root -NewSourceRoot $item.Root `
                -PreviousLauncherRoot $PreviousLauncherRoot `
                -PreviousSourceRoot $PreviousSourceRoot
            if ($r.Text -ne $content) {
                $mutations += @{ Path = $filePath; Old = $content; New = $r.Text }
            }
        }
    }
}

if ($blockers.Count -gt 0) {
    Write-Host "BLOCKED - fix these before rerunning (nothing was written):" `
        -ForegroundColor Red
    $blockers | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 2
}

# --- 2. Apply files atomically, then update the environment last ---
try {
    $applied = @(Invoke-UEFileMutationPlan -Mutations $mutations)
    if (-not ($MachineValue -and $EnvScope -eq "User")) {
        $written = Sync-UEUserEnv -NewLauncherRoot $UePath -Scope $EnvScope
    }
} catch {
    if ($applied.Count -gt 0) {
        Restore-UEFileMutationPlan -Mutations $applied
    }
    throw
}
foreach ($f in $applied.Path) {
    Write-Host "  [OK] updated $f" -ForegroundColor Green
}
if ($written) {
    Write-Host "  [OK] env UE_PATH ($EnvScope) = $written" -ForegroundColor Green
}

# Codex reads repo skills from .agents/skills; the canonical bodies live in
# .claude/skills. The junction is machine-local and gitignored, so a fresh
# clone has no Codex-facing skills until it is created. Creating it here means
# the normal setup command is sufficient - no second hidden step to remember.
$linkScript = Join-Path $PSScriptRoot "..\agents\link_codex_skills.ps1"
$skillsExposed = $false
if (Test-Path $linkScript) {
    try {
        & $linkScript | ForEach-Object { Write-Host "  $_" -ForegroundColor Green }
        $skillsExposed = ($LASTEXITCODE -eq 0 -or $null -eq $LASTEXITCODE)
    } catch {
        Write-Host "  [!] Codex skill exposure failed: $($_.Exception.Message)" `
            -ForegroundColor Yellow
    }
}
else {
    Write-Host "  [!] Missing $linkScript" -ForegroundColor Yellow
}

Write-Host ""
if (-not $skillsExposed) {
    # The engine environment is repaired above and stays repaired - that work is
    # not rolled back. But reporting full success while Codex has zero ALIS
    # skills would be a green light over a missing interface, which is the
    # failure mode this whole setup path exists to prevent.
    Write-Host "PARTIAL: engine environment synchronized, Codex skill exposure FAILED." `
        -ForegroundColor Yellow
    Write-Host "Fix with: scripts/agents/link_codex_skills.ps1" -ForegroundColor Yellow
    exit 3
}

Write-Host "Machine-local state synchronized with the conf SOT." -ForegroundColor Green
Write-Host "Restart VS Code / MCP host / persistent editor to inherit UE_PATH." -ForegroundColor Yellow
