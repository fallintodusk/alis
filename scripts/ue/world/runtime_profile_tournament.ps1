#Requires -Version 5.1
# License terms: see repository root LICENSE.

function Assert-ProjectWorldTournamentIdentity {
    param(
        [Parameter(Mandatory = $true)][object[]]$Candidates,
        [Parameter(Mandatory = $true)][string[]]$ExpectedProfiles,
        [string]$RequiredGpu = 'NVIDIA GeForce RTX 4070',
        [string]$RequiredBuildConfiguration = 'Development'
    )

    if ($Candidates.Count -ne $ExpectedProfiles.Count) {
        throw "Tournament candidate count differs from the frozen profile set."
    }
    $actualProfiles = @($Candidates | ForEach-Object { [string]$_.profile_id } | Sort-Object)
    $expected = @($ExpectedProfiles | Sort-Object)
    if (($actualProfiles -join '|') -cne ($expected -join '|')) {
        throw "Tournament candidates differ from the frozen profile set."
    }
    foreach ($candidate in $Candidates) {
        if (-not $candidate.identity_accepted) {
            throw "Candidate identity was not accepted: $($candidate.profile_id)"
        }
        if ([string]$candidate.gpu_adapter -cne $RequiredGpu) {
            throw "Candidate did not run on the required physical adapter: $($candidate.profile_id)"
        }
        if ([string]$candidate.rhi -cne 'D3D12' -or
            [string]$candidate.build_configuration -cne $RequiredBuildConfiguration) {
            throw "Candidate did not use the frozen instrumented/D3D12 surface: $($candidate.profile_id)"
        }
        if ([string]$candidate.quality_preset -cne 'High' -or
            [int]$candidate.resolution_x -ne 2560 -or
            [int]$candidate.resolution_y -ne 1440) {
            throw "Candidate did not use the frozen High 2560x1440 surface: $($candidate.profile_id)"
        }
        if (-not [bool]$candidate.render_offscreen) {
            throw "Candidate did not record the monitor-independent 1440p render surface: $($candidate.profile_id)"
        }
    }
    $executableHashes = @($Candidates | ForEach-Object { [string]$_.executable_sha256 } | Sort-Object -Unique)
    if ($executableHashes.Count -ne 1 -or $executableHashes[0] -notmatch '^[0-9a-f]{64}$') {
        throw "Tournament candidates did not reuse one byte-identical packaged executable."
    }
    return $executableHashes[0]
}

function Select-ProjectWorldRuntimeProfileWinner {
    param([Parameter(Mandatory = $true)][object[]]$Candidates)

    $eligible = @($Candidates | Where-Object {
        $_.identity_accepted -and
        [string]$_.correctness_status -ceq 'accepted' -and
        [string]$_.performance_status -ceq 'accepted'
    })
    if ($eligible.Count -eq 0) {
        throw "No runtime profile passed the frozen correctness and performance gates."
    }
    return @($eligible | Sort-Object `
        @{ Expression = { [double]$_.frame_p99_ms }; Ascending = $true }, `
        @{ Expression = { [Int64]$_.peak_process_physical_bytes }; Ascending = $true }, `
        @{ Expression = { [Int64]$_.activation_transitions }; Ascending = $true }, `
        @{ Expression = { [string]$_.profile_id }; Ascending = $true })[0]
}
