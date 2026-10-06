# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#Requires -Version 5.1

[CmdletBinding()]
param(
    [ValidateSet('WorldRuntime', 'WorldContracts', 'PerformanceEvidence', 'Projection', 'Mirror', 'Payload', 'ReleaseAssembly', 'Docs')]
    [string[]]$Owner,
    [string]$Base = 'HEAD',
    [switch]$PlanOnly
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))

function Get-ProjectReleasePreflightOwners {
    param([AllowEmptyCollection()][string[]]$Paths)
    $selected = [Collections.Generic.HashSet[string]]::new()
    foreach ($path in $Paths) {
        if ($path -match '\.(md|dsl|txt)$') { [void]$selected.Add('Docs'); continue }
        if ($path -match '^Plugins/(Gameplay/(ProjectCharacter|ProjectMenuPlay|ProjectSinglePlay)|UI/(ProjectMenuGame|ProjectMenuMain)|Systems/ProjectLoading)/(Source/|[^/]+\.uplugin$)' -or
            $path -ceq 'Alis.uproject' -or
            $path -match '^Source/Alis/' -or
            $path -match '^Config/Default(Engine|Game|Input|ProjectLoading|ProjectBoot|Scalability|DeviceProfiles)\.ini$' -or
            $path -match '^Plugins/Resources/ProjectExperienceData/(Content/Experiences|Data/Experiences)/' -or
            $path -match '^Plugins/Gameplay/ProjectSinglePlay/Data/PreviewFlightHints\.json$') {
            [void]$selected.Add('WorldRuntime')
            [void]$selected.Add('PerformanceEvidence')
        }
        if ($path -match '^scripts/ue/package/') {
            [void]$selected.Add('ReleaseAssembly')
            if ($path -match '(public_world_projection|isolated_source_workspace|release_definition_assets)') {
                [void]$selected.Add('Projection')
            }
        }
        if ($path -match '^scripts/git/mirror/') {
            [void]$selected.Add('Mirror')
            if ($path -match '(developer|payload|dependency)') { [void]$selected.Add('Payload') }
        }
    }
    return @($selected | Sort-Object)
}

function Get-ProjectReleasePesterCommand {
    return @'
$ErrorActionPreference = 'Stop'
Import-Module Pester -MinimumVersion 5.0 -ErrorAction Stop
$result = Invoke-Pester -Path $env:PROJECT_RELEASE_PREFLIGHT_TEST_PATH -Output Normal -PassThru
if ($null -eq $result -or $result.Result -cne 'Passed' -or $result.PassedCount -le 0) { exit 1 }
'@
}

