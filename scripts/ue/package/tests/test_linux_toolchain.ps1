#Requires -Version 5.1

BeforeAll {
    $Script:PackageDir = Split-Path -Parent $PSScriptRoot
    . (Join-Path $Script:PackageDir "linux_toolchain.ps1")

    function New-FakeLinuxSdk {
        param(
            [Parameter(Mandatory)][string]$Root,
            [Parameter(Mandatory)][string]$Version
        )
        $ConfigDir = Join-Path $Root "Engine\Config\Linux"
        New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
        @{ MainVersion = $Version; MinVersion = $Version; MaxVersion = $Version } |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $ConfigDir "Linux_SDK.json")
    }

    function New-FakeToolchain {
        param(
            [Parameter(Mandatory)][string]$Root,
            [Parameter(Mandatory)][string]$Version
        )
        $BinDir = Join-Path $Root "x86_64-unknown-linux-gnu\bin"
        New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $Root "ToolchainVersion.txt") -Value $Version
        Set-Content -LiteralPath (Join-Path $BinDir "clang++.exe") -Value "stub"
    }
}

Describe "Linux cross-toolchain preflight" {
    BeforeEach {
        $Script:EngineRoot = Join-Path $TestDrive "engine"
        $Script:ToolchainRoot = Join-Path $TestDrive "toolchain"
        New-FakeLinuxSdk -Root $Script:EngineRoot -Version "v26_clang-20.1.8-rockylinux8"
        New-FakeToolchain -Root $Script:ToolchainRoot -Version "v26_clang-20.1.8-rockylinux8"
    }

    It "accepts the toolchain required by the selected engine" {
        $Result = Test-LinuxToolchain -EngineRoot $Script:EngineRoot -ToolchainRoot $Script:ToolchainRoot
        $Result.Version | Should -Be "v26_clang-20.1.8-rockylinux8"
        $Result.Root | Should -Be (Resolve-Path $Script:ToolchainRoot).Path
    }

    It "rejects a mismatched toolchain marker before UAT" {
        Set-Content -LiteralPath (Join-Path $Script:ToolchainRoot "ToolchainVersion.txt") `
            -Value "v23_clang-18.1.0-rockylinux8"
        { Test-LinuxToolchain -EngineRoot $Script:EngineRoot -ToolchainRoot $Script:ToolchainRoot } |
            Should -Throw "*requires*v26_clang-20.1.8-rockylinux8*found*v23_clang-18.1.0-rockylinux8*"
    }

    It "rejects a missing compiler before UAT" {
        Remove-Item -LiteralPath (Join-Path $Script:ToolchainRoot "x86_64-unknown-linux-gnu\bin\clang++.exe")
        { Test-LinuxToolchain -EngineRoot $Script:EngineRoot -ToolchainRoot $Script:ToolchainRoot } |
            Should -Throw "*clang++.exe*"
    }
}

Describe "Linux cross-toolchain process scope" {
    BeforeEach {
        $Script:SavedRoot = [Environment]::GetEnvironmentVariable("LINUX_MULTIARCH_ROOT", "Process")
        [Environment]::SetEnvironmentVariable("LINUX_MULTIARCH_ROOT", "C:\PreviousToolchain", "Process")
    }

    AfterEach {
        [Environment]::SetEnvironmentVariable("LINUX_MULTIARCH_ROOT", $Script:SavedRoot, "Process")
    }

    It "restores the caller environment after success" {
        Invoke-WithLinuxToolchain -ToolchainRoot "C:\SelectedToolchain" -Action {
            $Env:LINUX_MULTIARCH_ROOT | Should -Be "C:\SelectedToolchain"
        }
        $Env:LINUX_MULTIARCH_ROOT | Should -Be "C:\PreviousToolchain"
    }

    It "passes an explicit action context without colliding with PowerShell Args" {
        $Context = [PSCustomObject]@{
            Command = "RunUAT.bat"
            Arguments = @("BuildCookRun", "-platform=Linux")
            Observed = $null
        }
        Invoke-WithLinuxToolchain -ToolchainRoot "C:\SelectedToolchain" `
            -Context $Context -Action {
                param($Value)
                $Value.Observed = "$($Value.Command) $($Value.Arguments -join ' ')"
            }
        $Context.Observed | Should -Be "RunUAT.bat BuildCookRun -platform=Linux"
    }

    It "restores the caller environment after failure" {
        { Invoke-WithLinuxToolchain -ToolchainRoot "C:\SelectedToolchain" -Action {
            throw "expected failure"
        } } | Should -Throw "expected failure"
        $Env:LINUX_MULTIARCH_ROOT | Should -Be "C:\PreviousToolchain"
    }
}
