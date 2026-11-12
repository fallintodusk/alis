# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

BeforeAll {
    $unit = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    $repo = (Resolve-Path (Join-Path $unit '..\..\..\..')).Path
    $probeRoot = Join-Path $repo ('tmp/world/test_runner_completion/' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $probeRoot -Force | Out-Null
    $sentinelExe = Join-Path $probeRoot 'UnrealEditor-OwnershipSentinel.exe'
    Copy-Item -LiteralPath (Join-Path $env:SystemRoot 'System32/ping.exe') -Destination $sentinelExe
    $harness = Join-Path $probeRoot 'completion.ps1'
    $body = @'
param([string]$EvidenceRoot, [string]$Helper, [string]$Mode, [int]$ChildExitCode)
$ErrorActionPreference = 'Stop'
. $Helper
$executable = (Get-Process -Id $PID).Path
$spawn = "function New-ProbeChild([string]`$payload) { `$s = New-Object Diagnostics.ProcessStartInfo; `$s.FileName = '$executable'; `$s.Arguments = '-NoProfile -NonInteractive -EncodedCommand ' + `$payload; `$s.UseShellExecute = `$false; `$s.CreateNoWindow = `$true; [Diagnostics.Process]::Start(`$s) };"
$rootCode = "`$ErrorActionPreference = 'Stop'; $spawn `$p = [Diagnostics.Process]::GetCurrentProcess(); @{ pid = `$p.Id; started_utc_ticks = `$p.StartTime.ToUniversalTime().Ticks.ToString() } | ConvertTo-Json | Set-Content -LiteralPath '$EvidenceRoot/root.process.json'; [Console]::Out.WriteLine('retained stdout'); [Console]::Error.WriteLine('retained stderr');"
if ($Mode -in @('timeout', 'timeout-failure', 'inherited-pipes')) {
    $grandCode = "[Console]::Out.WriteLine('grandchild pipe'); Start-Sleep -Seconds 60"
    $grandEncoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($grandCode))
    $descendantCode = "`$ErrorActionPreference = 'Stop'; $spawn `$p = New-ProbeChild '$grandEncoded'; @{ pid = `$p.Id; started_utc_ticks = `$p.StartTime.ToUniversalTime().Ticks.ToString() } | ConvertTo-Json | Set-Content -LiteralPath '$EvidenceRoot/grandchild.process.json';"
    if ($Mode -ne 'inherited-pipes') { $descendantCode += 'Start-Sleep -Seconds 60;' }
    $descendantEncoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($descendantCode))
    $rootCode += "`$p = New-ProbeChild '$descendantEncoded'; @{ pid = `$p.Id; started_utc_ticks = `$p.StartTime.ToUniversalTime().Ticks.ToString() } | ConvertTo-Json | Set-Content -LiteralPath '$EvidenceRoot/descendant.process.json';"
    $rootCode += "`$ready = [Diagnostics.Stopwatch]::StartNew(); while (-not (Test-Path '$EvidenceRoot/grandchild.process.json')) { if (`$ready.Elapsed.TotalSeconds -gt 4) { exit 33 }; Start-Sleep -Milliseconds 20 };"
    if ($Mode -ne 'inherited-pipes') { $rootCode += 'Start-Sleep -Seconds 60;' }
}
$rootCode += "exit $ChildExitCode"
$encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($rootCode))
$psi = New-Object Diagnostics.ProcessStartInfo
$psi.FileName = $executable
$psi.Arguments = "-NoProfile -NonInteractive -EncodedCommand $encoded"
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
if ($Mode -in @('cleanup-failure', 'timeout-failure')) {
    # Failure at the native termination boundary must refuse even when disposal cleans up.
    function Stop-ProjectOwnedTestProcess { return $false }
}
$limit = if ($Mode -in @('timeout', 'timeout-failure')) { 3 } else { 8 }
$result = Invoke-ProjectOwnedTestProcess -StartInfo $psi -TimeoutSeconds $limit -LogPath (Join-Path $EvidenceRoot 'streams.log') -ScratchRoot (Join-Path $EvidenceRoot 'compile')
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'result.json')
exit $result.ExitCode
'@
    Set-Content -LiteralPath $harness -Value $body

    function Get-ProbeProcesses([string]$Evidence) {
        foreach ($record in Get-ChildItem -LiteralPath $Evidence -Filter '*.process.json') {
            $identity = Get-Content -LiteralPath $record.FullName -Raw | ConvertFrom-Json
            try {
                $process = [Diagnostics.Process]::GetProcessById([int]$identity.pid)
                if ($process.StartTime.ToUniversalTime().Ticks.ToString() -eq $identity.started_utc_ticks -and -not $process.HasExited) {
                    $process
                } else { $process.Dispose() }
            } catch [ArgumentException] { }
            catch [InvalidOperationException] { }
        }
    }

    function Invoke-CompletionProbe([string]$Mode, [int]$ChildExitCode = 0) {
        $evidence = Join-Path $probeRoot ($Mode + '-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $evidence | Out-Null
        $sentinelInfo = New-Object Diagnostics.ProcessStartInfo
        $sentinelInfo.FileName = $sentinelExe
        $sentinelInfo.Arguments = '-t 127.0.0.1'
        $sentinelInfo.UseShellExecute = $false
        $sentinelInfo.CreateNoWindow = $true
        $sentinelInfo.RedirectStandardOutput = $true
        $sentinelInfo.RedirectStandardError = $true
        $sentinel = [Diagnostics.Process]::Start($sentinelInfo)
        $sentinelOutput = $sentinel.StandardOutput.ReadToEndAsync()
        $sentinelError = $sentinel.StandardError.ReadToEndAsync()
        $info = New-Object Diagnostics.ProcessStartInfo
        $info.FileName = (Get-Process -Id $PID).Path
        $helper = Join-Path $unit 'owned_test_process.ps1'
        if ($Mode -eq 'cold-start') {
            $runner = Join-Path $unit 'run_cpp_tests_safe.ps1'
            $info.Arguments = "-NoProfile -NonInteractive -File `"$runner`" -TestFilter Project.World.Realization.Constructed.DependencyPreparation -ExpectedTestNames Project.World.Realization.Constructed.DependencyPreparation -LogDirectory `"$evidence`""
        } else {
            $info.Arguments = "-NoProfile -NonInteractive -File `"$harness`" -EvidenceRoot `"$evidence`" -Helper `"$helper`" -Mode $Mode -ChildExitCode $ChildExitCode"
        }
        $info.UseShellExecute = $false
        $info.CreateNoWindow = $true
        $info.RedirectStandardOutput = $true
        $info.RedirectStandardError = $true
        $info.EnvironmentVariables['ALIS_NO_PERSISTENT_EDITOR'] = '1'
        $outer = New-Object Diagnostics.Process
        $outer.StartInfo = $info
        $clock = [Diagnostics.Stopwatch]::StartNew()
        try {
            $outer.Start() | Out-Null
            $output = $outer.StandardOutput.ReadToEndAsync()
            $errorOutput = $outer.StandardError.ReadToEndAsync()
            $finished = $outer.WaitForExit(25000)
            $code = if ($finished) { $outer.ExitCode } else { $null }
            $wait = [Diagnostics.Stopwatch]::StartNew()
            do {
                $remaining = @(Get-ProbeProcesses $evidence)
                if ($remaining.Count -eq 0) { break }
                foreach ($item in $remaining) { $item.Dispose() }
                Start-Sleep -Milliseconds 20
            } while ($wait.Elapsed.TotalSeconds -lt 3)
            $transcript = if ($finished -and [Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($output, $errorOutput), 3000)) {
                $output.Result + $errorOutput.Result
            } else { 'INCOMPLETE PROBE TRANSCRIPT' }
            Set-Content -LiteralPath (Join-Path $evidence 'probe.log') -Value $transcript
            return @{ Finished = $finished; ExitCode = $code; Seconds = $clock.Elapsed.TotalSeconds;
                Log = (Join-Path $evidence 'streams.log'); Transcript = $transcript;
                SentinelAlive = -not $sentinel.HasExited; RemainingOwned = $remaining.Count;
                RecordedOwned = @(Get-ChildItem -LiteralPath $evidence -Filter '*.process.json').Count;
                UniqueOwned = @(Get-ChildItem -LiteralPath $evidence -Filter '*.process.json' | ForEach-Object {
                    (Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json).pid
                } | Sort-Object -Unique).Count }
        } finally {
            if (-not $outer.HasExited) { $outer.Kill(); $outer.WaitForExit(3000) | Out-Null }
            foreach ($process in @(Get-ProbeProcesses $evidence)) { $process.Kill(); $process.WaitForExit(3000) | Out-Null; $process.Dispose() }
            if (-not $sentinel.HasExited) { $sentinel.Kill(); $sentinel.WaitForExit(3000) | Out-Null }
            $sentinel.Dispose()
            $outer.Dispose()
        }
    }
}

