#Requires -Version 5.1
# License terms: see repository root LICENSE.

function Assert-ProjectWorldShippingWater {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Invoke-ProjectWorldShippingWaterProof {
    param(
        [Parameter(Mandatory = $true)][string]$OperationId,
        [Parameter(Mandatory = $true)][string]$Experience,
        [Parameter(Mandatory = $true)][string]$MapPackage,
        [Parameter(Mandatory = $true)][string]$RuntimeProfileId,
        [Parameter(Mandatory = $true)][string]$RuntimeProfileHash,
        [Parameter(Mandatory = $true)][double[]]$Target,
        [int]$GameTimeoutSeconds = 720,
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$ResultPath,
        [Parameter(Mandatory = $true)][string]$ReferencePath,
        [Parameter(Mandatory = $true)][string]$RepeatPath,
        [Parameter(Mandatory = $true)][string]$ProductPath
    )
    $arguments = @(
        "-ProjectMenuPlayAutoExperience=$Experience",
        '-ProjectMenuPlayAutoMode=SinglePlayer',
        '-ProjectWorldShippingWaterProof',
        "-ProjectWorldShippingWaterOperation=$operationId",
        "-ProjectWorldShippingWaterResult=$ResultPath",
        "-ProjectWorldShippingWaterReference=$ReferencePath",
        "-ProjectWorldShippingWaterRepeat=$RepeatPath",
        "-ProjectWorldShippingWaterProduct=$ProductPath",
        "-ProjectWorldShippingWaterMap=$mapPackage",
        "-ProjectWorldShippingWaterRuntime=$RuntimeProfileId",
        "-ProjectWorldShippingWaterRuntimeHash=$RuntimeProfileHash",
        '-ProjectWorldShippingWaterMachine=rtx4070_primary',
        ('-ProjectWorldShippingWaterTarget={0},{1},0' -f `
            ($Target[0]).ToString('0.###', [Globalization.CultureInfo]::InvariantCulture), `
            ($Target[1]).ToString('0.###', [Globalization.CultureInfo]::InvariantCulture)),
        '-ResX=2560', '-ResY=1440', '-Windowed', '-ForceRes',
        '-RenderOffScreen', '-novsync', '-unattended', '-nosplash', '-NoMessaging'
    )
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments `
        -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit($GameTimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw 'Shipping Water proof process exceeded the bounded timeout.'
    }
    return $process.ExitCode
}

function Read-ProjectWorldShippingWaterProof {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$OperationId,
        [Parameter(Mandatory = $true)][string]$MapPackage,
        [Parameter(Mandatory = $true)][string]$RuntimeProfileId,
        [Parameter(Mandatory = $true)][string]$RuntimeProfileHash
    )
    Assert-ProjectWorldShippingWater (Test-Path -LiteralPath $Path -PathType Leaf) `
        'Shipping Water proof receipt is missing.'
    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $start = @($receipt.start_player_location_cm)
    $target = @($receipt.requested_water_target_cm)
    $allowedTargetError = [double]$receipt.allowed_target_xy_error_cm
    $requestedTargetDistance = [double]$receipt.requested_target_distance_cm
    $minimumRequiredTravel = $requestedTargetDistance - $allowedTargetError
    $computedTargetDistance = if ($start.Count -eq 3 -and $target.Count -eq 3) {
        [Math]::Sqrt(
            [Math]::Pow([double]$target[0] - [double]$start[0], 2) +
            [Math]::Pow([double]$target[1] - [double]$start[1], 2))
    }
    else { [double]::NaN }
    Assert-ProjectWorldShippingWater (
        [string]$receipt.status -ceq 'accepted' -and
        [string]$receipt.operation_id -ceq $operationId -and
        [string]$receipt.map_package -ceq $mapPackage -and
        [string]$receipt.runtime_profile -ceq $RuntimeProfileId -and
        [string]$receipt.runtime_profile_sha256 -ceq $RuntimeProfileHash -and
        [string]$receipt.build_configuration -ceq 'Shipping' -and
        [string]$receipt.gpu_adapter -ceq 'NVIDIA GeForce RTX 4070' -and
        [string]$receipt.rhi -ceq 'D3D12' -and
        [bool]$receipt.project_loading_provenance -and
        [bool]$receipt.preview_flight -and
        [string]$receipt.pawn_class -ceq '/Script/ProjectCharacter.DefinitionCharacter' -and
        [string]$receipt.input_method -ceq `
            'APlayerController::InputKey/FInputKeyEventArgs::CreateSimulated' -and
        [int]$receipt.input_event_count -gt 0 -and
        [int]$receipt.waypoints_reached -ge 1 -and
        [double]$receipt.ascent_cm -gt 4000.0 -and
        $requestedTargetDistance -gt $allowedTargetError -and
        [Math]::Abs($requestedTargetDistance - $computedTargetDistance) -le 1.0 -and
        [double]$receipt.horizontal_displacement_cm + 1.0 -ge $minimumRequiredTravel -and
        $null -ne $receipt.PSObject.Properties['target_xy_error_cm'] -and
        [double]$receipt.target_xy_error_cm -le $allowedTargetError -and
        [string]$receipt.capture_source -ceq 'SCS_SceneColorSceneDepth' -and
        [string]$receipt.capture_encoding -ceq 'visible_water_depth_mask_v1' -and
        [string]$receipt.capture_projection -ceq 'orthographic' -and
        [double]$receipt.capture_ortho_width_cm -eq 20000.0 -and
        [bool]$receipt.same_capture_session -and
        -not [string]::IsNullOrWhiteSpace([string]$receipt.capture_session_id) -and
        [string]$receipt.reference_capture_session_id -ceq [string]$receipt.capture_session_id -and
        [string]$receipt.repeat_capture_session_id -ceq [string]$receipt.capture_session_id -and
        [int]$receipt.capture_session_written_image_count -eq 2 -and
        [bool]$receipt.reference_captured -and
        [bool]$receipt.repeat_captured -and
        [string]$receipt.final_color_capture_source -ceq 'SCS_FinalColorLDR' -and
        [string]$receipt.final_color_capture_projection -ceq 'perspective' -and
        [double]$receipt.final_color_field_of_view_degrees -eq 70.0 -and
        -not [string]::IsNullOrWhiteSpace([string]$receipt.final_color_capture_session_id) -and
        [int]$receipt.final_color_capture_session_written_image_count -eq 1 -and
        [bool]$receipt.final_color_captured) `
        'Shipping Water proof receipt failed its product/input/capture contract.'
    Assert-ProjectWorldShippingWater (Test-Path -LiteralPath ([string]$receipt.reference_screenshot) -PathType Leaf) `
        'Shipping Water reference screenshot is missing.'
    Assert-ProjectWorldShippingWater (Test-Path -LiteralPath ([string]$receipt.repeat_screenshot) -PathType Leaf) `
        'Shipping Water repeat screenshot is missing.'
    Assert-ProjectWorldShippingWater (Test-Path -LiteralPath ([string]$receipt.final_color_screenshot) -PathType Leaf) `
        'Shipping Water final-color screenshot is missing.'
    return $receipt
}

