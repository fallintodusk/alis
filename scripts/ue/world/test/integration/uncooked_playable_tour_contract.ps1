# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#Requires -Version 5.1

. (Join-Path $PSScriptRoot '../performance/project_world_performance_evidence.ps1')

function Assert-ProjectWorldUncookedGameplayReceipt {
    param(
        [Parameter(Mandatory)][object]$Receipt,
        [Parameter(Mandatory)][object]$Correctness,
        [Parameter(Mandatory)][object]$Operation,
        [Parameter(Mandatory)][string]$OperationId,
        [Parameter(Mandatory)][string]$CorrectnessPath,
        [Parameter(Mandatory)][int]$ProcessExitCode
    )
    foreach ($row in @($Receipt, $Correctness)) {
        if ([string]$row.operation_id -cne $OperationId -or
            [string]$row.map_package -cne $Operation.Map -or
            [string]$row.runtime_profile -cne $Operation.Runtime -or
            [string]$row.runtime_profile_sha256 -cne $Operation.RuntimeHash -or
            $row.gameplay_interaction_required -isnot [bool] -or
            $row.gameplay_interaction_required -ne $Operation.InteractionRequired) {
            throw 'Uncooked operation, map, runtime or interaction policy mismatch.'
        }
        if ($Operation.InteractionRequired -and $row.gameplay_interaction -ne $true) {
            throw 'The ordinary product route did not prove gameplay interaction.'
        }
    }
    if ($Receipt.requires_cooked_data -isnot [bool] -or $Receipt.requires_cooked_data -or
        $Receipt.play_in_editor -isnot [bool] -or -not $Receipt.play_in_editor -or
        [string]$Receipt.correctness_status -cne 'accepted' -or
        [string]$Correctness.status -cne 'accepted' -or
        [string]$Correctness.game_mode -cne '/Script/ProjectSinglePlay.SinglePlayerGameMode' -or
        [string]$Correctness.pawn_class -cne '/Script/ProjectCharacter.DefinitionCharacter' -or
        -not ([IO.Path]::GetFullPath([string]$Receipt.correctness_receipt)).Equals(
            [IO.Path]::GetFullPath($CorrectnessPath), [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Uncooked gameplay envelope or product-route binding was rejected.'
    }
    $outcome = Get-ProjectWorldPerformanceChildOutcome -Receipt $Receipt `
        -ProcessExitCode $ProcessExitCode
    if (-not $outcome.valid) {
        $errors = @($Receipt.errors | ForEach-Object { "$($_.code): $($_.message)" })
        throw ('Uncooked native child rejected: ' + ($errors -join '; '))
    }
    foreach ($field in @('project_loading_provenance', 'possessed_player', 'normal_movement',
            'terrain_collision', 'road_collision', 'building_collision', 'center_unloaded_at_edge',
            'edge_loaded', 'center_reloaded', 'preview_flight_restored')) {
        if ($Correctness.$field -isnot [bool] -or -not $Correctness.$field) {
            throw "Uncooked product-route proof failed: $field"
        }
    }
    foreach ($field in @('playable_tour', 'center_cell_streaming_cycle', 'pause_menu_opened',
            'pause_menu_closed', 'collision_blocked_descent', 'collision_slide')) {
        if ($Receipt.$field -isnot [bool] -or -not $Receipt.$field) {
            throw "Uncooked real-input proof failed: $field"
        }
    }
    $minimums = @{
        sample_count = 300.0; input_event_count = 1.0; waypoints_reached = 3.0
        ascent_cm = 4000.01; descent_cm = 3000.01; horizontal_displacement_cm = 100000.01
        slide_displacement_cm = 100.0; frame_p95_ms = 0.000001; gpu_p95_ms = 0.000001
    }
    foreach ($field in $minimums.Keys) {
        $value = $Receipt.$field
        if (-not (Test-ProjectWorldPerformanceNumber $value) -or
            [double]::IsNaN([double]$value) -or [double]::IsInfinity([double]$value) -or
            [double]$value -lt $minimums[$field]) {
            throw "Uncooked numeric proof failed: $field"
        }
    }
    if ($Receipt.streaming_failures -ne 0 -or
        $Receipt.final_center_arrival_radius_cm -ne $Operation.Radius -or
        [string]$Receipt.input_method -cne
            'APlayerController::InputKey/FInputKeyEventArgs::CreateSimulated') {
        throw 'Uncooked radius, streaming or actual input path was rejected.'
    }
    $cycles = @($Receipt.initial_center_cell_ids | Where-Object {
            $Receipt.unloaded_center_cell_ids -contains $_ -and
            $Receipt.reloaded_center_cell_ids -contains $_
        })
    if ($cycles.Count -eq 0) { throw 'No identical center cell completed the streaming cycle.' }
    # The native hard-gate code also covers invalid samples and streaming. Those
    # conditions were independently checked above before allowing FPS-only diagnostics.
}
