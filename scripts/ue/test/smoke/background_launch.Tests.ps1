$ErrorActionPreference = 'Stop'

Describe 'Automated UE launch ownership' {
    BeforeAll {
        $projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        $packagedSource = Get-Content -LiteralPath (
            Join-Path $projectRoot 'scripts\ue\test\smoke\packaged_boot_test.ps1') -Raw
        $editorSource = Get-Content -LiteralPath (
            Join-Path $projectRoot 'scripts\ue\test\smoke\boot_test.bat') -Raw
        $fixedViewSource = Get-Content -LiteralPath (
            Join-Path $projectRoot 'scripts\ue\world\test\capture_packaged_fixed_view.ps1') -Raw
        $coldTestSource = Get-Content -LiteralPath (
            Join-Path $projectRoot 'scripts\ue\test\unit\run_cpp_tests_safe.ps1') -Raw
        $persistentTestSource = Get-Content -LiteralPath (
            Join-Path $projectRoot 'scripts\ue\test\unit\persistent_editor_start.ps1') -Raw
    }

    It 'runs the packaged smoke gate offscreen and hidden' {
        $packagedSource | Should -Match '"-RenderOffScreen"'
        $packagedSource | Should -Match '-WindowStyle Hidden'
    }

    It 'runs the editor smoke gate offscreen and hidden' {
        $editorSource | Should -Match "'-RenderOffScreen'"
        $editorSource | Should -Match '-WindowStyle Hidden'
    }

    It 'runs packaged fixed-view evidence offscreen and hidden' {
        $fixedViewSource | Should -Match "'-RenderOffScreen'"
        $fixedViewSource | Should -Match '-WindowStyle Hidden'
    }

    It 'runs cold render-capable automation offscreen without a process window' {
        $coldTestSource | Should -Match ' -RenderOffScreen'
        $coldTestSource | Should -Match 'CreateNoWindow\s*=\s*\$true'
    }

    It 'runs persistent automation offscreen and hidden' {
        $persistentTestSource | Should -Match '"-RenderOffScreen"'
        $persistentTestSource | Should -Match '-WindowStyle Hidden'
    }
}
