# scripts/ide/launch_vscode.ps1
# Launch VS Code with UE_PATH from scripts/config/ue_path.conf

param(
    [string]$Workspace = "Alis.code-workspace"
)

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

# TEMP: disable auto-sync of Claude settings from VS Code launcher
# $SyncScript = Join-Path $ProjectRoot "scripts\ide\sync_claude_settings.ps1"
# if (Test-Path $SyncScript) {
#     & $SyncScript
# }

$WorkspacePath = Join-Path $ProjectRoot $Workspace
if (-not (Test-Path $WorkspacePath)) {
    Write-Error "Workspace not found: $WorkspacePath"
    exit 1
}

$CodeCmd = Get-Command code -ErrorAction SilentlyContinue
if (-not $CodeCmd) {
    $CodeCmd = Get-Command code.cmd -ErrorAction SilentlyContinue
}

if (-not $CodeCmd) {
    $Fallbacks = @(
        "$env:LOCALAPPDATA\Programs\Microsoft VS Code\Code.exe",
        "$env:ProgramFiles\Microsoft VS Code\Code.exe",
        "$env:ProgramFiles(x86)\Microsoft VS Code\Code.exe"
    )

    foreach ($path in $Fallbacks) {
        if (Test-Path $path) {
            $CodeCmd = $path
            break
        }
    }
}

if (-not $CodeCmd) {
    Write-Error "VS Code not found. Install it or add 'code' to PATH."
    exit 1
}

Start-Process -FilePath $CodeCmd -ArgumentList "`"$WorkspacePath`"" -WorkingDirectory $ProjectRoot
