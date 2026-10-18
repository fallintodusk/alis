BeforeAll { . (Join-Path $PSScriptRoot '..\realization_evidence.ps1') }

Describe 'World realization evidence isolation' {
    It 'allows bounded scratch and retained validation destinations, rejecting sibling and traversal paths' {
        foreach ($path in @('tmp/world/probe/result.json','Saved/Validation/WorldRealization/probe/result.json')) {
            $location = Resolve-ProjectWorldEvidenceLocation -ProjectRoot $TestDrive -EvidencePath (Join-Path $TestDrive $path)
            $location.ResultPath | Should -Be ([System.IO.Path]::GetFullPath((Join-Path $TestDrive $path)))
        }
        foreach ($path in @('tmp/world_sibling/result.json','tmp/world/../../Content/result.json','tmp/other/result.json')) {
            { Resolve-ProjectWorldEvidenceLocation -ProjectRoot $TestDrive -EvidencePath (Join-Path $TestDrive $path) } | Should -Throw
        }
    }
    It 'keeps default invocation evidence deterministic' {
        $evidenceArgs = @{ProjectRoot=$TestDrive;ModeName='validate';InvocationIdentity=[ordered]@{schema_version=1; input='fixture'}}
        $first = Resolve-ProjectWorldEvidenceLocation @evidenceArgs
        $second = Resolve-ProjectWorldEvidenceLocation @evidenceArgs
        $first.ResultPath | Should -Be $second.ResultPath
        $first.ResultPath | Should -BeLike '*Saved*WorldRealization*validate.json'
    }
    It 'refuses canonical receipt, payload and profile-sidecar aliases without changing bytes' {
        $root = Join-Path $TestDrive 'tmp/world/fixture'
        $canonicalPath = Join-Path $root 'inputs/compile_result.json'
        $profile = Join-Path $root 'profile.json'
        New-Item -ItemType Directory -Path (Split-Path -Parent $canonicalPath) -Force | Out-Null
        Set-Content $canonicalPath '{}' -NoNewline
        Set-Content $profile '{}' -NoNewline
        $before = (Get-FileHash $canonicalPath).Hash, (Get-FileHash $profile).Hash
        foreach ($result in @($canonicalPath, (Join-Path $root 'inputs/canonical/cell.json'))) {
            { Assert-ProjectWorldEvidenceInputIsolation -ResultPath $result -CompileResultPath $canonicalPath } | Should -Throw '*canonical*'
        }
        foreach ($result in @($profile, (Join-Path $root 'profile.any'))) {
            $protected = if ($result -eq $profile) { $profile } else { [System.IO.Path]::ChangeExtension($result, '.log') }
            { Assert-ProjectWorldEvidenceInputIsolation -ResultPath $result -CompileResultPath $canonicalPath -InputPaths @($protected) } | Should -Throw '*requested input*'
        }
        $result = Join-Path $root 'evidence/result.json'
        { Assert-ProjectWorldEvidenceInputIsolation -ResultPath $result -CompileResultPath $canonicalPath -InputPaths @($result+'.tmp') } | Should -Throw '*requested input*'
        Assert-ProjectWorldEvidenceInputIsolation -ResultPath $result -CompileResultPath $canonicalPath -InputPaths @($profile)
        (Get-FileHash $canonicalPath).Hash, (Get-FileHash $profile).Hash | Should -Be $before
        Test-Path $result | Should -BeFalse
    }
    It 'protects manifest authority and profiles from all host output writers' {
        $root = Join-Path $TestDrive 'tmp/world/writers'
        $canonicalPath = Join-Path $root 'canonical/result.json'
        $manifestRoot = Join-Path $root 'manifests'
        $outputDirectory = Join-Path $root 'evidence'
        $result = Join-Path $outputDirectory 'result.json'
        $dirty = Join-Path $outputDirectory 'layer_dirty_input.json'
        foreach ($profile in @($dirty, ($dirty + '.tmp'))) {
            { Assert-ProjectWorldEvidenceInputIsolation -ResultPath $result -CompileResultPath $canonicalPath `
                -InputPaths @($profile) -AuxiliaryOutputPaths @($dirty, ($dirty + '.tmp')) } | Should -Throw '*requested input*'
        }
        { Assert-ProjectWorldEvidenceInputIsolation -ResultPath (Join-Path $manifestRoot 'active_set.json') `
            -CompileResultPath $canonicalPath -ProtectedRoots @($manifestRoot) } | Should -Throw '*manifest authority*'
        { Assert-ProjectWorldEvidenceInputIsolation -ResultPath $dirty -CompileResultPath $canonicalPath `
            -AuxiliaryOutputPaths @($dirty, ($dirty + '.tmp')) } | Should -Throw '*alias each other*'
        { Assert-ProjectWorldEvidenceInputIsolation -ResultPath (Join-Path $outputDirectory 'result.log') `
            -CompileResultPath $canonicalPath } | Should -Throw '*alias each other*'
    }
}
