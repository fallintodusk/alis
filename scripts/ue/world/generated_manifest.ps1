# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Generated-artifact manifest lifecycle for ProjectWorld generated content.
# Contract SOT: Plugins/World/ProjectWorld/docs/territory_generation.md
# ("Layered regeneration contract"). Activation is owned exclusively by the
# active-manifest-set record; manifests are immutable per-scope documents.
# Every mutating operation holds the OS-exclusive authority lock.

Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'generated_layer_manifest.ps1')

. (Join-Path $PSScriptRoot 'world_data_roots.ps1')

$script:ManifestSchemaId = 'https://alis.world/schemas/project-world/generated-manifest-v1.json'
$script:ActiveSetSchemaId = 'https://alis.world/schemas/project-world/active-manifest-set-v1.json'

function Get-ProjectWorldManifestSchemaReferences {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestRoot,
        [string]$ProjectRoot = ''
    )
    if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
        $cursor = [System.IO.DirectoryInfo]::new([System.IO.Path]::GetFullPath($ManifestRoot))
        for ($index = 0; $index -lt 5 -and $null -ne $cursor.Parent; ++$index) {
            $cursor = $cursor.Parent
        }
        $ProjectRoot = $cursor.FullName
    }
    $schemaRoot = Join-Path $ProjectRoot 'Plugins\World\ProjectWorld\Data\Schemas'
    function Get-RelativeReference {
        param([string]$From, [string]$To)
        $separator = [System.IO.Path]::DirectorySeparatorChar
        $fromUri = [System.Uri]::new([System.IO.Path]::GetFullPath($From).TrimEnd('\', '/') + $separator)
        $toUri = [System.Uri]::new([System.IO.Path]::GetFullPath($To))
        return [System.Uri]::UnescapeDataString($fromUri.MakeRelativeUri($toUri).ToString()).Replace('\', '/')
    }
    return [pscustomobject]@{
        Manifest = Get-RelativeReference `
            -From (Join-Path $ManifestRoot 'scopes') `
            -To (Join-Path $schemaRoot 'project_world_generated_manifest.schema.json')
        ActiveSet = Get-RelativeReference `
            -From $ManifestRoot `
            -To (Join-Path $schemaRoot 'project_world_active_manifest_set.schema.json')
    }
}

function Get-ProjectWorldDefaultManifestRoot {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$WorldDataPlugin
    )
    return (Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $WorldDataPlugin).ManifestRoot
}

function Enter-ProjectWorldContentLock {
    # ONE project-global generated-content mutation lock, independent of any
    # manifest root: durable and sandbox operations all mutate the same
    # Content/Generated trees, so they must serialize here first.
    # The holder keeps the file WRITE-exclusive (FileShare Read) and stamps a
    # random owner token so child processes it spawns can verify delegated
    # ownership instead of self-conflicting on the same lock.
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)
    $lockDir = Join-Path $ProjectRoot 'tmp\world\world_realization'
    New-Item -ItemType Directory -Path $lockDir -Force | Out-Null
    $lockPath = Join-Path $lockDir 'content_mutation.lock'

    $delegatedToken = $env:ALIS_WORLD_CONTENT_LOCK_TOKEN
    if (-not [string]::IsNullOrWhiteSpace($delegatedToken)) {
        # A delegation claim NEVER falls back to self-acquisition: an
        # unverifiable claim means the parent lifecycle is broken.
        $reader = $null
        try {
            $reader = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        }
        catch {
            throw "Delegated content-lock token is set but the live lock cannot be read: $lockPath"
        }
        $verified = $false
        try {
            $buffer = New-Object byte[] 256
            $count = $reader.Read($buffer, 0, $buffer.Length)
            $liveToken = [System.Text.Encoding]::ASCII.GetString($buffer, 0, $count).Trim()
            if ($liveToken -ne $delegatedToken.Trim()) {
                throw "Delegated content-lock token does not match the live lock owner: $lockPath"
            }
            # A matching token in a RELEASED lock file is stale. Only a failing
            # WRITE-access probe proves a live owner: the owner's Read-only share
            # denies it, while our own read handle (share ReadWrite) tolerates it.
            $probe = $null
            try {
                $probe = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::Open,
                    [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
            }
            catch [System.IO.IOException] { }
            if ($null -ne $probe) {
                $probe.Dispose()
                throw "Delegated content-lock token is set but no live owner holds the lock: $lockPath"
            }
            $verified = $true
        }
        finally {
            if (-not $verified) { $reader.Dispose() }
        }
        # The read handle is the delegated releaser: disposing it releases only
        # this child's view, never the parent's exclusive ownership.
        return $reader
    }

    try {
        $stream = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::OpenOrCreate,
            [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::Read)
    }
    catch [System.IO.IOException] {
        throw "Another operation holds the ProjectWorld content mutation lock: $lockPath"
    }
    $token = [System.Guid]::NewGuid().ToString('N')
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($token)
    $stream.SetLength(0)
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Flush()
    return $stream
}

function Get-ProjectWorldGeneratorFingerprint {
    # Deterministic fingerprint of the generator implementation: editor
    # module sources, frozen schemas, and the lifecycle scripts. Two
    # different generators with identical inputs must be distinguishable
    # in the durable authority.
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)
    $roots = @(
        (Join-Path $ProjectRoot 'Plugins\World\ProjectWorld\Source\ProjectWorldEditor'),
        (Join-Path $ProjectRoot 'Plugins\World\ProjectWorld\Data\Schemas'),
        (Join-Path $ProjectRoot 'scripts\ue\world')
    )
    # Repo-relative normalized paths ONLY: identical source bytes must
    # produce identical fingerprints under any checkout root.
    $repoPrefix = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File |
            Where-Object { $_.Extension -in @('.cpp', '.h', '.cs', '.json', '.ps1') }) {
            $relative = $file.FullName.Substring($repoPrefix.Length).Replace('\', '/')
            # Read-only verifiers and test sources cannot influence generated
            # bytes or manifest documents. Hashing them would make the audit's
            # fingerprint-currency gate fail on edits that provably change
            # nothing, which trains operators to bypass the gate.
            if ($relative -like 'scripts/ue/world/test/*' -or
                $relative -like 'Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/Tests/*' -or
                $relative -eq 'scripts/ue/world/audit_generated_authority.ps1') {
                continue
            }
            $lines.Add("$relative`0$(Get-ProjectWorldFileSha256 -Path $file.FullName)")
        }
    }
    # Explicit ordinal comparison: locale must never influence the result.
    $sorted = $lines.ToArray()
    [System.Array]::Sort($sorted, [System.StringComparer]::Ordinal)
    $lines = $sorted
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}

