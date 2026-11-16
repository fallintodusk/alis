BeforeAll {
    $repo=(Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
    . (Join-Path $repo 'scripts/ue/world/generated_manifest.ps1')
    . (Join-Path $repo 'scripts/ue/world/selected_refinement_consumers.ps1')
    $fixture=Join-Path $repo ('tmp/world/selected_consumer_tests/'+[guid]::NewGuid().ToString('N'))
    $performance=Join-Path $fixture 'scripts/ue/world/test/performance'
    New-Item -ItemType Directory -Force -Path $performance,(Join-Path $fixture 'Plugins/World/ProjectWorldData') | Out-Null
    Copy-Item (Join-Path $PSScriptRoot 'performance/project_world_performance_evidence.ps1') $performance
    foreach($row in @(@('Kazan','run_kazan_playable_tour.ps1'),@('Manhattan','run_manhattan_showcase_prototype.ps1'))) {
        $script=@'
param([switch]$DescribeOperation)
[pscustomobject]@{Name='CITY';Map='/CITY';Runtime='CITY_runtime';RuntimePath=(Join-Path $PSScriptRoot 'CITY.json')}
'@.Replace('CITY',$row[0])
        $script | Set-Content (Join-Path $performance $row[1])
    }
    '{}' | Set-Content (Join-Path $fixture 'plan.json')
    $request=[pscustomobject]@{consumer_verification=[pscustomobject]@{vantage_plan_path='plan.json';
        vantage_plan_sha256=(Get-ProjectWorldFileSha256 (Join-Path $fixture 'plan.json'))}}
    $settings=[pscustomobject]@{world_data_plugin='ProjectWorldData';map_package='/Manhattan'}
    $profile=[pscustomobject]@{profiles=[pscustomobject]@{
        kazan=[pscustomobject]@{world_data_plugin='ProjectWorldData';map_package='/Kazan';runtime_profile='scripts/ue/world/test/performance/Kazan.json'}
        manhattan=[pscustomobject]@{world_data_plugin='ProjectWorldData';map_package='/Manhattan';runtime_profile='scripts/ue/world/test/performance/Manhattan.json'}
    }}
}
Describe 'Selected runtime consumer admission' {
    It 'keeps the generic publication-only route unchanged' {
        {Assert-ProjectWorldSelectedConsumers -ProjectRoot $fixture -Request ([pscustomobject]@{}) -Profile $null -Settings $null} |
            Should -Not -Throw
    }
    It 'admits the exact two existing operation owners before World mutation' {
        {Assert-ProjectWorldSelectedConsumers -ProjectRoot $fixture -Request $request -Profile $profile -Settings $settings} |
            Should -Not -Throw
    }
    It 'refuses an unsupported owner or mismatched product runtime' {
        $wrong=[pscustomobject]@{world_data_plugin='Other';map_package='/Manhattan'}
        {Assert-ProjectWorldSelectedConsumers -ProjectRoot $fixture -Request $request -Profile $profile -Settings $wrong} | Should -Throw
        $old=$profile.profiles.manhattan.runtime_profile
        try {
            $profile.profiles.manhattan.runtime_profile='different.json'
            {Assert-ProjectWorldSelectedConsumers -ProjectRoot $fixture -Request $request -Profile $profile -Settings $settings} | Should -Throw
        } finally {$profile.profiles.manhattan.runtime_profile=$old}
    }
    It 'refuses changed plan bytes before any native work' {
        $old=$request.consumer_verification.vantage_plan_sha256
        try {
            $request.consumer_verification.vantage_plan_sha256='f'*64
            {Assert-ProjectWorldSelectedConsumers -ProjectRoot $fixture -Request $request -Profile $profile -Settings $settings} | Should -Throw
        } finally {$request.consumer_verification.vantage_plan_sha256=$old}
    }
    It 'records unchanged authority when a stale post-builder input refuses' {
        $evidence=Join-Path $fixture 'tmp/evidence'
        New-Item -ItemType Directory -Force -Path $evidence | Out-Null
        '{"layer_inventories":[{"layer_id":"terrain","canonical_inputs":[{}]}]}' |
            Set-Content (Join-Path $evidence 'saved-native.json')
        '{"status":"accepted","compile_result_sha256":"wrong","sections":[{}]}' |
            Set-Content (Join-Path $evidence 'terrain-postbuild-inventory.json')
        $request | Add-Member NoteProperty validation_profile_path 'plan.json' -Force
        $world=Join-Path $fixture 'Plugins/World/ProjectWorldData/world.json'
        $canonical=Join-Path $fixture 'Plugins/World/ProjectWorldData/canonical.json'
        '{}' | Set-Content $world,$canonical
        $before=Get-ProjectWorldFileSha256 $world
        {Invoke-ProjectWorldSelectedConsumers -ProjectRoot $fixture -Request $request -Profile $profile -Settings $settings `
            -Candidate ([pscustomobject]@{result_sha256=('a'*64)}) -EvidenceRoot $evidence -WorldPath $world -CanonicalPath $canonical} |
            Should -Throw '*census is unqualified*'
        $receipt=Get-Content (Join-Path $evidence 'consumer-verification.json') -Raw | ConvertFrom-Json
        $receipt.status | Should -Be 'rejected'
        $receipt.unchanged | Should -BeTrue
        $receipt.authority_after_sha256 | Should -Be $receipt.authority_before_sha256
        Get-ProjectWorldFileSha256 $world | Should -Be $before
    }
}
