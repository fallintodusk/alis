# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

$ErrorActionPreference = 'Stop'

Describe 'Manhattan existing Development package performance' {
    BeforeAll {
        $script:projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        $script:runner = Join-Path $script:projectRoot `
            'scripts\ue\world\test\performance\run_manhattan_showcase_prototype.ps1'
        . (Join-Path $script:projectRoot `
            'scripts\ue\world\test\performance\project_world_performance_evidence.ps1')
        . (Join-Path $script:projectRoot `
            'scripts\ue\world\test\performance\project_world_product_route_arguments.ps1')
        $tokens = $null
        $errors = $null
        $script:ast = [Management.Automation.Language.Parser]::ParseFile(
            $script:runner, [ref]$tokens, [ref]$errors)
        @($errors).Count | Should -Be 0
        foreach ($name in @('Assert-ManhattanShowcase', 'Test-ManhattanSamePath',
                'Get-ManhattanShowcaseExecutable', 'Get-ManhattanPackageSourceIdentity',
                'Assert-ManhattanPerformanceChild', 'Assert-ManhattanNormalExit',
                'Assert-ManhattanEvidenceInventory', 'Invoke-ManhattanExistingPackagePerformance',
                'Invoke-ManhattanShowcaseGame')) {
            $definition = $script:ast.Find({
                    param($node)
                    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
                        $node.Name -ceq $name
                }, $true)
            $null -ne $definition | Should -BeTrue
            Set-Item -Path "Function:script:$name" -Value $definition.Body.GetScriptBlock()
            if ($name -ceq 'Assert-ManhattanPerformanceChild') {
                Set-Item -Path 'Function:script:Assert-ManhattanPerformanceChildDirect' `
                    -Value $definition.Body.GetScriptBlock()
            }
            if ($name -ceq 'Invoke-ManhattanShowcaseGame') {
                Set-Item -Path 'Function:script:Invoke-ManhattanShowcaseGameDirect' `
                    -Value $definition.Body.GetScriptBlock()
            }
        }
        function Invoke-ManhattanShowcasePackage { throw 'Must never run in this test.' }
        function Read-ManhattanShowcaseCorrectness { throw 'Must be mocked in this test.' }
        $script:runtimeProfile = Join-Path $script:projectRoot `
            'Plugins\World\ProjectWorldData\Data\Runtime\manhattan_showcase_512_1536_v1.json'
        $script:mapPackage = '/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase'
        $script:experienceId = 'ManhattanShowcase'
        $script:edgeArgument = '651041,511455,60000'
        $script:GameTimeoutSeconds = 720
        $script:runtimeProfileHash = (Get-FileHash -LiteralPath $script:runtimeProfile `
            -Algorithm SHA256).Hash.ToLowerInvariant()
        $script:sourceRevision = (& git -C $script:projectRoot rev-parse HEAD).Trim()
        $script:sourceState = @(& python (Join-Path $script:projectRoot `
                    'scripts\ue\package\prepare_release.py') source-state `
                --source-root $script:projectRoot)[0]
    }

    BeforeEach {
        $script:ownerRoot = Join-Path $script:projectRoot `
            'tmp\world\manhattan_existing_package_test'
        $script:testRoot = Join-Path $script:ownerRoot ([Guid]::NewGuid().ToString('N'))
        $script:package = Join-Path $script:testRoot 'package'
        $script:executable = Join-Path $script:package `
            'Windows\Alis\Binaries\Win64\Alis.exe'
        $script:evidenceRoot = Join-Path $script:testRoot 'evidence'
        $script:runId = 'fixture'
        $script:operationId = 'manhattan_fixture'
        New-Item -ItemType Directory -Path (Split-Path -Parent $script:executable) `
            -Force | Out-Null
        [IO.File]::WriteAllText($script:executable, 'fixture executable')
        [IO.File]::WriteAllText((Join-Path $script:package 'package_summary.txt'),
            "SourceRevision=$script:sourceRevision`nSourceStateSha256=$script:sourceState`n")
        $script:packageHash = Get-ProjectWorldPackagePayloadDigest -Path $script:package
        $script:executableHash = (Get-FileHash -LiteralPath $script:executable `
            -Algorithm SHA256).Hash.ToLowerInvariant()
        $script:gameCalls = 0
        Mock Measure-ProjectWorldPerformanceHostLoad {
            [pscustomobject]@{ cpu_percent = 0.0; gpu_percent = 0.0 }
        }
        Mock Read-ManhattanShowcaseCorrectness {
            param($Path, $Configuration, $ExpectedOperationId)
            [pscustomobject]@{
                status = 'accepted'
                screenshot = (Join-Path (Split-Path -Parent $Path) 'product.png')
            }
        }
        Mock Assert-ManhattanPerformanceChild {
            param($Path, $CorrectnessPath, $Operation, $Executable,
                $SamplePath, $CsvPath, $ScreenshotPath, $ProcessExitCode)
            [pscustomobject]@{
                status = 'accepted'
                playable_tour_screenshot = $ScreenshotPath
            }
        }
        Mock New-ProjectWorldPerformanceAggregate {
            if (@($Children).Count -ne 3 -or @($HostLoadWindows).Count -ne 6 -or
                $ExpectedMapPackage -cne $script:mapPackage -or
                $ExpectedRuntimeProfile -cne 'manhattan_showcase_512_1536_v1' -or
                $ExpectedExecutableSha256 -cne $script:executableHash -or
                $ExpectedPackageSha256 -cne $script:packageHash -or
                -not $RequireCorrectnessBinding -or -not $RequireNonInteractivePolicy) {
                throw 'Manhattan aggregate call omitted a required identity or child.'
            }
            [pscustomobject]@{
                status = 'accepted'
                acceptance_reason = ''
                execution_count = @($Children).Count
                base_frame_p95_budget_ms = 16.67
                frame_p95_ms = 10.0
                children = @($Children)
                peak_process_physical_bytes = 1000
                peak_gpu_local_bytes = 2000
            }
        }
        Mock Invoke-ManhattanShowcaseGame {
            [IO.File]::WriteAllText($LogPath, "LogExit: Exiting.`nLog file closed")
            foreach ($path in @($CorrectnessPath, $PerformancePath, $CsvPath,
                    $SamplePath, $ScreenshotPath,
                    (Join-Path (Split-Path -Parent $CorrectnessPath) 'product.png'))) {
                [IO.File]::WriteAllText($path, "fixture $path")
            }
            $script:gameCalls++
            return 0
        }
        Mock Invoke-ManhattanShowcasePackage { throw 'Existing package mode must not cook.' }
    }

    AfterEach {
        Remove-Variable -Name AlisManhattanTestArguments -Scope Global -ErrorAction SilentlyContinue
        $owner = [IO.Path]::GetFullPath($script:ownerRoot).TrimEnd('\', '/')
        $target = [IO.Path]::GetFullPath($script:testRoot)
        if ($target.StartsWith($owner + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase) -and
            (Test-Path -LiteralPath $target)) {
            Remove-Item -LiteralPath $target -Recurse -Force
        }
    }

    It 'uses one existing package and executable for exactly three children without cooking' {
        $receiptPath = Invoke-ManhattanExistingPackagePerformance `
            -PackageRoot $script:package -ExpectedPackageHash $script:packageHash `
            -ExpectedExecutableHash $script:executableHash
        $script:gameCalls | Should -Be 3
        Should -Invoke Invoke-ManhattanShowcasePackage -Times 0
        Should -Invoke New-ProjectWorldPerformanceAggregate -Times 1
        $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
        $receipt.status | Should -BeExactly 'accepted'
        $receipt.source_revision | Should -BeExactly $script:sourceRevision
        $receipt.package_payload_sha256 | Should -BeExactly $script:packageHash
        $receipt.executable_sha256 | Should -BeExactly $script:executableHash
        $receipt.map_package | Should -BeExactly $script:mapPackage
        $receipt.gameplay_interaction_required | Should -BeFalse
        $receipt.performance.execution_count | Should -Be 3
        $receipt.performance.base_frame_p95_budget_ms | Should -Be 16.67
        @($receipt.artifacts.PSObject.Properties.Name).Count | Should -Be 21
        @($receipt.artifact_sha256.PSObject.Properties.Name).Count | Should -Be 21
        { Assert-ManhattanEvidenceInventory -ReceiptPath $receiptPath } | Should -Not -Throw
        foreach ($name in @('run-01', 'run-02', 'run-03')) {
            foreach ($suffix in @('correctness', 'product_screenshot', 'performance',
                    'samples', 'csv', 'playable_screenshot', 'log')) {
                $key = "${name}_$suffix"
                $path = [string]$receipt.artifacts.$key
                (Test-Path -LiteralPath $path -PathType Leaf) | Should -BeTrue
                $receipt.artifact_sha256.$key | Should -BeExactly `
                    (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
        foreach ($key in @('run-01_correctness', 'run-01_product_screenshot')) {
            $path = [string]$receipt.artifacts.$key
            $original = [IO.File]::ReadAllBytes($path)
            try {
                [IO.File]::AppendAllText($path, 'changed')
                { Assert-ManhattanEvidenceInventory -ReceiptPath $receiptPath } | Should -Throw
            }
            finally {
                [IO.File]::WriteAllBytes($path, $original)
            }
        }
    }

    It 'rejects a changed package payload after the first child' {
        Mock Invoke-ManhattanShowcaseGame {
            [IO.File]::WriteAllText($LogPath, "LogExit: Exiting.`nLog file closed")
            [IO.File]::WriteAllText($script:executable, 'changed executable')
            $script:gameCalls++
            return 0
        }
        { Invoke-ManhattanExistingPackagePerformance -PackageRoot $script:package `
                -ExpectedPackageHash $script:packageHash `
                -ExpectedExecutableHash $script:executableHash } | Should -Throw
        $script:gameCalls | Should -Be 1
        Should -Invoke Invoke-ManhattanShowcasePackage -Times 0
    }

    It 'rejects a child without a normal process exit before starting another' {
        Mock Invoke-ManhattanShowcaseGame {
            [IO.File]::WriteAllText($LogPath, 'process ended without LogExit')
            $script:gameCalls++
            return 0
        }
        { Invoke-ManhattanExistingPackagePerformance -PackageRoot $script:package `
                -ExpectedPackageHash $script:packageHash `
                -ExpectedExecutableHash $script:executableHash } | Should -Throw
        $script:gameCalls | Should -Be 1
    }

    It 'rejects incomplete and fatal terminal logs' {
        $path = Join-Path $script:testRoot 'terminal.log'
        foreach ($content in @('LogExit: Exiting.',
                "LogExit: Exiting.`nLog file closed`nFatal error:",
                "LogExit: Exiting.`nLog file closed`nAssertion failed:")) {
            [IO.File]::WriteAllText($path, $content)
            { Assert-ManhattanNormalExit -LogPath $path -ChildName 'fixture' } | Should -Throw
        }
    }

    It 'rejects the wrong initial package or executable identity before launch' {
        { Invoke-ManhattanExistingPackagePerformance -PackageRoot $script:package `
                -ExpectedPackageHash ('f' * 64) } | Should -Throw
        { Invoke-ManhattanExistingPackagePerformance -PackageRoot $script:package `
                -ExpectedExecutableHash ('f' * 64) } | Should -Throw
        $script:gameCalls | Should -Be 0
    }

    It 'rejects missing or ambiguous package source identity' {
        $summaryPath = Join-Path $script:package 'package_summary.txt'
        [IO.File]::WriteAllText($summaryPath, 'SourceRevision=invalid')
        { Get-ManhattanPackageSourceIdentity -PackageRoot $script:package } | Should -Throw
        [IO.File]::WriteAllText($summaryPath,
            "SourceRevision=$script:sourceRevision`n" +
            "SourceRevision=invalid`n" +
            "SourceStateSha256=$script:sourceState`n")
        { Get-ManhattanPackageSourceIdentity -PackageRoot $script:package } | Should -Throw
        [IO.File]::WriteAllText($summaryPath,
            "SourceRevision=$script:sourceRevision`n" +
            "SourceRevision=$script:sourceRevision`n" +
            "SourceStateSha256=$script:sourceState`n")
        { Get-ManhattanPackageSourceIdentity -PackageRoot $script:package } | Should -Throw
    }

    It 'refuses wrong Manhattan identities, policy, or playable input proof' {
        $path = Join-Path $script:testRoot 'performance.json'
        $correctness = Join-Path $script:testRoot 'product-route.json'
        $sample = Join-Path $script:testRoot 'samples.csv'
        $csv = Join-Path $script:testRoot 'rich.csv'
        $screenshot = Join-Path $script:testRoot 'tour.png'
        [IO.File]::WriteAllText($screenshot, 'fixture')
        $base = [ordered]@{
            status = 'accepted'; frame_p95_ms = 10.0; errors = @()
            operation_id = 'test-child'; map_package = $script:mapPackage
            runtime_profile = 'manhattan_showcase_512_1536_v1'
            runtime_profile_sha256 = $script:runtimeProfileHash
            build_configuration = 'Development'; correctness_status = 'accepted'
            correctness_receipt = $correctness; executable = $script:executable
            machine_profile_id = 'rtx4070_primary'; gpu_adapter = 'NVIDIA GeForce RTX 4070'
            rhi = 'D3D12'; quality_preset = 'High'; resolution_x = 2560
            resolution_y = 1440; playable_tour = $true
            requires_cooked_data = $true
            final_center_arrival_radius_cm = 500.0
            gameplay_interaction_required = $false; streaming_failures = 0
            raw_sample_capture = $sample; csv_capture = $csv
            playable_tour_screenshot = $screenshot
            center_cell_streaming_cycle = $true
            input_method = 'APlayerController::InputKey/FInputKeyEventArgs::CreateSimulated'
            input_event_count = 100; pause_menu_opened = $true
            pause_menu_closed = $true; waypoints_reached = 4
            ascent_cm = 7500.0; descent_cm = 6500.0
            horizontal_displacement_cm = 1500000.0
            collision_blocked_descent = $true; collision_slide = $true
            slide_displacement_cm = 900.0
        }
        $caseParameters = @{
            Path = $path; CorrectnessPath = $correctness; Operation = 'test-child'
            Executable = $script:executable; SamplePath = $sample; CsvPath = $csv
            ScreenshotPath = $screenshot; ProcessExitCode = 0
        }
        $base | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path -Encoding UTF8
        { Assert-ManhattanPerformanceChildDirect @caseParameters } | Should -Not -Throw
        foreach ($field in @('map_package', 'runtime_profile', 'executable',
                'correctness_receipt', 'gameplay_interaction_required')) {
            $receipt = [ordered]@{} + $base
            $receipt[$field] = if ($field -ceq 'gameplay_interaction_required') {
                $true
            } else { 'wrong' }
            $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path -Encoding UTF8
            { Assert-ManhattanPerformanceChildDirect @caseParameters } | Should -Throw
        }
        $receipt = [ordered]@{} + $base
        $receipt.Remove('gameplay_interaction_required')
        $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path -Encoding UTF8
        { Assert-ManhattanPerformanceChildDirect @caseParameters } | Should -Throw
        $receipt = [ordered]@{} + $base
        $receipt['requires_cooked_data'] = $false
        $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path -Encoding UTF8
        { Assert-ManhattanPerformanceChildDirect @caseParameters } | Should -Throw
        foreach ($field in @('center_cell_streaming_cycle', 'pause_menu_opened',
                'pause_menu_closed', 'collision_blocked_descent', 'collision_slide')) {
            $receipt = [ordered]@{} + $base
            $receipt[$field] = $false
            $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path -Encoding UTF8
            { Assert-ManhattanPerformanceChildDirect @caseParameters } | Should -Throw
        }
        $receipt = [ordered]@{} + $base
        $receipt['slide_displacement_cm'] = 0.0
        $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path -Encoding UTF8
        { Assert-ManhattanPerformanceChildDirect @caseParameters } | Should -Throw
        $receipt = [ordered]@{} + $base
        $receipt['final_center_arrival_radius_cm'] = 2500.0
        $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path -Encoding UTF8
        { Assert-ManhattanPerformanceChildDirect @caseParameters } | Should -Throw
    }

    It 'keeps the prototype package route separate and runs Manhattan before Development removal' {
        $manhattan = Get-Content -LiteralPath $script:runner -Raw
        $modeBranch = $manhattan.IndexOf(
            "if (`$PSBoundParameters.ContainsKey('ExistingDevelopmentPackageRoot'))")
        $legacyPackageCall = $manhattan.IndexOf(
            'Invoke-ManhattanShowcasePackage -OutputRoot $stagingRoot')
        $modeBranch | Should -BeGreaterThan 0
        $legacyPackageCall | Should -BeGreaterThan $modeBranch
        $manhattan | Should -Match '\-ProjectWorldProductRouteSkipInteraction'
        $kazan = Get-Content -LiteralPath (Join-Path $script:projectRoot `
                'scripts\ue\world\test\performance\run_kazan_playable_tour.ps1') -Raw
        $sharedPackageCall = $kazan.IndexOf('-ExistingDevelopmentPackageRoot $finalPackage')
        $removeDevelopment = $kazan.IndexOf('Remove-PlayableTourPackage -Path $finalPackage',
            $sharedPackageCall)
        $sharedPackageCall | Should -BeGreaterThan 0
        $removeDevelopment | Should -BeGreaterThan $sharedPackageCall
    }

    It 'passes the exact Manhattan performance flags while leaving prototype smoke flags unchanged' {
        $fakeProcess = [pscustomobject]@{ ExitCode = 0 }
        $fakeProcess | Add-Member -MemberType ScriptMethod -Name WaitForExit `
            -Value { param($milliseconds) return $true }
        $global:AlisManhattanTestArguments = @()
        Mock Start-Process {
            param($FilePath, $ArgumentList, $WorkingDirectory, $WindowStyle, $PassThru)
            $global:AlisManhattanTestArguments = @($ArgumentList)
            return $fakeProcess
        }
        $child = Join-Path $script:testRoot 'child'
        New-Item -ItemType Directory -Path $child -Force | Out-Null
        $correctness = Join-Path $child 'product-route.json'
        $performance = Join-Path $child 'performance.json'
        $csv = Join-Path $child 'performance.csv'
        $samples = Join-Path $child 'samples.csv'
        $screenshot = Join-Path $child 'tour.png'
        $log = Join-Path $child 'game.log'
        $common = @{
            Executable = $script:executable; Configuration = 'Development'
            RunOperationId = 'test-child'; CorrectnessPath = $correctness; LogPath = $log
        }
        Invoke-ManhattanShowcaseGameDirect @common -PerformancePath $performance `
            -CsvPath $csv -SamplePath $samples -ScreenshotPath $screenshot | Should -Be 0
        $global:AlisManhattanTestArguments | Should -Contain '-ProjectMenuPlayAutoExperience=ManhattanShowcase'
        $global:AlisManhattanTestArguments | Should -Contain '-ProjectWorldProductRouteSkipInteraction'
        $global:AlisManhattanTestArguments | Should -Contain '-ProjectWorldProductPerformanceGate'
        $global:AlisManhattanTestArguments | Should -Contain '-ProjectWorldPlayableTour'
        $global:AlisManhattanTestArguments | Should -Contain '-ProjectWorldPlayableTourPreciseCenterReturn'
        $global:AlisManhattanTestArguments | Should -Contain "-ProjectWorldPerformanceCorrectness=$correctness"
        $global:AlisManhattanTestArguments | Should -Contain "-ProjectWorldPerformanceResult=$performance"
        $global:AlisManhattanTestArguments | Should -Contain "-ProjectWorldPerformanceCsv=$csv"
        $global:AlisManhattanTestArguments | Should -Contain "-ProjectWorldPerformanceSamples=$samples"
        $global:AlisManhattanTestArguments | Should -Contain "-ProjectWorldPerformanceScreenshot=$screenshot"
        Invoke-ManhattanShowcaseGameDirect @common | Should -Be 0
        $global:AlisManhattanTestArguments | Should -Not -Contain '-ProjectWorldProductPerformanceGate'
        $global:AlisManhattanTestArguments | Should -Not -Contain '-ProjectWorldPlayableTourPreciseCenterReturn'
        $global:AlisManhattanTestArguments | Should -Contain '-ProjectWorldProductRouteSkipInteraction'
    }
}
