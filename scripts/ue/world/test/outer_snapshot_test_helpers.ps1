# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Fake project roots for outer-snapshot and outer-recovery tests: a
# ProjectWorldTestData authority with one enrolled map scope and generated
# content under the generated roots. Requires generated_content_transaction.ps1
# and generated_manifest.ps1 to be loaded.

function New-ProjectWorldOuterTestProject {
    param([Parameter(Mandatory = $true)][string]$Root)
    $plugin = Join-Path $Root 'Plugins\World\ProjectWorldTestData'
    $contentRoot = Join-Path $plugin 'Content'
    $manifestRoot = Join-Path $plugin 'Data\Manifests'
    $mapPackage = '/ProjectWorldTestData/Generated/Representative/L_TestWorld'
    $generatedPackageRoot = '/ProjectWorldTestData/Generated/'
    $mapRoot = Join-Path $contentRoot 'Generated\Representative'
    $externalRoot = Join-Path $contentRoot '__ExternalActors__\Generated\Representative\L_TestWorld'
    New-Item -ItemType Directory -Path $mapRoot, $externalRoot, $manifestRoot -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $plugin 'ProjectWorldTestData.uplugin') `
        -Value '{"FileVersion":3,"CanContainContent":true}' -NoNewline
    $mapFile = Join-Path $mapRoot 'L_TestWorld.umap'
    $actorFile = Join-Path $externalRoot 'actor.uasset'
    $secondActorFile = Join-Path $externalRoot 'second.uasset'
    Set-Content -LiteralPath $mapFile -Value 'map-bytes' -NoNewline
    Set-Content -LiteralPath $actorFile -Value 'actor-bytes' -NoNewline
    Set-Content -LiteralPath $secondActorFile -Value 'second-actor-bytes' -NoNewline

    $scopeId = Get-ProjectWorldMapScopeId -MapPackage $mapPackage -GeneratedPackageRoot $generatedPackageRoot
    $scopePaths = @(Get-ProjectWorldGeneratedPaths `
        -ContentRoot $contentRoot -MapPackage $mapPackage `
        -GeneratedPackageRoot $generatedPackageRoot)
    $identity = [ordered]@{
        compile_result_sha256 = 'a' * 64
        presentation_profile_sha256 = 'b' * 64
        runtime_profile_sha256 = 'none'
        map_package = $mapPackage
    }
    $candidate = New-ProjectWorldCandidateManifest `
        -ProjectRoot $Root -ScopeId $scopeId -Generation 1 -OwningLayer 'map' `
        -OperationId ('c' * 32) -InputIdentity $identity -ScopePaths $scopePaths `
        -GeneratorFingerprint ('d' * 64)
    Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -ProjectRoot $Root `
        -TransactionId ('e' * 32) -OperationId 'enrollment' -CandidateManifests @($candidate) | Out-Null
    return [pscustomobject]@{
        Root = $Root
        ContentRoot = $contentRoot
        ManifestRoot = $manifestRoot
        MapPackage = $mapPackage
        GeneratedPackageRoot = $generatedPackageRoot
        ScopeId = $scopeId
        MapFile = $mapFile
        ActorFile = $actorFile
        SecondActorFile = $secondActorFile
        ScopePaths = $scopePaths
    }
}

function Get-ProjectWorldOuterTestDigest {
    # Independent of the code under test: every file under the paths with its
    # relative path and SHA-256, sorted.
    param([Parameter(Mandatory = $true)][string[]]$Paths)
    $lines = foreach ($path in $Paths) {
        if (-not (Test-Path -LiteralPath $path)) {
            "absent:$path"
            continue
        }
        $root = [System.IO.Path]::GetFullPath($path).TrimEnd('\')
        foreach ($file in @(Get-ChildItem -LiteralPath $root -Recurse -File -Force)) {
            '{0}|{1}|{2}' -f $path, $file.FullName.Substring($root.Length), (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
    }
    return (@($lines | Sort-Object) -join "`n")
}

function Get-ProjectWorldOuterTestState {
    # Generated roots plus the authority entries of the manifest root.
    param([Parameter(Mandatory = $true)][object]$Project)
    $paths = @(Get-ProjectWorldGeneratedRoots -ContentRoot $Project.ContentRoot) + @(
        (Join-Path $Project.ManifestRoot 'active_set.json'),
        (Join-Path $Project.ManifestRoot 'scopes'),
        (Join-Path $Project.ManifestRoot 'archive'))
    return Get-ProjectWorldOuterTestDigest -Paths $paths
}
