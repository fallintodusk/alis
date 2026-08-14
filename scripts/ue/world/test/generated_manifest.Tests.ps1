# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Slice 0A frozen scenarios for the generated-artifact manifest lifecycle
# (contract: Plugins/World/ProjectWorld/docs/territory_generation.md):
# exclusive authority lock; immutable manifests; single active-set commit;
# prospective-set validation; partial-activation interruption with real
# staging debris; malformed/missing manifest fails closed; recovery
# rollback/completion/stale-journal; journal validation and confinement;
# initialization vs enrollment; drift and global-ownership rejection.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_content_transaction.ps1')
    . (Join-Path $PSScriptRoot '..\generated_manifest.ps1')

    $script:RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
    $script:ManifestSchemaValidator = @'
import json
import pathlib
import sys

import jsonschema

for raw_path in sys.argv[1:]:
    path = pathlib.Path(raw_path)
    document = json.loads(path.read_text(encoding='utf-8'))
    schema_path = (path.parent / document['$schema']).resolve()
    schema = json.loads(schema_path.read_text(encoding='utf-8'))
    validator_type = jsonschema.validators.validator_for(schema)
    validator_type.check_schema(schema)
    validator_type(schema).validate(document)
'@

    function script:AssertAuthoritySchemas {
        param([Parameter(Mandatory = $true)][string]$ManifestRoot)

        $activePath = Join-Path $ManifestRoot 'active_set.json'
        $active = Get-Content -LiteralPath $activePath -Raw | ConvertFrom-Json
        $documents = @($activePath)
        $documents += @($active.scopes | ForEach-Object {
            Join-Path $ManifestRoot ([string]$_.manifest_path)
        })
        & python -c $script:ManifestSchemaValidator @documents
        if ($LASTEXITCODE -ne 0) {
            throw "Manifest JSON-Schema validation failed for $ManifestRoot"
        }
    }
}

