# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Explicit transaction recovery for interrupted ProjectWorld generated
# content transactions. The ONLY supported route out of an interrupted
# state; normal Apply refuses while a journal exists.
# Contract SOT: Plugins/World/ProjectWorld/docs/territory_generation.md
# ("Three operation routes").

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$WorldDataPlugin,

    # Manifest authority root; defaults to the durable root.
    [string]$ManifestRoot = ""
)

$ErrorActionPreference = "Stop"
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $scriptDirectory))
. (Join-Path $scriptDirectory "generated_content_transaction.ps1")
. (Join-Path $scriptDirectory "generated_manifest.ps1")
$worldDataRoots = Resolve-ProjectWorldDataRoots -ProjectRoot $projectRoot -PluginName $WorldDataPlugin

$resolvedManifestRoot = if ([string]::IsNullOrWhiteSpace($ManifestRoot)) {
    $worldDataRoots.ManifestRoot
}
else {
    [System.IO.Path]::GetFullPath($ManifestRoot)
}
$contentRoot = $worldDataRoots.ContentRoot

$contentLock = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
$authorityLock = Enter-ProjectWorldAuthorityLock -ManifestRoot $resolvedManifestRoot
try {
    $result = Invoke-ProjectWorldTransactionRecovery `
        -ManifestRoot $resolvedManifestRoot `
        -ContentRoot $contentRoot `
        -ProjectRoot $projectRoot
}
finally {
    $authorityLock.Dispose()
    $contentLock.Dispose()
}
Write-Host "[WorldManifestRecovery] state=$($result.State) root=$resolvedManifestRoot"
if ($result.State -eq 'no_transaction') {
    exit 0
}
exit 0
