#Requires -Version 5.1

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$RepoRoot = (& git -C $PSScriptRoot rev-parse --show-toplevel 2>$null | Out-String).Trim()
if (-not $RepoRoot) {
    throw "Could not detect the ALIS repository root."
}

$ConfigDir = Join-Path $RepoRoot "scripts\config"
. (Join-Path $ConfigDir "Resolve-UEConfig.ps1")
$Config = Resolve-UEConfig -ConfigDir $ConfigDir
$Editor = Join-Path $Config.UE_PATH "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path -LiteralPath $Editor)) {
    throw "UnrealEditor-Cmd.exe not found under configured UE_PATH."
}

$Project = Join-Path $RepoRoot "Alis.uproject"
$Exporter = Join-Path $RepoRoot "scripts\ue\check\assets\export_generated_asset_inventory.py"
$Inventory = Join-Path $RepoRoot "Saved\Inspection\generated_asset_inventory.json"
$LogDir = Join-Path $RepoRoot "Saved\Logs"
$Log = Join-Path $LogDir "DeveloperAssetAuthority.log"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
Write-Host "[INFO] Exporting generated asset metadata through Unreal Asset Registry..."
& $Editor $Project -run=pythonscript "-script=$Exporter" -unattended -nop4 -NoSound -NullRHI *> $Log
$EditorExit = $LASTEXITCODE
if ($EditorExit -ne 0) {
    Get-Content -LiteralPath $Log -Tail 80
    throw "Unreal Asset Registry export failed with exit code $EditorExit. See $Log"
}

& python (Join-Path $PSScriptRoot "refresh_developer_asset_authority.py") `
    --repo-root $RepoRoot --inventory $Inventory --owner ProjectObject
if ($LASTEXITCODE -ne 0) {
    throw "Public generated asset authority refresh failed."
}

Write-Host "[OK] Review and commit the refreshed authority before composing a developer release."
