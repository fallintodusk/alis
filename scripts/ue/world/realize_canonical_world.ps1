# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CompileResult,

    [ValidateSet("Validate", "Apply", "Delete")]
    [string]$Mode = "Validate",

    [string]$Map = "",

    [string]$WorldDataPlugin = "",

    [string]$PresentationProfile = "",

	[string]$RuntimeProfile = "",

    [string]$AuthoredOverlayProfile = "",

    [string]$RealizationProfile = "",

    # Test-only realization SOT copies may live under a caller-owned tmp/world
    # root when the manifest authority is confined to that same sandbox.
    [string]$TransientRealizationProfileRoot = "",

    [string]$EvidencePath = "",

	[ValidateRange(0, 1000)]
	[int]$MaxRoads = 1,

	[ValidateRange(0, 1000)]
	[int]$MaxBuildings = 4,

    [switch]$RequireLandscape,

    [AllowEmptyCollection()]
    [string[]]$DirtyUnit = @(),

    # Manifest authority root; defaults to the durable root. Isolated
    # validation runs pass a transient sandbox root and never touch the
    # durable authority.
    [string]$ManifestRoot = "",

    # One-time enrollment of scopes that have no accepted manifest yet.
    # Without this switch, a missing active set or missing scope refuses.
    [switch]$EnrollManifests,

    # Explicit operator authorization for durable production enrollment. Only the
    # sanctioned L3 command (EndToEndValidation "enroll") passes this, and only
    # after the operator authorized that exact operation. Without it an
    # unattended -EnrollManifests into the durable root still refuses, so an
    # agent cannot decide to grant production authority on its own.
    [switch]$DurableEnrollmentAuthorized,

    # Explicit clean reconstruction (contract route 2). Requires accepted
    # manifests for the declared scope set; each participating scope must be
    # fully absent or exactly match its accepted state - partial absence or
    # mixed unknown content rejects. Absence alone NEVER authorizes
    # regeneration; this named flag does.
    [switch]$Reconstruct,

    # CI and other automation must opt in explicitly. Interactive mutation
    # requires an operator confirmation after the exact scope preview.
    [switch]$NonInteractive
)

# Manifest-refusal exit codes:
#   6 = interrupted transaction journal present (run recover_generated_transaction.ps1)
#   7 = manifest enrollment required (no accepted manifest for a scope)

$ErrorActionPreference = "Stop"
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $scriptDirectory))
$configDirectory = Join-Path $projectRoot "scripts\config"
. (Join-Path $configDirectory "Resolve-UEConfig.ps1")
. (Join-Path $scriptDirectory "generated_content_transaction.ps1")
. (Join-Path $scriptDirectory "generated_manifest.ps1")
. (Join-Path $scriptDirectory "realization_layer_operation.ps1")
. (Join-Path $scriptDirectory "operator_controls.ps1")
. (Join-Path $scriptDirectory "execution_envelope.ps1")
$config = Resolve-UEConfig -ConfigDir $configDirectory

