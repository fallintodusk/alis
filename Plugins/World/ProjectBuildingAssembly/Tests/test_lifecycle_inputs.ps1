$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$token=[Guid]::NewGuid().ToString('N')
$root=Join-Path $repo "tmp/world/building_assembly/runner_inputs/$token"
$inputs=Join-Path $repo "tmp/world/inputs/building_assembly/runner_inputs/$token"
New-Item -ItemType Directory -Path $root,$inputs -Force | Out-Null
$kit=Join-Path $inputs 'kit.json';$selection=Join-Path $inputs 'selection.json'
'{}' | Set-Content -LiteralPath $kit -Encoding ASCII
'{}' | Set-Content -LiteralPath $selection -Encoding ASCII
$tracked=Join-Path $repo 'Plugins/World/ProjectBuildingAssembly/Data/Kits/original_test_v1.json'
$before=(Get-FileHash -LiteralPath $tracked).Hash
$rows=@()
foreach ($scenario in @('kit-tracked','selection-tracked','kit-outside','selection-outside','aliased','valid')) {
    $case=Join-Path $root $scenario
    # Existing evidence is a stop before locking or UE if input admission succeeds.
    New-Item -ItemType Directory -Path (Join-Path $case 'lifecycle') -Force | Out-Null
    @{world_data_plugin='ProjectWorldTestData';layers=@()} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $case 'realization.json') -Encoding ASCII
    $value=[ordered]@{kit=$kit;selection=$selection}
    switch ($scenario) {
        kit-tracked {$value.kit=$tracked}
        selection-tracked {$value.selection=$tracked}
        kit-outside {$value.kit='../outside-kit.json'}
        selection-outside {$value.selection='../outside-selection.json'}
        aliased {$value.selection=$value.kit}
    }
    $value | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $case 'case-inputs.json') -Encoding ASCII
    $expected=switch ($scenario) {
        aliased {'Kit and selection inputs must be distinct.'}
        valid {'Lifecycle evidence must be fresh.'}
        default {'Lifecycle inputs must be existing files strictly below tmp/world/inputs/.'}
    }
    $observed=$null
    try {& (Join-Path $PSScriptRoot 'run_lifecycle.ps1') -CaseRoot $case} catch {$observed=$_.Exception.Message}
    if ($observed -cne $expected) {throw "Unexpected refusal for ${scenario}: $observed"}
    if ((Get-FileHash -LiteralPath $tracked).Hash -cne $before) {throw 'Protected kit changed.'}
    foreach ($input in @($kit,$selection)) {
        if ((Get-Content -LiteralPath $input -Raw).Trim() -cne '{}') {throw 'Preflight changed an input.'}
    }
    $rows+=@([ordered]@{scenario=$scenario;refusal=$observed;inputs_unchanged=$true})
}
$rows | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'receipt.json') -Encoding ASCII
Write-Output "PASS six input preflight paths; no native work. Evidence: $root"
