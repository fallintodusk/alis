# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

# The common checks' Pester entry. Besides the World suites it runs the shared
# generated-content lock and outer-recovery suites and the ProjectMaterial host
# recovery suite; they use isolated fake project roots and launch no Unreal process.
$ErrorActionPreference = 'Continue'
$ueScripts = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$paths = @(
    $PSScriptRoot,
    (Join-Path $ueScripts 'generated_content\test'),
    (Join-Path $ueScripts 'material\test\material_host_recovery.Tests.ps1')
)
$result = Invoke-Pester -Path $paths -PassThru
# A suite that failed to load or set up ran no test; that fails the gate too.
if ($result.FailedCount -gt 0 -or $result.FailedBlocksCount -gt 0 -or $result.FailedContainersCount -gt 0) {
    exit 1
}
