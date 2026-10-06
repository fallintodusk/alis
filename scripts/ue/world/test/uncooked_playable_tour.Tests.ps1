# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#Requires -Version 5.1

Describe 'Uncooked real-map gameplay admission' {
    BeforeAll {
        $repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
        . (Join-Path $repoRoot 'scripts/ue/world/test/integration/uncooked_playable_tour_contract.ps1')
        . (Join-Path $repoRoot 'scripts/ue/world/test/performance/project_world_product_route_arguments.ps1')
        $script:operation = [pscustomobject]@{
            Map = '/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase'
            Runtime = 'manhattan_showcase_512_1536_v1'; RuntimeHash = 'a' * 64
            InteractionRequired = $false; Radius = 500.0
        }
        $script:parameters = @{
            Operation = $script:operation; OperationId = 'fixture'
            CorrectnessPath = (Join-Path $repoRoot 'tmp/world/preflight-fixture/correctness.json')
            ProcessExitCode = 0
        }
        $script:correctness = [pscustomobject]@{
            status = 'accepted'; operation_id = 'fixture'; map_package = $operation.Map
            runtime_profile = $operation.Runtime; runtime_profile_sha256 = $operation.RuntimeHash
            game_mode = '/Script/ProjectSinglePlay.SinglePlayerGameMode'
            pawn_class = '/Script/ProjectCharacter.DefinitionCharacter'
            project_loading_provenance = $true; possessed_player = $true
            normal_movement = $true; terrain_collision = $true; road_collision = $true
            building_collision = $true; center_unloaded_at_edge = $true; edge_loaded = $true
            center_reloaded = $true; preview_flight_restored = $true
            gameplay_interaction_required = $false
        }
        $script:base = @{
            status = 'accepted'; errors = @(); operation_id = 'fixture'
            map_package = $operation.Map; runtime_profile = $operation.Runtime
            runtime_profile_sha256 = $operation.RuntimeHash
            correctness_receipt = $parameters.CorrectnessPath; correctness_status = 'accepted'
            requires_cooked_data = $false; play_in_editor = $true; gameplay_interaction_required = $false
            playable_tour = $true; final_center_arrival_radius_cm = 500.0
            sample_count = 500; frame_p95_ms = 12.0; gpu_p95_ms = 8.0
            streaming_failures = 0; center_cell_streaming_cycle = $true
            initial_center_cell_ids = @('center'); unloaded_center_cell_ids = @('center')
            reloaded_center_cell_ids = @('center')
            input_method = 'APlayerController::InputKey/FInputKeyEventArgs::CreateSimulated'
            input_event_count = 100; pause_menu_opened = $true; pause_menu_closed = $true
            waypoints_reached = 4; ascent_cm = 7500.0; descent_cm = 6500.0
            horizontal_displacement_cm = 1500000.0
            collision_blocked_descent = $true; collision_slide = $true
            slide_displacement_cm = 900.0
        }
    }

    It 'admits gameplay and labels native FPS rejection as diagnostic only' {
        Assert-ProjectWorldUncookedGameplayReceipt -Receipt ([pscustomobject]$base) `
            -Correctness $correctness @parameters
        $receipt = @{} + $base
        $receipt.status = 'rejected'; $receipt.frame_p95_ms = 25.0
        $receipt.errors = @([pscustomobject]@{ code = 'performance_hard_gate_failed' })
        $caseParameters = @{} + $parameters; $caseParameters.ProcessExitCode = 10
        { Assert-ProjectWorldUncookedGameplayReceipt -Receipt ([pscustomobject]$receipt) `
            -Correctness $correctness @caseParameters } | Should -Not -Throw
    }

    It 'refuses real movement, residency, radius and envelope failures' {
        foreach ($change in @(
                @{ collision_slide = $false }, @{ slide_displacement_cm = 0.0 },
                @{ final_center_arrival_radius_cm = 2500.0 }, @{ requires_cooked_data = $true },
                @{ play_in_editor = $false }, @{ center_cell_streaming_cycle = $false }, @{ streaming_failures = 1 },
                @{ reloaded_center_cell_ids = @('different') }, @{ input_event_count = 0 },
                @{ pause_menu_opened = $false }, @{ collision_blocked_descent = $false },
                @{ correctness_receipt = 'wrong.json' }, @{ runtime_profile_sha256 = 'b' * 64 })) {
            $receipt = @{} + $base
            foreach ($key in $change.Keys) { $receipt[$key] = $change[$key] }
            { Assert-ProjectWorldUncookedGameplayReceipt -Receipt ([pscustomobject]$receipt) `
                -Correctness $correctness @parameters } | Should -Throw
        }
    }

    It 'does not mistake the shared hard-gate error for FPS-only refusal' {
        foreach ($change in @(@{ sample_count = 1 }, @{ gpu_p95_ms = 0.0 },
                @{ gpu_p95_ms = [double]::NaN }, @{ frame_p95_ms = [double]::PositiveInfinity },
                @{ streaming_failures = 1 })) {
            $receipt = @{} + $base
            $receipt.status = 'rejected'; $receipt.frame_p95_ms = 25.0
            $receipt.errors = @([pscustomobject]@{ code = 'performance_hard_gate_failed' })
            foreach ($key in $change.Keys) { $receipt[$key] = $change[$key] }
            $caseParameters = @{} + $parameters; $caseParameters.ProcessExitCode = 10
            { Assert-ProjectWorldUncookedGameplayReceipt -Receipt ([pscustomobject]$receipt) `
                -Correctness $correctness @caseParameters } | Should -Throw
        }
    }

    It 'requires the real product route and its explicit interaction policy' {
        $broken = $correctness.PSObject.Copy(); $broken.project_loading_provenance = $false
        { Assert-ProjectWorldUncookedGameplayReceipt -Receipt ([pscustomobject]$base) `
            -Correctness $broken @parameters } | Should -Throw
        $broken = $correctness.PSObject.Copy(); $broken.gameplay_interaction_required = $true
        { Assert-ProjectWorldUncookedGameplayReceipt -Receipt ([pscustomobject]$base) `
            -Correctness $broken @parameters } | Should -Throw
    }

    It 'reports the native startup error before missing traversal evidence' {
        $receipt = @{} + $base
        $receipt.status = 'rejected'
        $receipt.center_cell_streaming_cycle = $false
        $receipt.errors = @([pscustomobject]@{ code = 'performance_start_invalid'; message = 'Wrong viewport size.' })
        { Assert-ProjectWorldUncookedGameplayReceipt -Receipt ([pscustomobject]$receipt) `
            -Correctness $correctness @parameters } | Should -Throw '*performance_start_invalid*'
    }

    It 'preserves Kazan Development traversal and Shipping correctness through the real wrapper' {
        $runner = Join-Path $repoRoot 'scripts/ue/world/test/performance/run_kazan_playable_tour.ps1'
        $tokens = $null; $parseErrors = $null
        $ast = [Management.Automation.Language.Parser]::ParseFile($runner, [ref]$tokens, [ref]$parseErrors)
        $definition = $ast.Find({ param($node)
                $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
                $node.Name -ceq 'Invoke-PlayableTourGame'
            }, $true)
        Set-Item Function:Invoke-PlayableTourGame -Value $definition.Body.GetScriptBlock()
        $script:mapPackage = '/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory'
        $script:runtimeProfileHash = 'a' * 64; $script:edgeArgument = '1,2,10000'
        $script:GameTimeoutSeconds = 720
        $script:launched = @()
        Mock Start-Process {
            param($FilePath, $ArgumentList)
            $script:launched = @($ArgumentList)
            $process = [pscustomobject]@{ ExitCode = 0 }
            $process | Add-Member ScriptMethod WaitForExit { param($milliseconds) return $true }
            return $process
        }
        $caseParameters = @{
            Executable = (Join-Path $repoRoot 'tmp/world/preflight-fixture/fixture.exe')
            Configuration = 'Development'; RunOperationId = 'fixture'
            CorrectnessPath = 'correctness.json'; LogPath = 'game.log'
            PerformancePath = 'performance.json'; CsvPath = 'diagnostic.csv'
            SamplePath = 'samples.csv'; ScreenshotPath = 'slide.png'
        }
        Invoke-PlayableTourGame @caseParameters | Should -Be 0
        $script:launched | Should -Contain '-ProjectWorldPlayableTour'
        $script:launched | Should -Not -Contain '-ProjectWorldPlayableTourPreciseCenterReturn'
        $script:launched | Should -Not -Contain '-ProjectWorldProductRouteSkipInteraction'
        $caseParameters.Configuration = 'Shipping'
        Invoke-PlayableTourGame @caseParameters | Should -Be 0
        $script:launched | Should -Contain '-ProjectWorldProductRouteGate'
        $script:launched | Should -Not -Contain '-ProjectWorldProductPerformanceGate'
        $script:launched | Should -Not -Contain '-ProjectWorldPlayableTour'
    }
}
