# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#Requires -Version 5.1

Describe 'Focused release preflight owner routing' {
    BeforeAll {
        $repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
        $fixtureRepositories = [Collections.Generic.List[string]]::new()
        $runner = Join-Path $repoRoot 'scripts/ue/package/run_release_preflight.ps1'
        $tokens = $null; $errors = $null
        $ast = [Management.Automation.Language.Parser]::ParseFile($runner, [ref]$tokens, [ref]$errors)
        $definition = $ast.Find({ param($node)
                $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
                $node.Name -ceq 'Get-ProjectReleasePreflightOwners'
            }, $true)
        Set-Item Function:Get-ProjectReleasePreflightOwners -Value $definition.Body.GetScriptBlock()
        $definition = $ast.Find({ param($node)
                $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
                $node.Name -ceq 'Get-ProjectReleasePesterCommand'
            }, $true)
        Set-Item Function:Get-ProjectReleasePesterCommand -Value $definition.Body.GetScriptBlock()
        foreach ($name in @('Get-ProjectReleasePreflightPlan', 'Get-ProjectReleasePreflightCommands')) {
            $definition = $ast.Find({ param($node)
                    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
                    $node.Name -ceq $name
                }, $true)
            if ($definition) { Set-Item "Function:$name" -Value $definition.Body.GetScriptBlock() }
        }
    }
    AfterAll {
        $scratchRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'tmp/release/preflight-tests')) + [IO.Path]::DirectorySeparatorChar
        foreach ($fixture in $fixtureRepositories) {
            $target = [IO.Path]::GetFullPath($fixture)
            if (-not $target.StartsWith($scratchRoot, [StringComparison]::OrdinalIgnoreCase)) {
                throw 'Fixture cleanup escaped the release test scratch root.'
            }
            if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
        }
    }
    It 'selects runtime proof for changed gameplay and no World proof for docs' {
        $owners = @(Get-ProjectReleasePreflightOwners @('Plugins/Gameplay/ProjectSinglePlay/Source/Driver.cpp'))
        $owners | Should -Contain 'WorldRuntime'
        $owners | Should -Not -Contain 'Projection'
        @(Get-ProjectReleasePreflightOwners @('docs/build/packaging_guide.md')) | Should -Be @('Docs')
        @(Get-ProjectReleasePreflightOwners @('scripts/git/mirror/README.md')) | Should -Be @('Docs')
    }
    It 'keeps mirror and payload failures away from runtime build and world generation' {
        $owners = @(Get-ProjectReleasePreflightOwners @('scripts/git/mirror/compose_developer_payload.py'))
        $owners | Should -Contain 'Payload'
        $owners | Should -Contain 'Mirror'
        $owners | Should -Not -Contain 'WorldRuntime'
        @(Get-ProjectReleasePreflightOwners @('scripts/ue/package/public_world_projection.ps1')) | Should -Contain 'Projection'
    }
    It 'selects real gameplay for direct movement, startup and experience inputs' {
        foreach ($path in @('Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlay/Private/PreviewFlight.cpp',
                'Source/Alis/Private/AlisGI.cpp', 'Config/DefaultEngine.ini', 'Config/DefaultInput.ini',
                'Plugins/Resources/ProjectExperienceData/Data/Experiences/kazan_territory.json',
                'Plugins/Resources/ProjectExperienceData/Content/Experiences/ManhattanShowcase.uasset',
                'Plugins/Gameplay/ProjectSinglePlay/Data/PreviewFlightHints.json')) {
            @(Get-ProjectReleasePreflightOwners @($path)) | Should -Contain 'WorldRuntime'
        }
        foreach ($path in @('Alis.uproject',
                'Plugins/Gameplay/ProjectSinglePlay/ProjectSinglePlay.uplugin')) {
            @(Get-ProjectReleasePreflightOwners @($path)) | Should -Contain 'WorldRuntime'
        }
    }
    It 'offers a read-only plan and excludes cook and package commands' {
        $json = & powershell -NoProfile -ExecutionPolicy Bypass -File $runner -Owner Docs -PlanOnly
        $LASTEXITCODE | Should -Be 0
        $plan = ($json -join "`n") | ConvertFrom-Json
        $plan.owners | Should -Contain 'Docs'
        $automatic = Get-ProjectReleasePreflightPlan -RepositoryRoot $repoRoot -Base HEAD
        foreach ($required in $automatic.owners) { $plan.owners | Should -Contain $required }
        $json = & powershell -NoProfile -ExecutionPolicy Bypass -File $runner -Owner WorldRuntime -PlanOnly
        $plan = ($json -join "`n") | ConvertFrom-Json
        @($plan.commands | Where-Object { $_.Path -match '(package_release|realize_canonical|prepare_release_inputs|run_kazan_playable_tour|run_manhattan_showcase_prototype)' }).Count | Should -Be 0
        @($plan.commands | Where-Object { $_.Path.EndsWith('run_uncooked_playable_tour.ps1') }).Count | Should -Be 1
    }
    It 'keeps both rename endpoints, untracked paths and additive owners through the executed plan' {
        $fixture = Join-Path $repoRoot ('tmp/release/preflight-tests/' + [Guid]::NewGuid().ToString('N'))
        $fixtureRepositories.Add($fixture)
        $source = 'Plugins/World/ProjectWorld/Source/Foo.cpp'
        New-Item -ItemType Directory -Path (Join-Path $fixture 'Plugins/World/ProjectWorld/Source') -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $fixture 'docs') -Force | Out-Null
        $producerFixture = @'
import sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]) / 'tools'))
from World.EndToEndValidation.tests.proof_input_fixture import write_producers
write_producers(Path(sys.argv[2]))
'@
        & python -c $producerFixture $repoRoot $fixture
        $LASTEXITCODE | Should -Be 0
        [IO.File]::WriteAllText((Join-Path $fixture $source), 'fixture runtime source')
        & git -C $fixture init --quiet
        & git -C $fixture config diff.renames true
        & git -C $fixture add -- .
        & git -C $fixture -c user.name=fixture -c user.email=fixture@example.invalid commit --quiet -m fixture
        $LASTEXITCODE | Should -Be 0
        [IO.File]::AppendAllText((Join-Path $fixture $source), ' modified')
        $runtimePlan = Get-ProjectReleasePreflightPlan -RepositoryRoot $fixture -Base HEAD -Owner Docs
        $runtimePlan.owners | Should -Contain 'WorldRuntime'
        $runtimePlan.owners | Should -Contain 'Docs'
        [IO.File]::WriteAllText((Join-Path $fixture $source), 'fixture runtime source')
        & git -C $fixture mv -- $source docs/Foo.md
        @(& git -C $fixture diff --name-only HEAD) | Should -Not -Contain $source
        [IO.File]::WriteAllText((Join-Path $fixture 'docs/untracked.md'), 'fixture untracked')
        $plan = Get-ProjectReleasePreflightPlan -RepositoryRoot $fixture -Base HEAD -Owner Docs
        $plan.changed_paths | Should -Contain $source
        $plan.changed_paths | Should -Contain 'docs/Foo.md'
        $plan.changed_paths | Should -Contain 'docs/untracked.md'
        $plan.owners | Should -Contain 'Docs'
        $plan.owners | Should -Contain 'WorldRuntime'
        @($plan.commands | Where-Object { $_.Path.EndsWith('run_uncooked_playable_tour.ps1') }).Count | Should -Be 1
        $plan.base | Should -Match '^[0-9a-f]{40}$'
        $plan.requested_base | Should -Be 'HEAD'
        foreach ($base in @('--help', 'missing-fixture-base')) {
            { Get-ProjectReleasePreflightPlan -RepositoryRoot $fixture -Base $base } | Should -Throw
        }
    }
    It 'selects only Docs for a real docs-only Git fixture' {
        $fixture = Join-Path $repoRoot ('tmp/release/preflight-tests/' + [Guid]::NewGuid().ToString('N'))
        $fixtureRepositories.Add($fixture)
        New-Item -ItemType Directory -Path $fixture -Force | Out-Null
        [IO.File]::WriteAllText((Join-Path $fixture 'README.md'), 'fixture documentation')
        & git -C $fixture init --quiet
        & git -C $fixture add -- README.md
        & git -C $fixture -c user.name=fixture -c user.email=fixture@example.invalid commit --quiet -m fixture
        $LASTEXITCODE | Should -Be 0
        [IO.File]::AppendAllText((Join-Path $fixture 'README.md'), ' modified')
        $plan = Get-ProjectReleasePreflightPlan -RepositoryRoot $fixture -Base HEAD
        @($plan.owners) | Should -Be @('Docs')
        @($plan.commands).Count | Should -Be 0
    }
    It 'routes World contracts to their schema and actual projection or gameplay consumers' {
        $worldPlanner = Join-Path $repoRoot 'scripts/ue/world/test/release_preflight_plan.py'
        $json = & python $worldPlanner --base HEAD --repo-root $repoRoot
        $LASTEXITCODE | Should -Be 0
        $plan = ($json -join "`n") | ConvertFrom-Json
        $plan.world_gates_executed | Should -BeFalse
        $schemaPlan = Get-ProjectReleasePreflightCommands -Owners WorldContracts,Projection
        @($schemaPlan | Where-Object { $_.Path.EndsWith('validate_release_contracts.py') }).Count | Should -Be 1
        @($schemaPlan | Where-Object { $_.Path.EndsWith('realization_profile_schema.Tests.ps1') }).Count | Should -Be 1
        @($schemaPlan | Where-Object { $_.Path.EndsWith('test_public_world_projection.ps1') }).Count | Should -Be 1
        @($schemaPlan | Where-Object { $_.Path.EndsWith('run_uncooked_playable_tour.ps1') }).Count | Should -Be 0
    }
    It 'fails closed when the Pester instrument or executed result is unknown' {
        $directory = Join-Path $repoRoot ('tmp/release/preflight-tests/' + [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        foreach ($fixture in @(
                @{ Body = "function Import-Module { throw 'fixture missing instrument' }; function Invoke-Pester {}"; Exit = 1 },
                @{ Body = 'function Import-Module {}; function Invoke-Pester { return $null }'; Exit = 1 },
                @{ Body = "function Import-Module {}; function Invoke-Pester { [pscustomobject]@{ Result='Failed'; PassedCount=0; FailedCount=0 } }"; Exit = 1 },
                @{ Body = "function Import-Module {}; function Invoke-Pester { [pscustomobject]@{ Result='Passed'; PassedCount=0 } }"; Exit = 1 },
                @{ Body = "function Import-Module {}; function Invoke-Pester { [pscustomobject]@{ Result='Passed'; PassedCount=1 } }"; Exit = 0 })) {
            $body = $fixture.Body + "`n" + (Get-ProjectReleasePesterCommand)
            $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($body))
            $process = Start-Process -FilePath powershell -ArgumentList @('-NoProfile',
                '-OutputFormat', 'Text', '-EncodedCommand', $encoded) -WindowStyle Hidden `
                -Wait -PassThru -RedirectStandardOutput (Join-Path $directory 'stdout.log') `
                -RedirectStandardError (Join-Path $directory 'stderr.log')
            $process.ExitCode | Should -Be $fixture.Exit
        }
    }
}

