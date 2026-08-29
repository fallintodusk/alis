#Requires -Version 5.1

Describe 'ProjectCinematic release capture contract' {
    BeforeAll {
        $script:projectRoot = Split-Path -Parent (Split-Path -Parent `
                (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
        $script:requestPath = Join-Path $script:projectRoot `
            'scripts\ue\cinematic\requests\kazan_release_v1.json'
        $script:projectPath = Join-Path $script:projectRoot 'Alis.uproject'
        $script:pluginPath = Join-Path $script:projectRoot `
            'Plugins\Editor\ProjectCinematic\ProjectCinematic.uplugin'
    }

    It 'uses one schema-bound Kazan request tied to the accepted package' {
        $text = Get-Content -LiteralPath $requestPath -Raw
        $text | Should -Match '^\s*\{\s*"\$schema"\s*:'
        $request = $text | ConvertFrom-Json
        $request.schema_version | Should -Be 2
        $request.map_package | Should -Be '/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory'
        $request.sequence | Should -Be '/Game/Cinematics/Kazan/LS_KazanRelease_v1'
        $request.camera_class | Should -Be 'CineCameraActor'
        $request.scalability_quality | Should -Be 2
        $request.release_acceptance | Should -Be `
            'Saved/Validation/WorldRealization/playable-tour/Candidate/operator-acceptance.json'
        $request.package_root | Should -Be `
            'Saved/PackageRelease/KazanPlayableTour/Candidate'
    }

    It 'keeps every capture plugin on Editor targets only' {
        $project = Get-Content -LiteralPath $projectPath -Raw | ConvertFrom-Json
        foreach ($name in @(
                'ProjectCinematic',
                'MovieRenderPipeline',
                'MoviePipelineMaskRenderPass',
                'AppleProResMedia')) {
            $entry = @($project.Plugins | Where-Object Name -eq $name)
            $entry.Count | Should -Be 1
            @($entry[0].TargetAllowList) | Should -Be @('Editor')
        }
        $plugin = Get-Content -LiteralPath $pluginPath -Raw | ConvertFrom-Json
        @($plugin.Modules | Where-Object Name -eq 'ProjectCinematic')[0].Type | Should -Be 'Editor'
        @($plugin.Plugins | Where-Object Name -eq 'Takes')[0].TargetAllowList |
            Should -Be @('Editor')
        $packaging = Get-Content -LiteralPath `
            (Join-Path $projectRoot 'Config\DefaultGame.ini') -Raw
        foreach ($root in @(
                '/Game/Cinematics',
                '/ProjectCinematic',
                '/MovieRenderPipeline',
                '/MoviePipelineMaskRenderPass',
                '/Takes',
                '/AppleProResMedia')) {
            $pattern = [regex]::Escape(
                ('DirectoriesToNeverCook=(Path="{0}")' -f $root))
            $packaging | Should -Match $pattern
        }
    }

    It 'uses the ProjectCinematic game mode and product-truth visibility policy' {
        foreach ($name in @('apply_dev_preset.py', 'apply_prod_preset.py')) {
            $path = Join-Path $projectRoot "scripts\ue\cinematic\$name"
            $text = Get-Content -LiteralPath $path -Raw
            $text | Should -Match '/Script/ProjectCinematic\.CinematicGameMode'
            $text | Should -Not -Match 'AlisCinematicGameMode'
            $text | Should -Match '"cinematic_quality_settings": False'
            $text | Should -Match '"texture_streaming": "NONE"'
            $text | Should -Match '"use_lod_zero": False'
            $text | Should -Match '"disable_hlods": False'
            $text | Should -Match '"override_view_distance_scale": False'
            $text | Should -Match '"override_grass_cull_distance_scale": False'
        }
        $gameMode = Get-Content -LiteralPath (Join-Path $projectRoot `
            'Plugins\Editor\ProjectCinematic\Source\ProjectCinematic\Private\CinematicGameMode.cpp') -Raw
        $gameMode | Should -Match '/\*bAffectsHUD\*/\s+bRenderMode'
        $gameMode | Should -Match 'RemoveAllViewportWidgets'
    }

    It 'drives MRQ from the request and owns bounded cleanup' {
        $driver = Get-Content -LiteralPath `
            (Join-Path $projectRoot 'scripts\ue\cinematic\release_capture_editor.py') -Raw
        $wrapper = Get-Content -LiteralPath `
            (Join-Path $projectRoot 'scripts\ue\cinematic\run_release_capture.ps1') -Raw
        $driver | Should -Not -Match 'City17'
        $driver | Should -Match 'PROJECT_CINEMATIC_CAPTURE_REQUEST'
        $driver | Should -Match 'MoviePipelinePIEExecutor'
        $driver | Should -Match 'quit_editor'
        $wrapper | Should -Match 'tmp\\cinematic\\release_capture'
        $wrapper | Should -Match 'Remove-CaptureOwnedTree'
        $wrapper | Should -Match 'Saved\\CinematicRelease\\Kazan'
    }

    It 'independently authenticates the accepted Candidate before render' {
        $bindingPath = Join-Path $projectRoot `
            'scripts\ue\cinematic\release_binding.ps1'
        Test-Path -LiteralPath $bindingPath -PathType Leaf | Should -BeTrue
        if (-not (Test-Path -LiteralPath $bindingPath -PathType Leaf)) {
            throw 'Release binding helper is missing.'
        }
        $binding = Get-Content -LiteralPath $bindingPath -Raw
        $binding | Should -Match 'Get-ProjectCinematicPackageTreeDigest'
        $binding | Should -Match 'shipping_executable_sha256'
        $binding | Should -Match 'source_state_sha256'
        $binding | Should -Match 'release_composite_sha256'
        $binding | Should -Match 'operator_accepted'

        $wrapper = Get-Content -LiteralPath (Join-Path $projectRoot `
            'scripts\ue\cinematic\run_release_capture.ps1') -Raw
        $wrapper | Should -Match 'Test-ProjectCinematicReleaseBinding'
        $wrapper | Should -Match 'Get-CaptureSourceStateDigest'
        $wrapper | Should -Match 'git -C \$projectRoot rev-parse HEAD'
        $wrapper | Should -Match 'CurrentSourceRevision'
        $wrapper | Should -Match 'source_revision = \$sourceRevision'
        $wrapper | Should -Match 'release_acceptance'
    }

    It 'rejects package bytes that differ from the operator acceptance receipt' {
        $bindingPath = Join-Path $projectRoot 'scripts\ue\cinematic\release_binding.ps1'
        . $bindingPath
        $ownerRoot = Join-Path $projectRoot 'tmp\cinematic\release_capture'
        $testRoot = Join-Path $ownerRoot ("binding-test-{0}" -f [Guid]::NewGuid().ToString('N'))
        $packageRoot = Join-Path $testRoot 'package'
        $executable = Join-Path $packageRoot `
            'Windows\Alis\Binaries\Win64\Alis-Win64-Shipping.exe'
        $compositePath = Join-Path $testRoot 'composite.json'
        $acceptancePath = Join-Path $testRoot 'operator-acceptance.json'
        try {
            New-Item -ItemType Directory -Path (Split-Path -Parent $executable) `
                -Force | Out-Null
            [IO.File]::WriteAllText($executable, 'accepted executable bytes')
            $packageHash = Get-ProjectCinematicPackageTreeDigest -Path $packageRoot
            $executableHash = (Get-FileHash -LiteralPath $executable `
                -Algorithm SHA256).Hash.ToLowerInvariant()
            $sourceHash = '0123456789abcdef'
            $sourceRevision = 'test-revision'
            $composite = [ordered]@{
                status = 'accepted'
                operation_id = 'playable_tour_test'
                revision = 'test-revision'
                source_state_sha256 = $sourceHash
                map_package = '/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory'
                runtime_profile = 'kazan_territory_512_1536_v1'
                runtime_profile_sha256 = 'runtime-hash'
                shipping_executable_sha256 = $executableHash
                shipping_package_sha256 = $packageHash
                final_package = $packageRoot
            }
            [IO.File]::WriteAllText($compositePath, ($composite | ConvertTo-Json))
            $compositeHash = (Get-FileHash -LiteralPath $compositePath `
                -Algorithm SHA256).Hash.ToLowerInvariant()
            $acceptance = [ordered]@{
                schema_version = 1
                status = 'operator_accepted'
                product_decision = 'accepted'
                package_root = $packageRoot
                package_tree_sha256 = $packageHash
                shipping_executable = $executable
                shipping_executable_sha256 = $executableHash
                source_revision = 'test-revision'
                source_state_sha256 = $sourceHash
                map_package = $composite.map_package
                runtime_profile = $composite.runtime_profile
                runtime_profile_sha256 = $composite.runtime_profile_sha256
                release_operation_id = $composite.operation_id
                release_composite = $compositePath
                release_composite_sha256 = $compositeHash
            }
            [IO.File]::WriteAllText($acceptancePath, ($acceptance | ConvertTo-Json))

            $binding = Test-ProjectCinematicReleaseBinding `
                -ProjectRoot $projectRoot -ReleaseAcceptancePath $acceptancePath `
                -ExpectedPackageRoot $packageRoot -CurrentSourceStateSha256 $sourceHash `
                -CurrentSourceRevision $sourceRevision
            $binding.package_tree_sha256 | Should -BeExactly $packageHash

            { Test-ProjectCinematicReleaseBinding `
                    -ProjectRoot $projectRoot -ReleaseAcceptancePath $acceptancePath `
                    -ExpectedPackageRoot $packageRoot `
                    -CurrentSourceStateSha256 $sourceHash `
                    -CurrentSourceRevision 'different-clean-revision' } |
                Should -Throw '*revision differs*'

            [IO.File]::AppendAllText($executable, 'tampered')
            { Test-ProjectCinematicReleaseBinding `
                    -ProjectRoot $projectRoot -ReleaseAcceptancePath $acceptancePath `
                    -ExpectedPackageRoot $packageRoot `
                    -CurrentSourceStateSha256 $sourceHash `
                    -CurrentSourceRevision $sourceRevision } |
                Should -Throw '*Candidate tree no longer matches*'
        }
        finally {
            $resolvedOwner = [IO.Path]::GetFullPath($ownerRoot).TrimEnd('\', '/')
            $resolvedTarget = [IO.Path]::GetFullPath($testRoot)
            if ($resolvedTarget.StartsWith(
                    $resolvedOwner + [IO.Path]::DirectorySeparatorChar,
                    [StringComparison]::OrdinalIgnoreCase) -and
                (Test-Path -LiteralPath $resolvedTarget)) {
                Remove-Item -LiteralPath $resolvedTarget -Recurse -Force
            }
        }
    }

    It 'allows only metadata-only Takes manifest provenance' {
        $audit = Get-Content -LiteralPath (Join-Path $projectRoot `
            'scripts\ue\cinematic\audit_capture_package.ps1') -Raw
        $audit | Should -Match 'Takes/Takes\.uplugin'
        $audit | Should -Match 'Takes/Config/DefaultTakes\.ini'
        $audit | Should -Match 'allowed_manifest_metadata'
        $audit | Should -Match 'iostore_entries'
        $audit | Should -Match 'loose_entries'
    }
}
