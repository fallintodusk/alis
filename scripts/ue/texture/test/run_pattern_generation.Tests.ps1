# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Describe 'ProjectTexture host transaction' -Tag 'Integration' {
    BeforeAll {
        $script:ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        $script:Wrapper = Join-Path $script:ProjectRoot 'scripts\ue\texture\run_pattern_generation.ps1'
        $script:TestRoot = Join-Path $script:ProjectRoot 'tmp\texture\generation\wrapper_integration'
        $script:RecipeRoot = Join-Path $script:TestRoot 'recipes\Terrain'
        $script:RecipePath = Join-Path $script:RecipeRoot 'project_terrain_structure.pattern.json'
        $script:Package = Join-Path $script:TestRoot 'content\Patterns\Terrain\DA_ProjectTerrainStructureCatalog.uasset'
        $script:Manifest = Join-Path $script:TestRoot 'manifests\accepted.pattern-manifest.json'
        $script:EvidenceRoot = Join-Path $script:ProjectRoot 'tmp\texture\generation\wrapper_evidence'
        $script:SourceRecipe = Join-Path $script:ProjectRoot `
            'Plugins\Resources\ProjectTexture\Data\Patterns\Terrain\project_terrain_structure.pattern.json'

        function Write-TestRecipe {
            param([int]$Seed = 1847)
            New-Item -ItemType Directory -Path $script:RecipeRoot -Force | Out-Null
            $recipe = [System.IO.File]::ReadAllText($script:SourceRecipe)
            if (([regex]::Matches($recipe, '"seed": 1847,')).Count -ne 1) {
                throw 'The production pattern recipe no longer carries the seed this test edits.'
            }
            $recipe = $recipe.Replace('"seed": 1847,', ('"seed": {0},' -f $Seed))
            [System.IO.File]::WriteAllText($script:RecipePath, $recipe)
        }

        function Get-AcceptedHashes {
            return @(
                (Get-FileHash -LiteralPath $script:Package -Algorithm SHA256).Hash,
                (Get-FileHash -LiteralPath $script:Manifest -Algorithm SHA256).Hash
            )
        }

        function Invoke-Pattern {
            param([string]$Mode = 'Regenerate', [switch]$CleanupOrphans)
            & $script:Wrapper `
                -Mode $Mode `
                -TestRoot $script:TestRoot `
                -EvidencePath $script:EvidenceRoot `
                -CleanupOrphans:$CleanupOrphans | ConvertFrom-Json
        }
    }

    BeforeEach {
        foreach ($path in @($script:TestRoot, $script:EvidenceRoot)) {
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Recurse -Force
            }
        }
        Write-TestRecipe
    }

    AfterAll {
        foreach ($path in @($script:TestRoot, $script:EvidenceRoot)) {
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Recurse -Force
            }
        }
        foreach ($emptyRoot in @(
            (Join-Path $script:ProjectRoot 'tmp\texture\generation'),
            (Join-Path $script:ProjectRoot 'tmp\texture'))) {
            if ((Test-Path -LiteralPath $emptyRoot) -and
                @(Get-ChildItem -LiteralPath $emptyRoot -Force).Count -eq 0) {
                Remove-Item -LiteralPath $emptyRoot -Force
            }
        }
    }

    It 'is idempotent and restores exact accepted bytes after child rejection' {
        $first = & $script:Wrapper `
            -Mode Regenerate `
            -TestRoot $script:TestRoot `
            -EvidencePath $script:EvidenceRoot | ConvertFrom-Json
        $first.generated | Should -Be 1
        $first.skipped | Should -Be 0
        $acceptedHashes = Get-AcceptedHashes
        $acceptedTimes = @(
            (Get-Item -LiteralPath $script:Package).LastWriteTimeUtc.Ticks,
            (Get-Item -LiteralPath $script:Manifest).LastWriteTimeUtc.Ticks
        )

        Start-Sleep -Milliseconds 1100
        $second = & $script:Wrapper `
            -Mode Regenerate `
            -TestRoot $script:TestRoot `
            -EvidencePath $script:EvidenceRoot | ConvertFrom-Json
        $second.generated | Should -Be 0
        $second.skipped | Should -Be 1
        (Get-AcceptedHashes) | Should -Be $acceptedHashes
        @(
            (Get-Item -LiteralPath $script:Package).LastWriteTimeUtc.Ticks,
            (Get-Item -LiteralPath $script:Manifest).LastWriteTimeUtc.Ticks
        ) | Should -Be $acceptedTimes

        Write-TestRecipe -Seed 1848
        { & $script:Wrapper `
            -Mode Regenerate `
            -TestRoot $script:TestRoot `
            -EvidencePath $script:EvidenceRoot `
            -InjectFailure post-save } | Should -Throw '*rejected*'
        (Get-AcceptedHashes) | Should -Be $acceptedHashes
        Test-Path -LiteralPath (Join-Path $script:TestRoot 'transaction\journal.json') |
            Should -BeFalse

        $replacement = & $script:Wrapper `
            -Mode Regenerate `
            -TestRoot $script:TestRoot `
            -EvidencePath $script:EvidenceRoot | ConvertFrom-Json
        $replacement.generated | Should -Be 1
        (Get-AcceptedHashes) | Should -Not -Be $acceptedHashes
    }

    It 'refreshes only the manifest after a formatting-only recipe edit' {
        (Invoke-Pattern).generated | Should -Be 1
        $package = (Get-FileHash -LiteralPath $script:Package -Algorithm SHA256).Hash
        $manifest = (Get-FileHash -LiteralPath $script:Manifest -Algorithm SHA256).Hash
        $text = [System.IO.File]::ReadAllText($script:RecipePath).Replace("`r`n", "`n")

        # CRLF line endings and a UTF-8 BOM are not recipe source changes.
        [System.IO.File]::WriteAllText(
            $script:RecipePath, $text.Replace("`n", "`r`n"), [System.Text.UTF8Encoding]::new($true))
        Invoke-Pattern -Mode Validate | Out-Null
        (Invoke-Pattern).generated | Should -Be 0
        (Get-FileHash -LiteralPath $script:Manifest -Algorithm SHA256).Hash | Should -Be $manifest

        # Extra whitespace changes the recipe source but not its semantics.
        [System.IO.File]::WriteAllText($script:RecipePath, $text + "`n")
        { Invoke-Pattern -Mode Validate } | Should -Throw '*rejected*'
        Get-Content -LiteralPath (Join-Path $script:EvidenceRoot 'Rejected\commandlet.log') -Raw |
            Should -Match 'Accepted pattern manifest is stale'
        (Invoke-Pattern).generated | Should -Be 0
        (Get-FileHash -LiteralPath $script:Package -Algorithm SHA256).Hash | Should -Be $package
        (Get-FileHash -LiteralPath $script:Manifest -Algorithm SHA256).Hash | Should -Not -Be $manifest
        Invoke-Pattern -Mode Validate | Out-Null
    }

    It 'keeps a pattern orphan that only an unloaded on-disk package references' {
        $recipe = [System.IO.File]::ReadAllText($script:RecipePath)
        foreach ($field in @('"pattern_id": "project_terrain_structure"', '"catalog_id": "DA_ProjectTerrainStructureCatalog"')) {
            ([regex]::Matches($recipe, [regex]::Escape($field))).Count | Should -Be 1
        }
        $copies = @('held', 'control') | ForEach-Object {
            $catalog = 'DA_ProjectTerrainStructure' + (Get-Culture).TextInfo.ToTitleCase($_)
            $path = Join-Path $script:RecipeRoot "project_terrain_structure_$_.pattern.json"
            [System.IO.File]::WriteAllText($path, $recipe.
                Replace('"pattern_id": "project_terrain_structure"', "`"pattern_id`": `"project_terrain_structure_$_`"").
                Replace('"catalog_id": "DA_ProjectTerrainStructureCatalog"', "`"catalog_id`": `"$catalog`""))
            [pscustomobject]@{ Recipe = $path; Package = Join-Path $script:TestRoot "content\Patterns\Terrain\$catalog.uasset" }
        }
        (Invoke-Pattern).generated | Should -Be 3
        $holderRoot = Join-Path $script:TestRoot 'content\Referencers'
        New-Item -ItemType Directory -Path $holderRoot -Force | Out-Null
        # A byte copy of the held catalog still references the catalog's package path; nothing in
        # this run loads or references the copy.
        Copy-Item -LiteralPath $copies[0].Package -Destination (Join-Path $holderRoot 'DA_ReferenceHolder.uasset')

        Remove-Item -LiteralPath $copies[0].Recipe, $copies[1].Recipe
        $cleanup = Invoke-Pattern -CleanupOrphans
        @($cleanup.retained_orphans) | Should -Be @('/ProjectTextureTest/Patterns/Terrain/DA_ProjectTerrainStructureHeld')
        Test-Path -LiteralPath $copies[0].Package | Should -BeTrue
        Test-Path -LiteralPath $copies[1].Package | Should -BeFalse
        Test-Path -LiteralPath $script:Package | Should -BeTrue
        $log = Get-Content -LiteralPath (Join-Path $script:EvidenceRoot 'Current\commandlet.log') -Raw
        $log | Should -Match 'Asset Registry gathered for orphan cleanup - candidates=2'
        $log | Should -Match (
            'orphan retained - package=/ProjectTextureTest/Patterns/Terrain/DA_ProjectTerrainStructureHeld ' +
            'referencers=/ProjectTextureTest/Referencers/DA_ReferenceHolder\(loaded=false\)')
        $log | Should -Match 'orphan deleted - package=/ProjectTextureTest/Patterns/Terrain/DA_ProjectTerrainStructureControl'
    }

    It 'rejects cleanup when an unreferenced pattern orphan cannot be deleted and recovers the accepted bytes' {
        $recipe = [System.IO.File]::ReadAllText($script:RecipePath)
        $controlRecipe = Join-Path $script:RecipeRoot 'project_terrain_structure_control.pattern.json'
        $controlText = $recipe.
            Replace('"pattern_id": "project_terrain_structure"', '"pattern_id": "project_terrain_structure_control"').
            Replace('"catalog_id": "DA_ProjectTerrainStructureCatalog"', '"catalog_id": "DA_ProjectTerrainStructureControl"')
        [System.IO.File]::WriteAllText($controlRecipe, $controlText)
        (Invoke-Pattern).generated | Should -Be 2
        $control = Join-Path $script:TestRoot 'content\Patterns\Terrain\DA_ProjectTerrainStructureControl.uasset'
        $accepted = @((Get-FileHash -LiteralPath $control -Algorithm SHA256).Hash) + (Get-AcceptedHashes)
        Remove-Item -LiteralPath $controlRecipe
        # A handle without delete sharing makes the delete fail as another process's handle would.
        $handle = [System.IO.File]::Open($control, 'Open', 'Read', 'ReadWrite')
        try {
            { Invoke-Pattern -CleanupOrphans } | Should -Throw '*rejected and its rollback failed*'
            Get-Content -LiteralPath (Join-Path $script:EvidenceRoot 'Rejected\commandlet.log') -Raw |
                Should -Match 'Could not delete unreferenced pattern orphan'
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
        [System.IO.File]::WriteAllText($controlRecipe, $controlText)
        Invoke-Pattern -Mode Validate | Out-Null
        (@((Get-FileHash -LiteralPath $control -Algorithm SHA256).Hash) + (Get-AcceptedHashes)) |
            Should -Be $accepted
        Test-Path -LiteralPath (Join-Path $script:TestRoot 'transaction\journal.json') | Should -BeFalse
    }
}
