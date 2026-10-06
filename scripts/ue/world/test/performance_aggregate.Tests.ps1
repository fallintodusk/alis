# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

$ErrorActionPreference = 'Stop'

Describe 'Packaged World performance aggregation' {
    BeforeAll {
        $script:projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        $helper = Join-Path $script:projectRoot `
            'scripts\ue\world\test\performance\project_world_performance_evidence.ps1'
        . $helper
		$script:runner = Join-Path $script:projectRoot `
			'scripts\ue\world\test\performance\run_kazan_playable_tour.ps1'

		function Import-RunnerFunction {
			param([string]$Name)
			$tokens = $null
			$parseErrors = $null
			$ast = [Management.Automation.Language.Parser]::ParseFile(
				$script:runner, [ref]$tokens, [ref]$parseErrors)
			@($parseErrors).Count | Should -Be 0
			$definition = $ast.Find({
					param($node)
					$node -is [Management.Automation.Language.FunctionDefinitionAst] -and
						$node.Name -ceq $Name
				}, $true)
			$null -ne $definition | Should -BeTrue
			Set-Item -Path "Function:script:$Name" -Value $definition.Body.GetScriptBlock()
		}

		Import-RunnerFunction -Name 'Assert-PlayableTour'
		Import-RunnerFunction -Name 'Measure-PlayableTourHostLoad'

        function Write-TestSamples {
            param([string]$Path, [int]$SlowCount = 0, [double]$SlowFrame = 31.0)
            $lines = [Collections.Generic.List[string]]::new()
            $lines.Add('FrameTime,GameThreadTime,RenderThreadTime,GPUTime')
            for ($index = 0; $index -lt 300; ++$index) {
                $frame = if ($index -lt $SlowCount) { $SlowFrame } else { 10.0 }
                $lines.Add(('{0},5,7,8' -f $frame.ToString(
                            '0.0', [Globalization.CultureInfo]::InvariantCulture)))
            }
            [IO.File]::WriteAllLines($Path, $lines, [Text.UTF8Encoding]::new($false))
        }

        function New-TestChild {
            param([int]$Index, [int]$SlowCount = 0, [double]$SlowFrame = 31.0)
            $root = Join-Path $script:testRoot ("run-{0:D2}" -f $Index)
            New-Item -ItemType Directory -Path $root -Force | Out-Null
            $samplePath = Join-Path $root 'performance.samples.csv'
            $richCsvPath = Join-Path $root 'performance.csv'
            $receiptPath = Join-Path $root 'performance.json'
            $correctnessPath = Join-Path $root 'product-route.json'
            Write-TestSamples -Path $samplePath -SlowCount $SlowCount -SlowFrame $SlowFrame
            [IO.File]::WriteAllText($richCsvPath, 'diagnostic')
            $samples = @(Import-ProjectWorldPerformanceSamples -Path $samplePath -ExpectedCount 300)
            $statistics = Get-ProjectWorldPerformanceStatistics -Samples $samples
            $accepted = [double]$statistics.frame_p95_ms -le 16.67
            $errors = [object[]]@()
            if (-not $accepted) {
                $errors = @([ordered]@{ code = 'performance_hard_gate_failed'; message = 'test' })
            }
            $receipt = [ordered]@{
                operation_id = "test-run-$Index"
                status = if ($accepted) { 'accepted' } else { 'rejected' }
                map_package = '/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase'
                runtime_profile = 'manhattan_showcase_512_1536_v1'
                runtime_profile_sha256 = $script:runtimeHash
                executable = $script:executable
                build_configuration = 'Development'
                correctness_status = 'accepted'
                correctness_receipt = $correctnessPath
                gameplay_interaction_required = $false
                machine_profile_id = 'rtx4070_primary'
                gpu_adapter = 'NVIDIA GeForce RTX 4070'
                rhi = 'D3D12'
                quality_preset = 'High'
                resolution_x = 2560
                resolution_y = 1440
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
                playable_tour = $true
                streaming_failures = 0
                raw_sample_capture = $samplePath
                csv_capture = $richCsvPath
                sample_count = $statistics.sample_count
                frame_p95_ms = $statistics.frame_p95_ms
                frame_p99_ms = $statistics.frame_p99_ms
                frame_max_ms = $statistics.frame_max_ms
                game_p95_ms = $statistics.game_p95_ms
                render_p95_ms = $statistics.render_p95_ms
                gpu_p95_ms = $statistics.gpu_p95_ms
                peak_process_physical_bytes = 1000
                peak_gpu_local_bytes = 2000
                errors = $errors
            }
            $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receiptPath `
                -Encoding UTF8
            return [pscustomobject][ordered]@{
                ExpectedOperationId = "test-run-$Index"
                ExpectedProcessExitCode = 0
                ReceiptPath = $receiptPath
                CorrectnessPath = $correctnessPath
                SamplePath = $samplePath
                RichCsvPath = $richCsvPath
            }
        }

        function Invoke-TestAggregate {
            param(
                [object[]]$Children,
                [string]$ExecutableHash = $script:executableHash,
                [double]$HostCpuLoadPercent = 0.0,
                [double]$HostGpuLoadPercent = 0.0,
                [object[]]$HostLoadWindows,
                [string]$ExpectedMapPackage,
                [string]$ExpectedRuntimeProfile,
                [switch]$RequireCorrectnessBinding,
                [switch]$RequireNonInteractivePolicy
            )
            if (-not $PSBoundParameters.ContainsKey('HostLoadWindows')) {
                $HostLoadWindows = @(1..3 | ForEach-Object {
                    $run = $_
                    @('before', 'after') | ForEach-Object {
                        [pscustomobject]@{
                            phase = ('run-{0:D2}-{1}' -f $run, $_)
                            cpu_percent = $HostCpuLoadPercent
                            gpu_percent = $HostGpuLoadPercent
                        }
                    }
                })
            }
            return New-ProjectWorldPerformanceAggregate -Children $Children `
                -OperationId 'test-operation' -SourceRevision $script:revision `
                -SourceStateSha256 $script:sourceHash `
                -RuntimeProfileSha256 $script:runtimeHash `
                -ExpectedExecutable $script:executable `
                -ExpectedExecutableSha256 $ExecutableHash `
                -ExpectedPackage $script:package `
                -ExpectedPackageSha256 $script:packageHash `
                -HostLoadWindows $HostLoadWindows `
                -ExpectedMapPackage $ExpectedMapPackage `
                -ExpectedRuntimeProfile $ExpectedRuntimeProfile `
                -RequireCorrectnessBinding:$RequireCorrectnessBinding `
                -RequireNonInteractivePolicy:$RequireNonInteractivePolicy
        }
    }

    BeforeEach {
        $script:ownerRoot = Join-Path $script:projectRoot 'tmp\world\performance_aggregate'
        $script:testRoot = Join-Path $script:ownerRoot ([Guid]::NewGuid().ToString('N'))
        $script:package = Join-Path $script:testRoot 'package'
        $script:executable = Join-Path $script:package 'ProjectGame.exe'
        New-Item -ItemType Directory -Path $script:package -Force | Out-Null
        [IO.File]::WriteAllText($script:executable, 'exact executable bytes')
        $script:executableHash = Get-ProjectWorldPerformanceFileHash -Path $script:executable
        $script:packageHash = ('b' * 64) -join ''
        $script:sourceHash = ('c' * 64) -join ''
        $script:runtimeHash = ('d' * 64) -join ''
        $script:revision = ('e' * 40) -join ''
    }

    AfterEach {
        $owner = [IO.Path]::GetFullPath($script:ownerRoot).TrimEnd('\', '/')
        $target = [IO.Path]::GetFullPath($script:testRoot)
        if ($target.StartsWith(
                $owner + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase) -and
            (Test-Path -LiteralPath $target)) {
            Remove-Item -LiteralPath $target -Recurse -Force
        }
        if ((Test-Path -LiteralPath $script:ownerRoot) -and
            @(Get-ChildItem -LiteralPath $script:ownerRoot -Force).Count -eq 0) {
            Remove-Item -LiteralPath $script:ownerRoot -Force
        }
    }

    It 'pools three identical good populations with the nearest-rank algorithm' {
        $children = @(1..3 | ForEach-Object { New-TestChild -Index $_ })
        $aggregate = Invoke-TestAggregate -Children $children
        $aggregate.status | Should -BeExactly 'accepted'
        $aggregate.execution_count | Should -Be 3
        $aggregate.total_sample_count | Should -Be 900
        $aggregate.frame_p95_ms | Should -Be 10.0
        @($aggregate.children).Count | Should -Be 3
    }

    It 'accepts a modestly slow aggregate under sustained bounded host load' {
        $children = @(1..3 | ForEach-Object {
                New-TestChild -Index $_ -SlowCount 300 -SlowFrame 17.5
            })
        $aggregate = Invoke-TestAggregate -Children $children `
            -HostCpuLoadPercent 35 -HostGpuLoadPercent 5
        $aggregate.status | Should -BeExactly 'accepted'
        $aggregate.base_frame_p95_budget_ms | Should -Be 16.67
        $aggregate.host_cpu_load_percent | Should -Be 35.0
        $aggregate.host_gpu_load_percent | Should -Be 5.0
        $aggregate.host_load_allowance_percent | Should -Be 10.0
        $aggregate.frame_p95_budget_ms | Should -Be 18.337
        $aggregate.frame_p95_ms | Should -Be 17.5
        { Invoke-TestAggregate -Children $children -HostCpuLoadPercent 101 } |
            Should -Throw
    }

    It 'keeps the base budget when host load does not exceed the baseline' {
        $children = @(1..3 | ForEach-Object {
                New-TestChild -Index $_ -SlowCount 300 -SlowFrame 17.0
            })
        $aggregate = Invoke-TestAggregate -Children $children `
            -HostCpuLoadPercent 20 -HostGpuLoadPercent 20
        $aggregate.status | Should -BeExactly 'rejected'
        $aggregate.host_load_allowance_percent | Should -Be 0.0
        $aggregate.frame_p95_budget_ms | Should -Be 16.67
    }

    It 'samples the same host CPU and GPU load for both territory campaigns' {
        Mock Get-CimInstance { [pscustomobject]@{ PercentProcessorTime = 34.0 } }
        Mock nvidia-smi { $global:LASTEXITCODE = 0; '7'; '12' }
        Mock Start-Sleep {}
        $load = Measure-ProjectWorldPerformanceHostLoad
        $load.cpu_percent | Should -Be 34.0
        $load.gpu_percent | Should -Be 12.0
        Should -Invoke Get-CimInstance -Times 3
        Should -Invoke nvidia-smi -Times 3
    }

	It 'uses the least-loaded window and rejects incomplete or invalid load evidence' {
		$children = @(1..3 | ForEach-Object {
				New-TestChild -Index $_ -SlowCount 300 -SlowFrame 17.5
			})
		$windows = @(1..3 | ForEach-Object {
			$run = $_
			@('before', 'after') | ForEach-Object {
				[pscustomobject]@{ phase = ('run-{0:D2}-{1}' -f $run, $_);
					cpu_percent = 35.0; gpu_percent = 5.0 }
			}
		})
		$windows[3].cpu_percent = 21.0
		$aggregate = Invoke-TestAggregate -Children $children -HostLoadWindows $windows
		$aggregate.host_load_percent | Should -Be 21.0
		$aggregate.host_load_allowance_percent | Should -Be 1.0
		$aggregate.status | Should -BeExactly 'rejected'
		{ Invoke-TestAggregate -Children $children -HostLoadWindows @($windows[0..4]) } |
			Should -Throw
		$windows[3].cpu_percent = [double]::NaN
		{ Invoke-TestAggregate -Children $children -HostLoadWindows $windows } |
			Should -Throw
	}

    It 'keeps every slow frame and is invariant to child order' {
        $children = @(
            (New-TestChild -Index 1),
            (New-TestChild -Index 2),
            (New-TestChild -Index 3 -SlowCount 60)
        )
        $forward = Invoke-TestAggregate -Children $children
        $reverse = Invoke-TestAggregate -Children @($children[2], $children[1], $children[0])
        $forward.status | Should -BeExactly 'rejected'
        $forward.total_sample_count | Should -Be 900
        $forward.frame_p95_ms | Should -Be 31.0
        $forward.frame_p95_ms | Should -Not -Be 17.0
        $reverse.frame_p95_ms | Should -Be $forward.frame_p95_ms
        $reverse.frame_p99_ms | Should -Be $forward.frame_p99_ms
    }

    It 'keeps a performance-only rejection with either observed normal exit code' {
        $children = @(
            (New-TestChild -Index 1),
            (New-TestChild -Index 2),
            (New-TestChild -Index 3 -SlowCount 60)
        )
        $aggregate = Invoke-TestAggregate -Children $children
        $aggregate.children[2].status | Should -BeExactly 'rejected'
        $aggregate.children[2].process_exit_code | Should -Be 0

        $children[2].ExpectedProcessExitCode = 10
        (Invoke-TestAggregate -Children $children).children[2].process_exit_code |
            Should -Be 10

        $children[2].ExpectedProcessExitCode = 1
        { Invoke-TestAggregate -Children $children } | Should -Throw
    }

    It 'fails closed on incomplete or unauthenticated child evidence' {
        $children = @(1..3 | ForEach-Object { New-TestChild -Index $_ })
        { Invoke-TestAggregate -Children @($children[0], $children[1]) } | Should -Throw
        { Invoke-TestAggregate -Children $children -ExecutableHash (('f' * 64) -join '') } |
            Should -Throw

        $receipt = Get-Content -LiteralPath $children[0].ReceiptPath -Raw | ConvertFrom-Json
        $receipt.PSObject.Properties.Remove('vsync')
        $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $children[0].ReceiptPath `
            -Encoding UTF8
        { Invoke-TestAggregate -Children $children } | Should -Throw
    }

    It 'binds Manhattan map, runtime, correctness receipt, and non-interactive policy' {
        $children = @(1..3 | ForEach-Object { New-TestChild -Index $_ })
        $options = @{
            Children = $children
            ExpectedMapPackage = '/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase'
            ExpectedRuntimeProfile = 'manhattan_showcase_512_1536_v1'
            RequireCorrectnessBinding = $true
            RequireNonInteractivePolicy = $true
        }
        (Invoke-TestAggregate @options).status | Should -BeExactly 'accepted'
        foreach ($field in @('map_package', 'runtime_profile', 'correctness_receipt',
                'gameplay_interaction_required')) {
            $receipt = Get-Content -LiteralPath $children[0].ReceiptPath -Raw | ConvertFrom-Json
            $original = $receipt.PSObject.Properties[$field].Value
            $receipt.PSObject.Properties[$field].Value = if ($field -ceq
                'gameplay_interaction_required') { $true } else { 'wrong' }
            $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $children[0].ReceiptPath `
                -Encoding UTF8
            { Invoke-TestAggregate @options } | Should -Throw
            $receipt.PSObject.Properties[$field].Value = $original
            $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $children[0].ReceiptPath `
                -Encoding UTF8
        }
    }

    It 'rejects non-performance child failures rather than pooling them' {
        $children = @(1..3 | ForEach-Object { New-TestChild -Index $_ })
        $receipt = Get-Content -LiteralPath $children[0].ReceiptPath -Raw | ConvertFrom-Json
        $receipt.status = 'rejected'
        $receipt.errors = @([pscustomobject]@{ code = 'streaming_failure'; message = 'test' })
        $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $children[0].ReceiptPath `
            -Encoding UTF8
        { Invoke-TestAggregate -Children $children } | Should -Throw
    }

    It 'pins exactly three executions in the existing release runner' {
		$runner = Get-Content -LiteralPath $script:runner -Raw
        $runner | Should -Match '\$developmentIndex -le 3'
        $runner | Should -Match 'New-ProjectWorldPerformanceAggregate'
        $arguments = Get-Content -LiteralPath (Join-Path (Split-Path $script:runner) 'project_world_product_route_arguments.ps1') -Raw
        $arguments | Should -Match 'ProjectWorldPerformanceSamples'
        $runner | Should -Match 'Development pooled performance rejected:'
        $runner | Should -Not -Match 'nvidia-smi\s+--id=0'
        $runner | Should -Match '\$beforeLoad = Measure-PlayableTourHostLoad'
        $runner | Should -Match '\$afterLoad = Measure-PlayableTourHostLoad'
        $runner | Should -Match '-HostLoadWindows @\(\$hostLoadWindows\)'
    }

    It 'hashes immutable package payload while excluding owned runtime state' {
        $packageRoot = Join-Path $script:testRoot 'package'
        $binaryPath = Join-Path $packageRoot 'Windows\Alis\Binaries\Win64\Alis.exe'
        $runtimeStatePath = Join-Path $packageRoot `
            'Windows\Alis\LocalAppData\Alis\State\latest.json'
        $gameSettingsPath = Join-Path $packageRoot `
            'Windows\Alis\Saved\Config\Windows\GameUserSettings.ini'
        $engineManifestPath = Join-Path $packageRoot `
            'Windows\Engine\Saved\Config\Windows\Manifest.ini'
        New-Item -ItemType Directory -Path (Split-Path -Parent $binaryPath) -Force | Out-Null
        New-Item -ItemType Directory -Path (Split-Path -Parent $runtimeStatePath) -Force | Out-Null
        New-Item -ItemType Directory -Path (Split-Path -Parent $gameSettingsPath) -Force | Out-Null
        New-Item -ItemType Directory -Path (Split-Path -Parent $engineManifestPath) -Force | Out-Null
        [IO.File]::WriteAllText($binaryPath, 'payload-a')
        $before = Get-ProjectWorldPackagePayloadDigest -Path $packageRoot

        [IO.File]::WriteAllText($runtimeStatePath, 'runtime-state-a')
        Get-ProjectWorldPackagePayloadDigest -Path $packageRoot | Should -BeExactly $before

        [IO.File]::WriteAllText($runtimeStatePath, 'runtime-state-b')
        Get-ProjectWorldPackagePayloadDigest -Path $packageRoot | Should -BeExactly $before

        [IO.File]::WriteAllText($gameSettingsPath, 'runtime-settings')
        Get-ProjectWorldPackagePayloadDigest -Path $packageRoot | Should -BeExactly $before

        [IO.File]::WriteAllText($engineManifestPath, 'runtime-engine-manifest')
        Get-ProjectWorldPackagePayloadDigest -Path $packageRoot | Should -BeExactly $before

        [IO.File]::WriteAllText($binaryPath, 'payload-b')
        Get-ProjectWorldPackagePayloadDigest -Path $packageRoot | Should -Not -BeExactly $before
    }
}