function Get-ProjectReleasePreflightPlan {
    param([string]$RepositoryRoot = $repoRoot, [string]$Base = 'HEAD', [string[]]$Owner)
    $helper = Join-Path $repoRoot 'scripts/ue/world/test/release_preflight_plan.py'
    $json = & python $helper "--base=$Base" --repo-root $RepositoryRoot
    if ($LASTEXITCODE -ne 0) { throw "Unable to plan release impact from $Base." }
    $snapshot = ($json -join "`n") | ConvertFrom-Json
    $owners = @(@(Get-ProjectReleasePreflightOwners -Paths $snapshot.changed_paths) +
        @($snapshot.owners) + @($Owner) | Where-Object { $_ } | Sort-Object -Unique)
    return [pscustomobject]@{
        requested_base = $snapshot.requested_base; base = $snapshot.base
        changed_paths = @($snapshot.changed_paths); owners = $owners
        commands = @(Get-ProjectReleasePreflightCommands -Owners $owners); hygiene = $true
        world_plan = $snapshot.world_plan; world_gates_executed = $false
    }
}
function Get-ProjectReleasePreflightCommands {
    param([AllowEmptyCollection()][string[]]$Owners)
    $commands = [Collections.Generic.List[object]]::new()
    $pesterFiles = [Collections.Generic.HashSet[string]]::new()
    if ($owners -contains 'WorldRuntime' -or $owners -contains 'PerformanceEvidence') {
        foreach ($path in @('scripts/ue/world/test/uncooked_playable_tour.Tests.ps1',
                'scripts/ue/world/test/manhattan_existing_package.Tests.ps1',
                'scripts/ue/world/test/performance_aggregate.Tests.ps1')) { [void]$pesterFiles.Add($path) }
    }
    foreach ($path in @($pesterFiles | Sort-Object)) {
        $commands.Add([pscustomobject]@{ Kind = 'Pester'; Path = $path })
    }
    if ($owners -contains 'WorldRuntime') {
        $commands.Add([pscustomobject]@{ Kind = 'PowerShell'; Path = 'scripts/ue/world/test/integration/run_uncooked_playable_tour.ps1' })
    }
    $ownerChecks = [ordered]@{
        WorldContracts = @('scripts/ue/world/test/validate_release_contracts.py',
            'scripts/ue/world/test/realization_profile_schema.Tests.ps1',
            'scripts/ue/world/test/test_release_preflight_plan.py')
        Projection = @('scripts/ue/package/tests/test_isolated_source_workspace.ps1',
            'scripts/ue/package/tests/test_public_world_projection.ps1')
        Mirror = @('scripts/git/mirror/tests/test_mirror_linked_worktree.ps1',
            'scripts/git/mirror/tests/test_stage_public_world_manifests.py',
            'scripts/git/mirror/tests/test_public_source_projection.py')
        Payload = @('scripts/git/mirror/tests/test_developer_payload.py',
            'scripts/git/mirror/tests/test_developer_dependency_audit.py')
        ReleaseAssembly = @('scripts/ue/package/tests/test_release_preflight.Tests.ps1',
            'scripts/ue/package/tests/test_release_entrypoint.ps1',
            'scripts/ue/package/tests/test_prepare_release.py',
            'scripts/ue/package/tests/test_prepare_release_v4.py')
    }
    foreach ($selected in $ownerChecks.Keys) {
        if ($owners -notcontains $selected) { continue }
        foreach ($path in $ownerChecks[$selected]) {
            $kind = if ($path.EndsWith('.Tests.ps1')) { 'Pester' } elseif ($path.EndsWith('.py')) {
                'Python'
            } else { 'PowerShell' }
            $commands.Add([pscustomobject]@{ Kind = $kind; Path = $path })
        }
    }
    return @($commands)
}
$plan = Get-ProjectReleasePreflightPlan -RepositoryRoot $repoRoot -Base $Base -Owner $Owner
$paths = $plan.changed_paths
$owners = $plan.owners
$commands = $plan.commands
if ($PlanOnly) {
    $plan | ConvertTo-Json -Depth 12
    return
}
$outputRoot = Join-Path $repoRoot ('tmp/release/preflight/' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$results = [Collections.Generic.List[object]]::new()
foreach ($command in $commands) {
    $path = Join-Path $repoRoot $command.Path
    $log = Join-Path $outputRoot (([IO.Path]::GetFileName($path)) + '.log')
    Write-Host "Release preflight: $($command.Kind) $($command.Path)"
    switch ($command.Kind) {
        'Pester' {
            $previousTestPath = $env:PROJECT_RELEASE_PREFLIGHT_TEST_PATH
            $env:PROJECT_RELEASE_PREFLIGHT_TEST_PATH = $path
            try {
                $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes(
                        (Get-ProjectReleasePesterCommand)))
                $process = Start-Process -FilePath powershell -ArgumentList @('-NoProfile',
                    '-ExecutionPolicy', 'Bypass', '-OutputFormat', 'Text', '-EncodedCommand', $encoded) -WindowStyle Hidden `
                    -Wait -PassThru -RedirectStandardOutput $log -RedirectStandardError ($log + '.stderr')
            }
            finally {
                if ($null -eq $previousTestPath) { Remove-Item Env:PROJECT_RELEASE_PREFLIGHT_TEST_PATH }
                else { $env:PROJECT_RELEASE_PREFLIGHT_TEST_PATH = $previousTestPath }
            }
        }
        'Python' {
            $process = Start-Process -FilePath python -ArgumentList ('"' + $path + '"') `
                -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $log `
                -RedirectStandardError ($log + '.stderr')
        }
        'PowerShell' {
            $process = Start-Process -FilePath powershell -ArgumentList @('-NoProfile',
                '-ExecutionPolicy', 'Bypass', '-File', ('"' + $path + '"')) -WindowStyle Hidden `
                -Wait -PassThru -RedirectStandardOutput $log -RedirectStandardError ($log + '.stderr')
        }
    }
    $code = $process.ExitCode
    $results.Add([pscustomobject]@{ command = $command; exit_code = $code; log = $log; stderr = $log + '.stderr' })
    if ($code -ne 0) { throw "Focused preflight failed at $($command.Path). See $log" }
}
& git -C $repoRoot diff --check
if ($LASTEXITCODE -ne 0) { throw 'Diff hygiene failed.' }
foreach ($relative in @($paths | Sort-Object -Unique)) {
    $path = Join-Path $repoRoot $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
    if ($relative -match '\.(md|dsl|txt)$' -and [IO.File]::ReadAllText($path) -match '[^\x00-\x7F]') {
        throw "Non-ASCII documentation: $relative"
    }
    if ($relative.EndsWith('.ps1')) {
        $tokens = $null; $errors = $null
        [void][Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
        if ($errors.Count) { throw "PowerShell parse failed: $relative" }
    }
}
& python (Join-Path $repoRoot 'scripts/ue/check/governance/validate_engine_env.py') --paths-only
if ($LASTEXITCODE -ne 0) { throw 'Portable-path governance failed.' }
& (Join-Path $repoRoot 'scripts/ue/check/governance/validate_no_alis_prefix.bat')
if ($LASTEXITCODE -ne 0) { throw 'Reusable-name governance failed.' }
$receipt = [ordered]@{
    schema = 'project-release-focused-preflight:v1'; status = 'accepted'
    requested_base = $Base; base = $plan.base; owners = $owners; changed_paths = @($paths | Sort-Object -Unique)
    world_plan = $plan.world_plan; world_gates_executed = $false
    checks = @($results); package_built = $false; generated_authority_promoted = $false
}
$receiptPath = Join-Path $outputRoot 'preflight.json'
[IO.File]::WriteAllText($receiptPath, ($receipt | ConvertTo-Json -Depth 8) + "`n",
    [Text.UTF8Encoding]::new($false))
Write-Host "Focused release preflight accepted: $receiptPath"
