BeforeAll {
    . (Join-Path $PSScriptRoot '../../generated_content/generated_content_outer_recovery.ps1')
    . (Join-Path $PSScriptRoot '../realization_layer_operation.ps1')
    . (Join-Path $PSScriptRoot '../layer_execution_selection.ps1')
    . (Join-Path $PSScriptRoot '../canonical_refinement_operation.ps1')
    . (Join-Path $PSScriptRoot '../selected_refinement_gameplay.ps1')
    . (Join-Path $PSScriptRoot 'canonical_compatibility_test_helpers.ps1')
    $repository=(Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
    $parent=Join-Path $repository 'tmp/world/saved_gameplay_tests'
}
Describe 'Saved product Hero evidence preparation' {
    BeforeEach {
        $root=Join-Path $parent ([guid]::NewGuid().ToString('N'))
        $manifestRoot=Join-Path $root 'Plugins/World/ProjectWorldTestData/Data/Manifests'
        $schemas=Join-Path $root 'Plugins/World/ProjectWorld/Data/Schemas'
        New-Item -ItemType Directory -Path $schemas,$manifestRoot,(Join-Path $root 'tmp/world/reference') -Force | Out-Null
        foreach($name in @('project_world_active_manifest_set.schema.json','project_world_generated_manifest.schema.json')) {
            Copy-Item (Join-Path $repository ('Plugins/World/ProjectWorld/Data/Schemas/'+$name)) $schemas
        }
        Write-ProjectWorldJson -Path (Join-Path $root 'Plugins/World/ProjectWorldTestData/ProjectWorldTestData.uplugin') -Document @{FileVersion=3;CanContainContent=$true}
        New-Item -ItemType Directory -Path (Join-Path $root 'Plugins/World/ProjectWorldTestData/Content') -Force | Out-Null
        $map='/ProjectWorldTestData/Generated/Representative/L_fixture'
        $mapScope=Get-ProjectWorldMapScopeId -MapPackage $map -GeneratedPackageRoot '/ProjectWorldTestData/Generated/'
        $fixture=New-CompatibilityFixture -ProjectRoot $root -ManifestRoot $manifestRoot -MapScope $mapScope
        $roots=Resolve-ProjectWorldDataRoots -ProjectRoot $root -PluginName ProjectWorldTestData
        Mock Resolve-ProjectWorldDataRoots {$roots}
        $plugin='tmp/world/reference/Reference.uplugin';$original='tmp/world/reference/original.json';$scene='tmp/world/reference/scene.json'
        Write-ProjectWorldJson -Path (Join-Path $root $plugin) -Document @{FileVersion=3;CanContainContent=$true}
        Write-ProjectWorldJson -Path (Join-Path $root $original) -Document @{canonical_result=@{sha256=('a'*64)}}
        $source=@{original_fixture=$original;saved_files=@{}}
        foreach($label in @($plugin,$original)){$source.saved_files[$label]=Get-ProjectWorldFileSha256 (Join-Path $root $label)}
        Write-ProjectWorldJson -Path (Join-Path $root $scene) -Document $source
        $inputPath='tmp/world/reference/input.json'
        Write-ProjectWorldJson -Path (Join-Path $root $inputPath) -Document @{scene_input=@{path=$scene;sha256=(Get-ProjectWorldFileSha256 (Join-Path $root $scene))}}
        $reference=@{path=$inputPath;sha256=(Get-ProjectWorldFileSha256 (Join-Path $root $inputPath))}
        $arguments=@{ProjectRoot=$root;Map=$fixture.Before.map_package;CompileSha256=('a'*64);Reference=$reference;
            OutputDirectory=(Join-Path $root 'tmp/world/output')}
    }
    AfterEach {
        $target=[IO.Path]::GetFullPath($root)
        if(-not $target.StartsWith(([IO.Path]::GetFullPath($parent)+'\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup escapes scratch.'}
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    It 'copies only current map and floor manifests without creating another authority' {
        $before=Get-ProjectWorldManifestRootSha256 $manifestRoot
        $prepared=New-ProjectWorldSavedRegionProof @arguments
        $proof=Get-Content $prepared.Proof -Raw | ConvertFrom-Json
        @($proof.manifests).Count | Should -Be 2
        $proof.world_active.sha256 | Should -Be $fixture.Active.Sha256
        $proof.reference_gameplay_sha256 | Should -Be $reference.sha256
        (Get-ProjectWorldManifestRootSha256 $manifestRoot) | Should -Be $before
        (Get-ProjectWorldFileSha256 $prepared.Input) | Should -Be $reference.sha256
    }
    It 'refuses another child before creating evidence or mutating World' {
        $arguments.CompileSha256='b'*64
        {New-ProjectWorldSavedRegionProof @arguments} | Should -Throw '*another canonical result*'
        Test-Path $arguments.OutputDirectory | Should -BeFalse
        (Read-ProjectWorldActiveSet -ProjectRoot $root -ManifestRoot $manifestRoot).Sha256 | Should -Be $fixture.Active.Sha256
    }
    It 'refuses a hash-correct code-bearing reference plugin before loading Unreal' {
        @{FileVersion=3;CanContainContent=$true;Modules=@(@{Name='Unexpected'})} | ConvertTo-Json -Depth 8 |
            Set-Content -LiteralPath (Join-Path $root $plugin) -Encoding ASCII
        $source.saved_files[$plugin]=Get-ProjectWorldFileSha256 (Join-Path $root $plugin)
        $source | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root $scene) -Encoding ASCII
        @{scene_input=@{path=$scene;sha256=(Get-ProjectWorldFileSha256 (Join-Path $root $scene))}} |
            ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root $inputPath) -Encoding ASCII
        $reference.sha256=Get-ProjectWorldFileSha256 (Join-Path $root $inputPath)
        {New-ProjectWorldSavedRegionProof @arguments} | Should -Throw '*content-only*'
        Test-Path $arguments.OutputDirectory | Should -BeFalse
    }
    It 'restores every configured pawn environment value when native walking refuses' {
        $prepared=New-ProjectWorldSavedRegionProof @arguments
        Mock New-ProjectWorldSavedRegionProof {$prepared}
        $runner=Join-Path $root 'scripts/ue/test/unit/run_cpp_tests_safe.ps1'
        New-Item -ItemType Directory -Path (Split-Path $runner) -Force | Out-Null
        @'
param($Map,$TestFilter,$ExpectedTestNames,$Mode,[switch]$UseTestExitOnly,$TimeoutSeconds,$LogDirectory,$ExtraArgs)
@{mode=$env:PROJECT_WORLD_STRUCTURE_GAME_MODE;pawn=$env:PROJECT_WORLD_STRUCTURE_PAWN_CLASS;
  definition=$env:PROJECT_WORLD_STRUCTURE_PAWN_DEFINITION;extra=$ExtraArgs} | ConvertTo-Json |
  Set-Content -LiteralPath (Join-Path (Split-Path $LogDirectory) 'dispatch.json')
$global:LASTEXITCODE=7
'@ | Set-Content -LiteralPath $runner -Encoding ASCII
        $names=@('PROJECT_WORLD_STRUCTURE_GAME_MODE','PROJECT_WORLD_STRUCTURE_PAWN_CLASS','PROJECT_WORLD_STRUCTURE_PAWN_DEFINITION')
        $prior=@{};foreach($name in $names){$prior[$name]=[Environment]::GetEnvironmentVariable($name);[Environment]::SetEnvironmentVariable($name,'prior-'+$name)}
        try {
            {Invoke-ProjectWorldSavedRegionGameplay @arguments} | Should -Throw '*Hero walking refused*'
            foreach($name in $names){[Environment]::GetEnvironmentVariable($name) | Should -Be ('prior-'+$name)}
            $dispatch=Get-Content (Join-Path $arguments.OutputDirectory 'dispatch.json') -Raw | ConvertFrom-Json
            $dispatch.definition | Should -Be 'ObjectDefinition:Hero'
            $dispatch.extra | Should -Match 'ProjectWorldSavedRegionGameplayHash='
        } finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$prior[$name])}}
    }
}
