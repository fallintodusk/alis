# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Map,
    [Parameter(Mandatory = $true)][string]$RealizationProfile,
    [Parameter(Mandatory = $true)][string]$EvidencePath,
    [Parameter(Mandatory = $true)][string]$WorldDataPlugin
)

$ErrorActionPreference = 'Stop'
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $scriptDirectory '..\..\..\..\..')).Path
. (Join-Path $projectRoot 'scripts\config\Resolve-UEConfig.ps1')
. (Join-Path $projectRoot 'scripts\ue\world\execution_envelope.ps1')
$config = Resolve-UEConfig -ConfigDir (Join-Path $projectRoot 'scripts\config')
$editor = Join-Path $config.UE_PATH 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$result = [System.IO.Path]::GetFullPath($EvidencePath)
$allowed = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'Saved\Validation\WorldRealization')) + [System.IO.Path]::DirectorySeparatorChar
if (-not $result.StartsWith($allowed, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Saved World projection evidence must stay under Saved/Validation/WorldRealization.'
}
New-Item -ItemType Directory -Path (Split-Path -Parent $result) -Force | Out-Null
$log = [System.IO.Path]::ChangeExtension($result, '.log')
$arguments = @(
    (Join-Path $projectRoot 'Alis.uproject'),
    '-run=ProjectWorldSavedProjection',
    "-Map=$Map",
    "-RealizationProfile=$([System.IO.Path]::GetFullPath($RealizationProfile))",
    "-Result=$result",
    "-EnablePlugins=$WorldDataPlugin",
    '-NoAssetRegistryCache',
    "-abslog=$log",
    '-unattended', '-nop4', '-nosplash', '-FullStdOutLogOutput'
)
$arguments += Get-ProjectWorldExecutionEnvelopeArguments -Rendering Required
Assert-ProjectWorldExecutionEnvelope -Rendering Required -Arguments $arguments
& $editor @arguments *> ([System.IO.Path]::ChangeExtension($result, '.console.log'))
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $result -PathType Leaf)) {
    throw "Saved World projection commandlet rejected; see $log"
}
$receipt = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
if ([string]$receipt.status -cne 'accepted' -or
    [string]$receipt.schema -cne 'project-world-saved-projection:v1' -or
    [string]$receipt.map_package -cne $Map) {
    throw "Saved World projection receipt is incomplete: $result"
}
foreach ($name in @('map', 'buildings')) {
    $part = $receipt.$name
    if ($null -eq $part -or [int]$part.record_count -lt 1 -or
        [string]$part.sha256 -notmatch '^[a-f0-9]{64}$') {
        throw "Saved World $name projection is incomplete: $result"
    }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes([string]$part.projection_json)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { $digest = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
    if ($digest -cne [string]$part.sha256 -or
        ($part.projection_json | ConvertFrom-Json).Count -ne [int]$part.record_count) {
        throw "Saved World $name projection digest or count disagrees: $result"
    }
}
Write-Host "[SavedWorldProjection] accepted map=$Map receipt=$result"
