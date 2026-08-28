# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Describe 'ProjectMaterial host transaction' -Tag 'Integration' {
    BeforeAll {
        $script:ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        $script:Wrapper = Join-Path $script:ProjectRoot 'scripts\ue\material\run_material_generation.ps1'
        $script:LockScript = Join-Path $script:ProjectRoot 'scripts\ue\generated_content\generated_content_mutation_lock.ps1'
        $script:TestRoot = Join-Path $script:ProjectRoot 'tmp\material\generation\wrapper_integration'
        $script:RecipeRoot = Join-Path $script:TestRoot 'recipes\Terrain'
        $script:ParentRecipePath = Join-Path $script:RecipeRoot 'M_ProjectTerrain.material.json'
        $script:InstanceRecipePath = Join-Path $script:RecipeRoot 'MI_ProjectTerrain_Default.material.json'
        $script:ParentPackage = Join-Path $script:TestRoot 'content\Generated\Terrain\M_ProjectTerrain.uasset'
        $script:InstancePackage = Join-Path $script:TestRoot 'content\Generated\Terrain\MI_ProjectTerrain_Default.uasset'
        $script:Manifest = Join-Path $script:TestRoot 'manifests\accepted.material-manifest.json'
        $script:EvidenceRoot = Join-Path $script:ProjectRoot 'tmp\material\generation\wrapper_integration_evidence'

        function Write-TestRecipes {
            param([double]$ParentRoughness = 0.9)
            New-Item -ItemType Directory -Path $script:RecipeRoot -Force | Out-Null
            $parent = @"
{
  "`$schema": "../../Schemas/material-recipe.schema.json",
  "schema_version": "1",
  "material_id": "M_ProjectTerrain",
  "artifact_kind": "parent",
  "family": "surface_opaque",
  "archetype": "landscape_basic_v1",
  "compiler_version": "1",
  "scalars": { "Roughness": $($ParentRoughness.ToString('0.000', [Globalization.CultureInfo]::InvariantCulture)), "SlopeContrast": 1.5 },
  "vectors": {
    "LowSlopeColor": [0.08, 0.18, 0.04, 1.0],
    "SteepSlopeColor": [0.16, 0.12, 0.08, 1.0]
  }
}
"@
            $instance = @'
{
  "$schema": "../../Schemas/material-recipe.schema.json",
  "schema_version": "1",
  "material_id": "MI_ProjectTerrain_Default",
  "artifact_kind": "instance",
  "family": "surface_opaque",
  "archetype": "landscape_basic_v1",
  "compiler_version": "1",
  "parent": "/ProjectMaterialTest/Generated/Terrain/M_ProjectTerrain.M_ProjectTerrain",
  "scalars": { "Roughness": 0.82, "SlopeContrast": 1.25 },
  "vectors": {
    "LowSlopeColor": [0.07, 0.20, 0.05, 1.0],
    "SteepSlopeColor": [0.19, 0.14, 0.09, 1.0]
  }
}
'@
            Set-Content -LiteralPath $script:ParentRecipePath -Value $parent -Encoding UTF8
            Set-Content -LiteralPath $script:InstanceRecipePath -Value $instance -Encoding UTF8
        }

        function Get-AcceptedHashes {
            return @(
                (Get-FileHash -LiteralPath $script:ParentPackage -Algorithm SHA256).Hash,
                (Get-FileHash -LiteralPath $script:InstancePackage -Algorithm SHA256).Hash,
                (Get-FileHash -LiteralPath $script:Manifest -Algorithm SHA256).Hash
            )
        }
    }

    BeforeEach {
        if (Test-Path -LiteralPath $script:TestRoot) {
            Remove-Item -LiteralPath $script:TestRoot -Recurse -Force
        }
        if (Test-Path -LiteralPath $script:EvidenceRoot) {
            Remove-Item -LiteralPath $script:EvidenceRoot -Recurse -Force
        }
        Write-TestRecipes
    }

    AfterAll {
        if (Test-Path -LiteralPath $script:TestRoot) {
            Remove-Item -LiteralPath $script:TestRoot -Recurse -Force
        }
        if ((Test-Path -LiteralPath $script:EvidenceRoot) -and
            -not (Test-Path -LiteralPath (Join-Path $script:EvidenceRoot 'Rejected'))) {
            Remove-Item -LiteralPath $script:EvidenceRoot -Recurse -Force
        }
        foreach ($emptyRoot in @(
            (Join-Path $script:ProjectRoot 'tmp\material\generation'),
            (Join-Path $script:ProjectRoot 'tmp\material'))) {
            if ((Test-Path -LiteralPath $emptyRoot) -and
                @(Get-ChildItem -LiteralPath $emptyRoot -Force).Count -eq 0) {
                Remove-Item -LiteralPath $emptyRoot -Force
            }
        }
    }

    It 'shares the global mutation lock with ProjectWorld' {
        . $script:LockScript
        $lock = Enter-ProjectGeneratedContentMutationLock `
            -ProjectRoot $script:ProjectRoot `
            -OwnerName 'integration-test'
        try {
            { & $script:Wrapper -Mode Validate -TestRoot $script:TestRoot -EvidencePath $script:EvidenceRoot } |
                Should -Throw '*content mutation lock*'
        }
        finally {
            $lock.Dispose()
        }
    }

    It 'refuses another Editor process for the same project without terminating it' {
        Mock Get-CimInstance {
            [pscustomobject]@{
                ProcessId = 4242
                CommandLine = "`"$script:ProjectRoot\Alis.uproject`" -log"
            }
        }
        { & $script:Wrapper `
            -Mode Validate `
            -TestRoot $script:TestRoot `
            -EvidencePath $script:EvidenceRoot } | Should -Throw '*Editor is running: pid=4242*'
        Assert-MockCalled Get-CimInstance -Times 1 -Exactly
    }

    It 'recovers an interrupted journal before admitting the next mutation' {
        $first = & $script:Wrapper -Mode Regenerate -TestRoot $script:TestRoot -EvidencePath $script:EvidenceRoot |
            ConvertFrom-Json
        $first.generated | Should -Be 2
        $acceptedHashes = Get-AcceptedHashes
        $interruptedId = 'a' * 32
        $transactionRoot = Join-Path $script:TestRoot 'transaction'
        $snapshotRoot = Join-Path $transactionRoot "$interruptedId\snapshot"
        New-Item -ItemType Directory -Path $snapshotRoot -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $script:TestRoot 'content\Generated') `
            -Destination (Join-Path $snapshotRoot 'output') -Recurse -Force
        Copy-Item -LiteralPath (Join-Path $script:TestRoot 'manifests') `
            -Destination (Join-Path $snapshotRoot 'manifests') -Recurse -Force
        $journal = [ordered]@{
            schema_version = '1'
            operation_id = $interruptedId
            mode = 'regenerate'
            output_root = [System.IO.Path]::GetFullPath((Join-Path $script:TestRoot 'content\Generated'))
            manifest_root = [System.IO.Path]::GetFullPath((Join-Path $script:TestRoot 'manifests'))
            snapshot_root = [System.IO.Path]::GetFullPath($snapshotRoot)
            output_was_present = $true
            manifest_was_present = $true
        }
        $journal | ConvertTo-Json -Depth 5 |
            Set-Content -LiteralPath (Join-Path $transactionRoot 'journal.json') -Encoding UTF8
        Set-Content -LiteralPath $script:ParentPackage -Value 'interrupted-candidate' -Encoding UTF8

        $recovered = & $script:Wrapper -Mode Regenerate -TestRoot $script:TestRoot -EvidencePath $script:EvidenceRoot |
            ConvertFrom-Json
        $recovered.generated | Should -Be 0
        $recovered.skipped | Should -Be 2
        (Get-AcceptedHashes) | Should -Be $acceptedHashes
        Test-Path -LiteralPath (Join-Path $transactionRoot 'journal.json') | Should -BeFalse
    }

    It 'is idempotent and restores exact accepted bytes after child rejection' {
        $first = & $script:Wrapper -Mode Regenerate -TestRoot $script:TestRoot -EvidencePath $script:EvidenceRoot |
            ConvertFrom-Json
        $first.generated | Should -Be 2
        $first.shader_compiles | Should -Be 1
        $acceptedHashes = Get-AcceptedHashes
        $acceptedTimes = @(
            (Get-Item -LiteralPath $script:ParentPackage).LastWriteTimeUtc.Ticks,
            (Get-Item -LiteralPath $script:InstancePackage).LastWriteTimeUtc.Ticks,
            (Get-Item -LiteralPath $script:Manifest).LastWriteTimeUtc.Ticks
        )

        Start-Sleep -Milliseconds 1100
        $second = & $script:Wrapper -Mode Regenerate -TestRoot $script:TestRoot -EvidencePath $script:EvidenceRoot |
            ConvertFrom-Json
        $second.generated | Should -Be 0
        $second.skipped | Should -Be 2
        (Get-AcceptedHashes) | Should -Be $acceptedHashes
        @(
            (Get-Item -LiteralPath $script:ParentPackage).LastWriteTimeUtc.Ticks,
            (Get-Item -LiteralPath $script:InstancePackage).LastWriteTimeUtc.Ticks,
            (Get-Item -LiteralPath $script:Manifest).LastWriteTimeUtc.Ticks
        ) | Should -Be $acceptedTimes

        Write-TestRecipes -ParentRoughness 0.7
        { & $script:Wrapper `
            -Mode Regenerate `
            -TestRoot $script:TestRoot `
            -EvidencePath $script:EvidenceRoot `
            -InjectFailure post-save } | Should -Throw '*rejected*'
        (Get-AcceptedHashes) | Should -Be $acceptedHashes
        Test-Path -LiteralPath (Join-Path $script:TestRoot 'transaction\journal.json') |
            Should -BeFalse

        $replacement = & $script:Wrapper -Mode Regenerate -TestRoot $script:TestRoot -EvidencePath $script:EvidenceRoot |
            ConvertFrom-Json
        $replacement.generated | Should -Be 2
        (Get-AcceptedHashes) | Should -Not -Be $acceptedHashes
        @(Get-ChildItem -LiteralPath $script:EvidenceRoot -Directory).Count |
            Should -BeLessOrEqual 2
    }
}
