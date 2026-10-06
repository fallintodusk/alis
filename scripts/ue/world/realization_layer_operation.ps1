# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function New-ProjectWorldLayerInputIdentity {
    param([Parameter(Mandatory = $true)][System.Collections.IDictionary]$OperationIdentity)

    $identity = [ordered]@{}
    foreach ($entry in $OperationIdentity.GetEnumerator()) {
        $identity[[string]$entry.Key] = $entry.Value
    }
    # Runtime partition policy is serialized only by the map owner. Generated
    # layer producers must never claim it as an input or republish for it.
    $identity.runtime_profile_sha256 = 'none'
    return $identity
}

function Resolve-ProjectWorldRealizationLayers {
    param(
        [object]$RealizationDocument,
        [Parameter(Mandatory = $true)][object]$WorldDataRoots
    )

    $definitions = [ordered]@{}
    $scopePaths = @{}
    if ($null -eq $RealizationDocument) {
        return [pscustomobject]@{ Definitions = $definitions; ScopePaths = $scopePaths }
    }
    foreach ($layer in @($RealizationDocument.layers)) {
        if ([string]$layer.layer_id -notmatch '^[a-z0-9_]+$') {
            throw "Realization layer has an invalid ID: $($layer.layer_id)"
        }
        $scopeId = "layer_$($RealizationDocument.profile_id)_$($layer.layer_id)"
        $artifactRoot = [string]$layer.artifact_root
        if ($definitions.Contains($scopeId) -or
            $artifactRoot.Length -le $WorldDataRoots.GeneratedPackageRoot.Length -or
            -not $artifactRoot.StartsWith(
                $WorldDataRoots.GeneratedPackageRoot,
            [System.StringComparison]::Ordinal) -or
            -not $artifactRoot.EndsWith('/')) {
            throw "Generated realization layer has an invalid or duplicate owned root: $scopeId"
        }
        $relativeRoot = $artifactRoot.Substring($WorldDataRoots.MountRoot.Length).Replace(
            '/',
            [System.IO.Path]::DirectorySeparatorChar).TrimEnd('\', '/')
        $diskRoot = Assert-ProjectWorldOwnedPath `
            -ContentRoot $WorldDataRoots.ContentRoot `
            -Path (Join-Path $WorldDataRoots.ContentRoot $relativeRoot)
        $definitions[$scopeId] = $layer
        $scopePaths[$scopeId] = @($diskRoot)
    }
    return [pscustomobject]@{ Definitions = $definitions; ScopePaths = $scopePaths }
}

function Get-ProjectWorldLayerScopePath {
    param(
        [Parameter(Mandatory = $true)][object]$WorldDataRoots,
        [Parameter(Mandatory = $true)][object]$LayerContract
    )

    $packageRoot = [string]$LayerContract.artifact_root
    if ($packageRoot.Length -le $WorldDataRoots.GeneratedPackageRoot.Length -or
        -not $packageRoot.StartsWith($WorldDataRoots.GeneratedPackageRoot, [System.StringComparison]::Ordinal)) {
        throw "Accepted layer root escapes the generated owner boundary: $packageRoot"
    }
    $relativeRoot = $packageRoot.Substring($WorldDataRoots.MountRoot.Length).Replace(
        '/',
        [System.IO.Path]::DirectorySeparatorChar).TrimEnd('\', '/')
    return Assert-ProjectWorldOwnedPath `
        -ContentRoot $WorldDataRoots.ContentRoot `
        -Path (Join-Path $WorldDataRoots.ContentRoot $relativeRoot)
}

function New-ProjectWorldLayerDirtyInput {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$OutputDirectory,
        [Parameter(Mandatory = $true)][object]$RealizationDocument,
        [Parameter(Mandatory = $true)][object]$LayerDefinitions,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$ProducerFingerprints,
        [object]$ActiveSet = $null,
        [AllowEmptyCollection()][string[]]$OperatorDirtyUnits = @()
    )

    $operatorUnits = [ordered]@{}
    foreach ($entry in $OperatorDirtyUnits) {
        $separator = $entry.IndexOf('=')
        if ($separator -lt 1 -or $separator -eq $entry.Length - 1) {
            throw "DirtyUnit must use layer_id=unit_id: $entry"
        }
        $layerId = $entry.Substring(0, $separator)
        $matchingScopes = @($LayerDefinitions.Keys | Where-Object {
            [string]$LayerDefinitions[$_].layer_id -ceq $layerId
        })
        if ($matchingScopes.Count -ne 1) {
            throw "DirtyUnit targets an unknown generated layer: $entry"
        }
        if (-not $operatorUnits.Contains($layerId)) {
            $operatorUnits[$layerId] = [System.Collections.Generic.List[string]]::new()
        }
        $operatorUnits[$layerId].Add($entry.Substring($separator + 1))
    }

    if ($ProducerFingerprints.Count -ne $LayerDefinitions.Count) {
        throw 'Layer request producer fingerprints do not cover exactly the generated layers.'
    }
    $fingerprints = @()
    foreach ($scopeId in $LayerDefinitions.Keys) {
        if (-not $ProducerFingerprints.Contains($scopeId) -or
            [string]$ProducerFingerprints[$scopeId] -cnotmatch '^[a-f0-9]{64}$') {
            throw "Layer request has no valid producer fingerprint: $scopeId"
        }
        $fingerprints += , ([ordered]@{
            layer_id = [string]$LayerDefinitions[$scopeId].layer_id
            generator_fingerprint = [string]$ProducerFingerprints[$scopeId]
        })
    }
    $baseLayers = @()
    $identityDirtyLayers = @()
    if ($null -ne $ActiveSet) {
        foreach ($scopeId in $LayerDefinitions.Keys) {
            if (-not $ActiveSet.Manifests.Contains($scopeId)) { continue }
            $priorManifest = $ActiveSet.Manifests[$scopeId]
            $definition = $LayerDefinitions[$scopeId]
            $layerId = [string]$definition.layer_id
            $currentFingerprint = [string]$ProducerFingerprints[$scopeId]
            if ([string]$priorManifest.generator_fingerprint -cne $currentFingerprint) {
                $identityDirtyLayers += $layerId
            }
            $contract = $priorManifest.layer_contract
            $baseLayers += , ([ordered]@{
                layer_id = $layerId
                normalized_layer_contract_sha256 = [string]$contract.normalized_layer_contract_sha256
                canonical_inputs = @($contract.canonical_inputs)
            })
        }
    }

    $schemaPath = Join-Path $ProjectRoot 'Plugins\World\ProjectWorld\Data\Schemas\project_world_layer_dirty_input.schema.json'
    $fromUri = [System.Uri]::new([System.IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar)
    $toUri = [System.Uri]::new([System.IO.Path]::GetFullPath($schemaPath))
    $schemaReference = [System.Uri]::UnescapeDataString($fromUri.MakeRelativeUri($toUri).ToString()).Replace('\', '/')
    $path = Join-Path $OutputDirectory 'layer_dirty_input.json'
    Write-ProjectWorldJson -Path $path -Document ([ordered]@{
        '$schema' = $schemaReference
        schema_version = 2
        realization_profile_id = [string]$RealizationDocument.profile_id
        producer_fingerprints = @($fingerprints | Sort-Object layer_id)
        base_layers = $baseLayers
        identity_dirty_layers = @($identityDirtyLayers | Sort-Object -Unique)
        operator_additions = @($operatorUnits.Keys | Sort-Object | ForEach-Object {
            [ordered]@{ layer_id = $_; units = @($operatorUnits[$_] | Sort-Object -Unique) }
        })
    })
    return [pscustomobject]@{
        Path = $path
        Sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Test-ProjectWorldManifestUnchanged {
    param(
        [object]$PriorManifest,
        [Parameter(Mandatory = $true)][object]$CandidateManifest,
        [Parameter(Mandatory = $true)][string]$GeneratorFingerprint,
        [switch]$CompareLayerContract
    )

    if ($null -eq $PriorManifest -or
        [string]$PriorManifest.generator_fingerprint -cne $GeneratorFingerprint -or
        (($PriorManifest.artifacts | ConvertTo-Json -Depth 6 -Compress) -cne
         ($CandidateManifest.artifacts | ConvertTo-Json -Depth 6 -Compress))) {
        return $false
    }
    foreach ($field in @(
        'compile_result_sha256', 'presentation_profile_sha256',
        'runtime_profile_sha256', 'authored_overlay_profile_sha256', 'map_package')) {
        $priorIdentity = $PriorManifest.input_identity
        $candidateIdentity = $CandidateManifest.input_identity
        $priorValue = if ($priorIdentity -is [System.Collections.IDictionary]) {
            if ($priorIdentity.Contains($field)) { [string]$priorIdentity[$field] } else { $null }
        } else {
            $property = $priorIdentity.PSObject.Properties[$field]
            if ($null -ne $property) { [string]$property.Value } else { $null }
        }
        $candidateValue = if ($candidateIdentity -is [System.Collections.IDictionary]) {
            if ($candidateIdentity.Contains($field)) { [string]$candidateIdentity[$field] } else { $null }
        } else {
            $property = $candidateIdentity.PSObject.Properties[$field]
            if ($null -ne $property) { [string]$property.Value } else { $null }
        }
        if ($priorValue -cne $candidateValue) {
            return $false
        }
    }
    if (-not $CompareLayerContract) {
        return $true
    }
    $stableFields = @(
        'realization_profile_id',
        'normalized_layer_contract_sha256', 'generator_id', 'generator_version',
        'artifact_root', 'canonical_inputs', 'dependency_inputs', 'semantic_outputs')
    $priorStable = [ordered]@{}
    $candidateStable = [ordered]@{}
    foreach ($field in $stableFields) {
        $priorStable[$field] = $PriorManifest.layer_contract.$field
        $candidateStable[$field] = $CandidateManifest.layer_contract.$field
    }
    return ($priorStable | ConvertTo-Json -Depth 8 -Compress) -ceq
        ($candidateStable | ConvertTo-Json -Depth 8 -Compress)
}

function Test-ProjectWorldReconstructionScopeState {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][object]$ActiveSet,
        [Parameter(Mandatory = $true)][hashtable]$ScopePathsById
    )

    foreach ($scopeId in $ScopePathsById.Keys) {
        if (-not $ActiveSet.Manifests.Contains($scopeId)) { continue }
        $current = @(Get-ProjectWorldScopeArtifactRecords `
            -ProjectRoot $ProjectRoot -ScopePaths $ScopePathsById[$scopeId])
        if ($current.Count -eq 0) { continue }
        Test-ProjectWorldScopeDrift `
            -ProjectRoot $ProjectRoot -ActiveSet $ActiveSet `
            -ScopePathsById @{ $scopeId = $ScopePathsById[$scopeId] }
    }
}

function Remove-ProjectWorldSupersededLayerArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [object]$PriorManifest,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$CandidateRecords
    )

    if ($null -eq $PriorManifest) { return @() }

    $projectPrefix = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/') +
        [System.IO.Path]::DirectorySeparatorChar
    $candidatePaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($record in $CandidateRecords) {
        [void]$candidatePaths.Add([string]$record.path)
    }

    $removed = [System.Collections.Generic.List[string]]::new()
    foreach ($artifact in @($PriorManifest.artifacts)) {
        $relative = [string]$artifact.path
        if ($candidatePaths.Contains($relative)) { continue }
        if ($relative -match '\.\.' -or $relative -match '^[/\\]' -or
            $relative -match ':' -or $relative -notmatch '^[A-Za-z0-9_./ -]+$') {
            throw "Superseded layer artifact is not a confined repository-relative path: $relative"
        }
        $fullPath = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $relative.Replace('/', '\')))
        if (-not $fullPath.StartsWith($projectPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Superseded layer artifact escapes the repository: $relative"
        }
        if (Test-Path -LiteralPath $fullPath -PathType Container) {
            throw "Superseded layer artifact unexpectedly resolves to a directory: $relative"
        }
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            Remove-Item -LiteralPath $fullPath -Force
            $removed.Add($relative)
        }
    }
    return @($removed | Sort-Object)
}

function Merge-ProjectWorldPostApplyArtifacts {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$LayerArtifacts,
        [AllowEmptyCollection()][object[]]$PriorBuilderArtifacts = @(),
        [Parameter(Mandatory = $true)][object[]]$BuilderArtifacts,
        [Parameter(Mandatory = $true)][string]$ProjectRoot
    )

    $records = [ordered]@{}
    $priorBuilderPaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($artifact in $PriorBuilderArtifacts) {
        $path = [string]$artifact.path
        if ($path -notmatch '^[A-Za-z0-9_./ -]+$' -or $path -match '\.\.' -or
            [string]$artifact.kind -notin @('asset', 'external_actor') -or
            -not $priorBuilderPaths.Add($path)) {
            throw "Prior post-apply inventory emitted an invalid or duplicate artifact: $path"
        }
    }
    foreach ($artifact in $LayerArtifacts) {
        $path = [string]$artifact.path
        if ($priorBuilderPaths.Contains($path)) { continue }
        $fullPath = Join-Path $ProjectRoot $path.Replace('/', '\')
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            throw "Post-apply builder removed a declared layer artifact: $path"
        }
        $artifact.digest = Get-ProjectWorldFileSha256 -Path $fullPath
        $records[$path] = $artifact
    }

    $builderPaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($artifact in $BuilderArtifacts) {
        $path = [string]$artifact.path
        if ($path -notmatch '^[A-Za-z0-9_./ -]+$' -or $path -match '\.\.' -or
            -not $builderPaths.Add($path) -or [string]$artifact.kind -notin @('asset', 'external_actor')) {
            throw "Post-apply inventory emitted an invalid or duplicate artifact: $path"
        }
        $fullPath = Join-Path $ProjectRoot $path.Replace('/', '\')
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            throw "Post-apply artifact is missing: $path"
        }
        $digest = Get-ProjectWorldFileSha256 -Path $fullPath
        $records[$path] = [pscustomobject]@{
            path = $path
            kind = [string]$artifact.kind
            digest_kind = 'sha256'
            digest = $digest
            semantic_sha256 = $digest
        }
    }
    return @($records.Values | Sort-Object -Property path)
}

function Test-ProjectWorldPostApplyBuildRequired {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet('validate', 'apply', 'delete')]
        [string]$Mode,
        [Parameter(Mandatory = $true)][object]$Result
    )

    if ($Mode -cne 'apply') {
        return $false
    }
    return @($Result.layer_inventories | Where-Object {
        $null -ne $_.PSObject.Properties['post_apply_builder'] -and
        $null -ne $_.post_apply_builder
    }).Count -gt 0
}

function Invoke-ProjectWorldPostApplyBuilders {
    param(
        [Parameter(Mandatory = $true)][string]$EditorCommand,
        [Parameter(Mandatory = $true)][string]$ProjectFile,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$MapPackage,
        [Parameter(Mandatory = $true)][string]$CompileResult,
        [Parameter(Mandatory = $true)][string]$EvidenceDirectory,
        [Parameter(Mandatory = $true)][object]$Result
    )

    $pending = @()
    foreach ($inventory in @($Result.layer_inventories)) {
        if ($null -eq $inventory.PSObject.Properties['post_apply_builder']) { continue }
        $builder = $inventory.post_apply_builder
        if ($null -eq $builder) { continue }
        $layerId = [string]$inventory.layer_id
        if ($layerId -notmatch '^[a-z0-9_]+$' -or
            [string]$builder.inventory_commandlet -notmatch '^[A-Za-z][A-Za-z0-9_]*$' -or
            [string]$builder.builder_commandlet -notmatch '^[A-Za-z][A-Za-z0-9_]*$' -or
            [string]$builder.inventory_schema -notmatch '^[A-Za-z0-9_.:-]+$') {
            throw "Unsafe post-apply builder declaration for layer: $layerId"
        }
        $declaredArgs = @($builder.inventory_arguments) + @($builder.builder_arguments)
        foreach ($argument in $declaredArgs) {
            if ([string]$argument -notmatch '^-[A-Za-z][A-Za-z0-9_.:=\[\]/-]*$') {
                throw "Unsafe post-apply builder argument for layer: $layerId"
            }
        }
        $pending += $inventory
    }
    foreach ($inventory in $pending) {
        $builder = $inventory.post_apply_builder
        $layerId = [string]$inventory.layer_id
        $receipts = @()
        foreach ($phase in @('prebuild', 'postbuild')) {
            $receiptPath = Join-Path $EvidenceDirectory "$layerId-$phase-inventory.json"
            $receiptLog = Join-Path $EvidenceDirectory "$layerId-$phase-inventory.log"
            $arguments = @(
                $ProjectFile, "-run=$($builder.inventory_commandlet)",
                "-Map=$MapPackage", "-CompileResult=$CompileResult") +
                @($builder.inventory_arguments) + @(
                "-Result=$receiptPath", '-unattended', '-nop4', '-nosplash',
                '-stdout', '-FullStdOutLogOutput', "-abslog=$receiptLog")
            & $EditorCommand @arguments | Out-Null
            $allowedExitCodes = if ($phase -eq 'prebuild') { @(0, 1) } else { @(0) }
            if ($LASTEXITCODE -notin $allowedExitCodes -or
                -not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
                throw "Post-apply $phase inventory failed for $layerId with exit code $LASTEXITCODE. See $receiptLog"
            }
            $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
            if ([string]$receipt.schema -cne [string]$builder.inventory_schema -or
                [string]$receipt.map -cne $MapPackage -or $null -eq $receipt.builder_artifacts -or
                ($phase -eq 'postbuild' -and [string]$receipt.status -cne 'accepted')) {
                throw "Post-apply $phase inventory is invalid for $layerId. See $receiptPath"
            }
            $receipts += $receipt
            if ($phase -eq 'prebuild') {
                $builderLog = Join-Path $EvidenceDirectory "$layerId-post-apply-builder.log"
                $builderArguments = @(
                    $ProjectFile, $MapPackage, "-run=$($builder.builder_commandlet)") +
                    @($builder.builder_arguments) + @(
                    '-unattended', '-nop4', '-nosplash',
                    '-stdout', '-FullStdOutLogOutput', "-abslog=$builderLog")
                & $EditorCommand @builderArguments | Out-Null
                if ($LASTEXITCODE -ne 0) {
                    throw "Post-apply builder failed for $layerId with exit code $LASTEXITCODE. See $builderLog"
                }
            }
        }
        $inventory.artifacts = @(Merge-ProjectWorldPostApplyArtifacts `
            -LayerArtifacts @($inventory.artifacts) `
            -PriorBuilderArtifacts @($receipts[0].builder_artifacts) `
            -BuilderArtifacts @($receipts[1].builder_artifacts) -ProjectRoot $ProjectRoot)
    }
}
