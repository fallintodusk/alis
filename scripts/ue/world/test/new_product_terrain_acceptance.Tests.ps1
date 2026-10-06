# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

Describe 'Product terrain acceptance derivation' {
    BeforeAll {
        $script:projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        $script:runner = Join-Path $PSScriptRoot 'new_product_terrain_acceptance.ps1'
        $script:testRoot = Join-Path $script:projectRoot `
            ('tmp\world\product-terrain-acceptance-tests\' + [guid]::NewGuid().ToString('N'))
        $script:compileRoot = Join-Path $script:testRoot 'compile'
        New-Item -ItemType Directory -Path (Join-Path $script:compileRoot 'canonical\terrain') -Force | Out-Null

        function Write-TestJson([string]$Path, [object]$Value) {
            New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
            [IO.File]::WriteAllText(
                $Path,
                ($Value | ConvertTo-Json -Depth 12) + "`n",
                [Text.UTF8Encoding]::new($false))
        }

        function New-TestCell([string]$Id, [double[]]$Bounds, [double]$Base) {
            return [ordered]@{
                grid_id = 'grid_test'
                cell_id = $Id
                bounds = $Bounds
                sample_spacing = @(45.0, 45.0)
                height_quantization = 0.1
                vertical_provenance = [ordered]@{
                    source_accuracy_m = 1.0
                    sampling_quantization_residual_m = 0.05
                }
                core_samples = @(
                    @($Base, ($Base + 1.0), ($Base + 2.0)),
                    @(($Base + 1.0), ($Base + 2.0), ($Base + 3.0)),
                    @(($Base + 2.0), ($Base + 3.0), ($Base + 4.0))
                )
                surface_semantics = [ordered]@{
                    core_samples = [ordered]@{
                        hydro_transition = @(
                            @(0.0, 0.0, 0.0),
                            @(0.0, 0.5, 0.0),
                            @(0.0, 0.0, 0.0)
                        )
                    }
                }
            }
        }

        $documents = @(
            @{ name = 'left.json'; value = New-TestCell 'grid_test:left' @(-90.0, -45.0, 0.0, 45.0) 10.0 },
            @{ name = 'right.json'; value = New-TestCell 'grid_test:right' @(0.0, -45.0, 90.0, 45.0) 11.0 },
            @{ name = 'edge.json'; value = New-TestCell 'grid_test:edge' @(300.0, -45.0, 390.0, 45.0) 20.0 }
        )
        $outputs = @()
        foreach ($document in $documents) {
            $path = Join-Path $script:compileRoot ('canonical\terrain\' + $document.name)
            Write-TestJson $path $document.value
            $outputs += [ordered]@{
                path = 'canonical/terrain/' + $document.name
                sha256 = (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
        $script:compileResultPath = Join-Path $script:compileRoot 'compile_result.json'
        $script:compilerProfilePath = Join-Path $script:testRoot 'compiler.json'
        $script:runtimeProfilePath = Join-Path $script:testRoot 'runtime.json'
        $script:outputPath = Join-Path $script:testRoot 'acceptance.json'
        Write-TestJson $script:compileResultPath ([ordered]@{
                status = 'accepted'; profile_id = 'test_profile'; outputs = $outputs
            })
        Write-TestJson $script:compilerProfilePath ([ordered]@{
                profile_id = 'test_profile'
                engine_georeference_origin = @(0.0, 0.0)
                grid = [ordered]@{ vertical_origin_m = 0.0 }
            })
        Write-TestJson $script:runtimeProfilePath ([ordered]@{
                profile_id = 'test_runtime'; profile_kind = 'territory_product'; grid_id = 'grid_test'
                runtime_partition = [ordered]@{ loading_range_m = 80.0 }
            })
    }

    AfterAll {
        $owner = [IO.Path]::GetFullPath((Join-Path $script:projectRoot 'tmp\world')).TrimEnd('\', '/')
        $target = [IO.Path]::GetFullPath($script:testRoot)
        if ($target.StartsWith($owner + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    It 'derives authenticated semantic height points and a cross-cell navigation path' {
        & $script:runner -CompileResultPath $script:compileResultPath `
            -CompilerProfilePath $script:compilerProfilePath `
            -RuntimeProfilePath $script:runtimeProfilePath `
            -MapPackage '/ProjectWorldData/Generated/Test/L_Test' `
            -TerrainGeneratorId 'project_mesh_terrain:v1' -OutputPath $script:outputPath | Out-Null
        $result = Get-Content $script:outputPath -Raw | ConvertFrom-Json
        $result.terrain_expectations.collision_components | Should -Be 'navigation_relevant_pawn_blocking'
        $result.terrain_expectations.non_main_pass_helpers | Should -Be 'required_navigation_irrelevant'
        $result.terrain_expectations.route_endpoints | Should -Be 'navigation_relevant_pawn_blocking'
        @($result.height_probes).Count | Should -Be 6
        @($result.height_probes.kind | Sort-Object) | Should -Be @(
            'cell_boundary', 'center', 'high', 'hydro_transition', 'low', 'perimeter')
        $result.navigation.start_cell_id | Should -Not -Be $result.navigation.end_cell_id
        $generationRadius = [double]$result.navigation.generation_radius_cm
        $removalRadius = [double]$result.navigation.removal_radius_cm
        $generationRadius | Should -BeLessThan $removalRadius
    }

    It 'rejects a canonical terrain document that fails its compile-result hash' {
        $tampered = Get-Content $script:compileResultPath -Raw | ConvertFrom-Json
        $tampered.outputs[0].sha256 = '0' * 64
        $badResult = Join-Path $script:compileRoot 'bad-compile-result.json'
        Write-TestJson $badResult $tampered
        {
            & $script:runner -CompileResultPath $badResult `
                -CompilerProfilePath $script:compilerProfilePath `
                -RuntimeProfilePath $script:runtimeProfilePath `
                -MapPackage '/ProjectWorldData/Generated/Test/L_Test' `
                -TerrainGeneratorId 'project_mesh_terrain:v1' `
                -OutputPath (Join-Path $script:testRoot 'bad-acceptance.json') | Out-Null
        } | Should -Throw '*Canonical terrain hash failed*'
    }

    It 'rejects an undeclared terrain producer before writing an acceptance contract' {
        $unknownOutput = Join-Path $script:testRoot 'unknown-acceptance.json'
        {
            & $script:runner -CompileResultPath $script:compileResultPath `
                -CompilerProfilePath $script:compilerProfilePath `
                -RuntimeProfilePath $script:runtimeProfilePath `
                -MapPackage '/ProjectWorldData/Generated/Test/L_Test' `
                -TerrainGeneratorId 'unknown_terrain:v1' -OutputPath $unknownOutput | Out-Null
        } | Should -Throw '*exactly one producer descriptor for unknown_terrain:v1*'
        Test-Path -LiteralPath $unknownOutput | Should -BeFalse
    }
}
