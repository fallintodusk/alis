#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$RepoRoot = (& git -C $PSScriptRoot rev-parse --show-toplevel 2>$null | Out-String).Trim()
if (-not $RepoRoot) {
    throw 'Could not detect the ALIS repository root.'
}
$ConfigDir = Join-Path $RepoRoot 'scripts\config'
. (Join-Path $ConfigDir 'Resolve-UEConfig.ps1')
$Config = Resolve-UEConfig -ConfigDir $ConfigDir
$Editor = Join-Path $Config.UE_PATH 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $Editor -PathType Leaf)) {
    throw "Launcher UnrealEditor-Cmd.exe is missing: $Editor"
}

$OperationId = [guid]::NewGuid().ToString('N')
$WorkRoot = Join-Path $RepoRoot "tmp\release\developer\dependency-audit\$OperationId"
$Seeds = Join-Path $WorkRoot 'seeds.json'
$Inventory = Join-Path $WorkRoot 'unreal-inventory.json'
$Log = Join-Path $WorkRoot 'unreal.log'
New-Item -ItemType Directory -Path $WorkRoot -Force | Out-Null

try {
    & python (Join-Path $PSScriptRoot 'audit_developer_payload.py') plan `
        --repo-root $RepoRoot --output $Seeds
    if ($LASTEXITCODE -ne 0) {
        throw 'Developer dependency seed planning failed.'
    }
    $env:ALIS_PUBLIC_DEPENDENCY_SEEDS = $Seeds
    $env:ALIS_PUBLIC_DEPENDENCY_OUTPUT = $Inventory
    $Exporter = Join-Path $RepoRoot 'scripts\ue\check\assets\export_public_dependency_inventory.py'
    & $Editor (Join-Path $RepoRoot 'Alis.uproject') -run=pythonscript `
        "-script=$Exporter" -unattended -nop4 -NoSound -NullRHI "-abslog=$Log" *> $null
    if ($LASTEXITCODE -ne 0) {
        Get-Content -LiteralPath $Log -Tail 80
        throw "Unreal dependency inventory failed with exit code $LASTEXITCODE."
    }
    & python (Join-Path $PSScriptRoot 'audit_developer_payload.py') validate `
        --repo-root $RepoRoot --engine-root $Config.UE_PATH `
        --seeds $Seeds --inventory $Inventory --output $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw 'Developer dependency closure validation failed.'
    }
}
finally {
    Remove-Item Env:ALIS_PUBLIC_DEPENDENCY_SEEDS -ErrorAction SilentlyContinue
    Remove-Item Env:ALIS_PUBLIC_DEPENDENCY_OUTPUT -ErrorAction SilentlyContinue
}

Write-Host "[OK] Developer dependency closure: $OutputPath"
