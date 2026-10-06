# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Map,
    [Parameter(Mandatory = $true)][string]$CompileResult,
    [Parameter(Mandatory = $true)][string]$EvidencePath,
    [Parameter(Mandatory = $true)][string]$WorldDataPlugin,
    [Parameter(Mandatory = $true)][int]$ExpectedBaseCount,
    [Parameter(Mandatory = $true)][int]$ExpectedSectionCount
)

$ErrorActionPreference = 'Stop'
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $scriptDirectory '..\..\..\..\..')).Path
. (Join-Path $projectRoot 'scripts\config\Resolve-UEConfig.ps1')
. (Join-Path $projectRoot 'scripts\ue\world\execution_envelope.ps1')
$config = Resolve-UEConfig -ConfigDir (Join-Path $projectRoot 'scripts\config')
$editor = Join-Path $config.UE_PATH 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$projectFile = Join-Path $projectRoot 'Alis.uproject'
$result = [System.IO.Path]::GetFullPath($EvidencePath)
$allowed = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'Saved\Validation\WorldRealization')) + [System.IO.Path]::DirectorySeparatorChar
if (-not $result.StartsWith($allowed, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Terrain projection evidence must stay under Saved/Validation/WorldRealization.'
}
if ($ExpectedBaseCount -lt 1 -or $ExpectedSectionCount -lt 1) {
    throw 'Expected base and section counts must be positive.'
}
New-Item -ItemType Directory -Path (Split-Path -Parent $result) -Force | Out-Null
$log = [System.IO.Path]::ChangeExtension($result, '.log')
$arguments = @(
    $projectFile,
    '-run=ProjectWorldMeshTerrainAudit',
    "-Map=$Map",
    "-CompileResult=$([System.IO.Path]::GetFullPath($CompileResult))",
    '-ExpectedSectionsPerVariant=0',
    "-Result=$result",
    '-TerrainProjection',
    "-EnablePlugins=$WorldDataPlugin",
    '-NoAssetRegistryCache',
    "-abslog=$log",
    '-unattended', '-nop4', '-nosplash', '-FullStdOutLogOutput'
)
$arguments += Get-ProjectWorldExecutionEnvelopeArguments -Rendering Required
Assert-ProjectWorldExecutionEnvelope -Rendering Required -Arguments $arguments
& $editor @arguments *> ([System.IO.Path]::ChangeExtension($result, '.console.log'))
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $result -PathType Leaf)) {
    throw "Terrain projection commandlet rejected; see $log"
}
$receipt = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
if ([string]$receipt.status -cne 'accepted' -or
    [string]$receipt.schema -cne 'project-world-mesh-terrain-audit:v2' -or
    [string]$receipt.map -cne $Map -or
    [string]$receipt.terrain_projection_sha256 -notmatch '^[a-f0-9]{64}$') {
    throw "Terrain projection receipt is incomplete: $result"
}
$records = $receipt.terrain_projection_json | ConvertFrom-Json
$baseCount = @($records | Where-Object kind -EQ 'base').Count
$sectionCount = @($records | Where-Object kind -EQ 'section').Count
$baseCells = @($records | Where-Object kind -EQ 'base' | ForEach-Object key | Sort-Object)
$tagCells = @($receipt.terrain_base_identity_tags | ForEach-Object cell | Sort-Object)
if ($baseCount -ne $ExpectedBaseCount -or $sectionCount -ne $ExpectedSectionCount -or
    $records.Count -ne [int]$receipt.terrain_projection_record_count -or
    @($receipt.sections).Count -ne $ExpectedSectionCount -or
    @($receipt.terrain_base_identity_tags).Count -ne $ExpectedBaseCount -or
    ($baseCells -join '|') -cne ($tagCells -join '|')) {
    throw "Terrain projection record counts disagree with the expected output: $result"
}
foreach ($entry in $receipt.terrain_base_identity_tags) {
    if (@($entry.tags | Where-Object { $_ -ceq "ProjectWorld.MeshTerrain.Cell=$($entry.cell)" }).Count -ne 1) {
        throw "Terrain base identity tags disagree with their cell: $result"
    }
}
$bytes = [System.Text.Encoding]::UTF8.GetBytes([string]$receipt.terrain_projection_json)
$sha = [System.Security.Cryptography.SHA256]::Create()
try { $digest = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
finally { $sha.Dispose() }
if ($digest -cne [string]$receipt.terrain_projection_sha256) {
    throw "Terrain projection digest disagrees with its records: $result"
}
Write-Host "[TerrainProjection] accepted map=$Map base=$baseCount sections=$sectionCount sha256=$digest receipt=$result"
