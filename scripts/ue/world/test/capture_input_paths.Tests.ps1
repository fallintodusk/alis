# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
BeforeAll {
    $worldScripts = Split-Path $PSScriptRoot -Parent
    . (Join-Path $worldScripts 'capture_input_paths.ps1')
    $repo = [IO.Path]::GetFullPath((Join-Path $worldScripts '../../..'))
    $fixture = Join-Path $repo ('tmp/world/capture_input_paths/' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $fixture -Force | Out-Null
    $plan = Join-Path $fixture 'plan.json'
    Set-Content -LiteralPath $plan -Value '{"capture_width":1280,"capture_height":720,"vantages":[]}'
    $relative = $plan.Substring($repo.Length + 1)
}

Describe 'Public capture input path contract' {
    It 'absolute and repository-relative callers resolve the same actual file' {
        $absolute = Resolve-ProjectWorldCapturePlanPath -ProjectRoot $repo -VantagePlan $plan
        $fromRepo = Resolve-ProjectWorldCapturePlanPath -ProjectRoot $repo -VantagePlan $relative
        $absolute | Should -BeExactly ([IO.Path]::GetFullPath($plan))
        $fromRepo | Should -BeExactly $absolute
    }

    It 'missing input refuses before output or editor work' {
        $before = @(Get-ChildItem -LiteralPath $fixture -Recurse -File | ForEach-Object FullName)
        { Resolve-ProjectWorldCapturePlanPath -ProjectRoot $repo -VantagePlan (Join-Path $fixture 'missing.json') } |
            Should -Throw '*Vantage plan does not exist*'
        @(Get-ChildItem -LiteralPath $fixture -Recurse -File | ForEach-Object FullName) | Should -BeExactly $before
    }
}
