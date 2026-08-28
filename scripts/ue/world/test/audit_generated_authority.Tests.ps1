# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_content_transaction.ps1')
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')
    $script:RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
}

Describe 'ProjectWorld generated-authority audit' {
    It 'accepts an active generated scope with zero artifacts' {
        $projectRoot = Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N'))
        $ownerRoot = Join-Path $projectRoot 'Plugins\World\ProjectWorldTestData'
        $contentRoot = Join-Path $ownerRoot 'Content'
        $manifestRoot = Join-Path $ownerRoot 'Data\Manifests'
        $schemaRoot = Join-Path $projectRoot 'Plugins\World\ProjectWorld\Data\Schemas'
        $mapFile = Join-Path $contentRoot 'Generated\P0\L_TestWorld.umap'
        New-Item -ItemType Directory -Path `
            (Split-Path -Parent $mapFile), $manifestRoot, $schemaRoot -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $ownerRoot 'ProjectWorldTestData.uplugin') `
            -Value '{"FileVersion":3,"CanContainContent":true}' -NoNewline
        Set-Content -LiteralPath $mapFile -Value 'map-bytes' -NoNewline
        foreach ($schema in @(
            'project_world_active_manifest_set.schema.json',
            'project_world_generated_manifest.schema.json')) {
            Copy-Item -LiteralPath (Join-Path $script:RepositoryRoot "Plugins\World\ProjectWorld\Data\Schemas\$schema") `
                -Destination $schemaRoot
        }

        $mapPackage = '/ProjectWorldTestData/Generated/P0/L_TestWorld'
        $identity = [ordered]@{
            compile_result_sha256 = 'a' * 64
            presentation_profile_sha256 = 'b' * 64
            runtime_profile_sha256 = 'none'
            authored_overlay_profile_sha256 = 'none'
            map_package = $mapPackage
        }
        $mapScopeId = Get-ProjectWorldMapScopeId `
            -MapPackage $mapPackage -GeneratedPackageRoot '/ProjectWorldTestData/Generated/'
        $presentationScopeId = Get-ProjectWorldPresentationScopeId -ProfileId 'test_profile'
        $mapPaths = @(Get-ProjectWorldGeneratedPaths `
            -ContentRoot $contentRoot -MapPackage $mapPackage `
            -GeneratedPackageRoot '/ProjectWorldTestData/Generated/' -IncludePresentation $false)
        $emptyPaths = @()
        $candidates = @(
            (New-ProjectWorldCandidateManifest -ProjectRoot $projectRoot `
                -ScopeId $mapScopeId -Generation 1 -OwningLayer 'map' `
                -OperationId ('a1' * 16) -InputIdentity $identity -ScopePaths $mapPaths `
                -ConsumerReferences @() -GeneratorFingerprint (Get-ProjectWorldGeneratorFingerprint `
                    -ProjectRoot $projectRoot -ProducerId 'map:v1')),
            (New-ProjectWorldCandidateManifest -ProjectRoot $projectRoot `
                -ScopeId $presentationScopeId -Generation 1 -OwningLayer 'presentation' `
                -OperationId ('a1' * 16) -InputIdentity $identity -ScopePaths $emptyPaths `
                -ConsumerReferences @($mapScopeId) -GeneratorFingerprint (Get-ProjectWorldGeneratorFingerprint `
                    -ProjectRoot $projectRoot -ProducerId 'presentation:v1')))
        Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot `
            -TransactionId ('b2' * 16) -OperationId 'empty-scope' `
            -CandidateManifests $candidates | Out-Null

        $audit = (Resolve-Path (Join-Path $PSScriptRoot '..\audit_generated_authority.ps1')).Path
        $evidence = Join-Path $projectRoot 'audit.json'
        $generatedRoot = Join-Path $contentRoot 'Generated'
        powershell.exe -NoProfile -ExecutionPolicy Bypass -Command `
            "& '$audit' -ProjectRoot '$projectRoot' -WorldDataPlugin 'ProjectWorldTestData' -ManifestRoot '$manifestRoot' -GeneratedRoots @('$generatedRoot') -EvidencePath '$evidence'" | Out-Null

        $LASTEXITCODE | Should -Be 0
        (Get-Content -LiteralPath $evidence -Raw | ConvertFrom-Json).status | Should -Be 'accepted'
    }
}
