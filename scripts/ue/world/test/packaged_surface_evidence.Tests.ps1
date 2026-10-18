#Requires -Version 5.1
# License terms: see repository root LICENSE.

Describe 'Packaged surface evidence admission' {
    BeforeAll {
        $script:projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        $runnerRoot = Join-Path $PSScriptRoot 'performance'
        . (Join-Path $runnerRoot 'project_world_building_evidence.ps1')
        foreach ($entry in @(
                @{ File = 'run_kazan_playable_tour.ps1'; Names = @('Assert-PlayableTour', 'Read-PlayableTourCorrectness') },
                @{ File = 'run_manhattan_showcase_prototype.ps1'; Names = @('Assert-ManhattanShowcase', 'Read-ManhattanShowcaseCorrectness') })) {
            $tokens = $null
            $errors = $null
            $ast = [Management.Automation.Language.Parser]::ParseFile(
                (Join-Path $runnerRoot $entry.File), [ref]$tokens, [ref]$errors)
            @($errors).Count | Should -Be 0
            foreach ($name in $entry.Names) {
                $definition = $ast.Find({param($node)
                    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name
                }, $true)
                Set-Item -Path "Function:script:$name" -Value $definition.Body.GetScriptBlock()
            }
        }
        $script:testRoot = Join-Path $script:projectRoot (
            'tmp\world\packaged_surface_evidence_tests\' + [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $script:testRoot -Force | Out-Null
        $script:image = Join-Path $script:testRoot 'product.png'
        [IO.File]::WriteAllText($script:image, 'fixture image identity')
        $script:path = Join-Path $script:testRoot 'receipt.json'
        $script:runtimeProfileHash = 'a' * 64
    }

    AfterAll {
        Remove-Item -LiteralPath $script:testRoot -Recurse -Force
    }

    It 'refuses Kazan Shipping collision presence without named wall and roof rays' {
        $script:mapPackage = '/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory'
        $receipt = @{
            status = 'accepted'; operation_id = 'surface-proof'; map_package = $script:mapPackage
            runtime_profile = 'kazan_territory_512_1536_v1'; runtime_profile_sha256 = $script:runtimeProfileHash
            build_configuration = 'Shipping'; game_mode = '/Script/ProjectSinglePlay.SinglePlayerGameMode'
            pawn_class = '/Script/ProjectCharacter.DefinitionCharacter'; screenshot = $script:image
            project_loading_provenance = $true; possessed_player = $true; grounded_player = $true
            normal_movement = $true; terrain_collision = $true; road_collision = $true
            building_collision = $true; gameplay_interaction = $true
            center_unloaded_at_edge = $true; edge_loaded = $true; center_reloaded = $true
            preview_flight_restored = $true; gpu_adapter = 'NVIDIA GeForce RTX 4070'; rhi = 'D3D12'
        }
        $receipt | ConvertTo-Json | Set-Content -LiteralPath $script:path -Encoding UTF8
        { Read-PlayableTourCorrectness -Path $script:path -Configuration Shipping `
            -ExpectedOperationId 'surface-proof' } | Should -Throw '*building*'
    }

    It 'refuses Manhattan Shipping collision presence without named wall and roof rays' {
        $script:mapPackage = '/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase'
        $receipt = @{
            status = 'accepted'; operation_id = 'surface-proof'; map_package = $script:mapPackage
            runtime_profile = 'manhattan_showcase_512_1536_v1'; runtime_profile_sha256 = $script:runtimeProfileHash
            build_configuration = 'Shipping'; game_mode = '/Script/ProjectSinglePlay.SinglePlayerGameMode'
            pawn_class = '/Script/ProjectCharacter.DefinitionCharacter'; screenshot = $script:image
            project_loading_provenance = $true; possessed_player = $true; normal_movement = $true
            terrain_collision = $true; road_collision = $true; building_collision = $true
            center_unloaded_at_edge = $true; edge_loaded = $true; center_reloaded = $true
            preview_flight_restored = $true; gameplay_interaction_required = $false
        }
        $receipt | ConvertTo-Json | Set-Content -LiteralPath $script:path -Encoding UTF8
        { Read-ManhattanShowcaseCorrectness -Path $script:path -Configuration Shipping `
            -ExpectedOperationId 'surface-proof' } | Should -Throw '*building*'
    }

    It 'admits six named rays with source-plane error reported separately from current-reference tolerance' {
        $samples = @(foreach ($index in 0..2) {
            foreach ($kind in 'wall', 'roof') {
                [pscustomobject]@{kind = $kind; mesh = "/ProjectWorldData/Generated/Territory/Buildings/SM_$index"
                    source_triangle = $index; cooked_hit_cm = @(0, 0, 5.228); reference_error_cm = 0.0
                    ray_query_status = 'hit_named_component'; ray_query_returned_hit = $true
                    cooked_hit_normal = @(0, 0, 1); capsule_query_status = 'blocked_named_component'
                    capsule_hit_mesh = "/ProjectWorldData/Generated/Territory/Buildings/SM_$index"
                    capsule_blocking_hit = $true; capsule_start_penetrating = $false; capsule_hit_time = 0.5
                    capsule_hit_count = 1; capsule_other_blocking_hit_count = 0; capsule_initial_penetration_count = 0
                    capsule_hit_cm = @(0, 0, 5.228); capsule_hit_normal = @(0, 0, 1)
                    capsule_source_plane_error_cm = 5.228; capsule_reference_plane_error_cm = 0.0}
            }
        })
        $receipt = [pscustomobject]@{building_surface_evidence = [pscustomobject]@{
                schema = 'project-world-building-surface-evidence:v2'; status = 'accepted'
                capsule_query = 'UWorld::ComponentSweepMulti'; capsule_radius_cm = 42.0; capsule_half_height_cm = 96.0
                capsule_pawn_class = '/Script/ProjectCharacter.DefinitionCharacter'; capsule_query_channel = 2
                reference_sha256 = 'a' * 64; reference_tolerance_cm = 0.05
                maximum_reference_error_cm = 0.0; maximum_source_error_cm = 5.228; samples = $samples
            }}
        { Assert-ProjectWorldBuildingEvidence -Receipt $receipt } | Should -Not -Throw
        $receipt.building_surface_evidence.reference_tolerance_cm =
            ('{"reference_tolerance_cm":0.050000000000000003}' | ConvertFrom-Json).reference_tolerance_cm
        { Assert-ProjectWorldBuildingEvidence -Receipt $receipt } | Should -Not -Throw
        $receipt.building_surface_evidence.reference_tolerance_cm = 0.051
        { Assert-ProjectWorldBuildingEvidence -Receipt $receipt } | Should -Throw '*building*'
        $receipt.building_surface_evidence.reference_tolerance_cm = 0.05
        $samples[1].capsule_query_status = 'no_blocking_hit'
        { Assert-ProjectWorldBuildingEvidence -Receipt $receipt } | Should -Throw '*building*'
        $samples[1].capsule_query_status = 'blocked_named_component'
        $samples[1].capsule_hit_normal = @(0, 0, 0)
        { Assert-ProjectWorldBuildingEvidence -Receipt $receipt } | Should -Throw '*building*'
        $samples[1].capsule_hit_normal = @(0, 0, 1)
        $samples[1].reference_error_cm = 0.06
        { Assert-ProjectWorldBuildingEvidence -Receipt $receipt } | Should -Throw '*building*'
    }
}
