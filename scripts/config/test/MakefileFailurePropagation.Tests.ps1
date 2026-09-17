BeforeAll {
    $Script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
    $Script:MakefileText = Get-Content -LiteralPath `
        (Join-Path $Script:RepoRoot "Makefile") -Raw
}

Describe "Makefile WSL child-process failure propagation" {
    It "guards every continued cmd.exe recipe before printing success" {
        $Script:MakefileText | Should -Not -Match `
            '\r?\n\tcmd\.exe /C \$\$CMD; \\'

        $guards = [regex]::Matches(
            $Script:MakefileText,
            'cmd\.exe /C \$\$CMD \|\| exit \$\$\?; \\'
        )
        $guards.Count | Should -Be 7
    }
}