Describe 'ProjectWorld generated-artifact manifest lifecycle' {
    BeforeEach {
        # TestDrive persists across tests in one block; isolate every test.
        $projectRoot = Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N'))
        $contentRoot = Join-Path $projectRoot 'Plugins\World\ProjectWorldTestData\Content'
        $manifestRoot = Join-Path $projectRoot 'Plugins\World\ProjectWorldTestData\Data\Manifests'
        $transactionParent = Join-Path $projectRoot 'tmp\world\world_realization\transactions'
        $mapPackage = '/ProjectWorldTestData/Generated/Representative/L_TestWorld'
        $generatedPackageRoot = '/ProjectWorldTestData/Generated/'
        $mapRoot = Join-Path $contentRoot 'Generated\Representative'
        $mapFile = Join-Path $mapRoot 'L_TestWorld.umap'
        $externalRoot = Join-Path $contentRoot '__ExternalActors__\Generated\Representative\L_TestWorld'
        $presentationRoot = Join-Path $contentRoot 'Generated\Presentation'
        New-Item -ItemType Directory -Path $mapRoot, $externalRoot, $presentationRoot, $manifestRoot, $transactionParent -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $projectRoot 'Plugins\World\ProjectWorldTestData\ProjectWorldTestData.uplugin') `
            -Value '{"FileVersion":3,"CanContainContent":true}' -NoNewline
        New-Item -ItemType Directory -Path (Join-Path $projectRoot 'Plugins\World\ProjectWorld') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $projectRoot 'Plugins\World\ProjectWorld\ProjectWorld.uplugin') `
            -Value '{"FileVersion":3,"CanContainContent":false}' -NoNewline

        $schemaRoot = Join-Path $projectRoot 'Plugins\World\ProjectWorld\Data\Schemas'
        New-Item -ItemType Directory -Path $schemaRoot -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $script:RepositoryRoot 'Plugins\World\ProjectWorld\Data\Schemas\project_world_active_manifest_set.schema.json') `
            -Destination $schemaRoot
        Copy-Item -LiteralPath (Join-Path $script:RepositoryRoot 'Plugins\World\ProjectWorld\Data\Schemas\project_world_generated_manifest.schema.json') `
            -Destination $schemaRoot
        Set-Content -LiteralPath $mapFile -Value 'map-bytes' -NoNewline
        Set-Content -LiteralPath (Join-Path $externalRoot 'actor.uasset') -Value 'actor-bytes' -NoNewline
        Set-Content -LiteralPath (Join-Path $presentationRoot 'material.uasset') -Value 'material-bytes' -NoNewline

        $mapScopeId = Get-ProjectWorldMapScopeId `
            -MapPackage $mapPackage -GeneratedPackageRoot $generatedPackageRoot
        $presentationScopeId = Get-ProjectWorldPresentationScopeId -ProfileId 'test_profile'
        $mapScopePaths = @(Get-ProjectWorldGeneratedPaths `
            -ContentRoot $contentRoot -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot -IncludePresentation $false)
        $presentationScopePaths = @($presentationRoot)
        $identity = [ordered]@{
            compile_result_sha256 = 'a' * 64
            presentation_profile_sha256 = 'b' * 64
            runtime_profile_sha256 = 'none'
            map_package = $mapPackage
        }

        # Default to the fingerprint the audit will actually compute for this
        # sandbox root, so fixtures exercise the fingerprint-currency gate
        # instead of tripping it with a placeholder.
        $script:SandboxFingerprint = Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot

        function script:NewCandidate {
            param([string]$ScopeId, [int]$Generation, [string[]]$Paths, [string]$Layer = 'map', [string[]]$Consumers = @(),
                [string]$GeneratorFingerprint = $script:SandboxFingerprint)
            return New-ProjectWorldCandidateManifest `
                -ProjectRoot $projectRoot -ScopeId $ScopeId -Generation $Generation `
                -OwningLayer $Layer -OperationId ('c' * 32) -InputIdentity $identity `
                -ScopePaths $Paths -ConsumerReferences $Consumers -GeneratorFingerprint $GeneratorFingerprint
        }

        function script:Enroll {
            $candidates = @(
                (NewCandidate -ScopeId $mapScopeId -Generation 1 -Paths $mapScopePaths),
                (NewCandidate -ScopeId $presentationScopeId -Generation 1 -Paths $presentationScopePaths -Layer 'presentation' -Consumers @($mapScopeId))
            )
            return Publish-ProjectWorldActiveSet `
                -ManifestRoot $manifestRoot -TransactionId ('d' * 32) -OperationId 'enrollment' `
                -CandidateManifests $candidates
        }

        function script:NewJournal {
            param([string]$Phase, [string]$SnapshotRoot, [object[]]$Records = @(), [string[]]$Candidates = @(), [string]$ExpectedSha = '', [string]$Prior = 'none', [string]$Operation = 'apply', [object[]]$RetiredScopes = @(), [string[]]$MutationScopes = $null)
            return [ordered]@{
                transaction_id = (Split-Path -Leaf $SnapshotRoot)
                phase = $Phase
                operation = $Operation
                map_package = $mapPackage
                snapshot_root = $SnapshotRoot
                snapshot_records = @($Records | ForEach-Object {
                    [ordered]@{
                        source = $_.Source
                        backup = $_.Backup
                        existed = $(if ($_.PSObject.Properties.Name -contains 'Existed') { [bool]$_.Existed } else { $true })
                    }
                })
                candidate_manifest_paths = $Candidates
                expected_active_set_sha256 = $ExpectedSha
                prior_active_set_sha256 = $Prior
                mutation_scope_ids = $(if ($null -ne $MutationScopes) { $MutationScopes } else { @($mapScopeId) })
                retired_scopes = $RetiredScopes
            }
        }
    }

    It 'enrolls scopes and validates a clean tree against the active set' {
        Enroll | Out-Null
        AssertAuthoritySchemas -ManifestRoot $manifestRoot
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $activeSet.Record.scopes.Count | Should -Be 2
        $participating = @{ $mapScopeId = $mapScopePaths; $presentationScopeId = $presentationScopePaths }
        { Test-ProjectWorldScopeDrift -ProjectRoot $projectRoot -ActiveSet $activeSet -ScopePathsById $participating } |
            Should -Not -Throw
    }

    It 'publishes and reads an empty ProjectWorldData authority with owner-relative schemas' {
        $ownerRoot = Join-Path $projectRoot 'Plugins\World\ProjectWorldData'
        $ownerContent = Join-Path $ownerRoot 'Content'
        $ownerManifestRoot = Join-Path $ownerRoot 'Data\Manifests'
        $ownerMap = Join-Path $ownerContent 'Generated\P0\L_Test.umap'
        New-Item -ItemType Directory -Path (Split-Path -Parent $ownerMap), $ownerManifestRoot -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $ownerRoot 'ProjectWorldData.uplugin') `
            -Value '{"FileVersion":3,"CanContainContent":true}' -NoNewline
        Set-Content -LiteralPath $ownerMap -Value 'production-map' -NoNewline
        $ownerPackage = '/ProjectWorldData/Generated/P0/L_Test'
        $ownerScope = Get-ProjectWorldMapScopeId `
            -MapPackage $ownerPackage `
            -GeneratedPackageRoot '/ProjectWorldData/Generated/'
        $ownerIdentity = [ordered]@{
            compile_result_sha256 = 'a' * 64
            presentation_profile_sha256 = 'b' * 64
            runtime_profile_sha256 = 'none'
            map_package = $ownerPackage
        }
        $candidate = New-ProjectWorldCandidateManifest `
            -ProjectRoot $projectRoot `
            -ScopeId $ownerScope `
            -Generation 1 `
            -OwningLayer 'map' `
            -OperationId ('9' * 32) `
            -InputIdentity $ownerIdentity `
            -ScopePaths @($ownerMap) `
            -GeneratorFingerprint (Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot)
        Publish-ProjectWorldActiveSet `
            -ProjectRoot $projectRoot `
            -ManifestRoot $ownerManifestRoot `
            -TransactionId ('8' * 32) `
            -OperationId 'project-world-data-enrollment' `
            -CandidateManifests @($candidate) | Out-Null

        AssertAuthoritySchemas -ManifestRoot $ownerManifestRoot

        $active = Read-ProjectWorldActiveSet -ManifestRoot $ownerManifestRoot
        $active.Manifests.Contains($ownerScope) | Should -BeTrue
        $active.Record.'$schema' | Should -Be '../../../ProjectWorld/Data/Schemas/project_world_active_manifest_set.schema.json'
        $active.Manifests[$ownerScope].'$schema' | Should -Be '../../../../ProjectWorld/Data/Schemas/project_world_generated_manifest.schema.json'
    }

    It 'resolves schema authority for a nonstandard validation sandbox root' {
        $sandboxRoot = Join-Path $projectRoot 'tmp\validation\manifests\kazan'
        $candidate = NewCandidate -ScopeId $mapScopeId -Generation 1 -Paths $mapScopePaths
        Publish-ProjectWorldActiveSet `
            -ProjectRoot $projectRoot `
            -ManifestRoot $sandboxRoot `
            -TransactionId ('7' * 32) `
            -OperationId 'sandbox-enrollment' `
            -CandidateManifests @($candidate) | Out-Null
        AssertAuthoritySchemas -ManifestRoot $sandboxRoot
        $active = Read-ProjectWorldActiveSet `
            -ManifestRoot $sandboxRoot `
            -ProjectRoot $projectRoot
        $active.Manifests.Contains($mapScopeId) | Should -BeTrue
    }

    It 'never borrows ProjectWorld authority while auditing ProjectWorldData' {
        Enroll | Out-Null
        $ownerRoot = Join-Path $projectRoot 'Plugins\World\ProjectWorldData'
        New-Item -ItemType Directory -Path (Join-Path $ownerRoot 'Content') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $ownerRoot 'ProjectWorldData.uplugin') `
            -Value '{"FileVersion":3,"CanContainContent":true}' -NoNewline
        $audit = (Resolve-Path (Join-Path $PSScriptRoot '..\audit_generated_authority.ps1')).Path
        $evidence = Join-Path $projectRoot 'wrong-owner-audit.json'
        powershell.exe -NoProfile -ExecutionPolicy Bypass -Command `
            "& '$audit' -ProjectRoot '$projectRoot' -WorldDataPlugin 'ProjectWorldData' -EvidencePath '$evidence'" | Out-Null
        $LASTEXITCODE | Should -Be 1
        $receipt = Get-Content -LiteralPath $evidence -Raw | ConvertFrom-Json
        $receipt.status | Should -Be 'rejected'
        $receipt.manifest_root | Should -BeLike '*ProjectWorldData/Data/Manifests'
        ($receipt.failures -join ';') | Should -BeLike '*No durable active-manifest-set*'
    }

    It 'enforces one OS-exclusive authority lock' {
        $lock = Enter-ProjectWorldAuthorityLock -ManifestRoot $manifestRoot
        try {
            { Enter-ProjectWorldAuthorityLock -ManifestRoot $manifestRoot } |
                Should -Throw '*holds the authority lock*'
        }
        finally { $lock.Dispose() }
        $second = Enter-ProjectWorldAuthorityLock -ManifestRoot $manifestRoot
        $second.Dispose()
    }

    It 'rejects drifted, missing, and unowned generated content before mutation' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $participating = @{ $mapScopeId = $mapScopePaths }
        Set-Content -LiteralPath $mapFile -Value 'hand-edited' -NoNewline
        { Test-ProjectWorldScopeDrift -ProjectRoot $projectRoot -ActiveSet $activeSet -ScopePathsById $participating } |
            Should -Throw '*drifted*'
        Set-Content -LiteralPath $mapFile -Value 'map-bytes' -NoNewline
        Remove-Item -LiteralPath (Join-Path $externalRoot 'actor.uasset')
        { Test-ProjectWorldScopeDrift -ProjectRoot $projectRoot -ActiveSet $activeSet -ScopePathsById $participating } |
            Should -Throw '*missing*'
        Set-Content -LiteralPath (Join-Path $externalRoot 'actor.uasset') -Value 'actor-bytes' -NoNewline
        Set-Content -LiteralPath (Join-Path $externalRoot 'injected.uasset') -Value 'injected' -NoNewline
        { Test-ProjectWorldScopeDrift -ProjectRoot $projectRoot -ActiveSet $activeSet -ScopePathsById $participating } |
            Should -Throw '*unowned artifact*'
    }

    It 'validates the complete prospective set before activation' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        # Generation must follow the prior generation exactly.
        $skip = NewCandidate -ScopeId $mapScopeId -Generation 3 -Paths $mapScopePaths
        { Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('e' * 32) `
            -OperationId 'skip' -CandidateManifests @($skip) -PriorActiveSet $activeSet } |
            Should -Throw '*does not follow prior generation*'
        # Two candidates claiming one artifact path are rejected together.
        $legit = NewCandidate -ScopeId $mapScopeId -Generation 2 -Paths $mapScopePaths
        $thief = NewCandidate -ScopeId 'map_thief' -Generation 1 -Paths $mapScopePaths -Layer 'map'
        { Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('e' * 32) `
            -OperationId 'theft' -CandidateManifests @($legit, $thief) -PriorActiveSet $activeSet } |
            Should -Throw '*Ambiguous ownership*'
        # A consumer reference must resolve inside the prospective set.
        $dangling = NewCandidate -ScopeId 'map_new' -Generation 1 -Paths @() -Layer 'map' -Consumers @('map_absent')
        { Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('e' * 32) `
            -OperationId 'dangling' -CandidateManifests @($dangling) -PriorActiveSet $activeSet } |
            Should -Throw '*not in the prospective active set*'
        # A scope with remaining active consumers cannot be retired.
        { Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('e' * 32) `
            -OperationId 'retire' -CandidateManifests @() -RetiredScopeIds @($presentationScopeId) -PriorActiveSet $activeSet } |
            Should -Throw '*active consumers remain*'
    }

    It 'rejects an incomplete input identity before activation' {
        $candidate = NewCandidate -ScopeId $mapScopeId -Generation 1 -Paths $mapScopePaths
        $candidate.input_identity = [ordered]@{ map_package = $mapPackage }
        { Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('6' * 32) `
                -OperationId 'invalid-identity' -CandidateManifests @($candidate) } |
            Should -Throw '*input_identity.compile_result_sha256*'
        Test-Path -LiteralPath (Join-Path $manifestRoot 'active_set.json') | Should -BeFalse
    }

    It 'keeps manifest documents immutable and advances past inert generations' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $inert = NewCandidate -ScopeId $mapScopeId -Generation 2 -Paths $mapScopePaths
        Write-ProjectWorldJson -Document $inert -Path (Join-Path $manifestRoot "scopes\$mapScopeId.2.json")
        $again = NewCandidate -ScopeId $mapScopeId -Generation 2 -Paths $mapScopePaths
        { Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('e' * 32) `
            -OperationId 'dup' -CandidateManifests @($again) -PriorActiveSet $activeSet } |
            Should -Throw '*does not follow prior generation 2*'
    }

    It 'activates only through the active-set record and rejects unknown root entries' {
        Enroll | Out-Null
        $inert = NewCandidate -ScopeId $mapScopeId -Generation 9 -Paths $mapScopePaths
        Write-ProjectWorldJson -Document $inert -Path (Join-Path $manifestRoot "scopes\$mapScopeId.9.json")
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        [int]$activeSet.Manifests[$mapScopeId].generation | Should -Be 1
        Set-Content -LiteralPath (Join-Path $manifestRoot 'stray.json') -Value '{}' -NoNewline
        { Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot } | Should -Throw '*Unknown entry*'
    }

    It 'fails closed on malformed, missing, or hash-mismatched referenced manifests' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $entry = $activeSet.Record.scopes | Where-Object { $_.scope_id -eq $mapScopeId }
        $manifestPath = Join-Path $manifestRoot ($entry.manifest_path.Replace('/', '\'))
        Add-Content -LiteralPath $manifestPath -Value ' '
        { Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot } | Should -Throw '*hash mismatch*'
        Remove-Item -LiteralPath $manifestPath
        { Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot } | Should -Throw '*missing*'
    }

    It 'refuses initialization over prior authority evidence' {
        Enroll | Out-Null
        # Simulate loss of the activation record with prior scopes present.
        Remove-Item -LiteralPath (Join-Path $manifestRoot 'active_set.json')
        { Assert-ProjectWorldAuthorityInitializable -ManifestRoot $manifestRoot } |
            Should -Throw '*initialization refused*'
    }

    It 'recovers an interrupted mutating transaction by full rollback' {
        Enroll | Out-Null
        $priorActiveSha = (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot).Sha256
        $snapshotRoot = Join-Path $transactionParent ('1' * 32)
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot -SnapshotRoot $snapshotRoot)
        Set-Content -LiteralPath $mapFile -Value 'half-written' -NoNewline
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal (NewJournal -Phase 'mutating' -SnapshotRoot $snapshotRoot -Records $records -Candidates @("scopes/$mapScopeId.2.json") -Prior $priorActiveSha)
        $result = Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot
        $result.State | Should -Be 'rolled_back'
        Get-Content -LiteralPath $mapFile -Raw | Should -Be 'map-bytes'
        Test-Path -LiteralPath (Join-Path $manifestRoot 'journal.json') | Should -BeFalse
        Test-Path -LiteralPath $snapshotRoot | Should -BeFalse
    }

    It 'recovers existing and initially absent layer roots from one interrupted transaction' {
        Enroll | Out-Null
        $priorActiveSha = (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot).Sha256
        $terrainRoot = Join-Path $contentRoot 'Generated\Representative\Terrain'
        $waterRoot = Join-Path $contentRoot 'Generated\Representative\Water'
        New-Item -ItemType Directory -Path $terrainRoot -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $terrainRoot 'terrain.uasset') -Value 'accepted-terrain' -NoNewline
        $snapshotRoot = Join-Path $transactionParent ('a' * 32)
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot -SnapshotRoot $snapshotRoot `
            -AdditionalPaths @($terrainRoot, $waterRoot))
        Set-Content -LiteralPath (Join-Path $terrainRoot 'terrain.uasset') -Value 'partial-terrain' -NoNewline
        New-Item -ItemType Directory -Path $waterRoot -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $waterRoot 'water.uasset') -Value 'partial-water' -NoNewline
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal (NewJournal `
            -Phase 'mutating' -SnapshotRoot $snapshotRoot -Records $records `
            -Candidates @('scopes/layer_terrain.1.json', 'scopes/layer_water.1.json') `
            -Prior $priorActiveSha `
            -MutationScopes @($mapScopeId, 'layer_terrain', 'layer_water'))

        $result = Invoke-ProjectWorldTransactionRecovery `
            -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot
        $result.State | Should -Be 'rolled_back'
        Get-Content -LiteralPath (Join-Path $terrainRoot 'terrain.uasset') -Raw | Should -Be 'accepted-terrain'
        Test-Path -LiteralPath $waterRoot | Should -BeFalse
    }

    It 'rolls back a real partial activation including active-set staging debris' {
        Enroll | Out-Null
        $priorActiveSha = (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot).Sha256
        $snapshotRoot = Join-Path $transactionParent ('2' * 32)
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot -MapPackage $mapPackage `
            -GeneratedPackageRoot $generatedPackageRoot -SnapshotRoot $snapshotRoot)
        Set-Content -LiteralPath $mapFile -Value 'candidate-bytes' -NoNewline
        # Real crash sequence: candidate written, staging active set written,
        # journal at 'publishing', process dies before the atomic replace.
        $candidate = NewCandidate -ScopeId $mapScopeId -Generation 2 -Paths $mapScopePaths
        Write-ProjectWorldJson -Document $candidate -Path (Join-Path $manifestRoot "scopes\$mapScopeId.2.json")
        Set-Content -LiteralPath (Join-Path $manifestRoot 'active_set.json.tmp') -Value '{"staged":true}' -NoNewline
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal (NewJournal -Phase 'publishing' -SnapshotRoot $snapshotRoot -Records $records -Candidates @("scopes/$mapScopeId.2.json") -ExpectedSha ('e' * 64) -Prior $priorActiveSha)
        { Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot } | Should -Throw '*Staging debris*'
        $result = Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot
        $result.State | Should -Be 'rolled_back'
        Get-Content -LiteralPath $mapFile -Raw | Should -Be 'map-bytes'
        Test-Path -LiteralPath (Join-Path $manifestRoot "scopes\$mapScopeId.2.json") | Should -BeFalse
        Test-Path -LiteralPath (Join-Path $manifestRoot 'active_set.json.tmp') | Should -BeFalse
        (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot).Sha256 | Should -Be $priorActiveSha
    }

    It 'recognizes a stale journal after a committed transaction and finalizes it' {
        Enroll | Out-Null
        $activeSha = (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot).Sha256
        $snapshotRoot = Join-Path $transactionParent ('3' * 32)
        New-Item -ItemType Directory -Path $snapshotRoot -Force | Out-Null
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal (NewJournal -Phase 'publishing' -SnapshotRoot $snapshotRoot -ExpectedSha $activeSha)
        $result = Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot
        $result.State | Should -Be 'completed'
        Get-Content -LiteralPath $mapFile -Raw | Should -Be 'map-bytes'
        Test-Path -LiteralPath (Join-Path $manifestRoot 'journal.json') | Should -BeFalse
        (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot).Sha256 | Should -Be $activeSha
    }

    It 'fails closed on an invalid or unconfined journal without deleting anything' {
        Enroll | Out-Null
        $outside = Join-Path $TestDrive ('5' * 32)
        New-Item -ItemType Directory -Path $outside -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $outside 'victim.txt') -Value 'must-survive' -NoNewline
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal (NewJournal -Phase 'mutating' -SnapshotRoot $outside)
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } |
            Should -Throw '*escapes the transaction parent*'
        Test-Path -LiteralPath (Join-Path $outside 'victim.txt') | Should -BeTrue
        Test-Path -LiteralPath (Join-Path $manifestRoot 'journal.json') | Should -BeTrue
    }

    It 'fails closed when an interrupted transaction has no recovery snapshot' {
        Enroll | Out-Null
        $priorActiveSha = (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot).Sha256
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal (NewJournal -Phase 'mutating' -SnapshotRoot (Join-Path $transactionParent ('4' * 32)) -Prior $priorActiveSha)
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } |
            Should -Throw '*fails closed*'
    }

    It 'enforces one project-global content mutation lock across different manifest roots' {
        $lock = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
        try {
            # A second operation using ANY manifest root still serializes.
            { Enter-ProjectWorldContentLock -ProjectRoot $projectRoot } |
                Should -Throw '*content mutation lock*'
        }
        finally { $lock.Dispose() }
    }

    It 'refuses completion and preserves the snapshot when the committed result fails validation' {
        Enroll | Out-Null
        $activeSha = (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot).Sha256
        $snapshotRoot = Join-Path $transactionParent ('6' * 32)
        New-Item -ItemType Directory -Path $snapshotRoot -Force | Out-Null
        # Active set committed, but a mutation-scope artifact is corrupted.
        Set-Content -LiteralPath $mapFile -Value 'corrupted-after-commit' -NoNewline
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal (NewJournal -Phase 'publishing' -SnapshotRoot $snapshotRoot -ExpectedSha $activeSha)
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } |
            Should -Throw '*post-commit validation*'
        Test-Path -LiteralPath $snapshotRoot | Should -BeTrue
        Test-Path -LiteralPath (Join-Path $manifestRoot 'journal.json') | Should -BeTrue
    }

    It 'cleans an interrupted journal write and fails closed on orphan active-set staging' {
        Enroll | Out-Null
        # Crash during the staged journal write: only journal.json.tmp exists.
        Set-Content -LiteralPath (Join-Path $manifestRoot 'journal.json.tmp') -Value '{' -NoNewline
        $result = Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot
        $result.State | Should -Be 'no_transaction'
        Test-Path -LiteralPath (Join-Path $manifestRoot 'journal.json.tmp') | Should -BeFalse
        # Orphan active-set staging with no journal is an unknown state.
        Set-Content -LiteralPath (Join-Path $manifestRoot 'active_set.json.tmp') -Value '{}' -NoNewline
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } |
            Should -Throw '*fails closed*'
    }

    It 'rejects duplicate candidates for the same existing scope' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $one = NewCandidate -ScopeId $mapScopeId -Generation 2 -Paths $mapScopePaths
        $two = NewCandidate -ScopeId $mapScopeId -Generation 2 -Paths @()
        { Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('e' * 32) `
            -OperationId 'dup' -CandidateManifests @($one, $two) -PriorActiveSet $activeSet } |
            Should -Throw '*Duplicate candidate scope*'
    }

    It 'produces identical generator fingerprints under different repo roots and differs on one byte' {
        $roots = @('rootA', 'rootB') | ForEach-Object { Join-Path $TestDrive "$_-$([System.Guid]::NewGuid().ToString('N'))" }
        foreach ($root in $roots) {
            $dir = Join-Path $root 'scripts\ue\world'
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
            Set-Content -LiteralPath (Join-Path $dir 'gen.ps1') -Value 'generator-bytes' -NoNewline
        }
        $first = Get-ProjectWorldGeneratorFingerprint -ProjectRoot $roots[0]
        $second = Get-ProjectWorldGeneratorFingerprint -ProjectRoot $roots[1]
        $first | Should -Be $second
        Set-Content -LiteralPath (Join-Path $roots[1] 'scripts\ue\world\gen.ps1') -Value 'generator-byteX' -NoNewline
        Get-ProjectWorldGeneratorFingerprint -ProjectRoot $roots[1] | Should -Not -Be $first
    }

    It 'writes authority documents with LF-only bytes so a clean clone still verifies' {
        Enroll | Out-Null
        # The recorded hashes ARE the activation authority and the repo
        # declares eol=lf for these paths. A CR byte here means git rewrites
        # the file on checkout and every manifest_sha256 mismatches on a
        # fresh clone - the authority fails closed for a non-content reason.
        foreach ($path in @(
            (Join-Path $manifestRoot 'active_set.json'),
            (Join-Path $manifestRoot "scopes\$mapScopeId.1.json"))) {
            $bytes = [System.IO.File]::ReadAllBytes($path)
            ($bytes -contains [byte]13) | Should -BeFalse -Because "$path must contain no CR bytes"
        }
    }

    It 'excludes read-only verifiers and test sources from the generator fingerprint' {
        $root = Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N'))
        $dir = Join-Path $root 'scripts\ue\world'
        $editorDir = Join-Path $root 'Plugins\World\ProjectWorld\Source\ProjectWorldEditor\Private'
        New-Item -ItemType Directory -Path (Join-Path $dir 'test') -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $editorDir 'Tests') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $dir 'gen.ps1') -Value 'generator-bytes' -NoNewline
        Set-Content -LiteralPath (Join-Path $dir 'audit_generated_authority.ps1') -Value 'auditor-v1' -NoNewline
        Set-Content -LiteralPath (Join-Path $dir 'test\some.Tests.ps1') -Value 'test-v1' -NoNewline
        Set-Content -LiteralPath (Join-Path $editorDir 'Generator.cpp') -Value 'cpp-generator-v1' -NoNewline
        Set-Content -LiteralPath (Join-Path $editorDir 'Tests\GeneratorTests.cpp') -Value 'cpp-test-v1' -NoNewline
        $baseline = Get-ProjectWorldGeneratorFingerprint -ProjectRoot $root

        # Editing the read-only auditor or a test cannot change generated bytes,
        # so it must not invalidate every accepted manifest.
        Set-Content -LiteralPath (Join-Path $dir 'audit_generated_authority.ps1') -Value 'auditor-v2-rewritten' -NoNewline
        Set-Content -LiteralPath (Join-Path $dir 'test\some.Tests.ps1') -Value 'test-v2-rewritten' -NoNewline
        Set-Content -LiteralPath (Join-Path $editorDir 'Tests\GeneratorTests.cpp') -Value 'cpp-test-v2-rewritten' -NoNewline
        Get-ProjectWorldGeneratorFingerprint -ProjectRoot $root | Should -Be $baseline

        # A real generator edit still moves it.
        Set-Content -LiteralPath (Join-Path $editorDir 'Generator.cpp') -Value 'cpp-generator-v2' -NoNewline
        Get-ProjectWorldGeneratorFingerprint -ProjectRoot $root | Should -Not -Be $baseline
    }

    It 'audit fails closed when a manifest was accepted by a different generator' {
        Enroll | Out-Null
        # Enroll writes candidate fingerprints from the sandbox project root,
        # which has no generator sources, so republish one scope with a
        # deliberately foreign fingerprint and prove the audit refuses it.
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $stale = NewCandidate -ScopeId $presentationScopeId -Generation 2 -Paths $presentationScopePaths -Layer 'presentation' `
            -GeneratorFingerprint ('c' * 64)
        Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('c3' * 16) `
            -OperationId 'stale-generator' -CandidateManifests @($stale) -PriorActiveSet $activeSet | Out-Null
        $audit = (Resolve-Path (Join-Path $PSScriptRoot '..\audit_generated_authority.ps1')).Path
        $evidence = Join-Path $projectRoot 'audit.json'
        $genA = Join-Path $contentRoot 'Generated'
        $genB = Join-Path $contentRoot '__ExternalActors__\Generated'
        powershell.exe -NoProfile -ExecutionPolicy Bypass -Command `
            "& '$audit' -ProjectRoot '$projectRoot' -WorldDataPlugin 'ProjectWorldTestData' -ManifestRoot '$manifestRoot' -GeneratedRoots @('$genA','$genB') -EvidencePath '$evidence'" | Out-Null
        $LASTEXITCODE | Should -Be 1
        $receipt = Get-Content -LiteralPath $evidence -Raw | ConvertFrom-Json
        ($receipt.failures -join ';') | Should -BeLike '*generator_fingerprint_current*'
    }

    It 'refuses retirement completion when a prior-owned artifact survives on disk' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $priorMapEntry = $activeSet.Record.scopes | Where-Object { $_.scope_id -eq $mapScopeId }
        # Commit a retirement (unlink consumer first), but leave the map
        # artifact on disk to simulate a crash between artifact deletion
        # and completion.
        $noConsumers = NewCandidate -ScopeId $presentationScopeId -Generation 2 -Paths $presentationScopePaths -Layer 'presentation'
        $published = Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('a7' * 16) `
            -OperationId 'unlink' -CandidateManifests @($noConsumers) -PriorActiveSet $activeSet
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $published = Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('b8' * 16) `
            -OperationId 'retire' -CandidateManifests @() -RetiredScopeIds @($mapScopeId) -PriorActiveSet $activeSet
        $snapshotRoot = Join-Path $transactionParent ('7' * 32)
        New-Item -ItemType Directory -Path $snapshotRoot -Force | Out-Null
        $journal = NewJournal -Phase 'publishing' -SnapshotRoot $snapshotRoot -ExpectedSha $published.Sha256 -Operation 'delete' -RetiredScopes @([ordered]@{
            scope_id = $mapScopeId
            prior_manifest_path = $priorMapEntry.manifest_path
            prior_manifest_sha256 = $priorMapEntry.manifest_sha256
        })
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } |
            Should -Throw '*unowned artifact on disk*'
        Test-Path -LiteralPath $snapshotRoot | Should -BeTrue
        Test-Path -LiteralPath (Join-Path $manifestRoot 'journal.json') | Should -BeTrue
    }

    It 'fails closed when committed retirement does not match the declared retired set' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $priorMapEntry = $activeSet.Record.scopes | Where-Object { $_.scope_id -eq $mapScopeId }
        # Retire BOTH scopes (unlink the consumer first), but declare only
        # the map scope as retired in the journal.
        $noConsumers = NewCandidate -ScopeId $presentationScopeId -Generation 2 -Paths $presentationScopePaths -Layer 'presentation'
        Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('c9' * 16) `
            -OperationId 'unlink' -CandidateManifests @($noConsumers) -PriorActiveSet $activeSet | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $published = Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('d0' * 16) `
            -OperationId 'retire-both' -CandidateManifests @() `
            -RetiredScopeIds @($mapScopeId, $presentationScopeId) -PriorActiveSet $activeSet
        Remove-Item -LiteralPath $mapFile -Force
        Remove-Item -LiteralPath (Join-Path $externalRoot 'actor.uasset') -Force
        Remove-Item -LiteralPath (Join-Path $presentationRoot 'material.uasset') -Force
        $snapshotRoot = Join-Path $transactionParent ('a' * 32)
        New-Item -ItemType Directory -Path $snapshotRoot -Force | Out-Null
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal (NewJournal `
            -Phase 'publishing' -SnapshotRoot $snapshotRoot -ExpectedSha $published.Sha256 `
            -Operation 'delete' -MutationScopes @($mapScopeId, $presentationScopeId) `
            -RetiredScopes @([ordered]@{
                scope_id = $mapScopeId
                prior_manifest_path = $priorMapEntry.manifest_path
                prior_manifest_sha256 = $priorMapEntry.manifest_sha256
            }))
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } |
            Should -Throw '*does not match the journal retirement evidence*'
        Test-Path -LiteralPath $snapshotRoot | Should -BeTrue
        Test-Path -LiteralPath (Join-Path $manifestRoot 'journal.json') | Should -BeTrue
    }

    It 'refuses impossible wrapper switch combinations before any mutation' {
        $wrapper = Join-Path $PSScriptRoot '..\realize_canonical_world.ps1'
        $sandboxRoot = Join-Path $projectRoot 'sandbox_manifests'
        foreach ($combo in @(
            @{ Mode = 'Delete'; Switch = '-Reconstruct' },
            @{ Mode = 'Delete'; Switch = '-EnrollManifests' },
            @{ Mode = 'Apply'; Switch = '-Reconstruct -EnrollManifests' }
        )) {
            $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -Command `
                "& '$wrapper' -CompileResult 'X:\missing.json' -Mode $($combo.Mode) $($combo.Switch) -ManifestRoot '$sandboxRoot'" 2>&1
            $LASTEXITCODE | Should -Not -Be 0
            ($output -join ' ') | Should -Match 'Invalid combination'
        }
        Test-Path -LiteralPath $sandboxRoot | Should -BeFalse
    }

    It 'fails closed when a publishing journal omits retirement evidence entirely' {
        Enroll | Out-Null
        $activeSha = (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot).Sha256
        $snapshotRoot = Join-Path $transactionParent ('8' * 32)
        New-Item -ItemType Directory -Path $snapshotRoot -Force | Out-Null
        $journal = NewJournal -Phase 'publishing' -SnapshotRoot $snapshotRoot -ExpectedSha $activeSha
        $journal.Remove('retired_scopes')
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal $journal
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } |
            Should -Throw "*missing required field 'retired_scopes'*"
        Test-Path -LiteralPath $snapshotRoot | Should -BeTrue
        Test-Path -LiteralPath (Join-Path $manifestRoot 'journal.json') | Should -BeTrue
    }

    It 'fails closed when a delete journal declares no retired scopes' {
        Enroll | Out-Null
        $activeSha = (Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot).Sha256
        $snapshotRoot = Join-Path $transactionParent ('9' * 32)
        New-Item -ItemType Directory -Path $snapshotRoot -Force | Out-Null
        Write-ProjectWorldTransactionJournal -ManifestRoot $manifestRoot -Journal (NewJournal -Phase 'publishing' -SnapshotRoot $snapshotRoot -ExpectedSha $activeSha -Operation 'delete')
        { Invoke-ProjectWorldTransactionRecovery -ManifestRoot $manifestRoot -ContentRoot $contentRoot -ProjectRoot $projectRoot } |
            Should -Throw '*no retirement evidence*'
        Test-Path -LiteralPath $snapshotRoot | Should -BeTrue
        Test-Path -LiteralPath (Join-Path $manifestRoot 'journal.json') | Should -BeTrue
    }

    It 'retires a consumer-free scope and archives its manifest only after commit' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $priorMapManifest = Join-Path $manifestRoot "scopes\$mapScopeId.1.json"
        # Retire the presentation consumer first (new generation, no consumers),
        # then the map; assert the retired manifest is still in place at the
        # pre-commit hook and archived only afterwards.
        $noConsumers = NewCandidate -ScopeId $presentationScopeId -Generation 2 -Paths $presentationScopePaths -Layer 'presentation'
        Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('a1' * 16) `
            -OperationId 'unlink' -CandidateManifests @($noConsumers) -PriorActiveSet $activeSet | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('b2' * 16) `
            -OperationId 'retire' -CandidateManifests @() -RetiredScopeIds @($mapScopeId) -PriorActiveSet $activeSet `
            -BeforeCommit { param($sha) if (-not (Test-Path -LiteralPath $priorMapManifest)) { throw 'retired manifest moved before commit' } } | Out-Null
        $after = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $after.Manifests.Contains($mapScopeId) | Should -BeFalse
        Test-Path -LiteralPath $priorMapManifest | Should -BeFalse
        @(Get-ChildItem -LiteralPath (Join-Path $manifestRoot 'archive')).Count | Should -Be 1
    }

    It 'retires a map and its last presentation consumer atomically' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('c3' * 16) `
            -OperationId 'retire-last-consumer' -CandidateManifests @() `
            -RetiredScopeIds @($mapScopeId, $presentationScopeId) -PriorActiveSet $activeSet | Out-Null
        $after = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $after.Manifests.Count | Should -Be 0
    }

    It 'continues immutable generations when a retired scope is re-enrolled' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('e5' * 16) `
            -OperationId 'retire-all' -CandidateManifests @() `
            -RetiredScopeIds @($mapScopeId, $presentationScopeId) -PriorActiveSet $activeSet | Out-Null
        $emptySet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $candidates = @(
            (NewCandidate -ScopeId $mapScopeId -Generation 2 -Paths $mapScopePaths),
            (NewCandidate -ScopeId $presentationScopeId -Generation 2 -Paths $presentationScopePaths `
                -Layer 'presentation' -Consumers @($mapScopeId))
        )
        Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('f6' * 16) `
            -OperationId 're-enroll' -CandidateManifests $candidates -PriorActiveSet $emptySet | Out-Null
        $after = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $after.Manifests[$mapScopeId].generation | Should -Be 2
        $after.Manifests[$presentationScopeId].generation | Should -Be 2
    }

    It 'preserves presentation provenance while removing one consumer' {
        Enroll | Out-Null
        $activeSet = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $prior = $activeSet.Manifests[$presentationScopeId]
        $priorIdentity = [ordered]@{
            compile_result_sha256 = [string]$prior.input_identity.compile_result_sha256
            presentation_profile_sha256 = [string]$prior.input_identity.presentation_profile_sha256
            runtime_profile_sha256 = [string]$prior.input_identity.runtime_profile_sha256
            map_package = [string]$prior.input_identity.map_package
        }
        $candidate = New-ProjectWorldCandidateManifest `
            -ProjectRoot $projectRoot -ScopeId $presentationScopeId -Generation 2 `
            -OwningLayer 'presentation' -OperationId ('c' * 32) `
            -InputIdentity $priorIdentity -ScopePaths $presentationScopePaths `
            -ConsumerReferences @() -GeneratorFingerprint $script:SandboxFingerprint
        Publish-ProjectWorldActiveSet -ManifestRoot $manifestRoot -TransactionId ('d4' * 16) `
            -OperationId 'unlink-consumer' -CandidateManifests @($candidate) -PriorActiveSet $activeSet | Out-Null
        $after = Read-ProjectWorldActiveSet -ManifestRoot $manifestRoot
        $after.Manifests[$presentationScopeId].input_identity.presentation_profile_sha256 |
            Should -Be ('b' * 64)
    }

    It 'honors delegated content-lock ownership only against a live matching owner' {
        $owner = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
        try {
            $lockPath = Join-Path $projectRoot 'tmp\world\world_realization\content_mutation.lock'
            $tokenReader = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            try {
                $buffer = New-Object byte[] 256
                $count = $tokenReader.Read($buffer, 0, $buffer.Length)
                $liveToken = [System.Text.Encoding]::ASCII.GetString($buffer, 0, $count).Trim()
            }
            finally { $tokenReader.Dispose() }
            $liveToken.Length | Should -Be 32

            # Direction 1: a child with the matching token joins without self-conflict.
            $env:ALIS_WORLD_CONTENT_LOCK_TOKEN = $liveToken
            $delegated = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
            $delegated | Should -Not -BeNullOrEmpty
            $delegated.Dispose()
            # Releasing the delegated view must not release the owner: the
            # held-probe inside a fresh delegation would fail otherwise.
            $stillDelegated = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
            $stillDelegated | Should -Not -BeNullOrEmpty
            $stillDelegated.Dispose()

            # Direction 2: a mismatched token fails closed while the owner lives.
            $env:ALIS_WORLD_CONTENT_LOCK_TOKEN = 'f' * 32
            { Enter-ProjectWorldContentLock -ProjectRoot $projectRoot } |
                Should -Throw '*does not match the live lock owner*'

            # Direction 2: without any token the normal exclusive conflict remains.
            Remove-Item Env:ALIS_WORLD_CONTENT_LOCK_TOKEN
            { Enter-ProjectWorldContentLock -ProjectRoot $projectRoot } |
                Should -Throw '*Another operation holds the ProjectWorld content mutation lock*'
        }
        finally {
            Remove-Item Env:ALIS_WORLD_CONTENT_LOCK_TOKEN -ErrorAction SilentlyContinue
            $owner.Dispose()
        }
    }

    It 'audits an enrolled durable authority as accepted with one receipt' {
        Enroll | Out-Null
        $audit = (Resolve-Path (Join-Path $PSScriptRoot '..\audit_generated_authority.ps1')).Path
        $evidence = Join-Path $projectRoot 'audit.json'
        $genA = Join-Path $contentRoot 'Generated'
        $genB = Join-Path $contentRoot '__ExternalActors__\Generated'
        powershell.exe -NoProfile -ExecutionPolicy Bypass -Command `
            "& '$audit' -ProjectRoot '$projectRoot' -WorldDataPlugin 'ProjectWorldTestData' -ManifestRoot '$manifestRoot' -GeneratedRoots @('$genA','$genB') -EvidencePath '$evidence'" | Out-Null
        $LASTEXITCODE | Should -Be 0
        $receipt = Get-Content -LiteralPath $evidence -Raw | ConvertFrom-Json
        $receipt.status | Should -Be 'accepted'
        @($receipt.failures).Count | Should -Be 0
        @($receipt.scopes).Count | Should -Be 2
        ($receipt.checks | Where-Object { -not $_.passed }) | Should -BeNullOrEmpty
        $receipt.active_set.transaction_id | Should -Match '^[a-f0-9]{32}$'
    }

    It 'audit rejects a drifted artifact and an unowned file with a rejected receipt' {
        Enroll | Out-Null
        Set-Content -LiteralPath $mapFile -Value 'tampered-bytes' -NoNewline
        Set-Content -LiteralPath (Join-Path $mapRoot 'stray.uasset') -Value 'stray' -NoNewline
        $audit = (Resolve-Path (Join-Path $PSScriptRoot '..\audit_generated_authority.ps1')).Path
        $evidence = Join-Path $projectRoot 'audit.json'
        $genA = Join-Path $contentRoot 'Generated'
        $genB = Join-Path $contentRoot '__ExternalActors__\Generated'
        powershell.exe -NoProfile -ExecutionPolicy Bypass -Command `
            "& '$audit' -ProjectRoot '$projectRoot' -WorldDataPlugin 'ProjectWorldTestData' -ManifestRoot '$manifestRoot' -GeneratedRoots @('$genA','$genB') -EvidencePath '$evidence'" | Out-Null
        $LASTEXITCODE | Should -Be 1
        $receipt = Get-Content -LiteralPath $evidence -Raw | ConvertFrom-Json
        $receipt.status | Should -Be 'rejected'
        ($receipt.failures -join ';') | Should -BeLike '*drifted artifact*'
        ($receipt.failures -join ';') | Should -BeLike '*unowned_scan*'
    }

    It 'audit default roots cover every generated external-package family' {
        Enroll | Out-Null
        # Regression: the default scan once omitted __ExternalObjects__, so an
        # unowned file in that family passed silently. Run WITHOUT
        # -GeneratedRoots so the script's own defaults are what is tested.
        $strayRoot = Join-Path $contentRoot '__ExternalObjects__\Generated\Representative\L_TestWorld'
        New-Item -ItemType Directory -Path $strayRoot -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $strayRoot 'orphan.uasset') -Value 'orphan' -NoNewline
        $audit = (Resolve-Path (Join-Path $PSScriptRoot '..\audit_generated_authority.ps1')).Path
        $evidence = Join-Path $projectRoot 'audit.json'
        powershell.exe -NoProfile -ExecutionPolicy Bypass -Command `
            "& '$audit' -ProjectRoot '$projectRoot' -WorldDataPlugin 'ProjectWorldTestData' -ManifestRoot '$manifestRoot' -EvidencePath '$evidence'" | Out-Null
        $LASTEXITCODE | Should -Be 1
        $receipt = Get-Content -LiteralPath $evidence -Raw | ConvertFrom-Json
        ($receipt.failures -join ';') | Should -BeLike '*orphan.uasset*'
    }

    It 'audit rejects while a transaction journal is pending' {
        Enroll | Out-Null
        Set-Content -LiteralPath (Join-Path $manifestRoot 'journal.json') -Value '{}' -NoNewline
        $audit = (Resolve-Path (Join-Path $PSScriptRoot '..\audit_generated_authority.ps1')).Path
        $evidence = Join-Path $projectRoot 'audit.json'
        $genA = Join-Path $contentRoot 'Generated'
        $genB = Join-Path $contentRoot '__ExternalActors__\Generated'
        powershell.exe -NoProfile -ExecutionPolicy Bypass -Command `
            "& '$audit' -ProjectRoot '$projectRoot' -WorldDataPlugin 'ProjectWorldTestData' -ManifestRoot '$manifestRoot' -GeneratedRoots @('$genA','$genB') -EvidencePath '$evidence'" | Out-Null
        $LASTEXITCODE | Should -Be 1
        $receipt = Get-Content -LiteralPath $evidence -Raw | ConvertFrom-Json
        $receipt.status | Should -Be 'rejected'
        ($receipt.failures -join ';') | Should -BeLike '*transaction_settled*'
    }

    It 'fails a delegation claim closed when no live owner holds the lock' {
        $lockDir = Join-Path $projectRoot 'tmp\world\world_realization'
        New-Item -ItemType Directory -Path $lockDir -Force | Out-Null
        # Stale token in a RELEASED lock file: claim must be rejected, never
        # silently downgraded to self-acquisition.
        Set-Content -LiteralPath (Join-Path $lockDir 'content_mutation.lock') -Value ('e' * 32) -NoNewline
        $env:ALIS_WORLD_CONTENT_LOCK_TOKEN = 'e' * 32
        try {
            { Enter-ProjectWorldContentLock -ProjectRoot $projectRoot } |
                Should -Throw '*no live owner holds the lock*'
        }
        finally {
            Remove-Item Env:ALIS_WORLD_CONTENT_LOCK_TOKEN -ErrorAction SilentlyContinue
        }
    }
}
