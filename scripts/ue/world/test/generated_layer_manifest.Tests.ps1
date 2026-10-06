# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_content_transaction.ps1')
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')
    . (Join-Path $PSScriptRoot '..\realization_layer_operation.ps1')
}

Describe 'ProjectWorld exact layer manifests' {
    BeforeEach {
        $projectRoot = Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N'))
        $pluginRoot = Join-Path $projectRoot 'Plugins\World\ProjectWorldTestData'
        $contentRoot = Join-Path $pluginRoot 'Content'
        $manifestRoot = Join-Path $pluginRoot 'Data\Manifests'
        $terrainRoot = Join-Path $contentRoot 'Generated\Twin\Terrain'
        $waterRoot = Join-Path $contentRoot 'Generated\Twin\Water'
        $mapRoot = Join-Path $contentRoot 'Generated\Twin'
        $mapFile = Join-Path $mapRoot 'L_Twin.umap'
        New-Item -ItemType Directory -Path $terrainRoot, $waterRoot, $manifestRoot -Force | Out-Null
        Set-Content -LiteralPath $mapFile -Value 'map' -NoNewline
        Set-Content -LiteralPath (Join-Path $terrainRoot 'terrain.uasset') -Value 'terrain' -NoNewline
        Set-Content -LiteralPath (Join-Path $waterRoot 'water.uasset') -Value 'water' -NoNewline
        $identity = [ordered]@{
            compile_result_sha256 = (('a' * 64) -join '')
            presentation_profile_sha256 = 'none'
            runtime_profile_sha256 = 'none'
            map_package = '/ProjectWorldTestData/Generated/Twin/L_Twin'
        }
        $fingerprint = (('b' * 64) -join '')
        $profileHash = (('c' * 64) -join '')

        function script:InventoryFor([string]$Root) {
            return @(Get-ChildItem -LiteralPath $Root -Recurse -File | ForEach-Object {
                [ordered]@{
                    path = $_.FullName.Substring($projectRoot.Length + 1).Replace('\', '/')
                    kind = 'asset'
                    digest_kind = 'sha256'
                    digest = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                }
            } | Sort-Object path)
        }

        function script:LayerContract(
            [string]$LayerId,
            [string]$ArtifactRoot,
            [object[]]$Inventory,
            [string[]]$Dirty = @('*')) {
            [object[]]$dependencyHashes = @()
            if ($LayerId -ne 'terrain') {
                $dependencyHashes = @((('d' * 64) -join ''))
            }
            return [ordered]@{
                realization_profile_id = 'synthetic_territory_twin'
                realization_profile_sha256 = $profileHash
                normalized_layer_contract_sha256 = $(if ($LayerId -eq 'terrain') { (('d' * 64) -join '') } else { (('e' * 64) -join '') })
                generator_id = $(if ($LayerId -eq 'terrain') { 'project_mesh_terrain' } else { 'project_water_mesh' })
                generator_version = 1
                artifact_root = $ArtifactRoot
                canonical_inputs = @([ordered]@{
                    unit_id = 'gridtwin:x0:y0'
                    sha256 = (('f' * 64) -join '')
                })
                dependency_inputs = @($dependencyHashes | ForEach-Object {
                    [ordered]@{ unit_id = 'layer:terrain:contract'; sha256 = $_ }
                })
                final_dirty_units = $Dirty
                semantic_outputs = @($Inventory | ForEach-Object {
                    [ordered]@{ artifact_path = $_.path; semantic_sha256 = $_.digest }
                })
            }
        }

        function script:LayerCandidate(
            [string]$LayerId,
            [int]$Generation,
            [string]$Root,
            [string]$PackageRoot) {
            $inventory = InventoryFor $Root
            $contract = LayerContract `
                -LayerId $LayerId `
                -ArtifactRoot $PackageRoot `
                -Inventory $inventory
            return New-ProjectWorldCandidateManifest `
                -ProjectRoot $projectRoot `
                -ScopeId "layer_$LayerId" `
                -Generation $Generation `
                -OwningLayer $LayerId `
                -OperationId (('1' * 32) -join '') `
                -InputIdentity $identity `
                -ScopePaths @() `
                -ConsumerReferences @('map_twin_l_twin') `
                -GeneratorFingerprint $fingerprint `
                -LayerContract $contract `
                -ArtifactRecords $inventory
        }
    }

    It 'passes a stale producer as an identity-dirty layer and never as an operator unit' {
        $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
        $output = Join-Path $TestDrive 'producer-drift'
        New-Item -ItemType Directory -Path $output -Force | Out-Null
        $scopeId = 'layer_synthetic_terrain'
        $definitions = [ordered]@{
            $scopeId = [pscustomobject]@{
                layer_id = 'terrain'
                generator_id = 'project_mesh_terrain'
                generator_version = 1
            }
        }
        $active = [pscustomobject]@{ Manifests = [ordered]@{
            $scopeId = [pscustomobject]@{
                generator_fingerprint = ('b' * 64)
                layer_contract = [pscustomobject]@{
                    normalized_layer_contract_sha256 = ('d' * 64)
                    canonical_inputs = @()
                }
            }
        } }

        $fingerprints = [ordered]@{ $scopeId = ('c' * 64) }
        $result = New-ProjectWorldLayerDirtyInput `
            -ProjectRoot $repoRoot -OutputDirectory $output `
            -RealizationDocument ([pscustomobject]@{ profile_id = 'synthetic' }) `
            -LayerDefinitions $definitions -ProducerFingerprints $fingerprints -ActiveSet $active
        $document = Get-Content -LiteralPath $result.Path -Raw | ConvertFrom-Json

        $document.schema_version | Should -Be 2
        @($document.operator_additions).Count | Should -Be 0
        @($document.identity_dirty_layers) | Should -Be @('terrain')
        $document.producer_fingerprints[0].generator_fingerprint | Should -Be ('c' * 64)
        { New-ProjectWorldLayerDirtyInput `
            -ProjectRoot $repoRoot -OutputDirectory $output `
            -RealizationDocument ([pscustomobject]@{ profile_id = 'synthetic' }) `
            -LayerDefinitions $definitions -ProducerFingerprints ([ordered]@{}) -ActiveSet $active } |
            Should -Throw '*do not cover exactly*'
    }

    It 'removes only prior layer artifacts omitted by the accepted replacement inventory' {
        $projectRoot = Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N'))
        $oldOnly = 'Plugins/World/Test/Content/Generated/old.uasset'
        $shared = 'Plugins/World/Test/Content/Generated/shared.uasset'
        $newOnly = 'Plugins/World/Test/Content/Generated/new.uasset'
        foreach ($relative in @($oldOnly, $shared, $newOnly)) {
            $path = Join-Path $projectRoot $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
            Set-Content -LiteralPath $path -Value $relative -NoNewline
        }
        $prior = [pscustomobject]@{ artifacts = @(
            [pscustomobject]@{ path = $oldOnly },
            [pscustomobject]@{ path = $shared }
        ) }
        $candidate = @(
            [pscustomobject]@{ path = $shared },
            [pscustomobject]@{ path = $newOnly }
        )

        $removed = @(Remove-ProjectWorldSupersededLayerArtifacts `
            -ProjectRoot $projectRoot -PriorManifest $prior -CandidateRecords $candidate)

        $removed | Should -Be @($oldOnly)
        Test-Path (Join-Path $projectRoot $oldOnly) | Should -BeFalse
        Test-Path (Join-Path $projectRoot $shared) | Should -BeTrue
        Test-Path (Join-Path $projectRoot $newOnly) | Should -BeTrue
    }

    It 'keeps all six generated layers stable across a runtime-only profile switch' {
        $runtimeA = [ordered]@{
            compile_result_sha256 = ('a' * 64)
            presentation_profile_sha256 = ('b' * 64)
            runtime_profile_sha256 = ('1' * 64)
            authored_overlay_profile_sha256 = ('c' * 64)
            map_package = '/ProjectWorldTestData/Generated/Twin/L_Twin'
        }
        $runtimeB = [ordered]@{}
        foreach ($entry in $runtimeA.GetEnumerator()) {
            $runtimeB[[string]$entry.Key] = $entry.Value
        }
        $runtimeB.runtime_profile_sha256 = ('2' * 64)

        foreach ($layerId in @('terrain', 'water', 'roads', 'vegetation', 'buildings', 'gameplay')) {
            $layerIdentityA = New-ProjectWorldLayerInputIdentity -OperationIdentity $runtimeA
            $layerIdentityB = New-ProjectWorldLayerInputIdentity -OperationIdentity $runtimeB
            $inventory = InventoryFor $(if ($layerId -eq 'terrain') { $terrainRoot } else { $waterRoot })
            $contract = LayerContract -LayerId $layerId `
                -ArtifactRoot "/ProjectWorldTestData/Generated/Twin/$layerId/" -Inventory $inventory
            $prior = New-ProjectWorldCandidateManifest `
                -ProjectRoot $projectRoot -ScopeId "layer_$layerId" -Generation 1 `
                -OwningLayer $layerId -OperationId ('1' * 32) `
                -InputIdentity $layerIdentityA -ScopePaths @() -ArtifactRecords $inventory `
                -LayerContract $contract -ConsumerReferences @('map_twin_l_twin') `
                -GeneratorFingerprint $fingerprint
            $candidate = New-ProjectWorldCandidateManifest `
                -ProjectRoot $projectRoot -ScopeId "layer_$layerId" -Generation 2 `
                -OwningLayer $layerId -OperationId ('2' * 32) `
                -InputIdentity $layerIdentityB -ScopePaths @() -ArtifactRecords $inventory `
                -LayerContract $contract -ConsumerReferences @('map_twin_l_twin') `
                -GeneratorFingerprint $fingerprint

            $candidate.input_identity.runtime_profile_sha256 | Should -Be 'none'
            ($candidate.artifacts | ConvertTo-Json -Depth 6 -Compress) | Should -Be `
                ($prior.artifacts | ConvertTo-Json -Depth 6 -Compress)
            Test-ProjectWorldManifestUnchanged `
                -PriorManifest $prior -CandidateManifest $candidate `
                -GeneratorFingerprint $fingerprint -CompareLayerContract | Should -BeTrue
        }
    }

    It 'refreshes provenance without regeneration on a compile-receipt-only change' {
        $prior = LayerCandidate 'terrain' 1 $terrainRoot '/ProjectWorldTestData/Generated/Twin/Terrain/'
        $identity.compile_result_sha256 = ('e' * 64)
        $candidate = LayerCandidate 'terrain' 2 $terrainRoot '/ProjectWorldTestData/Generated/Twin/Terrain/'

        ($candidate.artifacts | ConvertTo-Json -Depth 6 -Compress) | Should -Be `
            ($prior.artifacts | ConvertTo-Json -Depth 6 -Compress)
        $candidate.input_identity.compile_result_sha256 | Should -Be ('e' * 64)
        Test-ProjectWorldManifestUnchanged `
            -PriorManifest $prior -CandidateManifest $candidate `
            -GeneratorFingerprint $fingerprint -CompareLayerContract | Should -BeFalse
    }

    It 'namespaces wrapper layer scopes by realization profile before mutation' {
        $roots = [pscustomobject]@{
            ContentRoot = $contentRoot
            MountRoot = '/ProjectWorldTestData/'
            GeneratedPackageRoot = '/ProjectWorldTestData/Generated/'
        }
        $document = [pscustomobject]@{
            profile_id = 'synthetic_two'
            layers = @([pscustomobject]@{
                layer_id = 'terrain'
                layer_kind = 'generated_geography'
                generator_id = 'project_mesh_terrain'
                generator_version = 1
                canonical_selectors = @('terrain')
                artifact_root = '/ProjectWorldTestData/Generated/Twin/Terrain/'
                spatial_ownership = 'compiled_sections_from_canonical_cells'
                dirty_granularity = 'canonical_cell'
                dependency_halo_cells = 0
                runtime_mapping = 'world_partition_spatial'
                settings = [pscustomobject]@{
                    shared_definition = '/ProjectWorldMeshTerrain/Terrain/MPD_ProjectTerrain_Shared_v1.MPD_ProjectTerrain_Shared_v1'
                    surface_contract_id = 'terrain_surface_semantics'
                    surface_contract_version = 1
                    section_max_complexity = 2048
                    channel_texel_size_cm = 3000
                    channel_texture_max_dimension = 4096
                    collision = 'complex_as_simple'
                    render_variants = @('nanite', 'fallback')
                }
            })
        }
        $resolved = Resolve-ProjectWorldRealizationLayers `
            -RealizationDocument $document -WorldDataRoots $roots
        $resolved.Definitions.Contains('layer_synthetic_two_terrain') | Should -BeTrue
        $resolved.ScopePaths['layer_synthetic_two_terrain'].Count | Should -Be 1
    }

    It 'rejects a broad generated layer root before mutation' {
        $roots = [pscustomobject]@{
            ContentRoot = $contentRoot
            MountRoot = '/ProjectWorldTestData/'
            GeneratedPackageRoot = '/ProjectWorldTestData/Generated/'
        }
        $layer = [pscustomobject]@{
            layer_id = 'terrain'
            artifact_root = '/ProjectWorldTestData/Generated/'
        }
        $document = [pscustomobject]@{ profile_id = 'synthetic_two'; layers = @($layer) }
        {
            Resolve-ProjectWorldRealizationLayers `
                -RealizationDocument $document -WorldDataRoots $roots
        } | Should -Throw '*invalid or duplicate owned root*'
    }

    It 'requires an exact complete commandlet inventory under the layer root' {
        $inventory = InventoryFor $waterRoot
        $accepted = @(Get-ProjectWorldExactLayerArtifactRecords `
            -ProjectRoot $projectRoot -ArtifactRootPath $waterRoot -Inventory $inventory)
        $accepted.Count | Should -Be 1

        Set-Content -LiteralPath (Join-Path $waterRoot 'unreported.uasset') -Value 'extra' -NoNewline
        {
            Get-ProjectWorldExactLayerArtifactRecords `
                -ProjectRoot $projectRoot -ArtifactRootPath $waterRoot -Inventory $inventory
        } | Should -Throw '*not the exact artifact-root population*'
    }

    It 'accepts only exact commandlet-owned files under the target map external roots' {
        $externalRoot = Join-Path $contentRoot '__ExternalActors__\Generated\Twin\L_Twin'
        $otherRoot = Join-Path $contentRoot '__ExternalActors__\Generated\Other\L_Other'
        New-Item -ItemType Directory -Path $externalRoot, $otherRoot -Force | Out-Null
        $ownedExternal = Join-Path $externalRoot 'AA\owned.uasset'
        $foreignExternal = Join-Path $otherRoot 'BB\foreign.uasset'
        New-Item -ItemType Directory -Path (Split-Path -Parent $ownedExternal), (Split-Path -Parent $foreignExternal) -Force | Out-Null
        Set-Content -LiteralPath $ownedExternal -Value 'owned' -NoNewline
        Set-Content -LiteralPath $foreignExternal -Value 'foreign' -NoNewline
        $inventory = @(InventoryFor $waterRoot) + @([ordered]@{
            path = $ownedExternal.Substring($projectRoot.Length + 1).Replace('\', '/')
            kind = 'external_actor'
            digest_kind = 'sha256'
            digest = (Get-FileHash -LiteralPath $ownedExternal -Algorithm SHA256).Hash.ToLowerInvariant()
        })

        $accepted = @(Get-ProjectWorldExactLayerArtifactRecords `
            -ProjectRoot $projectRoot -ArtifactRootPath $waterRoot `
            -Inventory $inventory -AllowedExternalRoots @($externalRoot))
        $accepted.Count | Should -Be 2

        $inventory[-1].path = $foreignExternal.Substring($projectRoot.Length + 1).Replace('\', '/')
        $inventory[-1].digest = (Get-FileHash -LiteralPath $foreignExternal -Algorithm SHA256).Hash.ToLowerInvariant()
        {
            Get-ProjectWorldExactLayerArtifactRecords `
                -ProjectRoot $projectRoot -ArtifactRootPath $waterRoot `
                -Inventory $inventory -AllowedExternalRoots @($externalRoot)
        } | Should -Throw '*invalid external-package artifact*'
    }

    It 'audits layer-owned external actors through overlapping map roots' {
        $externalRoot = Join-Path $contentRoot '__ExternalActors__\Generated\Twin\L_Twin'
        $externalFile = Join-Path $externalRoot 'AA\water.uasset'
        New-Item -ItemType Directory -Path (Split-Path -Parent $externalFile) -Force | Out-Null
        Set-Content -LiteralPath $externalFile -Value 'water-actor' -NoNewline
        $externalRecord = [ordered]@{
            path = $externalFile.Substring($projectRoot.Length + 1).Replace('\', '/')
            kind = 'external_actor'
            digest_kind = 'sha256'
            digest = (Get-FileHash -LiteralPath $externalFile -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        $mapRecord = @(Get-ProjectWorldScopeArtifactRecords `
            -ProjectRoot $projectRoot -ScopePaths @($mapFile))[0]
        $waterRecords = @(InventoryFor $waterRoot) + @($externalRecord)
        $activeSet = [pscustomobject]@{ Manifests = @{
            map_twin_l_twin = [pscustomobject]@{ artifacts = @($mapRecord) }
            layer_water = [pscustomobject]@{ artifacts = $waterRecords }
        } }
        $participating = @{
            map_twin_l_twin = @($mapFile, $externalRoot)
            layer_water = @($waterRoot)
        }

        { Test-ProjectWorldScopeDrift `
            -ProjectRoot $projectRoot -ActiveSet $activeSet -ScopePathsById $participating } |
            Should -Not -Throw
        Set-Content -LiteralPath $externalFile -Value 'drifted' -NoNewline
        { Test-ProjectWorldScopeDrift `
            -ProjectRoot $projectRoot -ActiveSet $activeSet -ScopePathsById $participating } |
            Should -Throw '*drifted*'
    }

    It 'requires layer-only contract evidence and rejects ambiguous ownership' {
        $terrain = LayerCandidate 'terrain' 1 $terrainRoot '/ProjectWorldTestData/Generated/Twin/Terrain/'
        $terrainDocument = $terrain | ConvertTo-Json -Depth 10 | ConvertFrom-Json
        { Test-ProjectWorldManifestDocument -Manifest $terrainDocument } | Should -Not -Throw
        {
            New-ProjectWorldCandidateManifest `
                -ProjectRoot $projectRoot -ScopeId 'layer_missing' -Generation 1 `
                -OwningLayer 'missing' -OperationId (('2' * 32) -join '') -InputIdentity $identity `
                -ScopePaths @() -GeneratorFingerprint $fingerprint
        } | Should -Throw '*requires layer_contract*'
        {
            New-ProjectWorldCandidateManifest `
                -ProjectRoot $projectRoot -ScopeId 'map_twin_l_twin' -Generation 1 `
                -OwningLayer 'map' -OperationId (('2' * 32) -join '') -InputIdentity $identity `
                -ScopePaths @($mapFile) -GeneratorFingerprint $fingerprint `
                -LayerContract $terrain.layer_contract
        } | Should -Throw '*Non-layer scope*'

        $duplicate = $terrain | ConvertTo-Json -Depth 10 | ConvertFrom-Json
        $duplicate.scope_id = 'layer_duplicate'
        {
            Test-ProjectWorldProspectiveSet -CandidateManifests @($terrain, $duplicate)
        } | Should -Throw '*Ambiguous ownership*'
    }

    It 'advances only a changed layer and retires map plus layers atomically' {
        $terrainCompanion = Join-Path $terrainRoot 'terrain_metadata.uasset'
        Set-Content -LiteralPath $terrainCompanion -Value 'terrain-metadata' -NoNewline
        $mapCandidate = New-ProjectWorldCandidateManifest `
            -ProjectRoot $projectRoot -ScopeId 'map_twin_l_twin' -Generation 1 `
            -OwningLayer 'map' -OperationId (('3' * 32) -join '') -InputIdentity $identity `
            -ScopePaths @($mapFile) -GeneratorFingerprint $fingerprint
        $terrain = LayerCandidate 'terrain' 1 $terrainRoot '/ProjectWorldTestData/Generated/Twin/Terrain/'
        $water = LayerCandidate 'water' 1 $waterRoot '/ProjectWorldTestData/Generated/Twin/Water/'
        $first = Publish-ProjectWorldActiveSet `
            -ManifestRoot $manifestRoot -ProjectRoot $projectRoot `
            -TransactionId (('4' * 32) -join '') -OperationId 'full' `
            -CandidateManifests @($mapCandidate, $terrain, $water)
        $firstActive = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
        $firstActive.Record.scopes.Count | Should -Be 3

        Remove-Item -LiteralPath $terrainRoot -Recurse -Force
        {
            Test-ProjectWorldReconstructionScopeState `
                -ProjectRoot $projectRoot -ActiveSet $firstActive `
                -ScopePathsById @{ layer_terrain = @($terrainRoot) }
        } | Should -Not -Throw
        New-Item -ItemType Directory -Path $terrainRoot -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $terrainRoot 'terrain.uasset') -Value 'terrain' -NoNewline
        {
            Test-ProjectWorldReconstructionScopeState `
                -ProjectRoot $projectRoot -ActiveSet $firstActive `
                -ScopePathsById @{ layer_terrain = @($terrainRoot) }
        } | Should -Throw '*drift*'
        Set-Content -LiteralPath $terrainCompanion -Value 'terrain-metadata' -NoNewline

        $noOpMap = New-ProjectWorldCandidateManifest `
            -ProjectRoot $projectRoot -ScopeId 'map_twin_l_twin' -Generation 2 `
            -OwningLayer 'map' -OperationId (('7' * 32) -join '') `
            -InputIdentity ([ordered]@{
                compile_result_sha256 = ('a' * 64)
                presentation_profile_sha256 = 'none'
                runtime_profile_sha256 = 'none'
                map_package = '/ProjectWorldTestData/Generated/Twin/L_Twin'
            }) -ScopePaths @($mapFile) -GeneratorFingerprint $fingerprint
        Test-ProjectWorldManifestUnchanged `
            -PriorManifest $firstActive.Manifests['map_twin_l_twin'] `
            -CandidateManifest $noOpMap -GeneratorFingerprint $fingerprint | Should -BeTrue
        $noOpMap.input_identity.compile_result_sha256 = ('e' * 64)
        Test-ProjectWorldManifestUnchanged `
            -PriorManifest $firstActive.Manifests['map_twin_l_twin'] `
            -CandidateManifest $noOpMap -GeneratorFingerprint $fingerprint | Should -BeFalse

        $noOpTerrain = LayerCandidate 'terrain' 2 $terrainRoot '/ProjectWorldTestData/Generated/Twin/Terrain/'
        $noOpTerrain.input_identity.compile_result_sha256 = ('a' * 64)
        Test-ProjectWorldManifestUnchanged `
            -PriorManifest $firstActive.Manifests['layer_terrain'] `
            -CandidateManifest $noOpTerrain -GeneratorFingerprint $fingerprint `
            -CompareLayerContract | Should -BeTrue

        $noOpTerrain.layer_contract.realization_profile_sha256 = ('f' * 64)
        Test-ProjectWorldManifestUnchanged `
            -PriorManifest $firstActive.Manifests['layer_terrain'] `
            -CandidateManifest $noOpTerrain -GeneratorFingerprint $fingerprint `
            -CompareLayerContract | Should -BeTrue

        $noOpWater = LayerCandidate 'water' 2 $waterRoot '/ProjectWorldTestData/Generated/Twin/Water/'
        Test-ProjectWorldManifestUnchanged `
            -PriorManifest $firstActive.Manifests['layer_water'] `
            -CandidateManifest $noOpWater -GeneratorFingerprint $fingerprint `
            -CompareLayerContract | Should -BeTrue

        $noOpCommit = [pscustomobject]@{ Sha256 = '' }
        $noOp = Publish-ProjectWorldActiveSet `
            -ManifestRoot $manifestRoot -ProjectRoot $projectRoot `
            -TransactionId (('8' * 32) -join '') -OperationId 'no-op' `
            -CandidateManifests @() -PriorActiveSet $firstActive `
            -BeforeCommit { param($sha256) $noOpCommit.Sha256 = $sha256 }
        $noOp.Sha256 | Should -Be $firstActive.Sha256
        $noOpCommit.Sha256 | Should -Be $firstActive.Sha256
        (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot).Sha256 |
            Should -Be $firstActive.Sha256

        Set-Content -LiteralPath (Join-Path $waterRoot 'water.uasset') -Value 'water-v2' -NoNewline
        $waterV2 = LayerCandidate 'water' 2 $waterRoot '/ProjectWorldTestData/Generated/Twin/Water/'
        $second = Publish-ProjectWorldActiveSet `
            -ManifestRoot $manifestRoot -ProjectRoot $projectRoot `
            -TransactionId (('5' * 32) -join '') -OperationId 'incremental' `
            -CandidateManifests @($waterV2) -PriorActiveSet $firstActive
        $active = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
        $active.Manifests['map_twin_l_twin'].generation | Should -Be 1
        $active.Manifests['layer_terrain'].generation | Should -Be 1
        $active.Manifests['layer_water'].generation | Should -Be 2

        $withoutWater = Publish-ProjectWorldActiveSet `
            -ManifestRoot $manifestRoot -ProjectRoot $projectRoot `
            -TransactionId (('9' * 32) -join '') -OperationId 'remove-layer' `
            -CandidateManifests @() -RetiredScopeIds @('layer_water') `
            -PriorActiveSet $active
        $active = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot
        $active.Manifests.Contains('map_twin_l_twin') | Should -BeTrue
        $active.Manifests.Contains('layer_terrain') | Should -BeTrue
        $active.Manifests.Contains('layer_water') | Should -BeFalse
        Test-Path -LiteralPath (Join-Path $manifestRoot 'archive\layer_water.2.json') | Should -BeTrue

        $empty = Publish-ProjectWorldActiveSet `
            -ManifestRoot $manifestRoot -ProjectRoot $projectRoot `
            -TransactionId (('6' * 32) -join '') -OperationId 'delete' `
            -CandidateManifests @() `
            -RetiredScopeIds @('map_twin_l_twin', 'layer_terrain') `
            -PriorActiveSet $active
        (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $projectRoot).Record.scopes.Count | Should -Be 0
        @(Get-ChildItem -LiteralPath (Join-Path $manifestRoot 'archive') -File).Count | Should -Be 3
    }
}

Describe 'ProjectWorld post-apply builder' {
    It 'runs a declared builder only for an accepted Apply result' {
        $meshResult = [pscustomobject]@{
            layer_inventories = @([pscustomobject]@{
                layer_id = 'terrain'
                generator_id = 'project_mesh_terrain'
                post_apply_builder = [pscustomobject]@{ inventory_commandlet = 'SomeInventory' }
            })
        }
        $nonMeshResult = [pscustomobject]@{
            layer_inventories = @([pscustomobject]@{
                layer_id = 'another_layer'
                generator_id = 'project_water_mesh'
                post_apply_builder = $null
            })
        }

        Test-ProjectWorldPostApplyBuildRequired -Mode apply -Result $meshResult |
            Should -BeTrue
        Test-ProjectWorldPostApplyBuildRequired -Mode validate -Result $meshResult |
            Should -BeFalse
        Test-ProjectWorldPostApplyBuildRequired -Mode delete -Result $meshResult |
            Should -BeFalse
        Test-ProjectWorldPostApplyBuildRequired -Mode apply -Result $nonMeshResult |
            Should -BeFalse
    }

    It 'dispatches a declared inventory, builder, and final inventory in order' {
        $projectRoot = Join-Path $TestDrive 'generic-builder'
        $evidenceRoot = Join-Path $projectRoot 'tmp\world\builder-evidence'
        $relative = 'Plugins/World/ProjectWorldTestData/Content/generated.uasset'
        $artifactPath = Join-Path $projectRoot $relative.Replace('/', '\')
        New-Item -ItemType Directory -Path (Split-Path -Parent $artifactPath), $evidenceRoot -Force | Out-Null
        Set-Content -LiteralPath $artifactPath -Value 'generated' -NoNewline
        $fakeEditor = Join-Path $projectRoot 'fake-editor.ps1'
        $callLog = Join-Path $evidenceRoot 'calls.txt'
        $env:PROJECT_WORLD_TEST_BUILDER_CALLS = $callLog
        $env:PROJECT_WORLD_TEST_BUILDER_ARTIFACT = $relative
        $fakeSource = @'
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
$global:LASTEXITCODE = 0
$commandlet = @($Arguments | Where-Object { $_ -like '-run=*' })[0]
Add-Content -LiteralPath $env:PROJECT_WORLD_TEST_BUILDER_CALLS -Value $commandlet
$resultArgument = @($Arguments | Where-Object { $_ -like '-Result=*' })
if ($resultArgument.Count -gt 0) {
    $resultPath = $resultArgument[0].Substring(8)
    $map = @($Arguments | Where-Object { $_ -like '-Map=*' })[0].Substring(5)
    $receipt = @{
        schema = 'probe-inventory:v1'
        map = $map
        status = 'accepted'
        builder_artifacts = @(@{
            path = $env:PROJECT_WORLD_TEST_BUILDER_ARTIFACT
            kind = 'external_actor'
        })
    }
    $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $resultPath
}
'@
        Set-Content -LiteralPath $fakeEditor -Value $fakeSource
        $inventory = [pscustomobject]@{
            layer_id = 'probe'
            artifacts = @()
            post_apply_builder = [pscustomobject]@{
                inventory_commandlet = 'ProbeInventory'
                inventory_arguments = @('-AllowCommandletRendering')
                inventory_schema = 'probe-inventory:v1'
                builder_commandlet = 'ProbeBuilder'
                builder_arguments = @('-Builder=Probe')
            }
        }
        $result = [pscustomobject]@{ layer_inventories = @($inventory) }
        try {
            Invoke-ProjectWorldPostApplyBuilders -EditorCommand $fakeEditor `
                -ProjectFile 'Project.uproject' -ProjectRoot $projectRoot `
                -MapPackage '/ProjectWorldTestData/Generated/L_Probe' `
                -CompileResult 'compile.json' -EvidenceDirectory $evidenceRoot -Result $result
            @(Get-Content -LiteralPath $callLog) | Should -Be @(
                '-run=ProbeInventory', '-run=ProbeBuilder', '-run=ProbeInventory')
            @($inventory.artifacts).Count | Should -Be 1
            $inventory.artifacts[0].path | Should -Be $relative
        }
        finally {
            Remove-Item Env:PROJECT_WORLD_TEST_BUILDER_CALLS -ErrorAction SilentlyContinue
            Remove-Item Env:PROJECT_WORLD_TEST_BUILDER_ARTIFACT -ErrorAction SilentlyContinue
        }
    }

    It 'rejects an unsafe declared argument before starting the editor' {
        $inventory = [pscustomobject]@{
            layer_id = 'probe'
            artifacts = @()
            post_apply_builder = [pscustomobject]@{
                inventory_commandlet = 'ProbeInventory'
                inventory_arguments = @('-Good', ';Remove-Item')
                inventory_schema = 'probe-inventory:v1'
                builder_commandlet = 'ProbeBuilder'
                builder_arguments = @('-Builder=Probe')
            }
        }
        $safe = [pscustomobject]@{
            layer_id = 'safe'
            artifacts = @()
            post_apply_builder = [pscustomobject]@{
                inventory_commandlet = 'ProbeInventory'
                inventory_arguments = @('-Good')
                inventory_schema = 'probe-inventory:v1'
                builder_commandlet = 'ProbeBuilder'
                builder_arguments = @('-Builder=Probe')
            }
        }
        $result = [pscustomobject]@{ layer_inventories = @($safe, $inventory) }
        {
            Invoke-ProjectWorldPostApplyBuilders -EditorCommand 'missing-editor' `
                -ProjectFile 'Project.uproject' -ProjectRoot $TestDrive `
                -MapPackage '/ProjectWorldTestData/Generated/L_Probe' `
                -CompileResult 'compile.json' -EvidenceDirectory $TestDrive -Result $result
        } | Should -Throw '*Unsafe post-apply builder argument*'
    }

    It 'accepts a compiled section already present in the terrain inventory' {
        $projectRoot = Join-Path $TestDrive 'mesh-existing'
        $relative = 'Plugins/World/ProjectWorldTestData/Content/__ExternalActors__/Generated/Twin/0/00/SECTION.uasset'
        $artifact = Join-Path $projectRoot $relative.Replace('/', '\')
        New-Item -ItemType Directory -Path (Split-Path -Parent $artifact) -Force | Out-Null
        Set-Content -LiteralPath $artifact -Value 'compiled-section' -NoNewline
        $digest = Get-ProjectWorldFileSha256 -Path $artifact
        $inventory = @([pscustomobject]@{
            path = $relative
            kind = 'external_actor'
            digest_kind = 'sha256'
            digest = $digest
        })

        $merged = @(Merge-ProjectWorldPostApplyArtifacts `
            -LayerArtifacts $inventory `
            -BuilderArtifacts @([pscustomobject]@{ path = $relative; kind = 'external_actor' }) `
            -ProjectRoot $projectRoot)

        $merged.Count | Should -Be 1
        $merged[0].path | Should -Be $relative
        $merged[0].semantic_sha256 | Should -Be $digest
    }

    It 'still rejects duplicate section paths emitted by the audit' {
        $projectRoot = Join-Path $TestDrive 'mesh-duplicate'
        $relative = 'Plugins/World/ProjectWorldTestData/Content/__ExternalActors__/Generated/Twin/0/00/SECTION.uasset'
        $artifact = Join-Path $projectRoot $relative.Replace('/', '\')
        New-Item -ItemType Directory -Path (Split-Path -Parent $artifact) -Force | Out-Null
        Set-Content -LiteralPath $artifact -Value 'compiled-section' -NoNewline
        $sections = @(
            [pscustomobject]@{ path = $relative; kind = 'external_actor' },
            [pscustomobject]@{ path = $relative; kind = 'external_actor' }
        )

        {
            Merge-ProjectWorldPostApplyArtifacts `
                -LayerArtifacts @() -BuilderArtifacts $sections -ProjectRoot $projectRoot
        } | Should -Throw '*invalid or duplicate artifact*'
    }

    It 'replaces prior compiled sections but still rejects a missing authoring artifact' {
        $projectRoot = Join-Path $TestDrive 'mesh-replacement'
        $oldSection = 'Plugins/World/ProjectWorldTestData/Content/old-section.uasset'
        $newSection = 'Plugins/World/ProjectWorldTestData/Content/new-section.uasset'
        $authoring = 'Plugins/World/ProjectWorldTestData/Content/authoring.uasset'
        foreach ($relative in @($newSection, $authoring)) {
            $fullPath = Join-Path $projectRoot $relative.Replace('/', '\')
            New-Item -ItemType Directory -Path (Split-Path -Parent $fullPath) -Force | Out-Null
            Set-Content -LiteralPath $fullPath -Value $relative -NoNewline
        }
        $inventory = @($oldSection, $authoring) | ForEach-Object {
            [pscustomobject]@{ path = $_; kind = 'external_actor'; digest_kind = 'sha256'; digest = 'prior' }
        }
        $arguments = @{
            LayerArtifacts = $inventory
            PriorBuilderArtifacts = @([pscustomobject]@{ path = $oldSection; kind = 'external_actor' })
            BuilderArtifacts = @([pscustomobject]@{ path = $newSection; kind = 'external_actor' })
            ProjectRoot = $projectRoot
        }

        $merged = @(Merge-ProjectWorldPostApplyArtifacts @arguments)
        @($merged.path) | Should -Be @($authoring, $newSection)

        Remove-Item -LiteralPath (Join-Path $projectRoot $authoring.Replace('/', '\')) -Force
        { Merge-ProjectWorldPostApplyArtifacts @arguments } |
            Should -Throw '*removed a declared layer artifact*'
    }
}
