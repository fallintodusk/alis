# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [ValidateSet('Validate', 'Regenerate')]
    [string]$Mode = 'Validate',

    [string]$TestRoot = '',

    [string]$EvidencePath = '',

    [switch]$CleanupOrphans,

    [ValidateSet('', 'post-save')]
    [string]$InjectFailure = '',

    [ValidateRange(30, 3600)]
    [int]$TimeoutSeconds = 600
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$runner = Join-Path $projectRoot 'scripts\ue\material\run_material_generation.ps1'
& $runner `
    -Domain Texture `
    -Mode $Mode `
    -TestRoot $TestRoot `
    -EvidencePath $EvidencePath `
    -CleanupOrphans:$CleanupOrphans `
    -InjectFailure $InjectFailure `
    -TimeoutSeconds $TimeoutSeconds
