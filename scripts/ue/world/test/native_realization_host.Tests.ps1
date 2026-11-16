BeforeAll {
    $repo = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
    $scratch = Join-Path $repo ('tmp/world/native_host_tests/' + [guid]::NewGuid().ToString('N'))
    $hostPath = Join-Path $scratch 'scripts/ue/world/test/integration/verify_native_realization.ps1'
    New-Item -ItemType Directory -Force -Path (Split-Path $hostPath), (Join-Path $scratch 'scripts/ue/standalone'),
        (Join-Path $scratch 'scripts/ue/test/unit') | Out-Null
    Copy-Item (Join-Path $PSScriptRoot 'integration/verify_native_realization.ps1') $hostPath
    @'
$env:HOST_BUILD_CALLED='1'
$global:LASTEXITCODE=0
'@ | Set-Content (Join-Path $scratch 'scripts/ue/standalone/build.ps1')
    @'
param($TestFilter,$Mode,$Map,$TimeoutSeconds,[switch]$UseTestExitOnly,$LogDirectory,$ExtraArgs)
@{filter=$TestFilter;mode=$Mode;map=$Map;timeout=$TimeoutSeconds;exitOnly=[bool]$UseTestExitOnly;
  logs=$LogDirectory;extra=$ExtraArgs;cold=$env:ALIS_NO_PERSISTENT_EDITOR} |
    ConvertTo-Json | Set-Content (Join-Path (Split-Path $LogDirectory) 'dispatch.json')
$global:LASTEXITCODE=7
'@ | Set-Content (Join-Path $scratch 'scripts/ue/test/unit/run_cpp_tests_safe.ps1')
    $shell=(Get-Process -Id $PID).Path
}
Describe 'Fixed native World suite cold envelope' {
    It 'forwards the complete envelope and preserves runner failure' {
        $evidence=Join-Path $scratch 'tmp/valid'
        & $shell -NoProfile -File $hostPath -EvidenceDirectory $evidence
        $LASTEXITCODE | Should -Be 7
        $dispatch=Get-Content (Join-Path $evidence 'dispatch.json') -Raw | ConvertFrom-Json
        $dispatch.filter | Should -Be 'Project.World.Realization'
        $dispatch.mode | Should -Be 'Gate'
        $dispatch.map | Should -Be '/Engine/Maps/Entry'
        $dispatch.timeout | Should -Be 360
        $dispatch.exitOnly | Should -BeTrue
        $dispatch.cold | Should -Be '1'
        $dispatch.extra | Should -Match 'ProjectSkipFrontEnd'
        $dispatch.extra | Should -Match 'EditorPerProjectUserSettingsINI='
        $dispatch.logs | Should -Be (Join-Path $evidence 'runner')
    }
    It 'refuses outside scratch before dispatch or directory creation' {
        $evidence=Join-Path $scratch 'outside'
        & $shell -NoProfile -File $hostPath -EvidenceDirectory $evidence 2>$null
        $LASTEXITCODE | Should -Not -Be 0
        Test-Path $evidence | Should -BeFalse
    }
    It 'refuses ambient diagnostic selection without creating output' -TestCases @(
        @{Variable='PROJECT_WORLD_STRUCTURE_SHA256'},
        @{Variable='PROJECT_WORLD_GROUND_SCENE_FIXTURE'}
    ) {
        param($Variable)
        $prior=[Environment]::GetEnvironmentVariable($Variable,'Process')
        try {
            [Environment]::SetEnvironmentVariable($Variable,'partial','Process')
            $evidence=Join-Path $scratch ('tmp/ambient_' + $Variable)
            & $shell -NoProfile -File $hostPath -EvidenceDirectory $evidence 2>$null
            $LASTEXITCODE | Should -Not -Be 0
            Test-Path $evidence | Should -BeFalse
        } finally { [Environment]::SetEnvironmentVariable($Variable,$prior,'Process') }
    }
    It 'refuses reuse of a previous evidence directory' {
        $evidence=Join-Path $scratch 'tmp/reused'
        New-Item -ItemType Directory -Force -Path $evidence | Out-Null
        & $shell -NoProfile -File $hostPath -EvidenceDirectory $evidence 2>$null
        $LASTEXITCODE | Should -Not -Be 0
        @(Get-ChildItem $evidence).Count | Should -Be 0
    }
}
