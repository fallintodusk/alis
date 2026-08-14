# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$WorldDataPlugin,
    [string]$ManifestRoot = ''
)

$ErrorActionPreference = 'Stop'
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $scriptDirectory))
. (Join-Path $scriptDirectory 'world_data_roots.ps1')
. (Join-Path $scriptDirectory 'operator_controls.ps1')

$resolvedRoot = if ([string]::IsNullOrWhiteSpace($ManifestRoot)) {
    (Resolve-ProjectWorldDataRoots -ProjectRoot $projectRoot -PluginName $WorldDataPlugin).ManifestRoot
}
else {
    [System.IO.Path]::GetFullPath($ManifestRoot)
}

Get-ProjectWorldGenerationHistory -ManifestRoot $resolvedRoot |
    Format-Table scope_id, generation, state, accepted_at_utc, originating_run, manifest -AutoSize
