# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
param([Parameter(Mandatory=$true)][string]$CompilerProfile,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedCanonicalSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedWorldSha256,
    [Parameter(Mandatory=$true)][string]$EvidenceRoot,
    [Parameter(Mandatory=$true)][string]$RetainedRoot)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'canonical_schema_reference_operation.ps1')
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Invoke-ProjectWorldSchemaReferenceRepair -ProjectRoot $root -CompilerProfile $CompilerProfile `
    -ExpectedCanonicalSha256 $ExpectedCanonicalSha256 -ExpectedWorldSha256 $ExpectedWorldSha256 `
    -EvidenceRoot $EvidenceRoot -RetainedRoot $RetainedRoot | ConvertTo-Json -Depth 8
