# ResolverConformance.Tests.ps1 - shared-corpus conformance for the
# PowerShell and batch resolvers (native runners for sh/py live in
# conformance_sh.sh / conformance_py.py; run_conformance.ps1 aggregates).
#
# Corpus: scripts/config/test/fixtures/<case>/ with ue_path.conf
# (+ optional ue_path.local.conf) and expected.txt ("ERROR" or KEY=VALUE
# lines). Same fixtures drive every language runner.
#
# Run: Invoke-Pester scripts/config/test/ResolverConformance.Tests.ps1

BeforeDiscovery {
    # -ForEach data must exist at DISCOVERY time (Pester v5), not in BeforeAll.
    $script:Cases = Get-ChildItem (Join-Path $PSScriptRoot "fixtures") -Directory |
        Sort-Object Name
}

BeforeAll {
    $script:ConfigDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
    $script:FixtureRoot = Join-Path $PSScriptRoot "fixtures"
    $script:RepoRoot = (Resolve-Path (Join-Path $script:ConfigDir "..\..")).Path
    . (Join-Path $script:ConfigDir "Resolve-UEConfig.ps1")
    $script:CurrentUEPath = (Resolve-UEConfig -ConfigDir $script:ConfigDir).UE_PATH

    function Get-ExpectedMap([string]$CaseDir) {
        $lines = @(Get-Content (Join-Path $CaseDir "expected.txt"))
        if ($lines[0].Trim() -eq "ERROR") { return $null }
        $map = @{}
        foreach ($l in $lines) {
            if ($l -match '^([A-Z_]+)=(.*)$') { $map[$Matches[1]] = $Matches[2] }
        }
        return $map
    }

    function New-BatSandbox([string]$CaseDir) {
        # resolve_ue_path.bat resolves confs next to itself -> copy the
        # resolver + fixture confs + a probe wrapper into a temp dir.
        $sandbox = Join-Path $TestDrive ([IO.Path]::GetRandomFileName())
        New-Item -ItemType Directory -Path $sandbox | Out-Null
        Copy-Item (Join-Path $script:ConfigDir "resolve_ue_path.bat") $sandbox
        Copy-Item (Join-Path $script:ConfigDir "Resolve-UEConfig.ps1") $sandbox
        foreach ($f in @("ue_path.conf", "ue_path.local.conf")) {
            $src = Join-Path $CaseDir $f
            if (Test-Path $src) { Copy-Item $src $sandbox }
        }
        Set-Content -Path (Join-Path $sandbox "probe.bat") -Value @"
@echo off
call "%~dp0resolve_ue_path.bat"
if errorlevel 1 exit /b 1
echo UE_PATH=%UE_PATH%
echo UE_SOURCE_PATH=%UE_SOURCE_PATH%
"@
        return $sandbox
    }

    function Invoke-BatProbe([string]$Sandbox, [string]$EnvUEPath) {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = "cmd.exe"
        $psi.Arguments = "/d /c `"$(Join-Path $Sandbox 'probe.bat')`""
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        if ($null -ne $EnvUEPath) { $psi.EnvironmentVariables["UE_PATH"] = $EnvUEPath }
        else { $psi.EnvironmentVariables.Remove("UE_PATH") | Out-Null }
        $p = [System.Diagnostics.Process]::Start($psi)
        $out = $p.StandardOutput.ReadToEnd()
        $err = $p.StandardError.ReadToEnd()
        $p.WaitForExit()
        return @{ ExitCode = $p.ExitCode; Out = $out; Err = $err }
    }

    function Invoke-MakeDryRun([string]$EnvUEPath) {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = (Get-Command make).Source
        $psi.Arguments = "-n generate"
        $psi.WorkingDirectory = $script:RepoRoot
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.EnvironmentVariables["UE_PATH"] = $EnvUEPath
        $p = [System.Diagnostics.Process]::Start($psi)
        $out = $p.StandardOutput.ReadToEnd()
        $err = $p.StandardError.ReadToEnd()
        $p.WaitForExit()
        return @{ ExitCode = $p.ExitCode; Out = $out; Err = $err }
    }
}

Describe "Resolve-UEConfig (ps1) conformance" {
    It "case <_.Name>" -ForEach $script:Cases {
        $expected = Get-ExpectedMap $_.FullName
        if ($null -eq $expected) {
            { Resolve-UEConfig -ConfigDir $_.FullName } | Should -Throw
        }
        else {
            $config = Resolve-UEConfig -ConfigDir $_.FullName
            foreach ($k in $expected.Keys) {
                $config[$k] | Should -Be $expected[$k] -Because "key $k"
            }
        }
    }
}

Describe "resolve_ue_path.bat conformance" {
    It "case <_.Name>" -ForEach $script:Cases {
        $expected = Get-ExpectedMap $_.FullName
        $sandbox = New-BatSandbox $_.FullName
        $result = Invoke-BatProbe -Sandbox $sandbox -EnvUEPath $null
        if ($null -eq $expected) {
            $result.ExitCode | Should -Be 1 -Because $result.Err
        }
        else {
            $result.ExitCode | Should -Be 0 -Because ($result.Out + $result.Err)
            foreach ($k in @("UE_PATH", "UE_SOURCE_PATH")) {
                if ($expected.ContainsKey($k)) {
                    $result.Out | Should -Match ([regex]::Escape("$k=$($expected[$k])"))
                }
            }
        }
    }
}

Describe "stale-env hard fail" {
    It "ps1 wrapper (env.ps1 semantics via Test-UEStaleEnv)" {
        $err = $null
        $saved = $Env:UE_PATH
        try {
            $Env:UE_PATH = "C:\TestEngine\UE_Old"
            $err = Test-UEStaleEnv -ResolvedUEPath "C:/TestEngine/UE_New"
        } finally { $Env:UE_PATH = $saved }
        $err | Should -Match "stale UE_PATH env"
    }

    It "ps1 accepts matching env in any normalized form" {
        $saved = $Env:UE_PATH
        try {
            $Env:UE_PATH = "/c/TestEngine/UE_Same/"
            (Test-UEStaleEnv -ResolvedUEPath "C:\TESTENGINE\UE_SAME") | Should -BeNullOrEmpty
        } finally { $Env:UE_PATH = $saved }
    }

    It "bat fails on stale env, passes on matching env" {
        $case = Join-Path $script:FixtureRoot "01_basic"
        $sandbox = New-BatSandbox $case
        (Invoke-BatProbe -Sandbox $sandbox -EnvUEPath "C:\TestEngine\UE_Other").ExitCode |
            Should -Be 1
        (Invoke-BatProbe -Sandbox $sandbox -EnvUEPath "C:\TestEngine\UE_Basic\").ExitCode |
            Should -Be 0
    }
}

Describe "native Windows make cache guard" {
    It "accepts a normalized matching cache without POSIX command leakage" {
        $result = Invoke-MakeDryRun ($script:CurrentUEPath -replace '/', '\')
        $result.ExitCode | Should -Be 0 -Because ($result.Out + $result.Err)
        ($result.Out + $result.Err) | Should -Not -Match "not recognized"
    }

    It "fails closed on a stale cache" {
        $result = Invoke-MakeDryRun "C:\DefinitelyStale\UE"
        $result.ExitCode | Should -Not -Be 0
        $result.Err | Should -Match "stale UE_PATH env"
    }
}

Describe "launcher fallback numeric ordering" {
    It "picks 5.10 over 5.9 (numeric, not lexical) and skips invalid roots" {
        $root = Join-Path $TestDrive "engines"
        foreach ($v in @("UE_5.9", "UE_5.10", "UE_5.11")) {
            $bin = Join-Path $root "$v\Engine\Binaries\Win64"
            New-Item -ItemType Directory -Path $bin -Force | Out-Null
            if ($v -ne "UE_5.11") {
                Set-Content -Path (Join-Path $bin "UnrealEditor-Cmd.exe") -Value "stub"
            }
        }
        # 5.11 lacks the editor binary -> invalid; 5.10 must win over 5.9
        $picked = Resolve-UELauncherFallback -Roots @($root)
        $picked | Should -Match "UE_5\.10$"
    }

    It "returns nothing when no valid install exists" {
        $empty = Join-Path $TestDrive "no-engines"
        New-Item -ItemType Directory -Path $empty -Force | Out-Null
        Resolve-UELauncherFallback -Roots @($empty) | Should -BeNullOrEmpty
    }
}
