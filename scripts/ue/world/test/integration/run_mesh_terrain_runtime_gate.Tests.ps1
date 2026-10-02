# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

Describe 'Mesh Terrain runtime probe derivation' {
    BeforeAll {
        $script:projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..\..'))
        $script:runner = Join-Path $PSScriptRoot 'run_mesh_terrain_runtime_gate.ps1'
        $script:testRoot = Join-Path $script:projectRoot `
            ('tmp\world\mesh_terrain_runtime_probe_tests\' + [guid]::NewGuid().ToString('N'))
        $script:compileRoot = Join-Path $script:testRoot 'compile'
        New-Item -ItemType Directory -Path `
            (Join-Path $script:compileRoot 'canonical\terrain') -Force | Out-Null

        function Write-TestJson {
            param([string]$Path, [object]$Value)
            New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
            [IO.File]::WriteAllText(
                $Path,
                ($Value | ConvertTo-Json -Depth 10) + "`n",
                [Text.UTF8Encoding]::new($false))
        }

        $centerPath = Join-Path $script:compileRoot 'canonical\terrain\center.json'
        $edgePath = Join-Path $script:compileRoot 'canonical\terrain\edge.json'
        Write-TestJson $centerPath ([ordered]@{
                grid_id = 'grid_test'
                cell_id = 'grid_test:center'
                bounds = @(-5.0, -5.0, 5.0, 5.0)
                core_samples = @(@(1.0, 2.0), @(2.0, 1.0))
            })
        Write-TestJson $edgePath ([ordered]@{
                grid_id = 'grid_test'
                cell_id = 'grid_test:edge'
                bounds = @(100.0, 0.0, 110.0, 10.0)
                core_samples = @(@(6.0, 7.0), @(7.0, 6.0))
            })
        $centerHash = (Get-FileHash $centerPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $edgeHash = (Get-FileHash $edgePath -Algorithm SHA256).Hash.ToLowerInvariant()

        $script:compileResultPath = Join-Path $script:compileRoot 'compile_result.json'
        $script:compilerProfilePath = Join-Path $script:testRoot 'compiler-profile.json'
        $script:runtimeProfilePath = Join-Path $script:testRoot 'runtime-profile.json'
        Write-TestJson $script:compileResultPath ([ordered]@{
                status = 'accepted'
                profile_id = 'probe_test'
                outputs = @(
                    [ordered]@{ path = 'canonical/terrain/center.json'; sha256 = $centerHash },
                    [ordered]@{ path = 'canonical/terrain/edge.json'; sha256 = $edgeHash }
                )
            })
        Write-TestJson $script:compilerProfilePath ([ordered]@{
                profile_id = 'probe_test'
                engine_georeference_origin = @(0.0, 0.0)
                grid = [ordered]@{ vertical_origin_m = 0.0 }
            })
        Write-TestJson $script:runtimeProfilePath ([ordered]@{
                profile_id = 'probe_runtime'
                grid_id = 'grid_test'
                runtime_partition = [ordered]@{ loading_range_m = 10.0 }
                product_spawn = [ordered]@{
                    anchor = 'engine_georeference_origin'
                    height_above_terrain_m = 20.0
                }
            })
        $script:sourceIdentity = '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'
    }

    AfterAll {
        if ($script:testRoot.StartsWith(
                (Join-Path $script:projectRoot 'tmp\world\mesh_terrain_runtime_probe_tests'),
                [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $script:testRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    It 'derives center edge probe height and radius from authenticated canonical and runtime inputs' {
        $probePath = Join-Path $script:testRoot 'probe.json'
        $output = & $script:runner `
            -PackageRoot (Join-Path $script:testRoot 'package') `
            -ResultPath (Join-Path $script:projectRoot `
                'Saved\Validation\WorldRealization\mesh-terrain-runtime-probe-test-unused.json') `
            -LogPath (Join-Path $script:testRoot 'unused.log') `
            -SourceIdentity $script:sourceIdentity `
            -CompileResultPath $script:compileResultPath `
            -CompilerProfilePath $script:compilerProfilePath `
            -RuntimeProfilePath $script:runtimeProfilePath `
            -ProbeDerivationPath $probePath `
            -ProbeDerivationOnly
        $probe = $output | ConvertFrom-Json
        $probe.status | Should -BeExactly 'accepted'
        $probe.center.cell_id | Should -BeExactly 'grid_test:center'
        $probe.center.unreal_cm | Should -Be @(0.0, 0.0, 2700.0)
        $probe.edge.cell_id | Should -BeExactly 'grid_test:edge'
        $probe.edge.unreal_cm | Should -Be @(10500.0, -500.0, 2700.0)
        $probe.source_radius.centimeters | Should -Be 1000.0
        $probe.non_overlapping_source_diameters | Should -BeTrue
        $probe.source_identity_sha256 | Should -BeExactly $script:sourceIdentity
    }

    It 'rejects a canonical terrain document whose bytes do not match the compile result' {
        $edgePath = Join-Path $script:compileRoot 'canonical\terrain\edge.json'
        Add-Content -LiteralPath $edgePath -Value ' ' -Encoding utf8
        {
            & $script:runner `
                -PackageRoot (Join-Path $script:testRoot 'package') `
                -ResultPath (Join-Path $script:projectRoot `
                    'Saved\Validation\WorldRealization\mesh-terrain-runtime-probe-test-unused.json') `
                -LogPath (Join-Path $script:testRoot 'unused.log') `
                -SourceIdentity $script:sourceIdentity `
                -CompileResultPath $script:compileResultPath `
                -CompilerProfilePath $script:compilerProfilePath `
                -RuntimeProfilePath $script:runtimeProfilePath `
                -ProbeDerivationPath (Join-Path $script:testRoot 'tampered.json') `
                -ProbeDerivationOnly
        } | Should -Throw '*failed hash authentication*'
    }
}
