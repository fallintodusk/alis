# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#Requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$EvidenceDirectory)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../../..')).Path
$evidence = [IO.Path]::GetFullPath($EvidenceDirectory)
$scratch = Join-Path $projectRoot 'tmp'
if (-not $evidence.StartsWith($scratch + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $evidence)) {
    throw 'Native World verification requires a fresh directory under project tmp.'
}
# Ambient diagnostic scenes must not change the fixed common suite.
$ambient = @(Get-ChildItem Env: | Where-Object {
    $_.Name -match '^PROJECT_WORLD_(COMPOSITE_TERRAIN_|BASIN_|CIVIL_GRADE_FIXTURE$|STRUCTURE_|GROUND_SCENE_FIXTURE$|ROAD_NEIGHBORHOOD_|GANGWAY_ASSEMBLY_|NEIGHBORHOOD_|IDENTIFIED_BASIN_|CONSTRUCTED_SURFACE_FIXTURE$|ROAD_ELEVATION_)'
})
if ($ambient.Count -gt 0) { throw ('Remove ambient diagnostic inputs: ' + ($ambient.Name -join ', ')) }
New-Item -ItemType Directory -Path $evidence | Out-Null
$priorCold = [Environment]::GetEnvironmentVariable('ALIS_NO_PERSISTENT_EDITOR', 'Process')
$code = 1
try {
    $env:ALIS_NO_PERSISTENT_EDITOR = '1'
    & (Join-Path $projectRoot 'scripts/ue/standalone/build.ps1') | Out-Host
    $code = $LASTEXITCODE
    if ($code -eq 0) {
        $ini = (Join-Path $evidence 'EditorPerProjectUserSettings.ini').Replace('\', '/')
        # Cold startup and the complete saved-reload suite share one process deadline.
        & (Join-Path $projectRoot 'scripts/ue/test/unit/run_cpp_tests_safe.ps1') `
            -TestFilter 'Project.World.Realization' -Mode Gate -Map '/Engine/Maps/Entry' `
            -TimeoutSeconds 360 -UseTestExitOnly -LogDirectory (Join-Path $evidence 'runner') `
            -ExtraArgs "-ProjectSkipFrontEnd -SCCProvider=None -nop4 -EditorPerProjectUserSettingsINI=`"$ini`"" | Out-Host
        $code = $LASTEXITCODE
    }
}
finally { [Environment]::SetEnvironmentVariable('ALIS_NO_PERSISTENT_EDITOR', $priorCold, 'Process') }
exit $code
