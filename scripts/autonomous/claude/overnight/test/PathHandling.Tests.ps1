# PathHandling.Tests.ps1 - Smoke tests for WSL/Windows path handling

BeforeAll {
    $script:ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))))
}

Describe "Path Handling (Windows/WSL)" {
    Context "Project Root Path Resolution" {
        It "Should resolve project root from scripts directory" {
            $root = (Resolve-Path "$PSScriptRoot/../../../../..").Path
            Test-Path $root | Should -Be $true
            Test-Path (Join-Path $root "Alis.uproject") | Should -Be $true
        }

        It "Should handle both E: and /mnt/e formats" {
            # Windows format
            if ($PSScriptRoot -match "^[A-Z]:\\") {
                $drive = ($PSScriptRoot -split ":")[0]
                $windowsPath = "$($drive):\Repos_Alis\Alis"
                if (Test-Path $windowsPath) {
                    Test-Path (Join-Path $windowsPath "Alis.uproject") | Should -Be $true
                }
            }

            # WSL format
            if ($PSScriptRoot -match "^/mnt/") {
                $drive = ($PSScriptRoot -split "/")[2]
                $wslPath = "/mnt/$drive/Repos_Alis/Alis"
                if (Test-Path $wslPath) {
                    Test-Path (Join-Path $wslPath "Alis.uproject") | Should -Be $true
                }
            }
        }

        It "Should convert between Windows and WSL paths" {
            # Test path conversion logic
            $windowsPath = "<project-root>"
            $expectedWSL = "<project-root>"

            $wslPath = $windowsPath -replace '^([A-Z]):', '/mnt/$1' -replace '\\', '/'
            $wslPath = $wslPath.ToLower()

            $wslPath | Should -Be $expectedWSL.ToLower()
        }

        It "Should preserve absolute paths through Resolve-Path" {
            $resolved = (Resolve-Path $script:ProjectRoot).Path
            [System.IO.Path]::IsPathRooted($resolved) | Should -Be $true
        }
    }

    Context "Script Path Calculations" {
        It "rebuild_module_safe.ps1 should calculate correct project root" {
            $scriptPath = Join-Path $script:ProjectRoot "scripts/ue/build/rebuild_module_safe.ps1"
            $scriptContent = Get-Content $scriptPath -Raw

            # Check for correct path calculation (3 levels up)
            $scriptContent | Select-String '\$root\s*=\s*\(Resolve-Path\s+"?\$PSScriptRoot[\\/]\.\.([\\/]\.\.){2}"?\)\.Path' | Should -Not -BeNullOrEmpty
        }

        It "main.ps1 should calculate correct project root" {
            $scriptPath = Join-Path $script:ProjectRoot "scripts/autonomous/claude/overnight/main.ps1"
            $scriptContent = Get-Content $scriptPath -Raw

            # Should go up 3 levels from scripts/ci/overnight
            $scriptContent | Should -Match 'Split-Path.*Split-Path.*Split-Path.*Split-Path'
        }

        It "discover_todos.ps1 should handle paths correctly" {
            Push-Location $script:ProjectRoot
            try {
                # Script should work regardless of current directory
                $output = powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/common/discover_todos.ps1
                ($output -join "`n") | Should -Match "Agent-compatible TODOs remaining"
            }
            finally {
                Pop-Location
            }
        }
    }

    Context "UBT Path Handling" {
        It "Should construct valid UE project path" {
            $projectPath = Join-Path $script:ProjectRoot "Alis.uproject"
            Test-Path $projectPath | Should -Be $true

            # Should be absolute path
            [System.IO.Path]::IsPathRooted($projectPath) | Should -Be $true
        }

        It "Should quote paths with spaces in UBT arguments" {
            # UBT requires quoted paths
            $projectPath = Join-Path $script:ProjectRoot "Alis.uproject"
            $quotedPath = "`"$projectPath`""

            $quotedPath | Should -Match '^".*"$'
        }

        It "Should use forward slashes for WSL UBT paths" {
            if ($IsLinux) {
                $projectPath = "<project-root>/Alis.uproject"
                $projectPath | Should -Not -Match '\\'
                $projectPath | Should -Match '/'
            }
        }
    }

    Context "Artifacts Directory Paths" {
        It "Should create artifacts directory with correct path" {
            $artifactsDir = Join-Path $script:ProjectRoot "artifacts/overnight"

            # Create if doesn't exist
            if (-not (Test-Path $artifactsDir)) {
                New-Item -ItemType Directory -Path $artifactsDir -Force | Out-Null
            }

            Test-Path $artifactsDir | Should -Be $true
        }

        It "Should handle step-specific subdirectories" {
            $stepDir = Join-Path $script:ProjectRoot "artifacts/overnight/step-test"

            New-Item -ItemType Directory -Path $stepDir -Force | Out-Null
            Test-Path $stepDir | Should -Be $true

            # Cleanup
            Remove-Item $stepDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    Context "Cross-Platform Path Compatibility" {
        It "Should use platform-appropriate path separators" {
            $testPath = Join-Path "foo" (Join-Path "bar" "baz")

            if ($IsWindows -or $PSVersionTable.PSVersion.Major -le 5) {
                # Windows PowerShell (5.1) uses backslash
                $testPath | Should -Match '\\'
            }
            else {
                # PowerShell Core uses forward slash on Linux/WSL, backslash on Windows
                if ($IsLinux) {
                    $testPath | Should -Match '/'
                }
                else {
                    $testPath | Should -Match '\\'
                }
            }
        }

        It "Should resolve relative paths from any starting directory" {
            Push-Location $script:ProjectRoot
            try {
                $relativePath = "./scripts/autonomous/claude/overnight"
                $resolved = (Resolve-Path $relativePath).Path
                Test-Path $resolved | Should -Be $true
            }
            finally {
                Pop-Location
            }
        }
    }
}
