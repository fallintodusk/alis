# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Executable durable-authority audit for ProjectWorld generated content.
# One command, one receipt: active-set integrity, per-scope manifest and
# artifact verification, global ownership, unowned-file scan across the
# generated roots, and consumer-reference resolution. This is the
# post-enrollment gate of the frozen acceptance sequence (contract:
# Plugins/World/ProjectWorld/docs/territory_generation.md).
#
# Exit codes: 0 = accepted, 1 = rejected (receipt still written).

[CmdletBinding()]
param(
    [string]$ProjectRoot = '',
    [string]$WorldDataPlugin = 'ProjectWorld',
    [string]$ManifestRoot,
    [string[]]$GeneratedRoots,
    [string]$EvidencePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# $PSScriptRoot is not populated while param defaults are evaluated under
# -File, so resolve script-relative paths here (same pattern as the realize
# wrapper).
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDirectory 'generated_manifest.ps1')

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = (Resolve-Path (Join-Path $scriptDirectory '..\..\..')).Path
}
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
$worldDataRoots = Resolve-ProjectWorldDataRoots -ProjectRoot $ProjectRoot -PluginName $WorldDataPlugin
if (-not $ManifestRoot) { $ManifestRoot = $worldDataRoots.ManifestRoot }
if (-not $GeneratedRoots -or $GeneratedRoots.Count -eq 0) {
    # Must match every root the transaction helper treats as generated-owned
    # (Get-ProjectWorldGeneratedPaths), or the unowned scan silently misses a
    # whole external-package family.
    $GeneratedRoots = @(
        (Join-Path $worldDataRoots.ContentRoot 'Generated'),
        (Join-Path $worldDataRoots.ContentRoot '__ExternalActors__\Generated'),
        (Join-Path $worldDataRoots.ContentRoot '__ExternalObjects__\Generated')
    )
}
if (-not $EvidencePath) {
    $stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
    $EvidencePath = Join-Path $ProjectRoot "Saved\Validation\WorldAuthority\audit_$stamp.json"
}

$repoPrefix = $ProjectRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar

function ConvertTo-AuditRelativePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    $full = [System.IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($script:repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $full.Replace('\', '/')
    }
    return $full.Substring($script:repoPrefix.Length).Replace('\', '/')
}

$checks = [System.Collections.Generic.List[object]]::new()
$failures = [System.Collections.Generic.List[string]]::new()

function Add-AuditCheck {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][bool]$Passed,
        [Parameter(Mandatory = $true)][string]$Detail
    )
    $script:checks.Add([ordered]@{ name = $Name; passed = $Passed; detail = $Detail })
    if (-not $Passed) { $script:failures.Add("${Name}: $Detail") }
}

