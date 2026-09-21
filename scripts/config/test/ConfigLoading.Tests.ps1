# ConfigLoading.Tests.ps1 - Pester tests for Resolve-UEConfig shared function
#
# Covers the parts NOT exercised by the shared conformance corpus
# (ResolverConformance.Tests.ps1): defaults, ConfigFile/ConfigFiles
# bookkeeping, build-settings parsing, UE_SOURCE_PATH plumbing.
# Grammar + authority conformance lives in the corpus suite.
#
# Usage:
#   powershell scripts/config/test/Run-Tests.ps1
#   powershell scripts/config/test/Run-Tests.ps1 -Detailed

BeforeAll {
    $Script:ConfigDir = Split-Path -Parent $PSScriptRoot
    . (Join-Path $Script:ConfigDir "Resolve-UEConfig.ps1")
}

Describe "Resolve-UEConfig" {

    BeforeEach {
        $Script:TempDir = Join-Path $TestDrive (New-Guid).ToString()
        New-Item -ItemType Directory -Path $TempDir | Out-Null
    }

    Context "Only ue_path.conf exists (default)" {

        It "Should load UE_PATH from ue_path.conf" {
            Set-Content (Join-Path $TempDir "ue_path.conf") "UE_PATH=C:/TestEngine/UE_Default"
            $result = Resolve-UEConfig -ConfigDir $TempDir
            $result.UE_PATH | Should -Be "C:/TestEngine/UE_Default"
        }

        It "Should return defaults when no config files exist" {
            $result = Resolve-UEConfig -ConfigDir $TempDir
            $result.UE_PATH | Should -BeNullOrEmpty
            $result.UE_SOURCE_PATH | Should -BeNullOrEmpty
            $result.LINUX_MULTIARCH_ROOT | Should -BeNullOrEmpty
            $result.BUILD_TARGET | Should -Be "AlisEditor"
            $result.BUILD_CONFIG | Should -Be "Development"
            $result.BUILD_PLATFORM | Should -Be "Win64"
        }
    }

    Context "ue_path.local.conf overrides ue_path.conf (per key)" {

        It "Should prefer local.conf value over conf" {
            Set-Content (Join-Path $TempDir "ue_path.conf") "UE_PATH=C:/Default"
            Set-Content (Join-Path $TempDir "ue_path.local.conf") "UE_PATH=D:/Local"

            $result = Resolve-UEConfig -ConfigDir $TempDir
            $result.UE_PATH | Should -Be "D:/Local"
            $result.ConfigFile | Should -BeLike "*ue_path.local.conf"
            $result.ConfigFiles.Count | Should -Be 2
        }

        It "Should fall back to conf when local.conf does not exist" {
            Set-Content (Join-Path $TempDir "ue_path.conf") "UE_PATH=C:/Default"

            $result = Resolve-UEConfig -ConfigDir $TempDir
            $result.UE_PATH | Should -Be "C:/Default"
            (Split-Path -Leaf $result.ConfigFile) | Should -Be "ue_path.conf"
        }

        It "Should merge key subsets across both files" {
            Set-Content (Join-Path $TempDir "ue_path.conf") "UE_PATH=C:/Default"
            Set-Content (Join-Path $TempDir "ue_path.local.conf") @(
                "UE_SOURCE_PATH=G:/SourceEngine",
                "LINUX_MULTIARCH_ROOT=C:/UnrealToolchains/v26_clang-20.1.8-rockylinux8"
            )

            $result = Resolve-UEConfig -ConfigDir $TempDir
            $result.UE_PATH | Should -Be "C:/Default"
            $result.UE_SOURCE_PATH | Should -Be "G:/SourceEngine"
            $result.LINUX_MULTIARCH_ROOT | Should -Be "C:/UnrealToolchains/v26_clang-20.1.8-rockylinux8"
        }
    }

    Context "Build config values" {

        It "Should parse all build settings from local.conf" {
            Set-Content (Join-Path $TempDir "ue_path.local.conf") @(
                "UE_PATH=C:/TestEngine",
                "BUILD_TARGET=AlisServer",
                "BUILD_CONFIG=Shipping",
                "BUILD_PLATFORM=Linux"
            )

            $result = Resolve-UEConfig -ConfigDir $TempDir
            $result.UE_PATH | Should -Be "C:/TestEngine"
            $result.BUILD_TARGET | Should -Be "AlisServer"
            $result.BUILD_CONFIG | Should -Be "Shipping"
            $result.BUILD_PLATFORM | Should -Be "Linux"
        }

        It "Should use defaults for missing build settings" {
            Set-Content (Join-Path $TempDir "ue_path.conf") "UE_PATH=C:/TestEngine"

            $result = Resolve-UEConfig -ConfigDir $TempDir
            $result.BUILD_TARGET | Should -Be "AlisEditor"
            $result.BUILD_CONFIG | Should -Be "Development"
            $result.BUILD_PLATFORM | Should -Be "Win64"
        }
    }

    Context "Authority model (conf-first; env is a derived cache)" {

        It "Resolve-UEConfig itself NEVER consults env UE_PATH (pure reader)" {
            # This purity is what lets setup_ue_env.ps1 repair a stale
            # cache without a bootstrap deadlock. Guard it.
            Set-Content (Join-Path $TempDir "ue_path.conf") "UE_PATH=C:/FromFile"
            $saved = $Env:UE_PATH
            try {
                $Env:UE_PATH = "E:/FromEnvVar"
                $result = Resolve-UEConfig -ConfigDir $TempDir
                $result.UE_PATH | Should -Be "C:/FromFile"
            } finally { $Env:UE_PATH = $saved }
        }
    }

    Context "Live repository conf" {

        It "Tracked scripts/config/ue_path.conf parses strictly" {
            $result = Resolve-UEConfig -ConfigDir $Script:ConfigDir
            $result.UE_PATH | Should -Not -BeNullOrEmpty
            $result.UE_SOURCE_PATH | Should -Not -BeNullOrEmpty
            $result.LINUX_MULTIARCH_ROOT | Should -Not -BeNullOrEmpty
        }
    }
}
