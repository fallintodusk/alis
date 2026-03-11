# Common.Tests.ps1 - Pester unit tests for Common.psm1

BeforeAll {
    # Import module to test
    Import-Module "$PSScriptRoot/../../../common/Common.psm1" -Force
}

Describe "Invoke-WithExponentialBackoff" {
    It "Should succeed on first attempt with successful action" {
        $action = { @{ ExitCode = 0 } }
        $result = Invoke-WithExponentialBackoff -Action $action -MaxAttempts 3 -InitialDelay 1 -MaxDelay 10
        $result.ExitCode | Should -Be 0
    }

    It "Should retry and succeed on second attempt" {
        $script:attempt = 0
        $action = {
            $script:attempt++
            if ($script:attempt -lt 2) {
                throw "Simulated failure"
            }
            @{ ExitCode = 0 }
        }
        $result = Invoke-WithExponentialBackoff -Action $action -MaxAttempts 3 -InitialDelay 1 -MaxDelay 10
        $result.ExitCode | Should -Be 0
        $script:attempt | Should -Be 2
    }

    It "Should exhaust all retries and throw on repeated failures" {
        $action = { throw "Persistent failure" }
        { Invoke-WithExponentialBackoff -Action $action -MaxAttempts 2 -InitialDelay 1 -MaxDelay 10 } | Should -Throw
    }

    It "Should use exponential backoff delays" {
        $script:attempt = 0
        $script:delays = @()
        $action = {
            $script:attempt++
            throw "Simulated failure"
        }

        Mock -ModuleName Common Start-Sleep { param($Seconds) $script:delays += $Seconds }

        { Invoke-WithExponentialBackoff -Action $action -MaxAttempts 3 -InitialDelay 2 -MaxDelay 10 } | Should -Throw

        # Delays should be: 2s (first retry), 4s (second retry)
        $script:delays.Count | Should -Be 2
        $script:delays[0] | Should -Be 2
        $script:delays[1] | Should -Be 4
    }

    It "Should cap delays at MaxDelay" {
        $script:attempt = 0
        $script:delays = @()
        $action = {
            $script:attempt++
            throw "Simulated failure"
        }

        Mock -ModuleName Common Start-Sleep { param($Seconds) $script:delays += $Seconds }

        { Invoke-WithExponentialBackoff -Action $action -MaxAttempts 4 -InitialDelay 10 -MaxDelay 15 } | Should -Throw

        # Delays should be: 10s, 15s (capped), 15s (capped)
        $script:delays.Count | Should -Be 3
        $script:delays[0] | Should -Be 10
        $script:delays[1] | Should -Be 15  # Capped at MaxDelay
        $script:delays[2] | Should -Be 15  # Capped at MaxDelay
    }
}