Describe 'Native test runner owned-process completion' {
    It 'refuses actual cold startup while an unrelated named editor sentinel survives' {
        $result = Invoke-CompletionProbe 'cold-start'
        $result.Finished | Should -BeTrue
        $result.ExitCode | Should -Be 2
        $result.Transcript | Should -Match 'Cold test refused while pre-existing Unreal processes'
        $result.SentinelAlive | Should -BeTrue
    }

    It 'times out only the owned root, child and grandchild while the unrelated sentinel survives' {
        $result = Invoke-CompletionProbe 'timeout'
        $result.Finished | Should -BeTrue
        $result.ExitCode | Should -Be 124
        $result.SentinelAlive | Should -BeTrue
        $result.RecordedOwned | Should -Be 3
        $result.UniqueOwned | Should -Be 3
        $result.RemainingOwned | Should -Be 0
        Get-Content -LiteralPath $result.Log -Raw | Should -Match 'retained stdout'
        Get-Content -LiteralPath $result.Log -Raw | Should -Match 'retained stderr'
    }

    It 'refuses inherited-pipe evidence and cleans a grandchild after its intermediate parent exits' {
        $result = Invoke-CompletionProbe 'inherited-pipes'
        $result.Finished | Should -BeTrue
        $result.ExitCode | Should -Be 1
        $result.SentinelAlive | Should -BeTrue
        $result.RecordedOwned | Should -Be 3
        $result.UniqueOwned | Should -Be 3
        $result.RemainingOwned | Should -Be 0
        Get-Content -LiteralPath $result.Log -Raw | Should -Match 'redirected output did not drain within grace'
    }

    It 'retains normal output and exit code <ChildExitCode>' -ForEach @(@{ChildExitCode=0}, @{ChildExitCode=7}) {
        $result = Invoke-CompletionProbe 'normal' $ChildExitCode
        $result.Finished | Should -BeTrue
        $result.ExitCode | Should -Be $ChildExitCode
        $result.SentinelAlive | Should -BeTrue
        $result.RemainingOwned | Should -Be 0
        Get-Content -LiteralPath $result.Log -Raw | Should -Match 'retained stdout'
        Get-Content -LiteralPath $result.Log -Raw | Should -Match 'retained stderr'
    }

    It 'refuses unverified termination in <Mode> while Job disposal still cleans owned children' -ForEach @(
        @{Mode='cleanup-failure'; Expected=1; Owned=1}, @{Mode='timeout-failure'; Expected=124; Owned=3}) {
        $result = Invoke-CompletionProbe $Mode
        $result.Finished | Should -BeTrue
        $result.ExitCode | Should -Be $Expected
        $result.SentinelAlive | Should -BeTrue
        $result.RecordedOwned | Should -Be $Owned
        $result.UniqueOwned | Should -Be $Owned
        $result.RemainingOwned | Should -Be 0
        Get-Content -LiteralPath $result.Log -Raw | Should -Match 'Owned Job did not become empty within termination grace'
    }
}
