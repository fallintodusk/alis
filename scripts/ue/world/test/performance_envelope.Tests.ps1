$ErrorActionPreference = 'Stop'

Describe 'Packaged performance measurement envelope' {
    BeforeAll {
        $projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        $evidenceHelper = Join-Path $projectRoot `
            'scripts\ue\world\test\performance\project_world_performance_evidence.ps1'
        . $evidenceHelper
        . (Join-Path $PSScriptRoot 'performance/project_world_product_route_arguments.ps1')
        $scriptPaths = @(
            'scripts\ue\world\test\performance\run_kazan_playable_tour.ps1',
            'scripts\ue\world\test\performance\run_kazan_runtime_profile_tournament.ps1',
            'scripts\ue\gameplay\test\run_kazan_survival_proof.ps1'
        ) | ForEach-Object { Join-Path $projectRoot $_ }
        $launchers = @(
            @{ Path = $scriptPaths[0]; Name = 'Invoke-PlayableTourGame' },
            @{ Path = $scriptPaths[1]; Name = 'Invoke-TournamentGame' },
            @{ Path = $scriptPaths[2]; Name = 'Invoke-SurvivalRoute' },
            @{ Path = (Join-Path $PSScriptRoot 'performance/run_manhattan_showcase_prototype.ps1'); Name = 'Invoke-ManhattanShowcaseGame' }
        )
        foreach ($launcher in $launchers) {
            $tokens = $null; $errors = $null
            $ast = [Management.Automation.Language.Parser]::ParseFile($launcher.Path, [ref]$tokens, [ref]$errors)
            @($errors).Count | Should -Be 0
            $name = $launcher.Name
            $definition = $ast.Find({ param($node)
                $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name
            }, $true)
            $definition | Should -Not -BeNullOrEmpty
            Set-Item -Path "Function:script:$name" -Value $definition.Body.GetScriptBlock()
        }
        $script:mapPackage = '/Fixture/L_World'
        $script:runtimeProfileHash = 'a' * 64
        $script:edgeArgument = '0,0,100'
        $script:experienceId = 'Fixture'
        $script:GameTimeoutSeconds = 1
        $script:operationId = 'fixture'
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
        $receipt.vsync = 1
        (Test-ProjectWorldPerformanceEnvelopeReceipt -Receipt $receipt) | Should -BeFalse
        $receipt.vsync = 0
        $receipt.PSObject.Properties.Remove('vsync')
        (Test-ProjectWorldPerformanceEnvelopeReceipt -Receipt $receipt) | Should -BeFalse
    }

    It 'launches every actual owner uncapped, offscreen and hidden through native process arguments' {
        Mock Start-Process {
            $script:capturedArguments = @($ArgumentList)
            $script:capturedWindowStyle = [string]$WindowStyle
            $process = [pscustomobject]@{ ExitCode = 0; Id = 0 }
            $process | Add-Member ScriptMethod WaitForExit { param($Timeout) return $true }
            return $process
        }
        foreach ($launcher in $launchers) {
            $parameters = @{ Executable = (Join-Path $projectRoot 'tmp/never-launch-game.exe') }
            switch ($launcher.Name) {
                'Invoke-TournamentGame' {
                    $parameters += @{ OperationId = 'fixture'; Profile = [pscustomobject]@{profile_id='fixture'}
                        ProfileHash = 'a' * 64; CorrectnessPath = 'fixture.json'; PerformancePath = 'frame.json'
                        CsvPath = 'frame.csv'; SamplePath = 'samples.csv' }
                }
                'Invoke-SurvivalRoute' {
                    $parameters += @{ Configuration = 'Development'; Route = 'success'; ResultPath = 'fixture.json'
                        ScreenshotPath = 'view.png'; LogPath = 'fixture.log'; PerformancePath = 'frame.json'
                        CsvPath = 'frame.csv'; SamplePath = 'samples.csv'; PerformanceScreenshotPath = 'frame.png' }
                }
                default {
                    $parameters += @{ Configuration = 'Development'; RunOperationId = 'fixture'; CorrectnessPath = 'fixture.json'
                        LogPath = 'fixture.log'; PerformancePath = 'frame.json'; CsvPath = 'frame.csv'
                        SamplePath = 'samples.csv'; ScreenshotPath = 'view.png' }
                }
            }
            $script:capturedArguments = @(); $script:capturedWindowStyle = ''
            (& $launcher.Name @parameters) | Should -Be 0
            $script:capturedArguments | Should -Contain '-novsync'
            $script:capturedArguments | Should -Not -Contain '-VSync=0'
            $script:capturedArguments | Should -Contain '-RenderOffScreen'
            $script:capturedWindowStyle | Should -BeExactly 'Hidden'
        }
        Should -Invoke Start-Process -Exactly -Times 4
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