$editorCommand = Join-Path $config.UE_PATH "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$projectFile = Join-Path $projectRoot "Alis.uproject"
$compileResultPath = [System.IO.Path]::GetFullPath($CompileResult)
$modeName = $Mode.ToLowerInvariant()
# Impossible switch combinations are refused BEFORE any lock, snapshot,
# or journal so the recorded operation is always truthful.
if ($Reconstruct -and $EnrollManifests) {
    throw 'Invalid combination: -Reconstruct and -EnrollManifests are mutually exclusive.'
}
if ($Reconstruct -and $modeName -ne 'apply') {
    throw 'Invalid combination: -Reconstruct is only valid with -Mode Apply.'
}
if ($EnrollManifests -and $modeName -ne 'apply') {
    throw 'Invalid combination: -EnrollManifests is only valid with -Mode Apply.'
}
$runtimeProfilePath = if ($modeName -eq "delete" -or [string]::IsNullOrWhiteSpace($RuntimeProfile)) {
	""
}
else {
	[System.IO.Path]::GetFullPath($RuntimeProfile)
}
$authoredOverlayProfilePath = if ($modeName -eq "delete") {
    ""
}
elseif ([string]::IsNullOrWhiteSpace($AuthoredOverlayProfile)) {
    throw 'Apply and Validate require an explicit world-data-owned authored overlay profile.'
}
else {
    [System.IO.Path]::GetFullPath($AuthoredOverlayProfile)
}
$realizationProfilePath = if ([string]::IsNullOrWhiteSpace($RealizationProfile)) {
    ""
}
else {
    [System.IO.Path]::GetFullPath($RealizationProfile)
}
if (-not (Test-Path -LiteralPath $compileResultPath -PathType Leaf)) {
    throw "Compile result does not exist: $compileResultPath"
}
$compileReceipt = Get-Content -LiteralPath $compileResultPath -Raw | ConvertFrom-Json
$coverageDescriptor = @($compileReceipt.outputs | Where-Object { $_.path -eq 'canonical/coverage.json' })
if ($coverageDescriptor.Count -ne 1) {
    throw 'Compile result must identify exactly one canonical coverage document.'
}
$coveragePath = Join-Path (Split-Path -Parent $compileResultPath) $coverageDescriptor[0].path
if (-not (Test-Path -LiteralPath $coveragePath -PathType Leaf)) {
    throw "Canonical coverage document does not exist: $coveragePath"
}
$coverageInfo = Get-Item -LiteralPath $coveragePath
$coverageHash = (Get-FileHash -LiteralPath $coveragePath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($coverageInfo.Length -ne [int64]$coverageDescriptor[0].byte_size -or
    $coverageHash -cne [string]$coverageDescriptor[0].sha256) {
    throw 'Canonical coverage no longer matches the accepted compile receipt.'
}
$declaredWorldDataPlugin = [string](Get-Content -LiteralPath $coveragePath -Raw | ConvertFrom-Json).world_data_plugin
if ([string]::IsNullOrWhiteSpace($declaredWorldDataPlugin)) {
    throw 'Canonical coverage declares no world-data owner.'
}
if (-not [string]::IsNullOrWhiteSpace($WorldDataPlugin) -and $WorldDataPlugin -cne $declaredWorldDataPlugin) {
    throw "Requested world-data owner '$WorldDataPlugin' conflicts with canonical owner '$declaredWorldDataPlugin'."
}
$WorldDataPlugin = $declaredWorldDataPlugin
$worldDataRoots = Resolve-ProjectWorldDataRoots -ProjectRoot $projectRoot -PluginName $WorldDataPlugin
if ([string]::IsNullOrWhiteSpace($Map)) {
    throw 'World realization requires an explicit generated map package.'
}
$presentationProfilePath = if ($modeName -eq "delete") {
    ""
}
elseif (-not [string]::IsNullOrWhiteSpace($PresentationProfile)) {
    [System.IO.Path]::GetFullPath($PresentationProfile)
}
else {
    throw 'Apply and Validate require an explicit world-data-owned presentation profile.'
}
if ($modeName -ne "delete" -and -not (Test-Path -LiteralPath $presentationProfilePath -PathType Leaf)) {
    throw "Presentation profile does not exist: $presentationProfilePath"
}
if (-not [string]::IsNullOrWhiteSpace($runtimeProfilePath) -and
	-not (Test-Path -LiteralPath $runtimeProfilePath -PathType Leaf)) {
	throw "Runtime profile does not exist: $runtimeProfilePath"
}
if (-not [string]::IsNullOrWhiteSpace($authoredOverlayProfilePath) -and
    -not (Test-Path -LiteralPath $authoredOverlayProfilePath -PathType Leaf)) {
    throw "Authored overlay profile does not exist: $authoredOverlayProfilePath"
}
if (-not [string]::IsNullOrWhiteSpace($realizationProfilePath) -and
    -not (Test-Path -LiteralPath $realizationProfilePath -PathType Leaf)) {
    throw "Realization profile does not exist: $realizationProfilePath"
}
$dataRootPrefix = $worldDataRoots.DataRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
$transientRoot = ''
foreach ($profilePath in @($presentationProfilePath, $runtimeProfilePath, $authoredOverlayProfilePath)) {
    if (-not [string]::IsNullOrWhiteSpace($profilePath) -and
        -not $profilePath.StartsWith($dataRootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "World realization profile must belong to $($worldDataRoots.PluginName) Data: $profilePath"
    }
}
if (-not [string]::IsNullOrWhiteSpace($realizationProfilePath) -and
    -not $realizationProfilePath.StartsWith($dataRootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    $transientRoot = if ([string]::IsNullOrWhiteSpace($TransientRealizationProfileRoot)) {
        ''
    }
    else { [System.IO.Path]::GetFullPath($TransientRealizationProfileRoot).TrimEnd('\', '/') }
    $tmpWorldRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $projectRoot 'tmp\world')).TrimEnd('\', '/')
    $transientPrefix = $transientRoot + [System.IO.Path]::DirectorySeparatorChar
    $manifestRootPath = if ([string]::IsNullOrWhiteSpace($ManifestRoot)) {
        ''
    }
    else { [System.IO.Path]::GetFullPath($ManifestRoot) }
    $isConfinedTestProfile = $NonInteractive -and
        $transientRoot.StartsWith(
            $tmpWorldRoot + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase) -and
        $realizationProfilePath.StartsWith(
            $transientPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
        $manifestRootPath.StartsWith(
            $transientPrefix, [System.StringComparison]::OrdinalIgnoreCase)
    if (-not $isConfinedTestProfile) {
        throw "World realization profile must belong to $($worldDataRoots.PluginName) Data or an explicitly confined transient test root: $realizationProfilePath"
    }
}
if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
    throw "UnrealEditor-Cmd.exe does not exist under the configured UE_PATH."
}

$compileResultHash = (Get-FileHash -LiteralPath $compileResultPath -Algorithm SHA256).Hash.ToLowerInvariant()
$presentationProfileHash = if ($modeName -eq "delete") {
    "none"
}
else {
    (Get-FileHash -LiteralPath $presentationProfilePath -Algorithm SHA256).Hash.ToLowerInvariant()
}
$runtimeProfileHash = if ([string]::IsNullOrWhiteSpace($runtimeProfilePath)) {
	"none"
}
else {
	(Get-FileHash -LiteralPath $runtimeProfilePath -Algorithm SHA256).Hash.ToLowerInvariant()
}
$authoredOverlayProfileHash = if ([string]::IsNullOrWhiteSpace($authoredOverlayProfilePath)) {
    "none"
}
else {
    (Get-FileHash -LiteralPath $authoredOverlayProfilePath -Algorithm SHA256).Hash.ToLowerInvariant()
}
$realizationProfileHash = if ([string]::IsNullOrWhiteSpace($realizationProfilePath)) {
    "none"
}
else {
    (Get-FileHash -LiteralPath $realizationProfilePath -Algorithm SHA256).Hash.ToLowerInvariant()
}
$realizationDocument = $null
$layerDefinitions = [ordered]@{}
$layerScopePaths = @{}
if (-not [string]::IsNullOrWhiteSpace($realizationProfilePath)) {
    $realizationDocument = Get-Content -LiteralPath $realizationProfilePath -Raw | ConvertFrom-Json
    if ([string]$realizationDocument.world_data_plugin -cne $WorldDataPlugin -or
        [string]$realizationDocument.canonical_profile_id -cne [string]$compileReceipt.profile_id -or
        [string]$realizationDocument.map_package -cne $Map) {
        throw 'Realization profile owner, canonical profile, or map does not match this operation.'
    }
    $resolvedLayers = Resolve-ProjectWorldRealizationLayers `
        -RealizationDocument $realizationDocument `
        -WorldDataRoots $worldDataRoots
    $layerDefinitions = $resolvedLayers.Definitions
    $layerScopePaths = $resolvedLayers.ScopePaths
}
if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $identity = [ordered]@{
        schema_version = 1
        compile_result_sha256 = $compileResultHash
        presentation_profile_sha256 = $presentationProfileHash
		runtime_profile_sha256 = $runtimeProfileHash
        authored_overlay_profile_sha256 = $authoredOverlayProfileHash
        realization_profile_sha256 = $realizationProfileHash
        map = $Map
        max_roads = $MaxRoads
        max_buildings = $MaxBuildings
        require_landscape = [bool]$RequireLandscape
        dirty_units = @($DirtyUnit | Sort-Object -Unique)
    }
    $identityBytes = [System.Text.Encoding]::UTF8.GetBytes(($identity | ConvertTo-Json -Compress))
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $invocationHash = ([System.BitConverter]::ToString($sha256.ComputeHash($identityBytes))).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
    $evidenceDirectory = Join-Path $projectRoot "Saved\Validation\WorldRealization\$invocationHash"
    $resultPath = Join-Path $evidenceDirectory "$modeName.json"
}
else {
    $resultPath = [System.IO.Path]::GetFullPath($EvidencePath)
    $evidenceDirectory = Split-Path -Parent $resultPath
}
$evidenceRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "Saved\Validation\WorldRealization"))
$evidencePrefix = $evidenceRoot + [System.IO.Path]::DirectorySeparatorChar
if (-not $resultPath.StartsWith($evidencePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Evidence path must stay under $evidenceRoot"
}
New-Item -ItemType Directory -Path $evidenceDirectory -Force | Out-Null
$logPath = [System.IO.Path]::ChangeExtension($resultPath, ".log")
if (Test-Path -LiteralPath $resultPath -PathType Leaf) {
    Remove-Item -LiteralPath $resultPath -Force
}

$contentRoot = $worldDataRoots.ContentRoot
$transactionParent = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "tmp\world\world_realization\transactions"))
$transactionId = [System.Guid]::NewGuid().ToString("N")
$transactionRoot = Join-Path $transactionParent $transactionId
$transactionRecords = @()
$transactionActive = $modeName -eq "apply" -or $modeName -eq "delete"

