#Requires -Version 5.1

[CmdletBinding()]
param(
    [string]$Owner,
    [switch]$Regenerate,
    [switch]$Check
)

$ErrorActionPreference = "Stop"
$RepoRoot = (& git -C $PSScriptRoot rev-parse --show-toplevel 2>$null | Out-String).Trim()
if (-not $RepoRoot) {
    throw "Could not detect the ALIS repository root."
}

$RefreshScript = Join-Path $PSScriptRoot "refresh_developer_asset_authority.py"
$Python = "python"

if ($Check -and $Regenerate) {
    throw "-Check cannot be combined with -Regenerate."
}
if ($Regenerate -and $Owner) {
    throw "-Owner cannot be combined with -Regenerate; a regenerate all-types run must refresh and check every owner."
}

if ($Check) {
    $CheckArgs = @("--repo-root", $RepoRoot, "--check")
    if ($Owner) {
        $CheckArgs += @("--owner", $Owner)
    }
    & $Python $RefreshScript @CheckArgs
    exit $LASTEXITCODE
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
$InspectionDir = Join-Path $RepoRoot "Saved\Inspection"
$LogDir = Join-Path $RepoRoot "Saved\Logs"
$Log = Join-Path $LogDir "DeveloperAssetAuthority.log"
New-Item -ItemType Directory -Force -Path $InspectionDir, $LogDir | Out-Null

$RunId = [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssfffZ") + "-" + [Guid]::NewGuid().ToString("N").Substring(0, 8)
$Inventory = Join-Path $InspectionDir "generated_asset_inventory.$RunId.json"

if ($Regenerate) {
    $GenerationLog = Join-Path $LogDir "DefinitionGeneration.log"
    Write-Host "[INFO] Regenerating all generated definitions with cleanup..."
    & $Editor $Project -run=GenerateDefinitions -all -cleanup -unattended -nop4 -NoSound -NullRHI -stdout -FullStdOutLogOutput *> $GenerationLog
    $GenerationExit = $LASTEXITCODE
    if ($GenerationExit -ne 0) {
        Get-Content -LiteralPath $GenerationLog -Tail 80
        throw "Definition generation failed with exit code $GenerationExit. See $GenerationLog"
    }
}

Write-Host "[INFO] Exporting generated asset metadata through Unreal Asset Registry into fresh inventory: $Inventory"
$PriorInventory = $env:ALIS_GENERATED_ASSET_INVENTORY_OUTPUT
try {
    $env:ALIS_GENERATED_ASSET_INVENTORY_OUTPUT = $Inventory
    & $Editor $Project -run=pythonscript "-script=$Exporter" -unattended -nop4 -NoSound -NullRHI -stdout -FullStdOutLogOutput *> $Log
    $EditorExit = $LASTEXITCODE
}
finally {
    if ($null -eq $PriorInventory) {
        Remove-Item -Path "Env:\ALIS_GENERATED_ASSET_INVENTORY_OUTPUT" -ErrorAction SilentlyContinue
    }
    else {
        $env:ALIS_GENERATED_ASSET_INVENTORY_OUTPUT = $PriorInventory
    }
}
if ($EditorExit -ne 0) {
    Get-Content -LiteralPath $Log -Tail 80
    throw "Unreal Asset Registry export failed with exit code $EditorExit. See $Log"
}
if (-not (Test-Path -LiteralPath $Inventory)) {
    throw "Unreal Asset Registry export did not create a fresh inventory: $Inventory. The previous inventory was not reused."
}

$RefreshArgs = @("--repo-root", $RepoRoot, "--inventory", $Inventory)
if ($Owner) {
    $RefreshArgs += @("--owner", $Owner)
}
& $Python $RefreshScript @RefreshArgs
if ($LASTEXITCODE -ne 0) {
    throw "Public generated asset authority refresh failed."
}

Write-Host "[OK] Review and commit the refreshed authority before composing a developer release."
