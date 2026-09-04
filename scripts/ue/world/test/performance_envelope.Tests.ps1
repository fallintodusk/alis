$ErrorActionPreference = 'Stop'

Describe 'Packaged performance measurement envelope' {
    BeforeAll {
        $projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        $evidenceHelper = Join-Path $projectRoot `
            'scripts\ue\world\test\performance\project_world_performance_evidence.ps1'
        . $evidenceHelper
        $scriptPaths = @(
            'scripts\ue\world\test\performance\run_kazan_playable_tour.ps1',
            'scripts\ue\world\test\performance\run_kazan_runtime_profile_tournament.ps1',
            'scripts\ue\gameplay\test\run_kazan_survival_proof.ps1'
        ) | ForEach-Object { Join-Path $projectRoot $_ }
    }

    It 'rejects a missing envelope property before value conversion' {
        $receipt = [pscustomobject][ordered]@{
            vsync = 0
            max_fps = 0.0
            smooth_frame_rate = $false
            use_fixed_frame_rate = $false
            fixed_frame_rate = 30.0
            use_fixed_time_step = $false
            fixed_delta_time_seconds = 1.0 / 30.0
            benchmarking = $false
            dynamic_resolution_operation_mode = 0
            dynamic_resolution_status = 'disabled'
            dynamic_resolution_enabled = $false
        }
        (Test-ProjectWorldPerformanceEnvelopeReceipt -Receipt $receipt) | Should -BeTrue
        $receipt.PSObject.Properties.Remove('vsync')
        (Test-ProjectWorldPerformanceEnvelopeReceipt -Receipt $receipt) | Should -BeFalse
    }

    It 'uses UE boolean syntax to disable VSync in every packaged performance owner' {
        foreach ($scriptPath in $scriptPaths) {
            $source = Get-Content -LiteralPath $scriptPath -Raw
            $source | Should -Match "'-novsync'"
            $source | Should -Not -Match "'-VSync=0'"
        }
    }

    It 'routes every consumer through strict shared envelope authentication' {
        $source = Get-Content -LiteralPath $scriptPaths[0] -Raw
        $source | Should -Match 'Test-ProjectWorldPerformanceEnvelopeReceipt'
        $helperSource = Get-Content -LiteralPath $evidenceHelper -Raw
        foreach ($field in @(
                'vsync',
                'max_fps',
                'smooth_frame_rate',
                'use_fixed_frame_rate',
                'fixed_frame_rate',
                'use_fixed_time_step',
                'fixed_delta_time_seconds',
                'benchmarking',
                'dynamic_resolution_operation_mode',
                'dynamic_resolution_status',
                'dynamic_resolution_enabled')) {
            $helperSource | Should -Match ([regex]::Escape("'$field'"))
        }
        foreach ($scriptPath in $scriptPaths) {
            (Get-Content -LiteralPath $scriptPath -Raw) |
                Should -Match 'Test-ProjectWorldPerformanceEnvelopeReceipt'
        }
    }
}
