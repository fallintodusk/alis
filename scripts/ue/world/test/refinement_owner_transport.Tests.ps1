# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')
    . (Join-Path $PSScriptRoot '..\canonical_selection_transition.ps1')
    . (Join-Path $PSScriptRoot '..\canonical_refinement_operation.ps1')
    $script:PythonExecutable=(Get-Command python -CommandType Application | Select-Object -First 1).Source
    $script:RepositoryRoot=(Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
    $script:TransportParent=Join-Path $script:RepositoryRoot 'tmp/world/refinement_transport_tests'
}
Describe 'Public refinement subprocess transport' {
    BeforeEach {
        $projectRoot=Join-Path $script:TransportParent (([guid]::NewGuid().ToString('N'))+' with spaces')
        foreach ($owner in @('CanonicalCompilation','EndToEndValidation')) {
            $path=Join-Path $projectRoot ('tools/World/'+$owner+'/bootstrap.py')
            New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
            [IO.File]::WriteAllText($path,"import json,sys`nprint(json.dumps({'status':'accepted','state':'prior','arguments':sys.argv[1:]}))`n")
        }
        Mock Get-Command { @([pscustomobject]@{Source=$script:PythonExecutable},[pscustomobject]@{Source='shadowed-python.exe'}) } `
            -ParameterFilter {$Name -eq 'python' -and $CommandType -eq 'Application'}
    }
    AfterEach {
        $target=[IO.Path]::GetFullPath($projectRoot)
        if (-not $target.StartsWith(([IO.Path]::GetFullPath($script:TransportParent)+'\'),[StringComparison]::OrdinalIgnoreCase)) {throw 'Cleanup escapes scratch.'}
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    It 'uses only the first PATH application for candidate-owner calls and preserves argument boundaries' {
        $result=Invoke-ProjectWorldRefinementOwner -ProjectRoot $projectRoot -Owner EndToEndValidation `
            -Arguments @('verify-refinement-inputs','--selection','path with spaces.json') -EvidenceDirectory (Join-Path $projectRoot 'tmp/evidence')
        $result.arguments.Count | Should -Be 3
        $result.arguments[2] | Should -Be 'path with spaces.json'
        $result.status | Should -Be accepted
    }
    It 'uses the same first PATH application for canonical recovery calls' {
        $result=Invoke-ProjectWorldCanonicalSelection -ProjectRoot $projectRoot -Transition @{fixture='ticket'} -Action validate
        $result.status | Should -Be accepted
        $result.arguments[0] | Should -Be selection-transition
        $result.arguments[-1] | Should -Be validate
        Test-Path -LiteralPath $result.arguments[2] | Should -BeTrue
    }
}
