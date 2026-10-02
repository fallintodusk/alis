# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Describe 'ProjectMaterial surface host transaction' -Tag 'Integration' {
    BeforeAll {
        $script:ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        $script:Wrapper = Join-Path $script:ProjectRoot 'scripts\ue\material\run_surface_generation.ps1'
        $script:TestRoot = Join-Path $script:ProjectRoot 'tmp\material\generation\surface_wrapper'
        $script:RecipeRoot = Join-Path $script:TestRoot 'recipes'
        $script:SourceRecipes = Join-Path $script:ProjectRoot 'Plugins\Resources\ProjectMaterial\Data\SurfaceRecipes'
        $script:OutputRoot = Join-Path $script:TestRoot 'content\Surfaces'
        $script:Manifest = Join-Path $script:TestRoot 'manifests\accepted.surface-manifest.json'
        $script:EvidenceRoot = Join-Path $script:ProjectRoot 'tmp\material\generation\surface_wrapper_evidence'
        $script:LayoutReceipt = Join-Path $script:ProjectRoot 'Plugins\World\ProjectWorldMeshTerrain\Data\TestFixtures\mesh-terrain-layout-receipt.json'

        function Copy-TestRecipes {
            New-Item -ItemType Directory -Path $script:RecipeRoot -Force | Out-Null
            Copy-Item -LiteralPath (Join-Path $script:SourceRecipes 'Terrain') `
                -Destination $script:RecipeRoot -Recurse -Force
            Copy-Item -LiteralPath (Join-Path $script:SourceRecipes 'Object') `
                -Destination $script:RecipeRoot -Recurse -Force
        }

        function Get-OutputHashes {
            return @(Get-ChildItem -LiteralPath $script:OutputRoot -Recurse -File |
                Sort-Object FullName |
                ForEach-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash })
        }

        function Get-AcceptedHashes {
            return @(Get-OutputHashes) + @((Get-FileHash -LiteralPath $script:Manifest -Algorithm SHA256).Hash)
        }

        function Invoke-Surface {
            param([string]$Mode = 'Regenerate', [switch]$CleanupOrphans, [string]$PatternTestRoot = '')
            & $script:Wrapper `
                -Mode $Mode `
                -TestRoot $script:TestRoot `
                -EvidencePath $script:EvidenceRoot `
                -LayoutReceipt $script:LayoutReceipt `
                -PatternTestRoot $PatternTestRoot `
                -CleanupOrphans:$CleanupOrphans `
                -TimeoutSeconds 1200 | ConvertFrom-Json
        }

        function Get-ManifestRecords {
            return @((Get-Content -LiteralPath $script:Manifest -Raw | ConvertFrom-Json).records)
        }
    }

    BeforeEach {
        foreach ($path in @($script:TestRoot, $script:EvidenceRoot)) {
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Recurse -Force
            }
        }
        Copy-TestRecipes
    }

    AfterAll {
        foreach ($path in @($script:TestRoot, $script:EvidenceRoot)) {
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Recurse -Force
            }
        }
    }

    It 'is idempotent and restores exact accepted surface bytes after rejection' {
        $first = & $script:Wrapper `
            -Mode Regenerate `
            -TestRoot $script:TestRoot `
            -EvidencePath $script:EvidenceRoot `
            -LayoutReceipt $script:LayoutReceipt `
            -TimeoutSeconds 1200 | ConvertFrom-Json
        $first.generated | Should -Be 4
        $first.shader_compiles | Should -Be 2
        $acceptedHashes = Get-AcceptedHashes

        $second = & $script:Wrapper `
            -Mode Regenerate `
            -TestRoot $script:TestRoot `
            -EvidencePath $script:EvidenceRoot `
            -LayoutReceipt $script:LayoutReceipt `
            -TimeoutSeconds 1200 | ConvertFrom-Json
        $second.generated | Should -Be 0
        $second.skipped | Should -Be 4
        (Get-AcceptedHashes) | Should -Be $acceptedHashes

        $objectParent = Join-Path $script:RecipeRoot 'Object\M_ProjectMetricSurface.surface.json'
        $recipe = [System.IO.File]::ReadAllText($objectParent)
        $pattern = '"MacroStrength": [0-9.]+'
        [regex]::Matches($recipe, $pattern).Count | Should -Be 1
        [System.IO.File]::WriteAllText($objectParent,
            [regex]::Replace($recipe, $pattern, '"MacroStrength": 0.07'))
        { & $script:Wrapper `
            -Mode Regenerate `
            -TestRoot $script:TestRoot `
            -EvidencePath $script:EvidenceRoot `
            -LayoutReceipt $script:LayoutReceipt `
            -InjectFailure post-save `
            -TimeoutSeconds 1200 } | Should -Throw '*rejected*'
        (Get-AcceptedHashes) | Should -Be $acceptedHashes
        Test-Path -LiteralPath (Join-Path $script:TestRoot 'transaction\journal.json') |
            Should -BeFalse

        $replacement = & $script:Wrapper `
            -Mode Regenerate `
            -TestRoot $script:TestRoot `
            -EvidencePath $script:EvidenceRoot `
            -LayoutReceipt $script:LayoutReceipt `
            -TimeoutSeconds 1200 | ConvertFrom-Json
        $replacement.generated | Should -Be 2
        $replacement.skipped | Should -Be 2
        (Get-AcceptedHashes) | Should -Not -Be $acceptedHashes
    }

    It 'refreshes only the manifest after a formatting-only recipe edit' {
        (Invoke-Surface).generated | Should -Be 4
        $outputs = Get-OutputHashes
        $manifest = (Get-FileHash -LiteralPath $script:Manifest -Algorithm SHA256).Hash
        $recipePath = Join-Path $script:RecipeRoot 'Terrain\M_ProjectTerrain.surface.json'
        $text = [System.IO.File]::ReadAllText($recipePath).Replace("`r`n", "`n")

        # CRLF line endings and a UTF-8 BOM are not recipe source changes.
        [System.IO.File]::WriteAllText(
            $recipePath, $text.Replace("`n", "`r`n"), [System.Text.UTF8Encoding]::new($true))
        Invoke-Surface -Mode Validate | Out-Null
        (Invoke-Surface).generated | Should -Be 0
        (Get-FileHash -LiteralPath $script:Manifest -Algorithm SHA256).Hash | Should -Be $manifest

        # Extra whitespace changes the recipe source but not its semantics.
        [System.IO.File]::WriteAllText($recipePath, $text + "`n")
        { Invoke-Surface -Mode Validate } | Should -Throw '*rejected*'
        Get-Content -LiteralPath (Join-Path $script:EvidenceRoot 'Rejected\commandlet.log') -Raw |
            Should -Match 'Accepted surface manifest is stale'
        $refresh = Invoke-Surface
        $refresh.generated | Should -Be 0
        $refresh.shader_compiles | Should -Be 0
        (Get-OutputHashes) | Should -Be $outputs
        (Get-FileHash -LiteralPath $script:Manifest -Algorithm SHA256).Hash | Should -Not -Be $manifest
        Invoke-Surface -Mode Validate | Out-Null
    }

    It 'keeps an orphan that only an unloaded on-disk package references' {
        (Invoke-Surface).generated | Should -Be 4
        $objectRoot = Join-Path $script:OutputRoot 'Object'
        $holderRoot = Join-Path $script:TestRoot 'content\Referencers'
        New-Item -ItemType Directory -Path $holderRoot -Force | Out-Null
        # A byte copy of the cube instance still references the cube's package path; nothing in
        # this run loads or references the copy.
        Copy-Item -LiteralPath (Join-Path $objectRoot 'MI_ProjectMetricCube.uasset') `
            -Destination (Join-Path $holderRoot 'MI_ReferenceHolder.uasset')

        # The terrain instance is the unreferenced control; test packages are not used by World.
        Remove-Item -LiteralPath (Join-Path $script:RecipeRoot 'Object\MI_ProjectMetricCube.surface.json')
        Remove-Item -LiteralPath (Join-Path $script:RecipeRoot 'Terrain\MI_ProjectTerrain_Default.surface.json')
        $cleanup = Invoke-Surface -CleanupOrphans
        @($cleanup.retained_orphans) | Should -Be @('/ProjectMaterialTest/Surfaces/Object/MI_ProjectMetricCube')
        Test-Path -LiteralPath (Join-Path $objectRoot 'MI_ProjectMetricCube.uasset') | Should -BeTrue
        Test-Path -LiteralPath (Join-Path $script:OutputRoot 'Terrain\MI_ProjectTerrain_Default.uasset') |
            Should -BeFalse
        $log = Get-Content -LiteralPath (Join-Path $script:EvidenceRoot 'Current\commandlet.log') -Raw
        $log | Should -Match (
            'orphan retained - package=/ProjectMaterialTest/Surfaces/Object/MI_ProjectMetricCube ' +
            'referencers=/ProjectMaterialTest/Referencers/MI_ReferenceHolder\(loaded=false\)')
        $log | Should -Match 'orphan deleted - package=/ProjectMaterialTest/Surfaces/Terrain/MI_ProjectTerrain_Default'
    }

    It 'rejects cleanup when an unreferenced orphan cannot be deleted and recovers the accepted bytes' {
        (Invoke-Surface).generated | Should -Be 4
        $accepted = Get-AcceptedHashes
        $recipePath = Join-Path $script:RecipeRoot 'Terrain\MI_ProjectTerrain_Default.surface.json'
        $recipe = [System.IO.File]::ReadAllBytes($recipePath)
        Remove-Item -LiteralPath $recipePath
        $orphan = Join-Path $script:OutputRoot 'Terrain\MI_ProjectTerrain_Default.uasset'
        # A handle without delete sharing makes the delete fail as another process's handle would.
        $handle = [System.IO.File]::Open($orphan, 'Open', 'Read', 'ReadWrite')
        try {
            { Invoke-Surface -CleanupOrphans } | Should -Throw '*rejected and its rollback failed*'
            Get-Content -LiteralPath (Join-Path $script:EvidenceRoot 'Rejected\commandlet.log') -Raw |
                Should -Match 'Could not delete unreferenced surface orphan'
            # The held file also blocks the in-process restore, so recovery must stay pending intact.
            $journal = Join-Path $script:TestRoot 'transaction\journal.json'
            Test-Path -LiteralPath $journal | Should -BeTrue
            Test-Path -LiteralPath (Get-Content -LiteralPath $journal -Raw | ConvertFrom-Json).snapshot_root |
                Should -BeTrue
        }
        finally {
            $handle.Dispose()
        }
        # The next run restores the rejected transaction before it validates.
        [System.IO.File]::WriteAllBytes($recipePath, $recipe)
        Invoke-Surface -Mode Validate | Out-Null
        (Get-AcceptedHashes) | Should -Be $accepted
        Test-Path -LiteralPath (Join-Path $script:TestRoot 'transaction\journal.json') | Should -BeFalse
    }

    It 'regenerates only the instance whose own appearance scalar changed' {
        (Invoke-Surface).generated | Should -Be 4
        $parentPath = Join-Path $script:OutputRoot 'Object\M_ProjectMetricSurface.uasset'
        $parent = (Get-FileHash -LiteralPath $parentPath -Algorithm SHA256).Hash
        $recipePath = Join-Path $script:RecipeRoot 'Object\MI_ProjectMetricCube.surface.json'
        $recipe = [System.IO.File]::ReadAllText($recipePath)
        $pattern = '"MacroStrength": [0-9.]+'
        [regex]::Matches($recipe, $pattern).Count | Should -Be 1
        [System.IO.File]::WriteAllText($recipePath, [regex]::Replace($recipe, $pattern, '"MacroStrength": 0.09'))
        $edit = Invoke-Surface
        $edit.generated | Should -Be 1
        $edit.skipped | Should -Be 3
        $edit.shader_compiles | Should -Be 0
        (Get-FileHash -LiteralPath $parentPath -Algorithm SHA256).Hash | Should -Be $parent
    }

    It 'keeps surface artifacts when the pattern changes under the same output contract' {
        $patternRoot = Join-Path $script:ProjectRoot 'tmp\texture\generation\surface_firewall_pattern'
        $patternEvidence = Join-Path $script:ProjectRoot 'tmp\texture\generation\surface_firewall_evidence'
        $patternWrapper = Join-Path $script:ProjectRoot 'scripts\ue\texture\run_pattern_generation.ps1'
        $patternRecipe = Join-Path $patternRoot 'recipes\Terrain\project_terrain_structure.pattern.json'
        $sourcePattern = [System.IO.File]::ReadAllText((Join-Path $script:ProjectRoot `
            'Plugins\Resources\ProjectTexture\Data\Patterns\Terrain\project_terrain_structure.pattern.json'))
        ([regex]::Matches($sourcePattern, '"seed": 1847,')).Count | Should -Be 1
        function Invoke-PatternSeed([int]$Seed) {
            New-Item -ItemType Directory -Path (Split-Path -Parent $patternRecipe) -Force | Out-Null
            [System.IO.File]::WriteAllText(
                $patternRecipe, $sourcePattern.Replace('"seed": 1847,', ('"seed": {0},' -f $Seed)))
            & $patternWrapper -Mode Regenerate -TestRoot $patternRoot -EvidencePath $patternEvidence |
                ConvertFrom-Json
        }
        try {
            (Invoke-PatternSeed 1847).generated | Should -Be 1
            (Invoke-Surface -PatternTestRoot $patternRoot).generated | Should -Be 4
            $outputs = Get-OutputHashes
            $before = Get-ManifestRecords
            (Invoke-PatternSeed 1848).generated | Should -Be 1

            # The pattern changed under the same public contract: only the manifest's provenance is stale.
            { Invoke-Surface -Mode Validate -PatternTestRoot $patternRoot } | Should -Throw '*rejected*'
            Get-Content -LiteralPath (Join-Path $script:EvidenceRoot 'Rejected\commandlet.log') -Raw |
                Should -Match 'Accepted surface manifest is stale'
            $refresh = Invoke-Surface -PatternTestRoot $patternRoot
            $refresh.generated | Should -Be 0
            $refresh.shader_compiles | Should -Be 0
            (Get-OutputHashes) | Should -Be $outputs
            $after = Get-ManifestRecords
            @($after.semantic_identity) | Should -Be @($before.semantic_identity)
            @($after.package_sha256) | Should -Be @($before.package_sha256)
            @($after.pattern_package_sha256 | Select-Object -Unique).Count | Should -Be 1
            $after[0].pattern_package_sha256 | Should -Not -Be $before[0].pattern_package_sha256
            $after[0].pattern_semantic_identity | Should -Not -Be $before[0].pattern_semantic_identity
            Invoke-Surface -Mode Validate -PatternTestRoot $patternRoot | Out-Null
        }
        finally {
            foreach ($path in @($patternRoot, $patternEvidence)) {
                if (Test-Path -LiteralPath $path) {
                    Remove-Item -LiteralPath $path -Recurse -Force
                }
            }
        }
    }

    It 'refuses orphan cleanup while <Name> can still restore content' -ForEach @(
        @{ Name = 'a World transaction snapshot'; Owned = 'tmp\world\world_realization\transactions\material_cleanup_guard_test'; Relative = '' },
        @{ Name = 'a World harness outer snapshot'; Owned = 'tmp\world\material_cleanup_guard_test'; Relative = 'run\outer-snapshot' },
        @{ Name = 'a release projection rollback'; Owned = 'tmp\release\work\material_cleanup_guard_test'; Relative = 'world-projection-rollback' }
    ) {
        (Invoke-Surface).generated | Should -Be 4
        $accepted = Get-AcceptedHashes
        # Only the test's own folder is created and removed; shared parents are never deleted.
        $owned = Join-Path $script:ProjectRoot $Owned
        $restoreSource = if ($Relative) { Join-Path $owned $Relative } else { $owned }
        New-Item -ItemType Directory -Path $restoreSource -Force | Out-Null
        try {
            Remove-Item -LiteralPath (Join-Path $script:RecipeRoot 'Object\MI_ProjectMetricCube.surface.json')
            { Invoke-Surface -CleanupOrphans } |
                Should -Throw '*unrecovered transaction can still restore content*material_cleanup_guard_test*'
            (Get-AcceptedHashes) | Should -Be $accepted
            Test-Path -LiteralPath (Join-Path $script:TestRoot 'transaction\journal.json') | Should -BeFalse
        }
        finally {
            Remove-Item -LiteralPath $owned -Recurse -Force
        }
    }
}