Describe "Circuit Breaker" {
    BeforeEach {
        # Create temporary state file for each test
        $script:testStateFile = Join-Path $TestDrive "circuit_breaker.json"
    }

    Context "Get-CircuitBreakerState" {
        It "Should return default state when file doesn't exist" {
            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.ConsecutiveFailures | Should -Be 0
            $state.State | Should -Be "CLOSED"
            $state.LastFailureTime | Should -Be ([DateTime]::MinValue)
        }

        It "Should read state from existing file" {
            $testState = @{
                ConsecutiveFailures = 3
                LastFailureTime     = (Get-Date).ToString("o")
                State               = "CLOSED"
            }
            $testState | ConvertTo-Json | Set-Content $script:testStateFile

            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.ConsecutiveFailures | Should -Be 3
            $state.State | Should -Be "CLOSED"
        }
    }

    Context "Set-CircuitBreakerState" {
        It "Should write state to file" {
            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures 5 -LastFailureTime (Get-Date) -State "OPEN"

            Test-Path $script:testStateFile | Should -Be $true

            $state = Get-Content $script:testStateFile | ConvertFrom-Json
            $state.ConsecutiveFailures | Should -Be 5
            $state.State | Should -Be "OPEN"
        }

        It "Should create directory if it doesn't exist" {
            $nestedPath = Join-Path $TestDrive "nested/path/circuit_breaker.json"
            Set-CircuitBreakerState -StateFile $nestedPath -ConsecutiveFailures 0 -LastFailureTime ([DateTime]::MinValue) -State "CLOSED"

            Test-Path $nestedPath | Should -Be $true
        }
    }

    Context "Test-CircuitBreaker" {
        It "Should allow operation when circuit is CLOSED" {
            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures 2 -LastFailureTime (Get-Date) -State "CLOSED"

            $result = Test-CircuitBreaker -StateFile $script:testStateFile -Threshold 5 -CooldownMinutes 30
            $result | Should -Be $true
        }

        It "Should block operation when circuit is OPEN and cooldown not expired" {
            $recentFailure = (Get-Date).AddMinutes(-10)  # 10 minutes ago
            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures 5 -LastFailureTime $recentFailure -State "OPEN"

            $result = Test-CircuitBreaker -StateFile $script:testStateFile -Threshold 5 -CooldownMinutes 30
            $result | Should -Be $false
        }

        It "Should transition to HALF_OPEN when cooldown expired" {
            $oldFailure = (Get-Date).AddMinutes(-35)  # 35 minutes ago (cooldown is 30)
            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures 5 -LastFailureTime $oldFailure -State "OPEN"

            $result = Test-CircuitBreaker -StateFile $script:testStateFile -Threshold 5 -CooldownMinutes 30
            $result | Should -Be $true

            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.State | Should -Be "HALF_OPEN"
        }
    }

    Context "Update-CircuitBreakerOnSuccess" {
        It "Should reset state to CLOSED with zero failures" {
            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures 3 -LastFailureTime (Get-Date) -State "HALF_OPEN"

            Update-CircuitBreakerOnSuccess -StateFile $script:testStateFile

            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.ConsecutiveFailures | Should -Be 0
            $state.State | Should -Be "CLOSED"
            $state.LastFailureTime | Should -Be ([DateTime]::MinValue)
        }
    }

    Context "Update-CircuitBreakerOnFailure" {
        It "Should increment consecutive failures and stay CLOSED below threshold" {
            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures 2 -LastFailureTime (Get-Date).AddMinutes(-5) -State "CLOSED"

            Update-CircuitBreakerOnFailure -StateFile $script:testStateFile -Threshold 5

            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.ConsecutiveFailures | Should -Be 3
            $state.State | Should -Be "CLOSED"
        }

        It "Should open circuit when threshold reached" {
            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures 4 -LastFailureTime (Get-Date).AddMinutes(-5) -State "CLOSED"

            Update-CircuitBreakerOnFailure -StateFile $script:testStateFile -Threshold 5

            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.ConsecutiveFailures | Should -Be 5
            $state.State | Should -Be "OPEN"
        }

        It "Should update LastFailureTime to current time" {
            $beforeTest = Get-Date
            Start-Sleep -Milliseconds 100

            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures 0 -LastFailureTime ([DateTime]::MinValue) -State "CLOSED"
            Update-CircuitBreakerOnFailure -StateFile $script:testStateFile -Threshold 5

            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.LastFailureTime | Should -BeGreaterThan $beforeTest
        }
    }

    Context "Circuit Breaker Integration" {
        It "Should follow complete success flow: CLOSED → failure → failure → success → CLOSED" {
            # Start closed
            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures 0 -LastFailureTime ([DateTime]::MinValue) -State "CLOSED"

            # First failure
            Update-CircuitBreakerOnFailure -StateFile $script:testStateFile -Threshold 3
            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.ConsecutiveFailures | Should -Be 1
            $state.State | Should -Be "CLOSED"

            # Second failure
            Update-CircuitBreakerOnFailure -StateFile $script:testStateFile -Threshold 3
            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.ConsecutiveFailures | Should -Be 2
            $state.State | Should -Be "CLOSED"

            # Success - should reset
            Update-CircuitBreakerOnSuccess -StateFile $script:testStateFile
            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.ConsecutiveFailures | Should -Be 0
            $state.State | Should -Be "CLOSED"
        }

        It "Should follow complete failure flow: CLOSED → failures → OPEN → cooldown → HALF_OPEN → success → CLOSED" {
            # Start closed
            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures 0 -LastFailureTime ([DateTime]::MinValue) -State "CLOSED"

            # Accumulate failures to open circuit
            for ($i = 0; $i -lt 5; $i++) {
                Update-CircuitBreakerOnFailure -StateFile $script:testStateFile -Threshold 5
            }

            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.ConsecutiveFailures | Should -Be 5
            $state.State | Should -Be "OPEN"

            # Should block immediately
            Test-CircuitBreaker -StateFile $script:testStateFile -Threshold 5 -CooldownMinutes 30 | Should -Be $false

            # Simulate cooldown expiration by backdating failure time
            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $oldFailure = (Get-Date).AddMinutes(-35)
            Set-CircuitBreakerState -StateFile $script:testStateFile -ConsecutiveFailures $state.ConsecutiveFailures -LastFailureTime $oldFailure -State "OPEN"

            # Should transition to HALF_OPEN
            Test-CircuitBreaker -StateFile $script:testStateFile -Threshold 5 -CooldownMinutes 30 | Should -Be $true
            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.State | Should -Be "HALF_OPEN"

            # Success should close circuit
            Update-CircuitBreakerOnSuccess -StateFile $script:testStateFile
            $state = Get-CircuitBreakerState -StateFile $script:testStateFile
            $state.ConsecutiveFailures | Should -Be 0
            $state.State | Should -Be "CLOSED"
        }
    }
}

