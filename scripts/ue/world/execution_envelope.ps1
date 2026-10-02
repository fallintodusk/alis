# Execution envelope for World pipeline commandlets.
#
# The execution mode is part of correctness, not a launch detail. Mesh Terrain realization
# invokes Epic's builder and material-backed section compilation, so the production Apply route
# declares rendering Required. Read-only structural commands may explicitly declare Disabled.
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
    # Epic's rendering-dependent commandlets opt in this way; it makes
    # FApp::CanEverRender() true inside a commandlet.
    return , @('-AllowCommandletRendering')
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
            throw 'Execution envelope violation: a render-required World step cannot run with -NullRHI.'
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
