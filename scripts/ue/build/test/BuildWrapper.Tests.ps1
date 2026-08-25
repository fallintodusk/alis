# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

BeforeAll {
    $BuildDir = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
    $BuildWrapper = Get-Content -LiteralPath (Join-Path $BuildDir "build.bat") -Raw
}

Describe "Windows build wrapper contract" {
    It "parses the documented target platform configuration order" {
        $BuildWrapper | Should -Match 'set TARGET=%1'
        $BuildWrapper | Should -Match 'set PLATFORM=%2'
        $BuildWrapper | Should -Match 'set CONFIG=%3'
        $BuildWrapper | Should -Match 'set EXTRA_ARGS=%4 %5 %6 %7 %8 %9'
    }

    It "forwards the canonical order and stable hot-reload flag" {
        $BuildWrapper | Should -Match '%TARGET% %PLATFORM% %CONFIG% "%PROJECT_FILE%" %EXTRA_ARGS% -NoHotReloadFromIDE'
    }

    It "keeps legacy wrappers on the same argument contract" {
        foreach ($Name in @("build_editor.bat", "build_project.bat", "full_compile.bat")) {
            $Text = Get-Content -LiteralPath (Join-Path $BuildDir $Name) -Raw
            $Text | Should -Match 'build\.bat" AlisEditor Win64 (DebugGame|Development)'
            $Text | Should -Not -Match ' -NoHotReload(?:\s|>)'
        }
    }
}
