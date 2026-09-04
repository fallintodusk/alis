# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

Describe 'World and ProjectCinematic release source identity' {
    BeforeAll {
        $script:repoRoot = Split-Path -Parent (Split-Path -Parent `
                (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
        $script:worldRunner = Join-Path $script:repoRoot `
            'scripts\ue\world\test\performance\run_kazan_playable_tour.ps1'
        $script:captureRunner = Join-Path $script:repoRoot `
            'scripts\ue\cinematic\run_release_capture.ps1'

        function Assert-PlayableTour {
            param([bool]$Condition, [string]$Message)
            if (-not $Condition) { throw $Message }
        }

        function Assert-Capture {
            param([bool]$Condition, [string]$Message)
            if (-not $Condition) { throw $Message }
        }

        function Import-SourceIdentityFunction {
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

        Import-SourceIdentityFunction -Path $script:worldRunner `
            -Name 'Get-PlayableTourSourceStateDigest'
        Import-SourceIdentityFunction -Path $script:captureRunner `
            -Name 'Get-CaptureSourceStateDigest'
    }

    It 'computes the same dirty-state digest while revision remains separate' {
        $projectRoot = $script:repoRoot
        $worldDigest = Get-PlayableTourSourceStateDigest
        $captureDigest = Get-CaptureSourceStateDigest
        $revision = (& git -C $projectRoot rev-parse HEAD).Trim()

        $worldDigest | Should -BeExactly $captureDigest
        $worldDigest | Should -Match '^[a-f0-9]{64}$'
        $LASTEXITCODE | Should -Be 0
        $revision | Should -Match '^[a-f0-9]{40}$'
    }

    It 'detects dirty bytes while clean revision identity stays independent' {
        $ownerRoot = Join-Path $script:repoRoot 'tmp\world\source_identity'
        $testRoot = Join-Path $ownerRoot ([Guid]::NewGuid().ToString('N'))
        try {
            New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
            & git -C $testRoot init --quiet
            & git -C $testRoot config user.name 'ALIS Test'
            & git -C $testRoot config user.email 'test@invalid.local'
            [IO.File]::WriteAllText((Join-Path $testRoot 'tracked.txt'), 'revision one')
            & git -C $testRoot add -- tracked.txt
            & git -C $testRoot -c core.hooksPath=NUL commit --quiet -m 'revision one'
            $LASTEXITCODE | Should -Be 0

            $projectRoot = $testRoot
            $firstRevision = (& git -C $projectRoot rev-parse HEAD).Trim()
            $cleanDigest = Get-PlayableTourSourceStateDigest
            $cleanDigest | Should -BeExactly (Get-CaptureSourceStateDigest)

            $untrackedPath = Join-Path $testRoot 'untracked.txt'
            [IO.File]::WriteAllText($untrackedPath, 'untracked one')
            $untrackedDigest = Get-PlayableTourSourceStateDigest
            $untrackedDigest | Should -BeExactly (Get-CaptureSourceStateDigest)
            $untrackedDigest | Should -Not -BeExactly $cleanDigest
            [IO.File]::WriteAllText($untrackedPath, 'untracked two')
            (Get-PlayableTourSourceStateDigest) | Should -Not -BeExactly $untrackedDigest
            [IO.File]::Delete($untrackedPath)

            [IO.File]::WriteAllText((Join-Path $testRoot 'tracked.txt'), 'revision two')
            $trackedDigest = Get-PlayableTourSourceStateDigest
            $trackedDigest | Should -BeExactly (Get-CaptureSourceStateDigest)
            $trackedDigest | Should -Not -BeExactly $cleanDigest
            & git -C $testRoot add -- tracked.txt
            & git -C $testRoot -c core.hooksPath=NUL commit --quiet -m 'revision two'
            $LASTEXITCODE | Should -Be 0

            $secondRevision = (& git -C $projectRoot rev-parse HEAD).Trim()
            (Get-PlayableTourSourceStateDigest) | Should -BeExactly $cleanDigest
            (Get-CaptureSourceStateDigest) | Should -BeExactly $cleanDigest
            $secondRevision | Should -Not -BeExactly $firstRevision
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
