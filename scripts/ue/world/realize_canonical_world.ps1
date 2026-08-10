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

    [string]$EvidencePath = "",

	[ValidateRange(0, 1000)]
	[int]$MaxRoads = 1,

	[ValidateRange(0, 1000)]
	[int]$MaxBuildings = 4,

    [switch]$RequireLandscape,

    # Manifest authority root; defaults to the durable root. Isolated
    # validation runs pass a transient sandbox root and never touch the
    # durable authority.
    [string]$ManifestRoot = "",

    # One-time enrollment of scopes that have no accepted manifest yet.
    # Without this switch, a missing active set or missing scope refuses.
    [switch]$EnrollManifests,

    # Explicit clean reconstruction (contract route 2). Requires accepted
    # manifests for the declared scope set; each participating scope must be
    # fully absent or exactly match its accepted state - partial absence or
    # mixed unknown content rejects. Absence alone NEVER authorizes
    # regeneration; this named flag does.
    [switch]$Reconstruct
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
    if ($WorldDataPlugin -cne 'ProjectWorld') {
        throw 'Production realization requires an explicit generated map package.'
    }
    $Map = '/ProjectWorld/Generated/P0/L_ProjectWorldSynthetic'
}
$presentationProfilePath = if ($modeName -eq "delete") {
    ""
}
elseif (-not [string]::IsNullOrWhiteSpace($PresentationProfile)) {
    [System.IO.Path]::GetFullPath($PresentationProfile)
}
elseif ($WorldDataPlugin -ceq 'ProjectWorld') {
    Join-Path $worldDataRoots.DataRoot 'Presentation\kazan_representative_v1.json'
}
else {
    throw 'Production realization requires an explicit world-data-owned presentation profile.'
}
if ($modeName -ne "delete" -and -not (Test-Path -LiteralPath $presentationProfilePath -PathType Leaf)) {
    throw "Presentation profile does not exist: $presentationProfilePath"
}
if (-not [string]::IsNullOrWhiteSpace($runtimeProfilePath) -and
	-not (Test-Path -LiteralPath $runtimeProfilePath -PathType Leaf)) {
	throw "Runtime profile does not exist: $runtimeProfilePath"
}
$dataRootPrefix = $worldDataRoots.DataRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
foreach ($profilePath in @($presentationProfilePath, $runtimeProfilePath)) {
    if (-not [string]::IsNullOrWhiteSpace($profilePath) -and
        -not $profilePath.StartsWith($dataRootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "World realization profile must belong to $($worldDataRoots.PluginName) Data: $profilePath"
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
if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $identity = [ordered]@{
        schema_version = 1
        compile_result_sha256 = $compileResultHash
        presentation_profile_sha256 = $presentationProfileHash
		runtime_profile_sha256 = $runtimeProfileHash
        map = $Map
        max_roads = $MaxRoads
        max_buildings = $MaxBuildings
        require_landscape = [bool]$RequireLandscape
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
$mapScopeId = Get-ProjectWorldMapScopeId -MapPackage $Map -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot
$presentationScopeId = $null
if ($modeName -eq "apply") {
    $presentationProfileId = (Get-Content -LiteralPath $presentationProfilePath -Raw | ConvertFrom-Json).profile_id
    $presentationScopeId = Get-ProjectWorldPresentationScopeId -ProfileId $presentationProfileId
}
$mapScopePaths = @(Get-ProjectWorldGeneratedPaths -ContentRoot $contentRoot -MapPackage $Map -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot -IncludePresentation $false)
$presentationScopePaths = @(Join-Path $contentRoot 'Generated\Presentation')
$activeSet = $null
$scopeGenerations = @{}
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
    if ($modeName -eq 'delete' -and $null -ne $activeSet) {
        # A map delete also mutates any shared presentation scope that
        # lists this map as a consumer (consumer removal, same transaction).
        foreach ($scopeId in @($activeSet.Manifests.Keys)) {
            if ($scopeId -like 'presentation_*' -and
                $mapScopeId -in @($activeSet.Manifests[$scopeId].consumer_references)) {
                $presentationScopeId = $scopeId
                $participating[$presentationScopeId] = $presentationScopePaths
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
        foreach ($scopeId in @($participating.Keys)) {
            if (-not $activeSet.Manifests.Contains($scopeId) -and -not $EnrollManifests) {
                Write-Host "[WorldRealization] REFUSED: scope '$scopeId' has no accepted manifest. Enrollment via -EnrollManifests is required."
                exit 7
            }
        }
        if ($Reconstruct) {
            # Explicit clean reconstruction: each declared scope must be
            # fully absent or exactly match its accepted manifest.
            foreach ($scopeId in @($participating.Keys)) {
                if (-not $activeSet.Manifests.Contains($scopeId)) { continue }
                $current = @(Get-ProjectWorldScopeArtifactRecords -ProjectRoot $projectRoot -ScopePaths $participating[$scopeId])
                if ($current.Count -eq 0) { continue }
                Test-ProjectWorldScopeDrift -ProjectRoot $projectRoot -ActiveSet $activeSet -ScopePathsById @{ $scopeId = $participating[$scopeId] }
            }
        }
        else {
            Test-ProjectWorldScopeDrift -ProjectRoot $projectRoot -ActiveSet $activeSet -ScopePathsById $participating
        }
    }
    # A Delete must resolve its exact prior authority BEFORE any mutation;
    # retiring an unowned scope is refused, never recorded as empty evidence.
    $retiredJournalEntries = @()
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
    }
    foreach ($scopeId in @($participating.Keys)) {
        $prior = if ($null -ne $activeSet -and $activeSet.Manifests.Contains($scopeId)) {
            [int]$activeSet.Manifests[$scopeId].generation
        }
        else { 0 }
        $scopeGenerations[$scopeId] = $prior + 1
    }
    $transactionRecords = @(New-ProjectWorldGeneratedSnapshot `
        -ContentRoot $contentRoot `
        -MapPackage $Map `
        -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot `
        -SnapshotRoot $transactionRoot)
    $plannedCandidates = @($scopeGenerations.Keys | ForEach-Object { "scopes/$_.$($scopeGenerations[$_]).json" })
    Write-ProjectWorldTransactionJournal -ManifestRoot $resolvedManifestRoot -Journal ([ordered]@{
        transaction_id = $transactionId
        phase = 'mutating'
        map_package = $Map
        snapshot_root = $transactionRoot
        snapshot_records = @($transactionRecords | ForEach-Object { [ordered]@{ source = $_.Source; backup = $_.Backup } })
        candidate_manifest_paths = $plannedCandidates
        expected_active_set_sha256 = ''
        prior_active_set_sha256 = $(if ($null -ne $activeSet) { $activeSet.Sha256 } else { 'none' })
        mutation_scope_ids = @($participating.Keys | Sort-Object)
        operation = $(if ($EnrollManifests -and $null -eq $activeSet) { 'enroll' }
            elseif ($Reconstruct) { 'reconstruct' }
            elseif ($modeName -eq 'delete') { 'delete' }
            else { 'apply' })
        retired_scopes = $retiredJournalEntries
    })
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
    "-NullRHI"
    "-FullStdOutLogOutput"
)
if ($modeName -ne "delete") {
    $unrealArguments += "-PresentationProfile=$presentationProfilePath"
}
if (-not [string]::IsNullOrWhiteSpace($runtimeProfilePath)) {
	$unrealArguments += "-RuntimeProfile=$runtimeProfilePath"
}
if ($RequireLandscape) {
    $unrealArguments += "-RequireLandscape"
}

Write-Host "[WorldRealization] mode=$modeName"
Write-Host "[WorldRealization] input=$compileResultPath"
Write-Host "[WorldRealization] presentation=$($presentationProfilePath.Trim())"
Write-Host "[WorldRealization] runtime=$($runtimeProfilePath.Trim())"
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
        # Contract order: artifacts already replaced by the child; write
        # immutable candidate manifests; flip the journal to 'publishing'
        # with the expected hash; atomically replace the active set LAST.
        $inputIdentity = [ordered]@{
            compile_result_sha256 = $compileResultHash
            presentation_profile_sha256 = $presentationProfileHash
            runtime_profile_sha256 = $runtimeProfileHash
            map_package = $Map
        }
        $candidates = @()
        $retired = @()
        $generatorFingerprint = Get-ProjectWorldGeneratorFingerprint -ProjectRoot $projectRoot
        $priorPresentation = if ($null -ne $activeSet -and $null -ne $presentationScopeId -and
            $activeSet.Manifests.Contains($presentationScopeId)) {
            $activeSet.Manifests[$presentationScopeId]
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
            map_package = 'shared'
        }
        if ($modeName -eq 'delete') {
            $retired = @($mapScopeId)
            if ($null -ne $presentationScopeId -and $mapScopeId -in $priorConsumers) {
                # Consumer removal is a new immutable presentation generation
                # in the SAME transaction, even with unchanged bytes.
                $candidates += , (New-ProjectWorldCandidateManifest `
                    -ProjectRoot $projectRoot -ScopeId $presentationScopeId `
                    -Generation $scopeGenerations[$presentationScopeId] `
                    -OwningLayer 'presentation' -OperationId $transactionId `
                    -InputIdentity $presentationIdentity -ScopePaths $presentationScopePaths -GeneratorFingerprint $generatorFingerprint `
                    -ConsumerReferences @($priorConsumers | Where-Object { $_ -ne $mapScopeId }))
            }
        }
        else {
            # Re-expand the map scope paths: the preflight expansion predates
            # the child, which may have created the map and external roots.
            $mapScopePaths = @(Get-ProjectWorldGeneratedPaths -ContentRoot $contentRoot -MapPackage $Map -GeneratedPackageRoot $worldDataRoots.GeneratedPackageRoot -IncludePresentation $false)
            $candidates += , (New-ProjectWorldCandidateManifest `
                -ProjectRoot $projectRoot -ScopeId $mapScopeId `
                -Generation $scopeGenerations[$mapScopeId] `
                -OwningLayer 'map' -OperationId $transactionId `
                -InputIdentity $inputIdentity -ScopePaths $mapScopePaths -GeneratorFingerprint $generatorFingerprint)
            if ($null -ne $presentationScopeId) {
                $presentationCandidate = New-ProjectWorldCandidateManifest `
                    -ProjectRoot $projectRoot -ScopeId $presentationScopeId `
                    -Generation $scopeGenerations[$presentationScopeId] `
                    -OwningLayer 'presentation' -OperationId $transactionId `
                    -InputIdentity $presentationIdentity -ScopePaths $presentationScopePaths -GeneratorFingerprint $generatorFingerprint `
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
                    ([string]$priorPresentation.generator_fingerprint -eq $generatorFingerprint)
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