function Format-AuditProblems {
    param([Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$Problems)
    $shown = @($Problems | Select-Object -First 5)
    $suffix = if ($Problems.Count -gt 5) { " (+$($Problems.Count - 5) more)" } else { '' }
    return ($shown -join '; ') + $suffix
}

$activeSummary = $null
$scopeSummaries = @()
$currentFingerprint = $null
$contentLock = $null
$authorityLock = $null
try {
    # Serialize against every mutating operation; a read taken mid-transaction
    # would audit a tree no contract ever accepted.
    $contentLock = Enter-ProjectWorldContentLock -ProjectRoot $ProjectRoot
    $authorityLock = Enter-ProjectWorldAuthorityLock -ManifestRoot $ManifestRoot

    $journalPath = Join-Path $ManifestRoot 'journal.json'
    $settled = -not (Test-Path -LiteralPath $journalPath)
    Add-AuditCheck 'transaction_settled' $settled $(
        if ($settled) { 'No transaction journal is pending.' }
        else { 'A transaction journal is pending; run recover_generated_transaction.ps1 first.' })

    $active = $null
    if ($settled) {
        $active = Read-ProjectWorldActiveSet -ManifestRoot $ManifestRoot -ProjectRoot $ProjectRoot
        if ($null -eq $active) {
            Add-AuditCheck 'active_set_present' $false 'No durable active-manifest-set record is enrolled.'
        }
        else {
            Add-AuditCheck 'active_set_present' $true "Active set $($active.Sha256) with $(@($active.Manifests.Keys).Count) scopes."
            $activeSummary = [ordered]@{
                path = ConvertTo-AuditRelativePath -Path $active.Path
                sha256 = $active.Sha256
                transaction_id = [string]$active.Record.transaction_id
                scope_count = @($active.Manifests.Keys).Count
            }
        }
    }

    if ($null -ne $active) {
        # Read-ProjectWorldActiveSet already verified record grammar, manifest
        # hashes, and per-document structure; surface that as an explicit check.
        Add-AuditCheck 'manifests_valid' $true 'Every referenced scope manifest matches its recorded SHA-256 and the frozen document rules.'

        try {
            Test-ProjectWorldGlobalOwnership -Manifests $active.Manifests
            Add-AuditCheck 'global_ownership' $true 'No artifact path has two owners.'
        }
        catch {
            Add-AuditCheck 'global_ownership' $false $_.Exception.Message
        }

        $activeScopeIds = @($active.Manifests.Keys)
        $consumerProblems = [System.Collections.Generic.List[string]]::new()
        foreach ($scopeId in $activeScopeIds) {
            foreach ($consumer in @($active.Manifests[$scopeId].consumer_references)) {
                if ($activeScopeIds -notcontains [string]$consumer) {
                    $consumerProblems.Add("scope $scopeId references inactive consumer $consumer")
                }
            }
        }
        Add-AuditCheck 'consumer_references' ($consumerProblems.Count -eq 0) $(
            if ($consumerProblems.Count -eq 0) { 'Every consumer reference resolves to an active scope.' }
            else { Format-AuditProblems -Problems @($consumerProblems) })

        $currentFingerprint = Get-ProjectWorldGeneratorFingerprint -ProjectRoot $ProjectRoot
        $staleFingerprints = [System.Collections.Generic.List[string]]::new()
        foreach ($scopeId in $activeScopeIds) {
            if ([string]$active.Manifests[$scopeId].generator_fingerprint -ne $currentFingerprint) {
                $staleFingerprints.Add("scope $scopeId was accepted by a different generator")
            }
        }
        # This is the final pre-package authority proof: an accepted tree must
        # be reproducible by the CURRENT generator, so a stale fingerprint
        # fails closed rather than being reported as informational.
        Add-AuditCheck 'generator_fingerprint_current' ($staleFingerprints.Count -eq 0) $(
            if ($staleFingerprints.Count -eq 0) { "Every active manifest carries the current generator fingerprint $($currentFingerprint.Substring(0, 16))." }
            else { Format-AuditProblems -Problems @($staleFingerprints) })
        $ownedDigests = @{}
        $ownedScopes = @{}
        $artifactProblems = [System.Collections.Generic.List[string]]::new()
        $totalArtifacts = 0
        $totalBytes = [long]0
        foreach ($scopeId in $activeScopeIds) {
            $manifest = $active.Manifests[$scopeId]
            $scopeArtifacts = 0
            $scopeBytes = [long]0
            foreach ($artifact in @($manifest.artifacts)) {
                $relative = [string]$artifact.path
                $ownedDigests[$relative] = [string]$artifact.digest
                $ownedScopes[$relative] = $scopeId
                $fullPath = Join-Path $ProjectRoot ($relative.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
                if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
                    $artifactProblems.Add("missing artifact ($scopeId): $relative")
                    continue
                }
                $actual = Get-ProjectWorldFileSha256 -Path $fullPath
                if ($actual -ne [string]$artifact.digest) {
                    $artifactProblems.Add("drifted artifact ($scopeId): $relative")
                    continue
                }
                ++$scopeArtifacts
                $scopeBytes += (Get-Item -LiteralPath $fullPath).Length
            }
            $totalArtifacts += $scopeArtifacts
            $totalBytes += $scopeBytes
            # input_identity travels in the receipt so an acceptance step can
            # bind these manifests to the exact E2E evidence that produced
            # them, instead of trusting a run ID typed on the command line.
            $inputIdentity = [ordered]@{}
            foreach ($property in $manifest.input_identity.PSObject.Properties) {
                $inputIdentity[$property.Name] = [string]$property.Value
            }
            $manifestEntry = $active.Record.scopes | Where-Object { $_.scope_id -eq $scopeId }
            $scopeSummaries += [ordered]@{
                scope_id = $scopeId
                manifest_path = [string]$manifestEntry.manifest_path
                manifest_sha256 = [string]$manifestEntry.manifest_sha256
                input_identity = $inputIdentity
                consumer_references = @($manifest.consumer_references | ForEach-Object { [string]$_ })
                generation = [int]$manifest.generation
                owning_layer = [string]$manifest.owning_layer
                accepted_operation_id = [string]$manifest.accepted_operation_id
                artifact_count = @($manifest.artifacts).Count
                verified_artifact_count = $scopeArtifacts
                verified_artifact_bytes = $scopeBytes
                generator_fingerprint = [string]$manifest.generator_fingerprint
                generator_fingerprint_is_current = ([string]$manifest.generator_fingerprint -eq $currentFingerprint)
            }
        }
        Add-AuditCheck 'artifacts_intact' ($artifactProblems.Count -eq 0) $(
            if ($artifactProblems.Count -eq 0) { "$totalArtifacts artifacts verified byte-identical ($totalBytes bytes)." }
            else { Format-AuditProblems -Problems @($artifactProblems) })

        $unowned = [System.Collections.Generic.List[string]]::new()
        foreach ($root in $GeneratedRoots) {
            if (-not (Test-Path -LiteralPath $root -PathType Container)) { continue }
            foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Force) {
                $relative = ConvertTo-AuditRelativePath -Path $file.FullName
                if (-not $ownedDigests.ContainsKey($relative)) {
                    $unowned.Add($relative)
                }
            }
        }
        Add-AuditCheck 'unowned_scan' ($unowned.Count -eq 0) $(
            if ($unowned.Count -eq 0) { 'Every file under the generated roots is owned by exactly one active scope.' }
            else { Format-AuditProblems -Problems @($unowned) })
    }
}
catch {
    Add-AuditCheck 'authority_audit_execution' $false $_.Exception.Message
}
finally {
    if ($null -ne $authorityLock) { $authorityLock.Dispose() }
    if ($null -ne $contentLock) { $contentLock.Dispose() }
}

$status = if ($failures.Count -eq 0) { 'accepted' } else { 'rejected' }
$receipt = [ordered]@{
    schema_version = 1
    audit = 'project_world_generated_authority'
    generated_utc = [DateTime]::UtcNow.ToString('o')
    status = $status
    manifest_root = ConvertTo-AuditRelativePath -Path $ManifestRoot
    generated_roots = @($GeneratedRoots | ForEach-Object { ConvertTo-AuditRelativePath -Path $_ })
    generator_fingerprint_current = $currentFingerprint
    active_set = $activeSummary
    scopes = @($scopeSummaries)
    checks = @($checks)
    failures = @($failures)
}
Write-ProjectWorldJson -Document $receipt -Path $EvidencePath

Write-Host "Authority audit: $status"
foreach ($check in $checks) {
    $marker = if ($check.passed) { '[OK]' } else { '[X] ' }
    Write-Host "  $marker $($check.name): $($check.detail)"
}
Write-Host "Receipt: $EvidencePath"
if ($status -ne 'accepted') { exit 1 }
exit 0
