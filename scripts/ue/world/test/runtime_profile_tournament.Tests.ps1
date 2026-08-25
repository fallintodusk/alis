#Requires -Version 5.1
# License terms: see repository root LICENSE.

Describe 'Project World runtime profile tournament' {
    BeforeAll {
        $worldRoot = Split-Path -Parent $PSScriptRoot
        . (Join-Path $worldRoot 'runtime_profile_tournament.ps1')

        function New-TournamentCandidate {
            param(
                [string]$Profile,
                [double]$P99,
                [Int64]$Memory,
                [Int64]$Churn,
                [string]$PerformanceStatus = 'accepted'
            )
            return [pscustomobject]@{
                profile_id = $Profile
                identity_accepted = $true
                correctness_status = 'accepted'
                performance_status = $PerformanceStatus
                frame_p99_ms = $P99
                peak_process_physical_bytes = $Memory
                activation_transitions = $Churn
                executable_sha256 = 'a' * 64
                gpu_adapter = 'NVIDIA GeForce RTX 4070'
                rhi = 'D3D12'
                build_configuration = 'Development'
                quality_preset = 'High'
                resolution_x = 2560
                resolution_y = 1440
                render_offscreen = $true
            }
        }
    }

    It 'uses hard gates then p99, memory, and churn in that order' {
        $candidates = @(
            New-TournamentCandidate '128' 12.0 300 1
            New-TournamentCandidate '256' 10.0 300 9
            New-TournamentCandidate '512' 10.0 200 8
            New-TournamentCandidate 'rejected' 1.0 1 1 'rejected'
        )
        (Select-ProjectWorldRuntimeProfileWinner -Candidates $candidates).profile_id |
            Should -BeExactly '512'

        $candidates[2].activation_transitions = 10
        $candidates[1].peak_process_physical_bytes = 200
        (Select-ProjectWorldRuntimeProfileWinner -Candidates $candidates).profile_id |
            Should -BeExactly '256'
    }

    It 'rejects mixed executable identity before selection' {
        $candidates = @(
            New-TournamentCandidate '128' 12.0 300 1
            New-TournamentCandidate '256' 10.0 300 9
            New-TournamentCandidate '512' 10.0 200 8
        )
        $candidates[2].executable_sha256 = 'b' * 64
        {
            Assert-ProjectWorldTournamentIdentity `
                -Candidates $candidates -ExpectedProfiles @('128', '256', '512')
        } | Should -Throw '*byte-identical packaged executable*'
    }
}