Describe 'Packager inspection handoff' {
    It 'keeps IoStore diagnostics outside the Candidate with distinct invocation paths' {
        $repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
        $fixture = Join-Path $repo ('tmp/package/inspection-tests/' + [Guid]::NewGuid().ToString('N'))
        $ProjectRoot = $fixture
        $ScriptDir = Join-Path $fixture 'scripts/ue/package'
        $OutputDir = Join-Path $fixture 'candidate'
        $PlatformDir = Join-Path $OutputDir 'Windows'
        $Platform = 'Win64'
        $RequiredCookMap = '/ProjectWorldData/Generated/Territory/Fixture'
        $ObjectCookContract = Join-Path $fixture 'contract.json'
        $EngineRoot = Join-Path $fixture 'engine'
        New-Item -ItemType Directory -Path $ScriptDir,$PlatformDir -Force | Out-Null
        Set-Content (Join-Path $PlatformDir 'Alis.exe') 'unchanged-runtime' -Encoding Ascii
        Set-Content (Join-Path $OutputDir 'package_summary.txt') 'accepted' -Encoding Ascii
        @'
param($PackageRoot, $RequiredPackage, $ResultPath, $ObjectContractPath, $EngineRoot)
New-Item -ItemType Directory -Path (Split-Path -Parent $ResultPath) -Force | Out-Null
Set-Content -LiteralPath $ResultPath '{"status":"accepted"}' -Encoding Ascii
Set-Content (Join-Path (Split-Path -Parent $ResultPath) 'pakchunk.list.log') 'listing' -Encoding Ascii
'@ | Set-Content (Join-Path $ScriptDir 'inspect_iostore.ps1') -Encoding Ascii
        try {
            $tokens = $null; $errors = $null
            $ast = [Management.Automation.Language.Parser]::ParseFile(
                (Join-Path $repo 'scripts/ue/package/package_release.ps1'), [ref]$tokens, [ref]$errors)
            $errors.Count | Should -Be 0
            $blocks = @($ast.FindAll({ param($node)
                $node -is [Management.Automation.Language.IfStatementAst] -and
                $node.Extent.Text.Contains('$IoStoreInspectionRoot =')
            }, $true))
            $blocks.Count | Should -Be 1
            $block = [scriptblock]::Create($blocks[0].Extent.Text)
            $identity = Join-Path $repo 'scripts/ue/package/prepare_release.py'
            $before = & python $identity package-tree --package-root $OutputDir
            $LASTEXITCODE | Should -Be 0
            $paths = @()
            foreach ($invocation in 1..2) {
                . $block
                $paths += $IoStoreInspectionResult
                Test-Path -LiteralPath $IoStoreInspectionResult | Should -BeTrue
                $prefix = [IO.Path]::GetFullPath((Join-Path $fixture 'tmp/package/iostore')) +
                    [IO.Path]::DirectorySeparatorChar
                [IO.Path]::GetFullPath($IoStoreInspectionResult).StartsWith($prefix,
                    [StringComparison]::OrdinalIgnoreCase) | Should -BeTrue
                @(Get-ChildItem -LiteralPath $OutputDir | ForEach-Object Name | Sort-Object) |
                    Should -Be @('package_summary.txt', 'Windows')
                (& python $identity package-tree --package-root $OutputDir) | Should -Be $before
            }
            $paths[1] | Should -Not -Be $paths[0]
        }
        finally {
            $scratch = [IO.Path]::GetFullPath((Join-Path $repo 'tmp/package/inspection-tests')) +
                [IO.Path]::DirectorySeparatorChar
            if (-not [IO.Path]::GetFullPath($fixture).StartsWith($scratch,
                    [StringComparison]::OrdinalIgnoreCase)) { throw 'Inspection fixture escaped scratch.' }
            Remove-Item -LiteralPath $fixture -Recurse -Force
        }
    }
}
