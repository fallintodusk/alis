# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\operator_controls.ps1')
}

Describe 'ProjectWorld operator controls' {
    It 'permits explicit automation without reading interactive input' {
        Mock Read-Host { throw 'Read-Host must not run' }
        Confirm-ProjectWorldMutation `
            -Operation apply `
            -MapPackage '/ProjectWorldTestData/Generated/P0/L_Test' `
            -ParticipatingScopes @{ map_p0_l_test = @('generated-map') } `
            -ActiveSet $null `
            -AuthoredContentRoot 'authored-root' `
            -NonInteractive
        Should -Invoke Read-Host -Times 0
    }

    It 'refuses an operator response other than exact yes' {
        Mock Read-Host { 'no' }
        {
            Confirm-ProjectWorldMutation `
                -Operation apply `
                -MapPackage '/ProjectWorldTestData/Generated/P0/L_Test' `
                -ParticipatingScopes @{ map_p0_l_test = @('generated-map') } `
                -ActiveSet $null `
                -AuthoredContentRoot 'authored-root'
        } | Should -Throw '*did not type yes*'
    }

    It 'reports active and retired immutable manifest generations' {
        $root = Join-Path $TestDrive 'manifests'
        New-Item -ItemType Directory -Path (Join-Path $root 'scopes'), (Join-Path $root 'archive') -Force | Out-Null
        [ordered]@{
            scopes = @([ordered]@{ manifest_path = 'scopes/map_test.2.json' })
        } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $root 'active_set.json')
        [ordered]@{
            scope_id = 'map_test'
            generation = 2
            accepted_at_utc = '2026-08-11T12:00:00.0000000+00:00'
            accepted_operation_id = 'run-2'
        } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'scopes\map_test.2.json')
        [ordered]@{
            scope_id = 'map_test'
            generation = 1
            accepted_operation_id = 'run-1'
        } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'archive\map_test.1.json')

        $history = @(Get-ProjectWorldGenerationHistory -ManifestRoot $root)
        $history.Count | Should -Be 2
        $history[0].state | Should -Be 'retired'
        $history[0].accepted_at_utc | Should -Be 'not_recorded'
        $history[1].state | Should -Be 'active'
        $history[1].originating_run | Should -Be 'run-2'
    }
}