function Enter-ProjectWorldAuthorityLock {
    # One OS-enforced exclusive lock covering the authority root and every
    # generated-root mutation. Acquire BEFORE reading the journal or active
    # set; the handle is released on dispose or process exit.
    param([Parameter(Mandatory = $true)][string]$ManifestRoot)
    New-Item -ItemType Directory -Path $ManifestRoot -Force | Out-Null
    $lockPath = Join-Path $ManifestRoot 'authority.lock'
    try {
        return [System.IO.File]::Open($lockPath, [System.IO.FileMode]::OpenOrCreate,
            [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    }
    catch [System.IO.IOException] {
        throw "Another ProjectWorld generated-content operation holds the authority lock: $lockPath"
    }
}

function Get-ProjectWorldMapScopeId {
    param(
        [Parameter(Mandatory = $true)][string]$MapPackage,
        [Parameter(Mandatory = $true)][string]$GeneratedPackageRoot
    )
    if (-not $MapPackage.StartsWith($GeneratedPackageRoot, [System.StringComparison]::Ordinal) -or
        $MapPackage.Length -le $GeneratedPackageRoot.Length -or
        $MapPackage -notmatch '^/[A-Za-z][A-Za-z0-9_]*/Generated/[A-Za-z0-9_/]+$') {
        throw "Generated map package is outside the supported mount: $MapPackage"
    }
    $token = $MapPackage.Substring($GeneratedPackageRoot.Length).ToLowerInvariant() -replace '[^a-z0-9]', '_'
    return "map_$token"
}

function Get-ProjectWorldPresentationScopeId {
    param([Parameter(Mandatory = $true)][string]$ProfileId)
    $token = $ProfileId.ToLowerInvariant() -replace '[^a-z0-9]', '_'
    return "presentation_$token"
}

function Get-ProjectWorldHighestManifestGeneration {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestRoot,
        [Parameter(Mandatory = $true)][string]$ScopeId
    )
    if ($ScopeId -notmatch '^(map|presentation|layer)_[a-z0-9_]+$') {
        throw "Manifest scope ID is invalid: $ScopeId"
    }
    $highest = 0
    $pattern = '^{0}\.(?<generation>[1-9][0-9]*)\.json$' -f [Regex]::Escape($ScopeId)
    foreach ($directoryName in @('scopes', 'archive')) {
        $directory = Join-Path $ManifestRoot $directoryName
        if (-not (Test-Path -LiteralPath $directory -PathType Container)) { continue }
        foreach ($file in Get-ChildItem -LiteralPath $directory -Filter "$ScopeId.*.json" -File) {
            if ($file.Name -match $pattern) {
                $highest = [Math]::Max($highest, [int]$Matches.generation)
            }
        }
    }
    return $highest
}

function Get-ProjectWorldFileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-ProjectWorldArtifactKind {
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    if ($RelativePath -match '__ExternalActors__') { return 'external_actor' }
    if ($RelativePath -match '__ExternalObjects__') { return 'external_object' }
    if ($RelativePath -match '\.umap$') { return 'map' }
    if ($RelativePath -match 'BuiltData') { return 'built_data' }
    if ($RelativePath -match 'HLOD') { return 'hlod' }
    if ($RelativePath -match '\.uasset$') { return 'asset' }
    return 'other'
}

function Get-ProjectWorldScopeArtifactRecords {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$ScopePaths
    )
    $repoPrefix = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $records = [System.Collections.Generic.List[object]]::new()
    foreach ($scopePath in $ScopePaths) {
        if (-not (Test-Path -LiteralPath $scopePath)) { continue }
        $files = if (Test-Path -LiteralPath $scopePath -PathType Leaf) {
            @(Get-Item -LiteralPath $scopePath)
        }
        else {
            @(Get-ChildItem -LiteralPath $scopePath -Recurse -File)
        }
        foreach ($file in $files) {
            $full = $file.FullName
            if (-not $full.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Scope artifact escapes the repository: $full"
            }
            $relative = $full.Substring($repoPrefix.Length).Replace('\', '/')
            $records.Add([ordered]@{
                path = $relative
                kind = Get-ProjectWorldArtifactKind -RelativePath $relative
                digest_kind = 'sha256'
                digest = Get-ProjectWorldFileSha256 -Path $full
            })
        }
    }
    return @($records | Sort-Object -Property { $_.path })
}

function Assert-ProjectWorldManifestRootLayout {
    param([Parameter(Mandatory = $true)][string]$ManifestRoot)
    if (-not (Test-Path -LiteralPath $ManifestRoot -PathType Container)) { return }
    foreach ($entry in Get-ChildItem -LiteralPath $ManifestRoot -Force) {
        $allowed = ($entry.Name -in @('active_set.json', 'journal.json', 'authority.lock')) -or
            ($entry.PSIsContainer -and $entry.Name -in @('scopes', 'archive'))
        if (-not $allowed) {
            if ($entry.Name -in @('active_set.json.tmp', 'journal.json.tmp')) {
                throw "Staging debris in manifest authority root ($($entry.Name)); run recover_generated_transaction.ps1."
            }
            throw "Unknown entry in manifest authority root: $($entry.FullName)"
        }
    }
}

function Assert-ProjectWorldAuthorityInitializable {
    # Initialization (first enrollment) requires a genuinely new authority
    # root. Prior scopes, archive, or journal without a valid active set is
    # a damaged authority: fail closed, never re-legitimize the tree.
    param([Parameter(Mandatory = $true)][string]$ManifestRoot)
    foreach ($name in @('scopes', 'archive')) {
        $path = Join-Path $ManifestRoot $name
        if ((Test-Path -LiteralPath $path) -and @(Get-ChildItem -LiteralPath $path -Force).Count -gt 0) {
            throw "Authority root has prior evidence ($name/) but no valid active set; initialization refused. Operator investigation or recovery is required."
        }
    }
    foreach ($name in @('journal.json', 'active_set.json.tmp')) {
        if (Test-Path -LiteralPath (Join-Path $ManifestRoot $name)) {
            throw "Authority root has prior evidence ($name) but no valid active set; initialization refused."
        }
    }
}

function Test-ProjectWorldManifestDocument {
    # Structural validation of one manifest document (candidate or carried)
    # against the frozen schema rules. PowerShell-side enforcement; the
    # Python validator additionally applies the full JSON Schema.
    param(
        [Parameter(Mandatory = $true)][object]$Manifest,
        [string]$ExpectedSchema = $script:ManifestSchemaId
    )
    if ([string]$Manifest.'$schema' -ne $ExpectedSchema) {
        throw 'Manifest schema identity is invalid for its authority root.'
    }
    if ([int]$Manifest.schema_version -ne 1) { throw 'Manifest schema_version is unsupported.' }
    if ([string]$Manifest.scope_id -notmatch '^(map|presentation|layer)_[a-z0-9_]+$') {
        throw "Manifest scope_id is invalid: $($Manifest.scope_id)"
    }
    $layerContractProperty = $Manifest.PSObject.Properties['layer_contract']
    Test-ProjectWorldLayerManifestContract `
        -ScopeId ([string]$Manifest.scope_id) `
        -LayerContract $(if ($null -ne $layerContractProperty) { $layerContractProperty.Value } else { $null })
    if ([int]$Manifest.generation -lt 1) { throw "Manifest generation must be >= 1 for $($Manifest.scope_id)." }
    if ([string]$Manifest.owning_layer -notmatch '^[a-z0-9_]+$') {
        throw "Manifest owning_layer is invalid for $($Manifest.scope_id)."
    }
    if ([string]::IsNullOrWhiteSpace([string]$Manifest.accepted_operation_id)) {
        throw "Manifest accepted_operation_id is missing for $($Manifest.scope_id)."
    }
    $acceptedAtProperty = $Manifest.PSObject.Properties['accepted_at_utc']
    $acceptedAt = [DateTimeOffset]::MinValue
    if ($null -ne $acceptedAtProperty -and
        -not [DateTimeOffset]::TryParse(
            [string]$acceptedAtProperty.Value,
            [System.Globalization.CultureInfo]::InvariantCulture,
            [System.Globalization.DateTimeStyles]::RoundtripKind,
            [ref]$acceptedAt)) {
        throw "Manifest accepted_at_utc is invalid for $($Manifest.scope_id)."
    }
    if (-not ($Manifest.PSObject.Properties.Name -contains 'generator_fingerprint') -or
        [string]$Manifest.generator_fingerprint -notmatch '^[a-f0-9]{64}$') {
        throw "Manifest generator_fingerprint is missing or invalid for $($Manifest.scope_id)."
    }
    $inputIdentityProperty = $Manifest.PSObject.Properties['input_identity']
    if ($null -eq $inputIdentityProperty -or $null -eq $inputIdentityProperty.Value) {
        throw "Manifest input_identity is missing for $($Manifest.scope_id)."
    }
    $inputIdentity = $inputIdentityProperty.Value
    foreach ($field in @('compile_result_sha256', 'presentation_profile_sha256', 'runtime_profile_sha256')) {
        $property = $inputIdentity.PSObject.Properties[$field]
        $value = if ($null -ne $property) { [string]$property.Value } else { '' }
        if ($value -ne 'none' -and $value -notmatch '^[a-f0-9]{64}$') {
            throw "Manifest input_identity.$field is missing or invalid for $($Manifest.scope_id)."
        }
    }
    $authoredIdentity = $inputIdentity.PSObject.Properties['authored_overlay_profile_sha256']
    if ($null -ne $authoredIdentity -and [string]$authoredIdentity.Value -ne 'none' -and
        [string]$authoredIdentity.Value -notmatch '^[a-f0-9]{64}$') {
        throw "Manifest input_identity.authored_overlay_profile_sha256 is invalid for $($Manifest.scope_id)."
    }
    $mapProperty = $inputIdentity.PSObject.Properties['map_package']
    if ($null -eq $mapProperty -or [string]::IsNullOrWhiteSpace([string]$mapProperty.Value)) {
        throw "Manifest input_identity.map_package is missing for $($Manifest.scope_id)."
    }
    $paths = @{}
    foreach ($artifact in @($Manifest.artifacts)) {
        $p = [string]$artifact.path
        if ($p -match '\.\.' -or $p -match '^[/\\]' -or $p -match ':' -or $p -notmatch '^[A-Za-z0-9_./ -]+$') {
            throw "Manifest artifact path is not a confined repository-relative path: $p"
        }
        if ([string]$artifact.digest -notmatch '^[a-f0-9]{64}$' -or [string]$artifact.digest_kind -ne 'sha256') {
            throw "Manifest artifact digest is invalid for: $p"
        }
        if ($paths.ContainsKey($p)) { throw "Manifest lists artifact path twice: $p" }
        $paths[$p] = $true
    }
    if ([string]$Manifest.scope_id -like 'layer_*') {
        $semanticPaths = @($Manifest.layer_contract.semantic_outputs | ForEach-Object {
            [string]$_.artifact_path
        } | Sort-Object -Unique)
        $artifactPaths = @($paths.Keys | Sort-Object)
        if (($semanticPaths -join "`n") -cne ($artifactPaths -join "`n")) {
            throw "Layer semantic outputs do not exactly cover owned artifacts for $($Manifest.scope_id)."
        }
    }
    foreach ($consumer in @($Manifest.consumer_references)) {
        if ([string]$consumer -notmatch '^(map|presentation|layer)_[a-z0-9_]+$') {
            throw "Manifest consumer reference is invalid: $consumer"
        }
    }
}

function Read-ProjectWorldActiveSet {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestRoot,
        [string]$ProjectRoot = ''
    )
    Assert-ProjectWorldManifestRootLayout -ManifestRoot $ManifestRoot
    $schemaReferences = Get-ProjectWorldManifestSchemaReferences `
        -ManifestRoot $ManifestRoot `
        -ProjectRoot $ProjectRoot
    $activePath = Join-Path $ManifestRoot 'active_set.json'
    if (-not (Test-Path -LiteralPath $activePath -PathType Leaf)) { return $null }
    $active = Get-Content -LiteralPath $activePath -Raw | ConvertFrom-Json
    if ([string]$active.'$schema' -ne $schemaReferences.ActiveSet -or
        [int]$active.schema_version -ne 1 -or
        [string]$active.transaction_id -notmatch '^[a-f0-9]{32}$' -or
        $null -eq $active.scopes) {
        throw 'Active-manifest-set record is malformed or unsupported; activation fails closed.'
    }
    $manifests = [ordered]@{}
    $manifestPaths = @{}
    foreach ($scope in $active.scopes) {
        if ($manifests.Contains([string]$scope.scope_id)) {
            throw "Active-set record lists scope '$($scope.scope_id)' twice; activation fails closed."
        }
        if ($manifestPaths.ContainsKey([string]$scope.manifest_path)) {
            throw "Active-set record lists manifest path '$($scope.manifest_path)' twice; activation fails closed."
        }
        $manifestPaths[[string]$scope.manifest_path] = $true
        if ([string]$scope.manifest_path -notmatch '^scopes/(map|presentation|layer)_[a-z0-9_]+\.[0-9]+\.json$') {
            throw "Active manifest path violates the frozen grammar for scope $($scope.scope_id); activation fails closed."
        }
        $manifestPath = Join-Path $ManifestRoot ($scope.manifest_path.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
        if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
            throw "Active manifest missing for scope $($scope.scope_id); activation fails closed."
        }
        $actualSha = Get-ProjectWorldFileSha256 -Path $manifestPath
        if ($actualSha -ne $scope.manifest_sha256) {
            throw "Active manifest hash mismatch for scope $($scope.scope_id); activation fails closed."
        }
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        if ($manifest.scope_id -ne $scope.scope_id) {
            throw "Active manifest for scope $($scope.scope_id) is malformed; activation fails closed."
        }
        Test-ProjectWorldManifestDocument -Manifest $manifest -ExpectedSchema $schemaReferences.Manifest
        $manifests[$scope.scope_id] = $manifest
    }
    return [pscustomobject]@{
        Path = $activePath
        Sha256 = Get-ProjectWorldFileSha256 -Path $activePath
        Record = $active
        Manifests = $manifests
    }
}