# --- Manifest preflight (contract: territory_generation.md lifecycle) ---
$resolvedManifestRoot = if ([string]::IsNullOrWhiteSpace($ManifestRoot)) {
    $worldDataRoots.ManifestRoot
}
else {
    [System.IO.Path]::GetFullPath($ManifestRoot)
}
# C8 operator control, revised 2026-08-20. The authority boundary is the MANIFEST
# ROOT, not the owning plugin.
#
# DURABLE root (the plugin's Data/Manifests): enrollment grants durable authority
# to a brand-new production scope. Operator-executed, never unattended.
#
# TRANSIENT root (anywhere else, e.g. a candidate under tmp/): the durable active
# set is neither read, written, nor retired. Building pre-approval candidates and
# admitting new generated layers depends on this being unattended-legal - the
# territory Matrix itself enrolls into its own sandbox root, and every future
# layer admission does the same. Generated packages it rewrites are regenerable
# and transaction-protected.
#
# This lives HERE, not in a Claude/Codex permission rule, because it is a
# property of the irreversible operation itself. A tool-level rule only covers
# the tools someone remembered to configure; a third agent, a CI job, or a
# script would bypass it. Checked after the canonical owner and manifest root are
# resolved, and before any lock, snapshot, or journal, so a refused run leaves no
# trace. TestData enrollment stays unattended-legal in every root.
$durableManifestRoot =
    [System.IO.Path]::GetFullPath($worldDataRoots.ManifestRoot).TrimEnd('\', '/')
$enrollmentTargetsDurableAuthority =
    [System.IO.Path]::GetFullPath($resolvedManifestRoot).TrimEnd('\', '/') -ieq $durableManifestRoot
if ($EnrollManifests -and $NonInteractive -and -not $DurableEnrollmentAuthorized -and
    $WorldDataPlugin -ceq 'ProjectWorldData' -and $enrollmentTargetsDurableAuthority) {
    throw ('Refused: production enrollment (-EnrollManifests on ' +
        'ProjectWorldData into the durable manifest root) cannot run with ' +
        '-NonInteractive. Enrollment of production authority is ' +
        'operator-executed after approval.')
}

$mapScopeId = Get-ProjectWorldMapScopeId -MapPackage $Map -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot
$presentationScopeId = $null
if ($modeName -eq "apply") {
    $presentationProfileId = (Get-Content -LiteralPath $presentationProfilePath -Raw | ConvertFrom-Json).profile_id
    $presentationScopeId = Get-ProjectWorldPresentationScopeId -ProfileId $presentationProfileId
}
$mapScopePaths = @(Get-ProjectWorldGeneratedPaths -ContentRoot $contentRoot -MapPackage $Map -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot -IncludePresentation $false)
$presentationScopePaths = @(Get-ProjectWorldPresentationRoot -ContentRoot $contentRoot)
$activeSet = $null
$scopeGenerations = @{}
$retirePresentation = $false
$firstLayerApply = $false
$removedLayerScopes = @()
$authorityLock = $null
if ($transactionActive) {
    # The project-global content mutation lock serializes EVERY generated
    # tree mutation regardless of manifest root (durable or sandbox); the
    # authority lock additionally guards this manifest root. Both acquired
    # BEFORE reading the journal or active set; released on process exit.
    $contentLock = Enter-ProjectWorldContentLock -ProjectRoot $projectRoot
    $authorityLock = Enter-ProjectWorldAuthorityLock -ManifestRoot $resolvedManifestRoot
    if ($null -ne (Read-ProjectWorldTransactionJournal -ManifestRoot $resolvedManifestRoot)) {
        Write-Host "[WorldRealization] REFUSED: interrupted transaction journal present. Run scripts/ue/world/recover_generated_transaction.ps1"
        exit 6
    }
    $activeSet = Read-ProjectWorldActiveSet `
        -ManifestRoot $resolvedManifestRoot `
        -ProjectRoot $ProjectRoot
    $participating = @{ $mapScopeId = $mapScopePaths }
    if ($null -ne $presentationScopeId) {
        $participating[$presentationScopeId] = $presentationScopePaths
    }
    if ($modeName -eq 'apply') {
        foreach ($scopeId in $layerScopePaths.Keys) {
            $participating[$scopeId] = $layerScopePaths[$scopeId]
        }
    }
    if ($modeName -eq 'delete' -and $null -ne $activeSet) {
        # A map delete also mutates any shared presentation scope that
        # lists this map as a consumer (consumer removal, same transaction).
        foreach ($scopeId in @($activeSet.Manifests.Keys)) {
            if ($scopeId -like 'presentation_*' -and
                $mapScopeId -in @($activeSet.Manifests[$scopeId].consumer_references)) {
                $presentationScopeId = $scopeId
                $participating[$presentationScopeId] = $presentationScopePaths
                $consumers = @($activeSet.Manifests[$presentationScopeId].consumer_references)
                $retirePresentation = $consumers.Count -eq 1
            }
        }
        foreach ($scopeId in @($activeSet.Manifests.Keys)) {
            if ($scopeId -like 'layer_*' -and
                $mapScopeId -in @($activeSet.Manifests[$scopeId].consumer_references)) {
                $contract = $activeSet.Manifests[$scopeId].layer_contract
                $layerScopePaths[$scopeId] = @(Get-ProjectWorldLayerScopePath `
                    -WorldDataRoots $worldDataRoots -LayerContract $contract)
                $participating[$scopeId] = $layerScopePaths[$scopeId]
            }
        }
    }
    if ($null -eq $activeSet) {
        if (-not $EnrollManifests) {
            Write-Host "[WorldRealization] REFUSED: no accepted manifest set. Enrollment via -EnrollManifests is required."
            exit 7
        }
        # Initialization is only valid for a genuinely new authority root;
        # prior evidence without a valid active set fails closed.
        Assert-ProjectWorldAuthorityInitializable -ManifestRoot $resolvedManifestRoot
    }
    else {
        Test-ProjectWorldGlobalOwnership -Manifests $activeSet.Manifests
        if ($modeName -eq 'apply') {
            $otherPresentationScopes = @($activeSet.Manifests.Keys | Where-Object {
                $_ -like 'presentation_*' -and $_ -ne $presentationScopeId
            })
            if ($otherPresentationScopes.Count -gt 0) {
                Write-Host "[WorldRealization] REFUSED: presentation profile transition requires explicit retirement of: $($otherPresentationScopes -join ', ')."
                exit 7
            }
            if ($null -ne $realizationDocument) {
                foreach ($scopeId in @($activeSet.Manifests.Keys)) {
                    if ($scopeId -notlike 'layer_*' -or $layerDefinitions.Contains($scopeId) -or
                        $mapScopeId -notin @($activeSet.Manifests[$scopeId].consumer_references)) {
                        continue
                    }
                    $contract = $activeSet.Manifests[$scopeId].layer_contract
                    $layerScopePaths[$scopeId] = @(Get-ProjectWorldLayerScopePath `
                        -WorldDataRoots $worldDataRoots -LayerContract $contract)
                    $participating[$scopeId] = $layerScopePaths[$scopeId]
                    $removedLayerScopes += $scopeId
                }
            }
        }
        foreach ($scopeId in @($participating.Keys)) {
            if (-not $activeSet.Manifests.Contains($scopeId) -and -not $EnrollManifests) {
                Write-Host "[WorldRealization] REFUSED: scope '$scopeId' has no accepted manifest. Enrollment via -EnrollManifests is required."
                exit 7
            }
        }
        if ($Reconstruct) {
            # Explicit clean reconstruction: each declared scope must be
            # fully absent or exactly match its accepted manifest.
            Test-ProjectWorldReconstructionScopeState `
                -ProjectRoot $projectRoot -ActiveSet $activeSet `
                -ScopePathsById $participating
        }
        else {
            Test-ProjectWorldScopeDrift -ProjectRoot $projectRoot -ActiveSet $activeSet -ScopePathsById $participating
        }
    }
    if ($modeName -eq 'apply' -and $layerScopePaths.Count -gt 0) {
        $acceptedLayerCount = if ($null -eq $activeSet) { 0 } else {
            @($layerScopePaths.Keys | Where-Object { $activeSet.Manifests.Contains($_) }).Count
        }
        $firstLayerApply = $acceptedLayerCount -eq 0
    }
    # A Delete must resolve its exact prior authority BEFORE any mutation;
    # retiring an unowned scope is refused, never recorded as empty evidence.
    $retiredJournalEntries = @()
    if ($modeName -eq 'apply') {
        foreach ($scopeId in $removedLayerScopes) {
            $priorLayerEntry = $activeSet.Record.scopes |
                Where-Object { $_.scope_id -eq $scopeId }
            $retiredJournalEntries += , ([ordered]@{
                scope_id = $scopeId
                prior_manifest_path = $priorLayerEntry.manifest_path
                prior_manifest_sha256 = $priorLayerEntry.manifest_sha256
            })
        }
    }
    if ($modeName -eq 'delete') {
        $priorEntry = if ($null -ne $activeSet) {
            $activeSet.Record.scopes | Where-Object { $_.scope_id -eq $mapScopeId }
        }
        else { $null }
        if ($null -eq $priorEntry) {
            Write-Host "[WorldRealization] REFUSED: delete requires an accepted manifest for scope '$mapScopeId'."
            exit 7
        }
        $retiredJournalEntries = @([ordered]@{
            scope_id = $mapScopeId
            prior_manifest_path = $priorEntry.manifest_path
            prior_manifest_sha256 = $priorEntry.manifest_sha256
        })
        if ($retirePresentation) {
            $priorPresentationEntry = $activeSet.Record.scopes |
                Where-Object { $_.scope_id -eq $presentationScopeId }
            $retiredJournalEntries += , ([ordered]@{
                scope_id = $presentationScopeId
                prior_manifest_path = $priorPresentationEntry.manifest_path
                prior_manifest_sha256 = $priorPresentationEntry.manifest_sha256
            })
        }
        foreach ($scopeId in @($layerScopePaths.Keys)) {
            $priorLayerEntry = $activeSet.Record.scopes |
                Where-Object { $_.scope_id -eq $scopeId }
            $retiredJournalEntries += , ([ordered]@{
                scope_id = $scopeId
                prior_manifest_path = $priorLayerEntry.manifest_path
                prior_manifest_sha256 = $priorLayerEntry.manifest_sha256
            })
        }
    }
    foreach ($scopeId in @($participating.Keys)) {
        $activeGeneration = if ($null -ne $activeSet -and $activeSet.Manifests.Contains($scopeId)) {
            [int]$activeSet.Manifests[$scopeId].generation
        }
        else { 0 }
        $historicalGeneration = Get-ProjectWorldHighestManifestGeneration `
            -ManifestRoot $resolvedManifestRoot -ScopeId $scopeId
        $prior = [Math]::Max($activeGeneration, $historicalGeneration)
        $scopeGenerations[$scopeId] = $prior + 1
    }
    $operationRoute = if ($EnrollManifests -and $null -eq $activeSet) { 'enroll' }
        elseif ($Reconstruct) { 'reconstruct' }
        elseif ($modeName -eq 'delete') { 'delete' }
        else { 'apply' }
    Confirm-ProjectWorldMutation `
        -Operation $operationRoute `
        -MapPackage $Map `
        -ParticipatingScopes $participating `
        -ActiveSet $activeSet `
        -AuthoredContentRoot (Join-Path $contentRoot 'Authored') `
        -NonInteractive:$NonInteractive
    $transactionRecords = @(New-ProjectWorldGeneratedSnapshot `
        -ContentRoot $contentRoot `
        -MapPackage $Map `
        -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot `
        -SnapshotRoot $transactionRoot `
        -AdditionalPaths @($layerScopePaths.Values | ForEach-Object { @($_) }))
    $plannedCandidates = @($scopeGenerations.Keys | ForEach-Object { "scopes/$_.$($scopeGenerations[$_]).json" })
    Write-ProjectWorldTransactionJournal -ManifestRoot $resolvedManifestRoot -Journal ([ordered]@{
        transaction_id = $transactionId
        phase = 'mutating'
        map_package = $Map
        snapshot_root = $transactionRoot
        snapshot_records = @($transactionRecords | ForEach-Object {
            [ordered]@{ source = $_.Source; backup = $_.Backup; existed = [bool]$_.Existed }
        })
        candidate_manifest_paths = $plannedCandidates
        expected_active_set_sha256 = ''
        prior_active_set_sha256 = $(if ($null -ne $activeSet) { $activeSet.Sha256 } else { 'none' })
        mutation_scope_ids = @($participating.Keys | Sort-Object)
        operation = $operationRoute
        retired_scopes = $retiredJournalEntries
    })
}

$layerDirtyInputPath = ""
$layerDirtyInputHash = "none"
if ($modeName -eq 'apply' -and $layerDefinitions.Count -gt 0) {
    $dirtyInput = New-ProjectWorldLayerDirtyInput `
        -ProjectRoot $projectRoot `
        -OutputDirectory $evidenceDirectory `
        -RealizationDocument $realizationDocument `
        -LayerDefinitions $layerDefinitions `
        -ActiveSet $activeSet `
        -OperatorDirtyUnits $DirtyUnit
    $layerDirtyInputPath = $dirtyInput.Path
    $layerDirtyInputHash = $dirtyInput.Sha256
}

$unrealArguments = @(
    $projectFile
    "-run=ProjectWorldRealize"
    "-CompileResult=$compileResultPath"
    "-Result=$resultPath"
    "-Mode=$modeName"
    "-Map=$Map"
	"-EnablePlugins=$WorldDataPlugin"
	"-MaxRoads=$MaxRoads"
	"-MaxBuildings=$MaxBuildings"
    "-abslog=$logPath"
    "-unattended"
    "-nop4"
    "-nosplash"
    "-FullStdOutLogOutput"
)
# ProjectWorldRealize composes Landscape edit layers through UE's RDG merge, so it is a
# render-required step. The envelope helper owns the flags; see execution_envelope.ps1.
$projectWorldRealizeRendering = 'Required'
$unrealArguments += Get-ProjectWorldExecutionEnvelopeArguments -Rendering $projectWorldRealizeRendering
if ($modeName -ne "delete") {
    $unrealArguments += "-PresentationProfile=$presentationProfilePath"
    $unrealArguments += "-AuthoredOverlayProfile=$authoredOverlayProfilePath"
}
if (-not [string]::IsNullOrWhiteSpace($runtimeProfilePath)) {
	$unrealArguments += "-RuntimeProfile=$runtimeProfilePath"
}
if (-not [string]::IsNullOrWhiteSpace($realizationProfilePath)) {
    $unrealArguments += "-RealizationProfile=$realizationProfilePath"
}
if (-not [string]::IsNullOrWhiteSpace($transientRoot)) {
    $unrealArguments += "-TransientRealizationProfileRoot=$transientRoot"
}
if (-not [string]::IsNullOrWhiteSpace($layerDirtyInputPath)) {
    $unrealArguments += "-LayerDirtyInput=$layerDirtyInputPath"
}
if ($firstLayerApply) {
    $unrealArguments += "-FirstLayerApply"
}
if ($RequireLandscape) {
    $unrealArguments += "-RequireLandscape"
}

# Contradictory envelopes fail before the editor launches, not after a flat map is written.
Assert-ProjectWorldExecutionEnvelope -Rendering $projectWorldRealizeRendering -Arguments $unrealArguments
Write-Host "[WorldRealization] rendering=$projectWorldRealizeRendering"
Write-Host "[WorldRealization] mode=$modeName"
Write-Host "[WorldRealization] input=$compileResultPath"
Write-Host "[WorldRealization] presentation=$($presentationProfilePath.Trim())"
Write-Host "[WorldRealization] runtime=$($runtimeProfilePath.Trim())"
Write-Host "[WorldRealization] authored_overlay=$($authoredOverlayProfilePath.Trim())"
Write-Host "[WorldRealization] realization=$($realizationProfilePath.Trim())"
Write-Host "[WorldRealization] evidence=$resultPath"
$engineExitCode = -1
$result = $null
$childStatus = "missing"
$invocationFailure = $null
try {
    & $editorCommand @unrealArguments | Out-Null
    $engineExitCode = $LASTEXITCODE
}
catch {
    $invocationFailure = $_
}
if (Test-Path -LiteralPath $resultPath -PathType Leaf) {
    try {
        $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
        $childStatus = [string]$result.status
    }
    catch {
        if ($null -eq $invocationFailure) {
            $invocationFailure = $_
        }
        $childStatus = "invalid"
    }
}
elseif ($null -eq $invocationFailure) {
    $invocationFailure = [System.InvalidOperationException]::new(
        "World realization emitted no structured result. See $logPath")
}

if ($transactionActive) {
    $accepted = ($engineExitCode -eq 0 -and $childStatus -eq 'accepted')
    if ($accepted) {
        $layerCandidatesByScope = @{}
        if ($modeName -eq 'apply' -and $layerDefinitions.Count -gt 0) {
            $mapScopePaths = @(Get-ProjectWorldGeneratedPaths `
                -ContentRoot $contentRoot -MapPackage $Map `
                -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot `
                -IncludePresentation $false)
            $layerExternalRoots = @($mapScopePaths | Where-Object {
                (Test-Path -LiteralPath $_ -PathType Container) -and
                ($_ -match '[\\/]__External(Actors|Objects)__[\\/]')
            })
            if ([string]$result.realization_profile -cne [string]$realizationDocument.profile_id -or
                [string]$result.realization_profile_sha256 -cne $realizationProfileHash -or
                [string]$result.layer_dirty_input_sha256 -cne $layerDirtyInputHash) {
                throw 'Accepted child result does not authenticate the selected realization profile.'
            }
            $inventories = @($result.layer_inventories)
            if ($inventories.Count -ne $layerDefinitions.Count) {
                throw 'Accepted child result does not contain exactly one inventory per generated layer.'
            }
            foreach ($inventory in $inventories) {
                $scopeId = [string]$inventory.scope_id
                if (-not $layerDefinitions.Contains($scopeId) -or $layerCandidatesByScope.ContainsKey($scopeId)) {
                    throw "Accepted child emitted an unknown or duplicate layer inventory: $scopeId"
                }
                $definition = $layerDefinitions[$scopeId]
                if ([string]$inventory.layer_id -cne [string]$definition.layer_id -or
                    [string]$inventory.generator_id -cne [string]$definition.generator_id -or
                    [int]$inventory.generator_version -ne [int]$definition.generator_version -or
                    [string]$inventory.artifact_root -cne [string]$definition.artifact_root -or
                    [string]$inventory.normalized_layer_contract_sha256 -notmatch '^[a-f0-9]{64}$') {
                    throw "Accepted child layer inventory conflicts with the realization profile: $scopeId"
                }
                $declaredRecords = @($inventory.artifacts | ForEach-Object {
                    [ordered]@{
                        path = [string]$_.path
                        kind = [string]$_.kind
                        digest_kind = [string]$_.digest_kind
                        digest = [string]$_.digest
                    }
                })
                $records = @(Get-ProjectWorldExactLayerArtifactRecords `
                    -ProjectRoot $projectRoot `
                    -ArtifactRootPath $layerScopePaths[$scopeId][0] `
                    -Inventory $declaredRecords `
                    -AllowedExternalRoots $layerExternalRoots)
                $semanticOutputs = @($inventory.artifacts | ForEach-Object {
                    if ([string]$_.semantic_sha256 -notmatch '^[a-f0-9]{64}$') {
                        throw "Layer artifact has no valid semantic identity: $($_.path)"
                    }
                    [ordered]@{
                        artifact_path = [string]$_.path
                        semantic_sha256 = [string]$_.semantic_sha256
                    }
                })
                $contract = [ordered]@{
                    realization_profile_id = [string]$realizationDocument.profile_id
                    realization_profile_sha256 = $realizationProfileHash
                    normalized_layer_contract_sha256 = [string]$inventory.normalized_layer_contract_sha256
                    generator_id = [string]$inventory.generator_id
                    generator_version = [int]$inventory.generator_version
                    artifact_root = [string]$inventory.artifact_root
                    canonical_inputs = @($inventory.canonical_inputs)
                    dependency_inputs = @($inventory.dependency_inputs)
                    final_dirty_units = @($inventory.final_dirty_units)
                    semantic_outputs = $semanticOutputs
                }
                $layerCandidatesByScope[$scopeId] = [pscustomobject]@{
                    Records = $records
                    Contract = $contract
                }
            }
        }
        # The commandlet removes generated actors but can leave an empty map,
        # HLOD assets, or external-package directories. Delete owns complete
        # scope absence, so remove every confined map artifact before the
        # active-set retirement commit. The transaction snapshot restores all
        # of them if any later publication step fails.
        if ($modeName -eq 'delete') {
            Remove-ProjectWorldGeneratedPaths -ContentRoot $contentRoot `
                -MapPackage $Map `
                -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot `
                -IncludePresentation $false
            foreach ($scopeId in $layerScopePaths.Keys) {
                foreach ($path in $layerScopePaths[$scopeId]) {
                    if (Test-Path -LiteralPath $path) {
                        Remove-Item -LiteralPath $path -Recurse -Force
                    }
                }
            }
        }
        if ($modeName -eq 'apply') {
            # The commandlet has already removed every map/runtime HLOD
            # reference. Retire the old companion definitions inside the same
            # recoverable transaction before publishing the new map manifest.
            Remove-ProjectWorldGeneratedHLODArtifacts -ContentRoot $contentRoot `
                -MapPackage $Map `
                -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot
            foreach ($scopeId in $removedLayerScopes) {
                foreach ($path in $layerScopePaths[$scopeId]) {
                    if (Test-Path -LiteralPath $path) {
                        Remove-Item -LiteralPath $path -Recurse -Force
                    }
                }
            }
        }
        # The last map consumer and its shared presentation scope retire in
        # one recoverable transaction, so owner migration cannot leave an
        # orphaned generated profile behind.
        if ($modeName -eq 'delete' -and $retirePresentation) {
            foreach ($path in $presentationScopePaths) {
                if (Test-Path -LiteralPath $path) {
                    Remove-Item -LiteralPath $path -Recurse -Force
                }
            }
        }
        # Contract order: artifacts already replaced by the child; write
        # immutable candidate manifests; flip the journal to 'publishing'
        # with the expected hash; atomically replace the active set LAST.
        $inputIdentity = [ordered]@{
            compile_result_sha256 = $compileResultHash
            presentation_profile_sha256 = $presentationProfileHash
            runtime_profile_sha256 = $runtimeProfileHash
            authored_overlay_profile_sha256 = $authoredOverlayProfileHash
            map_package = $Map
        }
		$layerInputIdentity = New-ProjectWorldLayerInputIdentity `
			-OperationIdentity $inputIdentity
        $candidates = @()
        $retired = @($removedLayerScopes)
        $mapGeneratorFingerprint = Get-ProjectWorldGeneratorFingerprint `
            -ProjectRoot $projectRoot -ProducerId 'map:v1'
        $presentationGeneratorFingerprint = Get-ProjectWorldGeneratorFingerprint `
            -ProjectRoot $projectRoot -ProducerId 'presentation:v1'
        $priorPresentation = if ($null -ne $activeSet -and $null -ne $presentationScopeId -and
            $activeSet.Manifests.Contains($presentationScopeId)) {
            $activeSet.Manifests[$presentationScopeId]
        }
        else { $null }
        $priorPresentationIdentity = if ($null -ne $priorPresentation) {
            [ordered]@{
                compile_result_sha256 = [string]$priorPresentation.input_identity.compile_result_sha256
                presentation_profile_sha256 = [string]$priorPresentation.input_identity.presentation_profile_sha256
                runtime_profile_sha256 = [string]$priorPresentation.input_identity.runtime_profile_sha256
                authored_overlay_profile_sha256 = [string]$priorPresentation.input_identity.authored_overlay_profile_sha256
                map_package = [string]$priorPresentation.input_identity.map_package
            }
        }
        else { $null }
        $priorConsumers = if ($null -ne $priorPresentation) { @($priorPresentation.consumer_references) } else { @() }
        # The shared presentation scope is profile-owned: its identity never
        # binds to whichever map touched it, and its consumer set is the
        # union of every active consuming map.
        $presentationIdentity = [ordered]@{
            compile_result_sha256 = 'none'
            presentation_profile_sha256 = $presentationProfileHash
            runtime_profile_sha256 = 'none'
            authored_overlay_profile_sha256 = 'none'
            map_package = 'shared'
        }
        if ($modeName -eq 'delete') {
            $retired = @($mapScopeId) + @($layerScopePaths.Keys)
            if ($null -ne $presentationScopeId -and $mapScopeId -in $priorConsumers) {
                if ($retirePresentation) {
                    $retired += $presentationScopeId
                }
                else {
                    # Consumer removal is a new immutable presentation
                    # generation in the SAME transaction.
                    $candidates += , (New-ProjectWorldCandidateManifest `
                        -ProjectRoot $projectRoot -ScopeId $presentationScopeId `
                        -Generation $scopeGenerations[$presentationScopeId] `
                        -OwningLayer 'presentation' -OperationId $transactionId `
                        -InputIdentity $priorPresentationIdentity -ScopePaths $presentationScopePaths -GeneratorFingerprint $presentationGeneratorFingerprint `
                        -ConsumerReferences @($priorConsumers | Where-Object { $_ -ne $mapScopeId }))
                }
            }
        }
        else {
            # Re-expand the map scope paths: the preflight expansion predates
            # the child, which may have created the map and external roots.
            $mapScopePaths = @(Get-ProjectWorldGeneratedPaths -ContentRoot $contentRoot -MapPackage $Map -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot -IncludePresentation $false)
            $layerOwnedPaths = @{}
            foreach ($scopeId in $layerCandidatesByScope.Keys) {
                foreach ($record in @($layerCandidatesByScope[$scopeId].Records)) {
                    $layerOwnedPaths[[string]$record.path] = $true
                }
            }
            $mapRecords = @(Get-ProjectWorldScopeArtifactRecords `
                -ProjectRoot $projectRoot -ScopePaths $mapScopePaths | Where-Object {
                    -not $layerOwnedPaths.ContainsKey([string]$_.path)
                })
            $mapCandidate = New-ProjectWorldCandidateManifest `
                -ProjectRoot $projectRoot -ScopeId $mapScopeId `
                -Generation $scopeGenerations[$mapScopeId] `
                -OwningLayer 'map' -OperationId $transactionId `
                -InputIdentity $inputIdentity -ScopePaths @() -ArtifactRecords $mapRecords `
                -GeneratorFingerprint $mapGeneratorFingerprint
            $priorMap = if ($null -ne $activeSet -and $activeSet.Manifests.Contains($mapScopeId)) {
                $activeSet.Manifests[$mapScopeId]
            }
            else { $null }
            if (-not (Test-ProjectWorldManifestSemanticallyUnchanged `
                -PriorManifest $priorMap -CandidateManifest $mapCandidate `
                -GeneratorFingerprint $mapGeneratorFingerprint)) {
                $candidates += , $mapCandidate
            }
            foreach ($scopeId in $layerCandidatesByScope.Keys) {
                $candidateInfo = $layerCandidatesByScope[$scopeId]
                $layerProducerId = "$([string]$candidateInfo.Contract.generator_id):v$([int]$candidateInfo.Contract.generator_version)"
                $layerGeneratorFingerprint = Get-ProjectWorldGeneratorFingerprint `
                    -ProjectRoot $projectRoot -ProducerId $layerProducerId
                $layerCandidate = New-ProjectWorldCandidateManifest `
                    -ProjectRoot $projectRoot -ScopeId $scopeId `
                    -Generation $scopeGenerations[$scopeId] `
                    -OwningLayer ([string]$layerDefinitions[$scopeId].layer_id) `
                    -OperationId $transactionId -InputIdentity $layerInputIdentity `
                    -ScopePaths @() -ArtifactRecords $candidateInfo.Records `
                    -LayerContract $candidateInfo.Contract `
                    -ConsumerReferences @($mapScopeId) `
                    -GeneratorFingerprint $layerGeneratorFingerprint
                $priorLayer = if ($null -ne $activeSet -and $activeSet.Manifests.Contains($scopeId)) {
                    $activeSet.Manifests[$scopeId]
                }
                else { $null }
                if (-not (Test-ProjectWorldManifestSemanticallyUnchanged `
                    -PriorManifest $priorLayer -CandidateManifest $layerCandidate `
                    -GeneratorFingerprint $layerGeneratorFingerprint -CompareLayerContract)) {
                    $candidates += , $layerCandidate
                }
            }
            if ($null -ne $presentationScopeId) {
                $presentationCandidate = New-ProjectWorldCandidateManifest `
                    -ProjectRoot $projectRoot -ScopeId $presentationScopeId `
                    -Generation $scopeGenerations[$presentationScopeId] `
                    -OwningLayer 'presentation' -OperationId $transactionId `
                    -InputIdentity $presentationIdentity -ScopePaths $presentationScopePaths -GeneratorFingerprint $presentationGeneratorFingerprint `
                    -ConsumerReferences @(@($priorConsumers) + $mapScopeId | Sort-Object -Unique)
                # Unchanged means artifacts AND consumers AND identity equal;
                # any difference publishes a new immutable generation.
                $presentationUnchanged = $null -ne $priorPresentation -and
                    (($priorPresentation.artifacts | ConvertTo-Json -Depth 4) -eq
                     (($presentationCandidate.artifacts) | ConvertTo-Json -Depth 4)) -and
                    ((@($priorPresentation.consumer_references) -join ',') -eq
                     ($presentationCandidate.consumer_references -join ',')) -and
                    (($priorPresentation.input_identity | ConvertTo-Json) -eq
                     ($presentationCandidate.input_identity | ConvertTo-Json)) -and
                    ([string]$priorPresentation.generator_fingerprint -eq $presentationGeneratorFingerprint)
                if (-not $presentationUnchanged) {
                    $candidates += , $presentationCandidate
                }
            }
        }
        $journalPath = Join-Path $resolvedManifestRoot 'journal.json'
        $published = Publish-ProjectWorldActiveSet `
            -ManifestRoot $resolvedManifestRoot `
            -ProjectRoot $ProjectRoot `
            -TransactionId $transactionId `
            -OperationId $transactionId `
            -CandidateManifests $candidates `
            -RetiredScopeIds $retired `
            -PriorActiveSet $activeSet `
            -BeforeCommit {
                param($expectedSha)
                $journal = Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
                $journal.phase = 'publishing'
                $journal.expected_active_set_sha256 = $expectedSha
                Write-ProjectWorldTransactionJournal -ManifestRoot $resolvedManifestRoot -Journal $journal
            }
        # Evidence comes from the PUBLISHED active set, never from planned
        # generations: skipped candidates and carried scopes report their
        # actually-activated manifest path and hash.
        $route = if ($EnrollManifests -and $null -eq $activeSet) { 'enroll' }
        elseif ($Reconstruct) { 'reconstruct' }
        elseif ($modeName -eq 'delete') { 'delete' }
        else { 'apply' }
        $participatingIds = @($scopeGenerations.Keys)
        $manifestEvidence = [ordered]@{
            manifest_root = $resolvedManifestRoot
            route = $route
            active_set_sha256 = $published.Sha256
            prior_active_set_sha256 = $published.PriorSha256
            enrolled = ($route -eq 'enroll')
            scopes = @($published.Entries | Where-Object { $_.scope_id -in $participatingIds -or $_.scope_id -eq $presentationScopeId } | ForEach-Object {
                [ordered]@{
                    scope_id = $_.scope_id
                    manifest_path = $_.manifest_path
                    manifest_sha256 = $_.manifest_sha256
                }
            })
        }
        Write-ProjectWorldJson -Document $manifestEvidence -Path "$resultPath.manifests.json"
    }
    $transactionResult = Complete-ProjectWorldGeneratedTransaction `
        -ContentRoot $contentRoot `
        -MapPackage $Map `
        -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot `
        -Records $transactionRecords `
        -TransactionParent $transactionParent `
        -TransactionRoot $transactionRoot `
        -ResultPath $resultPath `
        -EngineExitCode $engineExitCode `
        -ChildStatus $childStatus
    # Journal removal is the final step: after this point there is no
    # in-flight transaction in either the accepted or rolled-back outcome.
    $journalFile = Join-Path $resolvedManifestRoot 'journal.json'
    if (Test-Path -LiteralPath $journalFile -PathType Leaf) {
        Remove-Item -LiteralPath $journalFile -Force
    }
    if ($null -ne $authorityLock) {
        $authorityLock.Dispose()
    }
    if ($null -ne $contentLock) {
        $contentLock.Dispose()
    }
    Write-Host "[WorldRealization] transaction=$($transactionResult.State)"
}
if ($null -ne $invocationFailure) {
    throw $invocationFailure
}

Write-Host "[WorldRealization] child_status=$($result.status) engine_exit=$engineExitCode"
Write-Host "[WorldRealization] roundtrip_m=$($result.coordinate_roundtrip_error_m)"
if ($engineExitCode -ne 0) {
    exit $engineExitCode
}
if ($result.status -ne "accepted") {
    exit 4
}
exit 0
