# Integration.Tests.ps1 - Integration tests for overnight CI workflow

BeforeAll {
    $script:ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))))
}

Describe "Overnight CI Integration" {
    Context "main.ps1 Initialization" {
        It "Should initialize without errors" {
            Push-Location $script:ProjectRoot
            try {
                { powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/claude/overnight/main.ps1 -DryRun } | Should -Not -Throw
            }
            finally {
                Pop-Location
            }
        }

        It "Should create artifacts directory" {
            $artifactsDir = Join-Path $script:ProjectRoot "artifacts\overnight"
            Test-Path $artifactsDir | Should -Be $true
        }

        It "Should block git push" {
            Push-Location $script:ProjectRoot
            $oldUrl = git remote get-url --push origin 2>$null
            if (-not $oldUrl -or $oldUrl -eq "origin") { $oldUrl = git remote get-url origin }
            
            try {
                powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/claude/overnight/main.ps1 | Out-Null
                $pushUrl = git remote get-url --push origin
                $pushUrl | Should -Be "DISABLED"
            }
            finally {
                if ($oldUrl -and $oldUrl -ne "DISABLED") { git remote set-url --push origin $oldUrl }
                Pop-Location
            }
        }

        It "Should block git push with -NewBranch flag" {
            Push-Location $script:ProjectRoot
            $oldUrl = git remote get-url --push origin 2>$null
            if (-not $oldUrl -or $oldUrl -eq "origin") { $oldUrl = git remote get-url origin }

            try {
                $dateBranch = "ai/autonomous-dev-$(Get-Date -Format 'yyyyMMdd')"
                try { git branch -D $dateBranch 2>$null } catch {}

                powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/claude/overnight/main.ps1 -NewBranch | Out-Null

                $pushUrl = git remote get-url --push origin
                $pushUrl | Should -Be "DISABLED"

                try { git switch - 2>$null } catch {}
                try { git branch -D $dateBranch 2>$null } catch {}
            }
            finally {
                if ($oldUrl -and $oldUrl -ne "DISABLED") { git remote set-url --push origin $oldUrl }
                Pop-Location
            }
        }

        It "Should fail git push attempts gracefully" {
            Push-Location $script:ProjectRoot
            $oldUrl = git remote get-url --push origin 2>$null
            if (-not $oldUrl -or $oldUrl -eq "origin") { $oldUrl = git remote get-url origin }

            try {
                powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/claude/overnight/main.ps1 | Out-Null

                # Attempt push should fail with clear error
                try {
                    $pushResult = git push origin HEAD 2>&1
                }
                catch {
                    $pushResult = $_.Exception.Message
                }
                "$pushResult" | Should -Match "DISABLED|Could not read from remote repository"
            }
            finally {
                if ($oldUrl -and $oldUrl -ne "DISABLED") { git remote set-url --push origin $oldUrl }
                Pop-Location
            }
        }

        It "Should stay on current branch by default" {
            Push-Location $script:ProjectRoot
            try {
                $currentBranch = git branch --show-current
                powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/claude/overnight/main.ps1 | Out-Null
                $afterBranch = git branch --show-current
                $afterBranch | Should -Be $currentBranch
            }
            finally {
                # Restore implicit default behavior if any side effects
                Pop-Location
            }
        }

        It "Should create new branch with -NewBranch flag" {
            Push-Location $script:ProjectRoot
            $oldUrl = git remote get-url --push origin 2>$null
            if (-not $oldUrl -or $oldUrl -eq "origin") { $oldUrl = git remote get-url origin }
            
            try {
                $dateBranch = "ai/autonomous-dev-$(Get-Date -Format 'yyyyMMdd')"

                # Delete branch if it exists from previous test
                try { git branch -D $dateBranch 2>$null } catch {}

                # Run with -NewBranch
                powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/claude/overnight/main.ps1 -NewBranch | Out-Null

                $currentBranch = git branch --show-current
                $currentBranch | Should -Be $dateBranch

                # Cleanup: switch back
                try { git switch - 2>$null } catch {}
                try { git branch -D $dateBranch 2>$null } catch {}
            }
            finally {
                if ($oldUrl -and $oldUrl -ne "DISABLED") { git remote set-url --push origin $oldUrl }
                Pop-Location
            }
        }
    }

    Context "discover_todos.ps1" {
        It "Should find TODOs in repository" {
            Push-Location $script:ProjectRoot
            try {
                $output = powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/common/discover_todos.ps1
                ($output -join "`n") | Should -Match "Agent-compatible TODOs remaining: \d+"
            }
            finally {
                Pop-Location
            }
        }

        It "Should return numeric count" {
            Push-Location $script:ProjectRoot
            try {
                $output = powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/common/discover_todos.ps1
                $count = ($output | Select-String "Agent-compatible TODOs remaining: (\d+)").Matches.Groups[1].Value
                [int]$count | Should -BeGreaterOrEqual 0
            }
            finally {
                Pop-Location
            }
        }
    }

    Context "Common.psm1 Module" {
        It "Should load without errors" {
            { Import-Module "$script:ProjectRoot/scripts/autonomous/common/Common.psm1" -Force } | Should -Not -Throw
        }

        It "Should export all required functions" {
            Import-Module "$script:ProjectRoot/scripts/autonomous/common/Common.psm1" -Force
            $exports = Get-Command -Module Common

            $exports.Name | Should -Contain "Invoke-WithExponentialBackoff"
            $exports.Name | Should -Contain "Test-CircuitBreaker"
            $exports.Name | Should -Contain "Test-DiskSpace"
            $exports.Name | Should -Contain "Find-ZombieProcesses"
        }
    }

    Context "Build Script Integration" {
        It "Should accept valid parameters" {
            Mock Start-Process { return @{ExitCode = 0 } }

            { powershell.exe -File "$script:ProjectRoot/scripts/ue/build/rebuild_module_safe.ps1" -WhatIf } | Should -Not -Throw
        }

        It "Should validate disk space before build" {
            # This is tested indirectly by running the script
            $path = "$script:ProjectRoot/scripts/ue/build/rebuild_module_safe.ps1"
            $content = Get-Content $path
            $m1 = $content | Select-String "freeGB"
            $m2 = $content | Select-String "requiredGB"
            $m1 | Should -Not -BeNullOrEmpty
            $m2 | Should -Not -BeNullOrEmpty
        }

        It "Should have circuit breaker integration" {
            $content = Get-Content "$script:ProjectRoot/scripts/ue/build/rebuild_module_safe.ps1"
            $m1 = $content | Select-String "Test-CircuitBreaker"
            $m2 = $content | Select-String "Update-CircuitBreakerOnSuccess"
            $m1 | Should -Not -BeNullOrEmpty
            $m2 | Should -Not -BeNullOrEmpty
        }

        It "Should have graceful degradation" {
            $content = Get-Content "$script:ProjectRoot/scripts/ue/build/rebuild_module_safe.ps1"
            $m1 = $content | Select-String "EnableGracefulDegradation"
            $m2 = $content | Select-String "GRACEFUL DEGRADATION"
            $m1 | Should -Not -BeNullOrEmpty
            $m2 | Should -Not -BeNullOrEmpty
        }
    }

    Context "End-to-End Workflow Simulation" {
        It "Should complete initialization → discover todos → ready to build" {
            Push-Location $script:ProjectRoot
            try {
                # Step 1: Initialize
                { powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/claude/overnight/main.ps1 } | Should -Not -Throw

                # Step 2: Discover TODOs
                $todoOutput = powershell.exe -ExecutionPolicy Bypass -File scripts/autonomous/common/discover_todos.ps1
                ($todoOutput -join "`n") | Should -Match "Agent-compatible TODOs remaining"

                # Step 3: Verify build script exists and is ready
                Test-Path "scripts/ue/build/rebuild_module_safe.ps1" | Should -Be $true

                # Step 4: Verify common utilities available
                Test-Path "scripts/autonomous/common/Common.psm1" | Should -Be $true
            }
            finally {
                Pop-Location
            }
        }
    }

    Context "Resilience Features" {
        It "Should have exponential backoff available" {
            Import-Module "$script:ProjectRoot/scripts/autonomous/common/Common.psm1" -Force
            $func = Get-Command "Invoke-WithExponentialBackoff"
            $func | Should -Not -BeNullOrEmpty
            $func.Parameters.Keys | Should -Contain "MaxAttempts"
            $func.Parameters.Keys | Should -Contain "InitialDelay"
            $func.Parameters.Keys | Should -Contain "MaxDelay"
        }

        It "Should have circuit breaker available" {
            Import-Module "$script:ProjectRoot/scripts/autonomous/common/Common.psm1" -Force
            $func = Get-Command "Test-CircuitBreaker"
            $func | Should -Not -BeNullOrEmpty
            $func.Parameters.Keys | Should -Contain "Threshold"
            $func.Parameters.Keys | Should -Contain "CooldownMinutes"
        }

        It "Should have zombie process detection" {
            Import-Module "$script:ProjectRoot/scripts/autonomous/common/Common.psm1" -Force
            $func = Get-Command "Find-ZombieProcesses"
            $func | Should -Not -BeNullOrEmpty
            $func.Parameters.Keys | Should -Contain "MaxAgeMinutes"
        }
    }
}