function Test-ProjectWorldGlobalOwnership {
    param([Parameter(Mandatory = $true)][object]$Manifests)
    $owners = @{}
    foreach ($scopeId in $Manifests.Keys) {
        foreach ($artifact in @($Manifests[$scopeId].artifacts)) {
            if ($owners.ContainsKey($artifact.path)) {
                throw "Ambiguous ownership: '$($artifact.path)' is owned by '$($owners[$artifact.path])' and '$scopeId'."
            }
            $owners[$artifact.path] = $scopeId
        }
    }
}

function Test-ProjectWorldScopeDrift {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][object]$ActiveSet,
        [Parameter(Mandatory = $true)][hashtable]$ScopePathsById
    )
    $acceptedGlobally = @{}
    foreach ($manifest in $ActiveSet.Manifests.Values) {
        foreach ($artifact in @($manifest.artifacts)) {
            $acceptedGlobally[[string]$artifact.path] = [string]$artifact.digest
        }
    }
    $projectPrefix = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/') +
        [System.IO.Path]::DirectorySeparatorChar
    foreach ($scopeId in $ScopePathsById.Keys) {
        if (-not $ActiveSet.Manifests.Contains($scopeId)) { continue }
        $accepted = @{}
        foreach ($artifact in $ActiveSet.Manifests[$scopeId].artifacts) {
            $accepted[$artifact.path] = $artifact.digest
        }
        foreach ($path in $accepted.Keys) {
            $fullPath = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $path.Replace('/', '\')))
            if (-not $fullPath.StartsWith($projectPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
                -not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
                throw "Unowned or drifted generated content: accepted artifact missing for scope ${scopeId}: $path"
            }
            if ((Get-ProjectWorldFileSha256 -Path $fullPath) -ne $accepted[$path]) {
                throw "Unowned or drifted generated content: artifact drifted in scope ${scopeId}: $path"
            }
        }
        $current = Get-ProjectWorldScopeArtifactRecords `
            -ProjectRoot $ProjectRoot -ScopePaths $ScopePathsById[$scopeId]
        foreach ($record in $current) {
            $path = [string]$record.path
            if (-not $acceptedGlobally.ContainsKey($path)) {
                throw "Unowned or drifted generated content: unowned artifact in scope ${scopeId}: $path"
            }
            if ([string]$record.digest -ne $acceptedGlobally[$path]) {
                throw "Unowned or drifted generated content: artifact drifted in scope ${scopeId}: $path"
            }
        }
    }
}

function New-ProjectWorldCandidateManifest {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$ScopeId,
        [Parameter(Mandatory = $true)][int]$Generation,
        [Parameter(Mandatory = $true)][string]$OwningLayer,
        [Parameter(Mandatory = $true)][string]$OperationId,
        [Parameter(Mandatory = $true)][hashtable]$InputIdentity,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$ScopePaths,
        [AllowEmptyCollection()][string[]]$ConsumerReferences = @(),
        [Parameter(Mandatory = $true)][string]$GeneratorFingerprint,
        [object]$LayerContract = $null,
        [object[]]$ArtifactRecords = $null
    )
    Test-ProjectWorldLayerManifestContract -ScopeId $ScopeId -LayerContract $LayerContract
    [object[]]$resolvedArtifacts = @()
    if ($null -ne $ArtifactRecords) {
        $resolvedArtifacts = @($ArtifactRecords | Sort-Object path)
    } else {
        $resolvedArtifacts = @(Get-ProjectWorldScopeArtifactRecords -ProjectRoot $ProjectRoot -ScopePaths $ScopePaths)
    }
    $manifest = [ordered]@{
        '$schema' = $script:ManifestSchemaId
        schema_version = 1
        scope_id = $ScopeId
        generation = $Generation
        owning_layer = $OwningLayer
        accepted_operation_id = $OperationId
        accepted_at_utc = [DateTimeOffset]::UtcNow.ToString('o', [System.Globalization.CultureInfo]::InvariantCulture)
        generator_fingerprint = $GeneratorFingerprint
        input_identity = $InputIdentity
        artifacts = $resolvedArtifacts
        consumer_references = @($ConsumerReferences | Sort-Object -Unique)
    }
    if ($null -ne $LayerContract) { $manifest['layer_contract'] = $LayerContract }
    return $manifest
}

function Write-ProjectWorldJson {
    # Durable staged replacement: full write to .tmp, then atomic move.
    param(
        [Parameter(Mandatory = $true)][object]$Document,
        [Parameter(Mandatory = $true)][string]$Path
    )
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    # LF-only, deliberately. These documents are hashed on disk and the
    # recorded hashes ARE the activation authority, while the repository
    # declares `eol: lf` for them. ConvertTo-Json emits CRLF on Windows, so
    # writing it verbatim produces bytes that git rewrites on checkout - and
    # every manifest_sha256 would then mismatch on a clean clone, failing the
    # authority closed for a reason that has nothing to do with content.
    $json = ($Document | ConvertTo-Json -Depth 8) -replace "`r`n", "`n"
    $staging = "$Path.tmp"
    [System.IO.File]::WriteAllText($staging, $json + "`n", [System.Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $staging -Destination $Path -Force
}

function Write-ProjectWorldTransactionJournal {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestRoot,
        [Parameter(Mandatory = $true)][object]$Journal
    )
    Write-ProjectWorldJson -Document $Journal -Path (Join-Path $ManifestRoot 'journal.json')
}

function Read-ProjectWorldTransactionJournal {
    param([Parameter(Mandatory = $true)][string]$ManifestRoot)
    $path = Join-Path $ManifestRoot 'journal.json'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
    return Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
}

function Test-ProjectWorldJournal {
    # An invalid journal fails closed BEFORE any recovery mutation: schema,
    # phase enum, transaction id, and strict path confinement for every
    # destructive operation recovery would perform.
    param(
        [Parameter(Mandatory = $true)][object]$Journal,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$ManifestRoot,
        [Parameter(Mandatory = $true)][string]$ContentRoot
    )
    foreach ($field in @('transaction_id', 'phase', 'operation', 'map_package', 'snapshot_root', 'snapshot_records', 'candidate_manifest_paths', 'expected_active_set_sha256', 'prior_active_set_sha256', 'mutation_scope_ids', 'retired_scopes')) {
        if (-not ($Journal.PSObject.Properties.Name -contains $field)) {
            throw "Recovery journal is missing required field '$field'; recovery fails closed."
        }
    }
    if ([string]$Journal.transaction_id -notmatch '^[a-f0-9]{32}$') { throw 'Recovery journal transaction_id is invalid; recovery fails closed.' }
    if ([string]$Journal.phase -notin @('mutating', 'publishing')) { throw 'Recovery journal phase is invalid; recovery fails closed.' }
    $pluginName = Split-Path -Leaf (Split-Path -Parent $ContentRoot)
    $generatedPackageRoot = "/$pluginName/Generated/"
    if (-not ([string]$Journal.map_package).StartsWith($generatedPackageRoot, [System.StringComparison]::Ordinal) -or
        [string]$Journal.map_package -notmatch '^/[A-Za-z][A-Za-z0-9_]*/Generated/[A-Za-z0-9_/]+$') {
        throw 'Recovery journal map package is invalid for its content owner; recovery fails closed.'
    }
    if ([string]$Journal.prior_active_set_sha256 -notmatch '^([a-f0-9]{64}|none)$') { throw 'Recovery journal prior active-set hash is invalid; recovery fails closed.' }
    if ($Journal.phase -eq 'publishing' -and [string]$Journal.expected_active_set_sha256 -notmatch '^[a-f0-9]{64}$') {
        throw 'Recovery journal expected active-set hash is invalid for the publishing phase; recovery fails closed.'
    }
    foreach ($scopeId in @($Journal.mutation_scope_ids)) {
        if ([string]$scopeId -notmatch '^(map|presentation|layer)_[a-z0-9_]+$') { throw 'Recovery journal mutation scope set is invalid; recovery fails closed.' }
    }
    if ([string]$Journal.operation -notin @('apply', 'delete', 'reconstruct', 'enroll')) {
        throw 'Recovery journal operation is invalid; recovery fails closed.'
    }
    if ($Journal.operation -eq 'delete' -and @($Journal.retired_scopes).Count -lt 1) {
        throw 'Recovery journal for a delete operation has no retirement evidence; recovery fails closed.'
    }
    if ($Journal.operation -ne 'delete' -and @($Journal.retired_scopes | Where-Object {
        [string]$_.scope_id -notlike 'layer_*'
    }).Count -gt 0) {
        throw 'Recovery journal declares non-layer retirement outside a delete operation; recovery fails closed.'
    }
    $seenRetired = @{}
    foreach ($retired in @($Journal.retired_scopes)) {
        if ([string]$retired.scope_id -notmatch '^(map|presentation|layer)_[a-z0-9_]+$' -or
            [string]$retired.prior_manifest_path -notmatch '^scopes/(map|presentation|layer)_[a-z0-9_]+\.[0-9]+\.json$' -or
            [string]$retired.prior_manifest_sha256 -notmatch '^[a-f0-9]{64}$') {
            throw 'Recovery journal retired-scope evidence is invalid; recovery fails closed.'
        }
        if ($seenRetired.ContainsKey([string]$retired.scope_id)) { throw 'Recovery journal lists a retired scope twice; recovery fails closed.' }
        $seenRetired[[string]$retired.scope_id] = $true
        if ([string]$retired.scope_id -notin @($Journal.mutation_scope_ids)) {
            throw 'Recovery journal retired scope is outside the declared mutation scope set; recovery fails closed.'
        }
    }
    if ((Split-Path -Leaf ([string]$Journal.snapshot_root)) -ne [string]$Journal.transaction_id) {
        throw 'Recovery journal snapshot root is not bound to the transaction ID; recovery fails closed.'
    }
    $transactionParent = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot 'tmp\world\world_realization\transactions')).TrimEnd('\') + '\'
    $contentPrefix = [System.IO.Path]::GetFullPath($ContentRoot).TrimEnd('\') + '\'
    $snapshotRoot = [System.IO.Path]::GetFullPath([string]$Journal.snapshot_root)
    if (-not ($snapshotRoot + '\').StartsWith($transactionParent, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'Recovery journal snapshot root escapes the transaction parent; recovery fails closed.'
    }
    foreach ($record in @($Journal.snapshot_records)) {
        $source = [System.IO.Path]::GetFullPath([string]$record.source)
        $backup = [System.IO.Path]::GetFullPath([string]$record.backup)
        if (-not $source.StartsWith($contentPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw 'Recovery journal snapshot source escapes world-data content; recovery fails closed.'
        }
        if (-not ($backup + '\').StartsWith($snapshotRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
            throw 'Recovery journal snapshot backup escapes the snapshot root; recovery fails closed.'
        }
    }
    foreach ($candidate in @($Journal.candidate_manifest_paths)) {
        if ([string]$candidate -notmatch '^scopes/(map|presentation|layer)_[a-z0-9_]+\.[0-9]+\.json$') {
            throw 'Recovery journal candidate path violates the frozen grammar; recovery fails closed.'
        }
    }
}

function Test-ProjectWorldProspectiveSet {
    # Validates the COMPLETE prospective authority (carried + candidates -
    # retired) before anything is written: document structure, unique scope
    # IDs, generation progression, global ownership, consumer integrity,
    # and the no-active-consumers retirement rule.
    param(
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$CandidateManifests,
        [AllowEmptyCollection()][string[]]$RetiredScopeIds = @(),
        [object]$PriorActiveSet = $null,
        [hashtable]$PriorGenerations = @{}
    )
    $prospective = [ordered]@{}
    if ($null -ne $PriorActiveSet) {
        foreach ($scopeId in $PriorActiveSet.Manifests.Keys) {
            if ($scopeId -in $RetiredScopeIds) { continue }
            $prospective[$scopeId] = $PriorActiveSet.Manifests[$scopeId]
        }
    }
    $candidateSeen = @{}
    foreach ($candidate in $CandidateManifests) {
        $candidateObject = $candidate | ConvertTo-Json -Depth 8 | ConvertFrom-Json
        Test-ProjectWorldManifestDocument -Manifest $candidateObject
        $scopeId = [string]$candidateObject.scope_id
        if ($candidateSeen.ContainsKey($scopeId)) { throw "Duplicate candidate scope: $scopeId" }
        $candidateSeen[$scopeId] = $true
        if ($scopeId -in $RetiredScopeIds) { throw "Scope '$scopeId' is both candidate and retired." }
        $prior = if ($PriorGenerations.ContainsKey($scopeId)) {
            [int]$PriorGenerations[$scopeId]
        }
        elseif ($null -ne $PriorActiveSet -and $PriorActiveSet.Manifests.Contains($scopeId)) {
            [int]$PriorActiveSet.Manifests[$scopeId].generation
        }
        else { 0 }
        if ([int]$candidateObject.generation -ne $prior + 1) {
            throw "Scope '$scopeId' candidate generation $($candidateObject.generation) does not follow prior generation $prior."
        }
        $prospective[$scopeId] = $candidateObject
    }
    Test-ProjectWorldGlobalOwnership -Manifests $prospective
    foreach ($scopeId in $prospective.Keys) {
        foreach ($consumer in @($prospective[$scopeId].consumer_references)) {
            if (-not $prospective.Contains($consumer)) {
                throw "Scope '$scopeId' references consumer '$consumer' which is not in the prospective active set."
            }
        }
    }
    foreach ($retired in $RetiredScopeIds) {
        if ($null -eq $PriorActiveSet -or -not $PriorActiveSet.Manifests.Contains($retired)) { continue }
        $remaining = @($PriorActiveSet.Manifests[$retired].consumer_references | Where-Object { $_ -notin $RetiredScopeIds })
        if ($retired -notlike 'layer_*' -and $remaining.Count -gt 0) {
            throw "Scope '$retired' cannot be retired while active consumers remain: $($remaining -join ',')."
        }
    }
    return $prospective
}

function Publish-ProjectWorldActiveSet {
    # Validates the prospective set, writes immutable candidate manifests,
    # then atomically replaces the single active-set record LAST. Retired
    # manifests are archived only AFTER the commit (post-commit
    # housekeeping); a crash before commit leaves the prior authority fully
    # intact.
    param(
        [Parameter(Mandatory = $true)][string]$ManifestRoot,
        [Parameter(Mandatory = $true)][string]$TransactionId,
        [Parameter(Mandatory = $true)][string]$OperationId,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$CandidateManifests,
        [string]$ProjectRoot = '',
        [AllowEmptyCollection()][string[]]$RetiredScopeIds = @(),
        [object]$PriorActiveSet = $null,
        [scriptblock]$BeforeCommit = $null
    )
    $priorGenerations = @{}
    foreach ($candidate in $CandidateManifests) {
        $scopeId = [string]$candidate.scope_id
        $priorGenerations[$scopeId] = Get-ProjectWorldHighestManifestGeneration `
            -ManifestRoot $ManifestRoot -ScopeId $scopeId
    }
    [void](Test-ProjectWorldProspectiveSet `
        -CandidateManifests $CandidateManifests `
        -RetiredScopeIds $RetiredScopeIds `
        -PriorActiveSet $PriorActiveSet `
        -PriorGenerations $priorGenerations)
    if ($CandidateManifests.Count -eq 0 -and $RetiredScopeIds.Count -eq 0) {
        if ($null -eq $PriorActiveSet) {
            throw 'An empty candidate set cannot initialize manifest authority.'
        }
        if ($null -ne $BeforeCommit) {
            & $BeforeCommit $PriorActiveSet.Sha256
        }
        return [pscustomobject]@{
            Path = Join-Path $ManifestRoot 'active_set.json'
            Sha256 = $PriorActiveSet.Sha256
            PriorSha256 = $PriorActiveSet.Sha256
            Entries = @($PriorActiveSet.Record.scopes)
        }
    }
    $scopesDir = Join-Path $ManifestRoot 'scopes'
    $schemaReferences = Get-ProjectWorldManifestSchemaReferences `
        -ManifestRoot $ManifestRoot `
        -ProjectRoot $ProjectRoot
    $entries = [System.Collections.Generic.List[object]]::new()
    $carried = if ($null -ne $PriorActiveSet) { $PriorActiveSet.Record.scopes } else { @() }
    $candidateIds = @($CandidateManifests | ForEach-Object { $_.scope_id })
    foreach ($existing in $carried) {
        if ($existing.scope_id -in $candidateIds -or $existing.scope_id -in $RetiredScopeIds) { continue }
        $entries.Add([ordered]@{
            scope_id = $existing.scope_id
            manifest_path = $existing.manifest_path
            manifest_sha256 = $existing.manifest_sha256
        })
    }
    foreach ($candidate in $CandidateManifests) {
        $fileName = "$($candidate.scope_id).$($candidate.generation).json"
        $manifestPath = Join-Path $scopesDir $fileName
        if (Test-Path -LiteralPath $manifestPath) {
            throw "Immutable manifest already exists: $manifestPath"
        }
        $document = $candidate | ConvertTo-Json -Depth 8 | ConvertFrom-Json
        $document.'$schema' = $schemaReferences.Manifest
        Write-ProjectWorldJson -Document $document -Path $manifestPath
        $entries.Add([ordered]@{
            scope_id = $candidate.scope_id
            manifest_path = "scopes/$fileName"
            manifest_sha256 = Get-ProjectWorldFileSha256 -Path $manifestPath
        })
    }
    $priorSha = if ($null -ne $PriorActiveSet) { $PriorActiveSet.Sha256 } else { 'none' }
    $record = [ordered]@{
        '$schema' = $schemaReferences.ActiveSet
        schema_version = 1
        transaction_id = $TransactionId
        accepted_operation_id = $OperationId
        prior_active_set_sha256 = $priorSha
        scopes = @($entries | Sort-Object -Property { $_.scope_id })
    }
    $target = Join-Path $ManifestRoot 'active_set.json'
    $staging = "$target.tmp"
    # LF-only for the same reason as Write-ProjectWorldJson: this record's own
    # SHA is the activation authority, and git normalizes these paths to LF.
    $json = ($record | ConvertTo-Json -Depth 8) -replace "`r`n", "`n"
    [System.IO.File]::WriteAllText($staging, $json + "`n", [System.Text.UTF8Encoding]::new($false))
    $publishedSha = Get-ProjectWorldFileSha256 -Path $staging
    if ($null -ne $BeforeCommit) {
        & $BeforeCommit $publishedSha
    }
    Move-Item -LiteralPath $staging -Destination $target -Force
    # Post-commit housekeeping only: archive retired manifests.
    foreach ($retired in $RetiredScopeIds) {
        $priorEntry = $carried | Where-Object { $_.scope_id -eq $retired }
        if ($null -ne $priorEntry) {
            $archiveDir = Join-Path $ManifestRoot 'archive'
            New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null
            $source = Join-Path $ManifestRoot ($priorEntry.manifest_path.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
            if (Test-Path -LiteralPath $source -PathType Leaf) {
                Move-Item -LiteralPath $source -Destination $archiveDir -Force
            }
        }
    }
    return [pscustomobject]@{
        Path = $target
        Sha256 = $publishedSha
        PriorSha256 = $priorSha
        Entries = @($entries)
    }
}

function Invoke-ProjectWorldTransactionRecovery {
    # The ONLY operation allowed to resolve an interrupted transaction.
    # Validates and confines the journal before any mutation; removes
    # active-set staging debris; rolls back or finalizes per the journal.
    param(
        [Parameter(Mandatory = $true)][string]$ManifestRoot,
        [Parameter(Mandatory = $true)][string]$ContentRoot,
        [Parameter(Mandatory = $true)][string]$ProjectRoot
    )
    $journalPath = Join-Path $ManifestRoot 'journal.json'
    $activePath = Join-Path $ManifestRoot 'active_set.json'
    $activeStaging = "$activePath.tmp"
    $journal = Read-ProjectWorldTransactionJournal -ManifestRoot $ManifestRoot
    if ($null -eq $journal) {
        # A journal staging file without a committed journal proves the
        # transaction never became active (mutation strictly follows the
        # atomic journal move); cleaning it is safe. An orphan active-set
        # staging file without a journal is an unknown state: fail closed.
        $journalStaging = "$journalPath.tmp"
        if (Test-Path -LiteralPath $journalStaging) {
            Remove-Item -LiteralPath $journalStaging -Force
        }
        if (Test-Path -LiteralPath $activeStaging) {
            throw 'Active-set staging exists with no transaction journal; recovery fails closed. Operator investigation is required.'
        }
        return [pscustomobject]@{ State = 'no_transaction' }
    }
    Test-ProjectWorldJournal -Journal $journal -ProjectRoot $ProjectRoot -ManifestRoot $ManifestRoot -ContentRoot $ContentRoot
    $worldDataPlugin = Split-Path -Leaf (Split-Path -Parent $ContentRoot)
    $generatedPackageRoot = "/$worldDataPlugin/Generated/"
    $currentSha = if (Test-Path -LiteralPath $activePath -PathType Leaf) {
        Get-ProjectWorldFileSha256 -Path $activePath
    }
    else { 'none' }
    if ($journal.phase -eq 'publishing' -and $currentSha -eq $journal.expected_active_set_sha256) {
        # Completion is claimed ONLY after the published authority fully
        # validates: active set, every referenced manifest, and every
        # artifact in the mutation scope set. A validation failure
        # preserves the snapshot; it never reports completion.
        $published = Read-ProjectWorldActiveSet -ManifestRoot $ManifestRoot -ProjectRoot $ProjectRoot
        # Exact operation semantics: mutation scopes absent from the
        # committed active set must equal the declared retired set for a
        # delete, and must not exist at all for any other operation.
        $absentScopes = @($journal.mutation_scope_ids | Where-Object { -not $published.Manifests.Contains($_) } | Sort-Object)
        $retiredScopeIds = @($journal.retired_scopes | ForEach-Object { [string]$_.scope_id } | Sort-Object)
        if ($journal.operation -eq 'delete') {
            if (($absentScopes -join ',') -ne ($retiredScopeIds -join ',')) {
                throw 'Committed active-set retirement does not match the journal retirement evidence; recovery preserves the snapshot and fails closed.'
            }
        }
        elseif ($absentScopes.Count -gt 0) {
            throw 'A non-delete operation left declared mutation scopes absent from the committed active set; recovery preserves the snapshot and fails closed.'
        }
        foreach ($scopeId in @($journal.mutation_scope_ids)) {
            if (-not $published.Manifests.Contains($scopeId)) { continue }
            foreach ($artifact in @($published.Manifests[$scopeId].artifacts)) {
                $file = Join-Path $ProjectRoot ($artifact.path.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
                if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or
                    (Get-ProjectWorldFileSha256 -Path $file) -ne $artifact.digest) {
                    throw "Committed transaction failed post-commit validation (scope $scopeId, artifact $($artifact.path)); recovery preserves the snapshot and fails closed."
                }
            }
        }
        # A retired scope must prove COMPLETE absence using the EXACT prior
        # authority recorded in the journal - never directory inference.
        # Missing or mismatched prior-manifest evidence fails closed.
        $retiredEntries = @($journal.retired_scopes)
        foreach ($retired in $retiredEntries) {
            $priorPath = Join-Path $ManifestRoot ($retired.prior_manifest_path.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
            if (-not (Test-Path -LiteralPath $priorPath -PathType Leaf)) {
                # Post-commit archiving may have moved it; the file name is
                # authoritative, the hash check below still binds identity.
                $priorPath = Join-Path (Join-Path $ManifestRoot 'archive') (Split-Path -Leaf $retired.prior_manifest_path)
            }
            if (-not (Test-Path -LiteralPath $priorPath -PathType Leaf) -or
                (Get-ProjectWorldFileSha256 -Path $priorPath) -ne $retired.prior_manifest_sha256) {
                throw "Retired scope $($retired.scope_id) prior-manifest evidence is missing or mismatched; recovery preserves the snapshot and fails closed."
            }
            $priorManifest = Get-Content -LiteralPath $priorPath -Raw | ConvertFrom-Json
            if ($priorManifest.scope_id -ne $retired.scope_id) {
                throw "Retired scope $($retired.scope_id) prior manifest identity mismatch; recovery fails closed."
            }
            foreach ($artifact in @($priorManifest.artifacts)) {
                $file = Join-Path $ProjectRoot ($artifact.path.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
                if (Test-Path -LiteralPath $file -PathType Leaf) {
                    throw "Committed retirement left an unowned artifact on disk ($($artifact.path)); recovery preserves the snapshot and fails closed."
                }
            }
        }
        if (Test-Path -LiteralPath $activeStaging) {
            Remove-Item -LiteralPath $activeStaging -Force
        }
        if ($journal.snapshot_root -and (Test-Path -LiteralPath $journal.snapshot_root)) {
            Remove-Item -LiteralPath $journal.snapshot_root -Recurse -Force
        }
        Remove-Item -LiteralPath $journalPath -Force
        return [pscustomobject]@{ State = 'completed' }
    }
    if ($currentSha -ne $journal.prior_active_set_sha256) {
        throw "Recovery found an active set matching neither the prior nor the expected transaction state; recovery fails closed and preserves all evidence."
    }
    if (-not (Test-Path -LiteralPath $journal.snapshot_root)) {
        throw 'Interrupted transaction has no recovery snapshot; partial state fails closed. A separately named destructive operator procedure is required.'
    }
    $records = @($journal.snapshot_records | ForEach-Object {
        [pscustomobject]@{
            Source = $_.source
            Backup = $_.backup
            Existed = $(if ($_.PSObject.Properties.Name -contains 'existed') { [bool]$_.existed } else { $true })
        }
    })
    Restore-ProjectWorldGeneratedSnapshot `
        -ContentRoot $ContentRoot `
        -MapPackage $journal.map_package `
        -GeneratedPackageRoot $generatedPackageRoot `
        -Records $records
    foreach ($candidate in @($journal.candidate_manifest_paths)) {
        $candidateFull = Join-Path $ManifestRoot ($candidate.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
        if (Test-Path -LiteralPath $candidateFull -PathType Leaf) {
            Remove-Item -LiteralPath $candidateFull -Force
        }
    }
    if (Test-Path -LiteralPath $activeStaging) {
        Remove-Item -LiteralPath $activeStaging -Force
    }
    $scopesDir = Join-Path $ManifestRoot 'scopes'
    if (Test-Path -LiteralPath $scopesDir) {
        Get-ChildItem -LiteralPath $scopesDir -Filter '*.tmp' | Remove-Item -Force
    }
    Remove-Item -LiteralPath $journal.snapshot_root -Recurse -Force
    Remove-Item -LiteralPath $journalPath -Force
    return [pscustomobject]@{ State = 'rolled_back' }
}
