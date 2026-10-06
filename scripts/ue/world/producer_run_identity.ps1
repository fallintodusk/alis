# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function New-ProjectWorldRunIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$LayerDefinitions,
        [Parameter(Mandatory = $true)][ValidateSet('apply', 'delete', 'validate')][string]$Mode
    )

    $fingerprints = [ordered]@{}
    $layerFingerprints = [ordered]@{}
    if ($Mode -eq 'validate') {
        return [pscustomobject]@{ Fingerprints = $fingerprints; LayerFingerprints = $layerFingerprints }
    }
    $catalog = Get-ProjectWorldProducerCatalog -ProjectRoot $ProjectRoot
    $engineIdentity = Get-ProjectWorldEngineBuildIdentity -ProjectRoot $ProjectRoot
    $producerIds = if ($Mode -eq 'apply') { @('map:v1', 'presentation:v1') } else { @('presentation:v1') }
    if ($Mode -eq 'apply') {
        foreach ($scopeId in $LayerDefinitions.Keys) {
            $definition = $LayerDefinitions[$scopeId]
            $producerId = "$([string]$definition.generator_id):v$([int]$definition.generator_version)"
            $producerIds += $producerId
        }
    }
    foreach ($producerId in @($producerIds | Sort-Object -Unique)) {
        $fingerprints[$producerId] = Get-ProjectWorldGeneratorFingerprint `
            -ProjectRoot $ProjectRoot -ProducerId $producerId `
            -Catalog $catalog -EngineIdentity $engineIdentity
    }
    if ($Mode -eq 'apply') {
        foreach ($scopeId in $LayerDefinitions.Keys) {
            $definition = $LayerDefinitions[$scopeId]
            $producerId = "$([string]$definition.generator_id):v$([int]$definition.generator_version)"
            $layerFingerprints[$scopeId] = $fingerprints[$producerId]
        }
    }
    return [pscustomobject]@{ Fingerprints = $fingerprints; LayerFingerprints = $layerFingerprints }
}
