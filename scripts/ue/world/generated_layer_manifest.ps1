# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Test-ProjectWorldLayerManifestContract {
    param(
        [Parameter(Mandatory = $true)][string]$ScopeId,
        [object]$LayerContract = $null
    )
    $isLayer = $ScopeId -like 'layer_*'
    if (-not $isLayer) {
        if ($null -ne $LayerContract) { throw "Non-layer scope cannot carry layer_contract: $ScopeId" }
        return
    }
    if ($null -eq $LayerContract) { throw "Layer scope requires layer_contract: $ScopeId" }
    $contract = $LayerContract | ConvertTo-Json -Depth 8 | ConvertFrom-Json
    foreach ($field in @(
        'realization_profile_id', 'realization_profile_sha256',
        'normalized_layer_contract_sha256', 'generator_id', 'generator_version',
        'artifact_root', 'canonical_inputs', 'dependency_inputs',
        'final_dirty_units', 'semantic_outputs')) {
        if (-not ($contract.PSObject.Properties.Name -contains $field)) {
            throw "Layer contract is missing '$field' for $ScopeId."
        }
    }
    foreach ($field in @('realization_profile_sha256', 'normalized_layer_contract_sha256')) {
        if ([string]$contract.$field -notmatch '^[a-f0-9]{64}$') {
            throw "Layer contract $field is invalid for $ScopeId."
        }
    }
    foreach ($field in @('canonical_inputs', 'dependency_inputs')) {
        $unitIds = @{}
        foreach ($inputRecord in @($contract.$field)) {
            if ($null -eq $inputRecord) { continue }
            if ([string]::IsNullOrWhiteSpace([string]$inputRecord.unit_id) -or
                [string]$inputRecord.sha256 -notmatch '^[a-f0-9]{64}$' -or
                $unitIds.ContainsKey([string]$inputRecord.unit_id)) {
                throw "Layer input identity is invalid or duplicated for ${ScopeId}.${field}."
            }
            $unitIds[[string]$inputRecord.unit_id] = $true
        }
    }
    if ([string]$contract.realization_profile_id -notmatch '^[a-z0-9_]+$' -or
        [string]$contract.generator_id -notmatch '^[a-z0-9_]+$' -or
        [int]$contract.generator_version -lt 1 -or
        [string]$contract.artifact_root -notmatch '^/Project[A-Za-z0-9]+/Generated/[A-Za-z0-9_/]+/$') {
        throw "Layer identity is invalid for $ScopeId."
    }
    $semanticPaths = @{}
    foreach ($output in @($contract.semantic_outputs)) {
        if ([string]$output.artifact_path -notmatch '^[A-Za-z0-9_./ -]+$' -or
            [string]$output.semantic_sha256 -notmatch '^[a-f0-9]{64}$' -or
            $semanticPaths.ContainsKey([string]$output.artifact_path)) {
            throw "Layer semantic output is invalid or duplicated for $ScopeId."
        }
        $semanticPaths[[string]$output.artifact_path] = $true
    }
}

function Get-ProjectWorldExactLayerArtifactRecords {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$ArtifactRootPath,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$Inventory,
        [AllowEmptyCollection()][string[]]$AllowedExternalRoots = @()
    )
    $root = [System.IO.Path]::GetFullPath($ArtifactRootPath).TrimEnd('\', '/')
    $project = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    if (-not ($root + '\').StartsWith($project + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Layer artifact root escapes the project: $root"
    }
    $externalRoots = @($AllowedExternalRoots | ForEach-Object {
        $candidate = [System.IO.Path]::GetFullPath($_).TrimEnd('\', '/')
        if (-not ($candidate + '\').StartsWith($project + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Layer external root escapes the project: $candidate"
        }
        $candidate
    } | Sort-Object -Unique)
    $actual = @()
    if (Test-Path -LiteralPath $root -PathType Container) {
        $actual = @(Get-ChildItem -LiteralPath $root -Recurse -File | ForEach-Object {
            $relative = $_.FullName.Substring($project.Length + 1).Replace('\', '/')
            [ordered]@{
                path = $relative
                kind = 'asset'
                digest_kind = 'sha256'
                digest = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        } | Sort-Object path)
    }
    $declaredPaths = @{}
    $declaredRoot = @()
    $declaredExternal = @()
    foreach ($record in @($Inventory | Sort-Object path)) {
        $relative = [string]$record.path
        if ([string]::IsNullOrWhiteSpace($relative) -or $declaredPaths.ContainsKey($relative)) {
            throw "Layer inventory contains an invalid or duplicate path: $relative"
        }
        $declaredPaths[$relative] = $true
        $full = [System.IO.Path]::GetFullPath((Join-Path $project $relative.Replace('/', '\')))
        if (($full + '\').StartsWith($root + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
            $declaredRoot += , $record
            continue
        }
        $matchedRoots = @($externalRoots | Where-Object {
            ($full + '\').StartsWith($_ + '\', [System.StringComparison]::OrdinalIgnoreCase)
        })
        $expectedKind = if (@($matchedRoots | Where-Object { $_ -match '[\\/]__ExternalActors__[\\/]' }).Count -gt 0) {
            'external_actor'
        }
        elseif (@($matchedRoots | Where-Object { $_ -match '[\\/]__ExternalObjects__[\\/]' }).Count -gt 0) {
            'external_object'
        }
        else { '' }
        if ($matchedRoots.Count -eq 0 -or -not (Test-Path -LiteralPath $full -PathType Leaf) -or
            [string]$record.kind -cne $expectedKind -or [string]$record.digest_kind -cne 'sha256' -or
            [string]$record.digest -cne (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash.ToLowerInvariant()) {
            throw "Layer inventory contains an invalid external-package artifact: $relative"
        }
        $declaredExternal += , $record
    }
    $sortedDeclaredRoot = @($declaredRoot | Sort-Object path)
    $actualByPath = @{}
    $declaredByPath = @{}
    foreach ($record in $actual) { $actualByPath[[string]$record.path] = $record }
    foreach ($record in $sortedDeclaredRoot) { $declaredByPath[[string]$record.path] = $record }
    $unexpected = @($actual | Where-Object { -not $declaredByPath.ContainsKey([string]$_.path) } | ForEach-Object path)
    $missing = @($sortedDeclaredRoot | Where-Object { -not $actualByPath.ContainsKey([string]$_.path) } | ForEach-Object path)
    $changed = @($sortedDeclaredRoot | Where-Object {
        $actualByPath.ContainsKey([string]$_.path) -and
        ([string]$actualByPath[[string]$_.path].digest -cne [string]$_.digest -or
            [string]$actualByPath[[string]$_.path].digest_kind -cne [string]$_.digest_kind -or
            [string]$actualByPath[[string]$_.path].kind -cne [string]$_.kind)
    } | ForEach-Object path)
    if ($actual.Count -ne $sortedDeclaredRoot.Count -or $missing.Count -gt 0 -or
        $unexpected.Count -gt 0 -or $changed.Count -gt 0) {
        throw "Layer inventory is not the exact artifact-root population: $root; missing=[$($missing -join ',')]; unexpected=[$($unexpected -join ',')]; changed=[$($changed -join ',')]"
    }
    return @($actual) + @($declaredExternal | Sort-Object path)
}