Describe "Disk Space Monitoring" {
    Context "Test-DiskSpace" {
        It "Should return true when sufficient space available" {
            Mock -ModuleName Common Get-PSDrive {
                [PSCustomObject]@{
                    Name = "C"
                    Free = 100GB
                }
            }

            $result = Test-DiskSpace -Path "C:\" -RequiredGB 50
            $result | Should -Be $true
        }

        It "Should return false when insufficient space" {
            Mock -ModuleName Common Get-PSDrive {
                [PSCustomObject]@{
                    Name = "C"
                    Free = 30GB
                }
            }

            $result = Test-DiskSpace -Path "C:\" -RequiredGB 50
            $result | Should -Be $false
        }

        It "Should fail open (return true) on monitoring failure" {
            Mock -ModuleName Common Get-PSDrive { throw "Disk access error" }

            $result = Test-DiskSpace -Path "C:\" -RequiredGB 50
            $result | Should -Be $true
        }

        It "Should use default path when not specified" {
            Mock -ModuleName Common Get-Location {
                [PSCustomObject]@{
                    Drive = [PSCustomObject]@{
                        Root = "C:\"
                    }
                }
            }

            Mock -ModuleName Common Get-PSDrive {
                [PSCustomObject]@{
                    Name = "C"
                    Free = 100GB
                }
            }

            { Test-DiskSpace -RequiredGB 50 } | Should -Not -Throw
        }

        It "Should use default 50GB requirement when not specified" {
            Mock -ModuleName Common Get-PSDrive {
                [PSCustomObject]@{
                    Name = "C"
                    Free = 60GB
                }
            }

            $result = Test-DiskSpace -Path "C:\"
            $result | Should -Be $true
        }
    }

    Context "Get-DiskSpaceInfo" {
        It "Should return disk space information" {
            Mock -ModuleName Common Get-PSDrive {
                [PSCustomObject]@{
                    Name = "C"
                    Used = 400GB
                    Free = 100GB
                }
            }

            $info = Get-DiskSpaceInfo -Path "C:\"

            $info | Should -Not -BeNullOrEmpty
            $info.Drive | Should -Be "C"
            $info.TotalGB | Should -Be 500
            $info.UsedGB | Should -Be 400
            $info.FreeGB | Should -Be 100
            $info.FreePercent | Should -Be 20.0
        }

        It "Should return null on error" {
            Mock -ModuleName Common Get-PSDrive { throw "Disk access error" }

            $info = Get-DiskSpaceInfo -Path "C:\"
            $info | Should -BeNullOrEmpty
        }

        It "Should calculate percentages correctly" {
            Mock -ModuleName Common Get-PSDrive {
                [PSCustomObject]@{
                    Name = "E"
                    Used = 750GB
                    Free = 250GB
                }
            }

            $info = Get-DiskSpaceInfo -Path "E:\"

            $info.FreePercent | Should -Be 25.0
        }
    }
}
