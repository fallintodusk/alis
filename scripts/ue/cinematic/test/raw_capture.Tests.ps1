#Requires -Version 5.1

Describe 'ProjectCinematic raw capture contract' {
    BeforeAll {
        $script:projectRoot = Split-Path -Parent (Split-Path -Parent `
                (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
        $script:wrapperPath = Join-Path $script:projectRoot `
            'scripts\ue\cinematic\run_shot_capture.ps1'
        $script:driverPath = Join-Path $script:projectRoot `
            'scripts\ue\cinematic\shot_capture_editor.py'
        $script:rawDocPath = Join-Path $script:projectRoot 'docs\cinematics\raw_capture.md'
        $script:shotBoardPath = Join-Path $script:projectRoot `
            'scripts\ue\cinematic\shots\README.md'
        $script:skillPath = Join-Path $script:projectRoot `
            '.claude\skills\alis-cinematics\SKILL.md'
        $script:gameModeHeaderPath = Join-Path $script:projectRoot `
            'Plugins\Editor\ProjectCinematic\Source\ProjectCinematic\Public\CinematicGameMode.h'
        $script:cleanupPath = Join-Path $script:projectRoot `
            'scripts\ue\cinematic\cleanup_workspace.ps1'
    }

    It 'records enough identity to authenticate one raw take' {
        $wrapper = Get-Content -LiteralPath $wrapperPath -Raw
        $driver = Get-Content -LiteralPath $driverPath -Raw

        foreach ($field in @(
                'schema_version', 'status', 'sequence', 'camera_binding',
                'preset', 'scalability', 'engine_version', 'plan_sha256',
                'master_path', 'master_sha256', 'frame_sha256')) {
            $wrapper | Should -Match ([regex]::Escape($field))
        }
        $wrapper | Should -Not -Match '\[IO\.Path\]::GetRelativePath'
        $driver | Should -Match 'camera_binding'
        $driver | Should -Match 'get_id\(\)\.to_string\(\)'
        $driver | Should -Match 'preset='
    }

    It 'removes large render output when capture fails' {
        $wrapper = Get-Content -LiteralPath $wrapperPath -Raw
        $wrapper | Should -Match 'catch\s*\{'
        $wrapper | Should -Match 'Remove-ShotCaptureRenderScratch'
        $wrapper | Should -Match 'Stop-Process -Id \$editorPid'
    }

    It 'describes the implemented camera-following streaming source' {
        foreach ($path in @($rawDocPath, $shotBoardPath, $skillPath)) {
            $text = Get-Content -LiteralPath $path -Raw
            $text | Should -Not -Match 'pinned (at|near) world origin'
            $text | Should -Not -Match 'not follow(ing)? the camera'
            $text | Should -Match '3 km|3km'
            $text | Should -Match 'follow'
        }
    }

    It 'gives offline wide shots a three-kilometre streaming envelope' {
        $header = Get-Content -LiteralPath $gameModeHeaderPath -Raw
        $header | Should -Match 'CinematicStreamingRadiusCm\s*=\s*300000\.0f'
    }

    It 'keeps the accepted final library while cleaning raw-shot working data' {
        $cleanup = Get-Content -LiteralPath $cleanupPath -Raw
        $cleanup | Should -Match '\[switch\]\$RawShotWorkingData'
        $cleanup | Should -Match 'Join-Path \$finalRoot ''manifest\.json'''
        $cleanup | Should -Match 'Assert-RawShotFinalManifest'
        $cleanup | Should -Match 'Get-FileHash -LiteralPath \$path -Algorithm SHA256'
        $cleanup | Should -Match '\$totalBytes -ne \[Int64\]\$manifest\.total_bytes'
        $cleanup | Should -Match '\$_\.FullName -ne \$finalRoot'
        $cleanup | Should -Match '\$preserved \+= ''Saved/CinematicRaw/Final'''
    }
}
