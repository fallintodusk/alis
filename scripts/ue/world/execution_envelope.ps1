# Execution envelope for World pipeline commandlets.
#
# The execution mode is part of the input to correctness, not a launch detail. UE 5.8 gates
# Landscape edit-layer composition on FApp::CanEverRender():
#
#   LandscapeEditLayers.cpp  ALandscape::PrepareTextureResources
#     if (Info == nullptr || !FApp::CanEverRender()) { return false; }
#
# Under -NullRHI that guard fails, the RDG batched merge never runs, and the final Landscape
# heightmap silently keeps its initialized flat contents (raw height 0 = -256 m) while the
# Generated Base edit layer looks perfect. Every structural gate passes; the shipped terrain
# is flat. See docs/agents/scientific_debugging.md rule 6.
#
# Each pipeline step therefore DECLARES its requirement and this helper owns the flags:
#
#   Disabled : proven render-independent      -> -NullRHI allowed
#   Required : uses textures/RDG/render merge -> -NullRHI FORBIDDEN,
#                                                -AllowCommandletRendering required
#
# Do not hand-write -NullRHI or -AllowCommandletRendering in a World commandlet call.

Set-StrictMode -Version Latest

$script:ProjectWorldRenderingModes = @('Required', 'Disabled')

function Get-ProjectWorldExecutionEnvelopeArguments {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet('Required', 'Disabled')]
        [string]$Rendering
    )

    if ($Rendering -eq 'Disabled') {
        return , @('-NullRHI')
    }
    # Epic's own rendering-dependent commandlets (e.g. World Partition builders) opt in this
    # way; it makes FApp::CanEverRender() true inside a commandlet.
    #
    # r.VolumetricFog=0 is an envelope setting, not terrain behavior. The Landscape edit-layer
    # merge creates a scene view (FMergeRenderContext::RenderComponentIds ->
    # FRendererModule::CreateAndInitSingleView), which reaches volumetric fog setup and
    # crashes in a commandlet at territory scale:
    #   EXCEPTION_ACCESS_VIOLATION in ShouldRenderVolumetricFog (VolumetricFog.cpp:1355)
    # That predicate short-circuits on GVolumetricFog (the r.VolumetricFog CVar) BEFORE it
    # dereferences Scene->ExponentialFogs, so disabling the feature avoids the faulting path.
    # Volumetric fog contributes nothing to heightmap composition. Disable only this one
    # proven-crashing feature - do not speculatively strip renderer features.
    return , @('-AllowCommandletRendering', '-ini:Engine:[SystemSettings]:r.VolumetricFog=0')
}

function Assert-ProjectWorldExecutionEnvelope {
    <#
        Fail fast on a contradictory envelope. This is the guard that stops a future edit
        from silently re-adding -NullRHI to a render-required step.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet('Required', 'Disabled')]
        [string]$Rendering,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [string[]]$Arguments
    )

    $hasNullRhi = @($Arguments | Where-Object { $_ -ieq '-NullRHI' }).Count -gt 0
    $hasAllowRendering = @($Arguments | Where-Object { $_ -ieq '-AllowCommandletRendering' }).Count -gt 0

    if ($Rendering -eq 'Required') {
        if ($hasNullRhi) {
            throw 'Execution envelope violation: a render-required World step cannot run with -NullRHI. UE skips Landscape edit-layer composition when FApp::CanEverRender() is false, which silently produces flat terrain.'
        }
        if (-not $hasAllowRendering) {
            throw 'Execution envelope violation: a render-required World step must pass -AllowCommandletRendering.'
        }
    }
    else {
        if ($hasAllowRendering) {
            throw 'Execution envelope violation: a renderless World step must not pass -AllowCommandletRendering.'
        }
        if (-not $hasNullRhi) {
            throw 'Execution envelope violation: a renderless World step must pass -NullRHI.'
        }
    }
}
