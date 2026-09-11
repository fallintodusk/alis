# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

Describe 'World, ProjectCinematic, and release package identity' {
    BeforeAll {
        $script:repoRoot = Split-Path -Parent (Split-Path -Parent `
                (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
        $script:worldEvidence = Join-Path $script:repoRoot `
            'scripts\ue\world\test\performance\project_world_performance_evidence.ps1'
        $script:cinematicBinding = Join-Path $script:repoRoot `
            'scripts\ue\cinematic\release_binding.ps1'
        $script:packageIdentityTool = Join-Path $script:repoRoot `
            'scripts\ue\package\prepare_release.py'

        function Import-PackageIdentityFunction {
            param([string]$Path, [string]$Name)
            $tokens = $null
            $parseErrors = $null
            $ast = [Management.Automation.Language.Parser]::ParseFile(
                $Path, [ref]$tokens, [ref]$parseErrors)
            @($parseErrors).Count | Should -Be 0
            $definition = $ast.Find({
                    param($node)
                    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
                        $node.Name -ceq $Name
                }, $true)
            $null -ne $definition | Should -BeTrue
            Set-Item -Path "Function:script:$Name" -Value $definition.Body.GetScriptBlock()
        }

        Import-PackageIdentityFunction -Path $script:worldEvidence `
            -Name 'Get-ProjectWorldPackagePayloadDigest'
        Import-PackageIdentityFunction -Path $script:cinematicBinding `
            -Name 'Get-ProjectCinematicPackageTreeDigest'
    }

    It 'uses one deterministic payload digest and ignores runtime-written state' {
        $ownerRoot = Join-Path $script:repoRoot 'tmp\world\package_identity'
        $testRoot = Join-Path $ownerRoot ([Guid]::NewGuid().ToString('N'))
        try {
            foreach ($relative in @(
                    'Windows\Alis\Content\Paks\pakchunk0-Windows.pak',
                    'Windows\Alis\Content\Paks\pakchunk1-Windows.pak',
                    'Windows\Alis\Content\Paks\pakchunk10-Windows.pak',
                    'Windows\Alis\Binaries\Win64\Alis-Win64-Shipping.exe')) {
                $path = Join-Path $testRoot $relative
                New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
                [IO.File]::WriteAllText($path, $relative)
            }
            $runtimePath = Join-Path $testRoot 'Windows\Alis\Saved\State\latest.json'
            New-Item -ItemType Directory -Path (Split-Path -Parent $runtimePath) -Force | Out-Null
            [IO.File]::WriteAllText($runtimePath, 'runtime one')

            $worldDigest = Get-ProjectWorldPackagePayloadDigest -Path $testRoot
            $cinematicDigest = Get-ProjectCinematicPackageTreeDigest -Path $testRoot
            $releaseOutput = @(& python $script:packageIdentityTool package-tree `
                    --package-root $testRoot)

            $LASTEXITCODE | Should -Be 0
            $releaseOutput.Count | Should -Be 1
            $worldDigest | Should -BeExactly $cinematicDigest
            $worldDigest | Should -BeExactly $releaseOutput[0]

            [IO.File]::WriteAllText($runtimePath, 'runtime two')
            (Get-ProjectWorldPackagePayloadDigest -Path $testRoot) | Should -BeExactly $worldDigest
            (Get-ProjectCinematicPackageTreeDigest -Path $testRoot) | Should -BeExactly $worldDigest
            @(& python $script:packageIdentityTool package-tree --package-root $testRoot)[0] |
                Should -BeExactly $worldDigest
        }
        finally {
            $owner = [IO.Path]::GetFullPath($ownerRoot).TrimEnd('\', '/')
            $target = [IO.Path]::GetFullPath($testRoot)
            if ($target.StartsWith(
                    $owner + [IO.Path]::DirectorySeparatorChar,
                    [StringComparison]::OrdinalIgnoreCase) -and
                (Test-Path -LiteralPath $target)) {
                Remove-Item -LiteralPath $target -Recurse -Force
            }
            if (Test-Path -LiteralPath $ownerRoot) {
                $children = @(Get-ChildItem -LiteralPath $ownerRoot -Force)
                if ($children.Count -eq 0) {
                    Remove-Item -LiteralPath $ownerRoot -Force
                }
            }
        }
    }
}
