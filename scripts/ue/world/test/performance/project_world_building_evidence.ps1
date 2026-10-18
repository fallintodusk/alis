#Requires -Version 5.1
# License terms: see repository root LICENSE.

function New-ProjectWorldBuildingReference {
    param(
        [Parameter(Mandatory)][string]$ProjectRoot,
        [Parameter(Mandatory)][string]$MapPackage,
        [Parameter(Mandatory)][string]$RuntimeProfileHash,
        [Parameter(Mandatory)][string]$RealizationProfile,
        [Parameter(Mandatory)][string]$EvidenceRoot
    )
    $path = Join-Path $EvidenceRoot 'building-surface-reference.json'
    & (Join-Path $ProjectRoot 'scripts/ue/world/test/integration/verify_saved_world_projection.ps1') `
        -Map $MapPackage -RealizationProfile $RealizationProfile -WorldDataPlugin ProjectWorldData `
        -EvidencePath (Join-Path $EvidenceRoot 'saved-building-projection.json') `
        -BuildingSamplesPath $path -RuntimeProfileSha256 $RuntimeProfileHash
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw 'Current saved building surface reference was not produced.'
    }
    return [IO.Path]::GetFullPath($path)
}

function Assert-ProjectWorldBuildingEvidence {
    param([Parameter(Mandatory)]$Receipt)
    $proof = $Receipt.building_surface_evidence
    # UE writes round-trip double precision; Windows PowerShell can parse it as Decimal.
    if ($null -eq $proof -or $proof.schema -cne 'project-world-building-surface-evidence:v2' -or
        $proof.status -cne 'accepted' -or $proof.reference_sha256 -cnotmatch '^[a-f0-9]{64}$' -or
        [double]$proof.reference_tolerance_cm -ne 0.05 -or $null -eq $proof.maximum_reference_error_cm -or
        [double]::IsNaN([double]$proof.maximum_reference_error_cm) -or
        [double]::IsInfinity([double]$proof.maximum_reference_error_cm) -or
        [double]$proof.maximum_reference_error_cm -lt 0 -or [double]$proof.maximum_reference_error_cm -gt 0.05 -or
        $null -eq $proof.maximum_source_error_cm -or [double]::IsNaN([double]$proof.maximum_source_error_cm) -or
        [double]::IsInfinity([double]$proof.maximum_source_error_cm) -or [double]$proof.maximum_source_error_cm -lt 0) {
        throw 'Named building wall/roof evidence is missing or failed its current-reference contract.'
    }
    if ($proof.capsule_query -cne 'UWorld::ComponentSweepMulti' -or
        $proof.capsule_pawn_class -cne '/Script/ProjectCharacter.DefinitionCharacter' -or $proof.capsule_query_channel -ne 2 -or
        $null -eq $proof.capsule_radius_cm -or $null -eq $proof.capsule_half_height_cm -or
        [double]::IsNaN([double]$proof.capsule_radius_cm) -or [double]::IsInfinity([double]$proof.capsule_radius_cm) -or
        [double]::IsNaN([double]$proof.capsule_half_height_cm) -or [double]::IsInfinity([double]$proof.capsule_half_height_cm) -or
        [double]$proof.capsule_radius_cm -le 0 -or [double]$proof.capsule_half_height_cm -lt [double]$proof.capsule_radius_cm) {
        throw 'Named building evidence requires the actual possessed capsule query.'
    }
    $samples = @($proof.samples)
    $keys = @($samples | ForEach-Object { '{0}|{1}' -f $_.mesh, $_.kind } | Sort-Object -Unique)
    if ($samples.Count -ne 6 -or $keys.Count -ne 6 -or
        @($samples | Where-Object kind -CEQ wall).Count -ne 3 -or
        @($samples | Where-Object kind -CEQ roof).Count -ne 3 -or
        @($samples.mesh | Sort-Object -Unique).Count -ne 3) {
        throw 'Named building evidence requires a wall and roof on each of three source meshes.'
    }
    foreach ($sample in $samples) {
        if ($sample.ray_query_status -cne 'hit_named_component' -or $sample.ray_query_returned_hit -isnot [bool] -or
            -not $sample.ray_query_returned_hit -or $null -eq $sample.reference_error_cm -or [double]::IsNaN([double]$sample.reference_error_cm) -or
            [double]::IsInfinity([double]$sample.reference_error_cm) -or [double]$sample.reference_error_cm -lt 0 -or
            [double]$sample.reference_error_cm -gt 0.05 -or @($sample.cooked_hit_cm).Count -ne 3 -or
            $sample.mesh -cnotlike '/ProjectWorldData/Generated/*/Buildings/*' -or
            $null -eq $sample.source_triangle -or [int]$sample.source_triangle -lt 0) {
            throw 'A named building wall/roof ray has missing or invalid cooked-reference evidence.'
        }
        if ($sample.capsule_query_status -cne 'blocked_named_component' -or $sample.capsule_hit_mesh -cne $sample.mesh -or
            $sample.capsule_blocking_hit -isnot [bool] -or -not $sample.capsule_blocking_hit -or
            $sample.capsule_start_penetrating -isnot [bool] -or $sample.capsule_start_penetrating -or
            $null -eq $sample.capsule_hit_count -or $sample.capsule_hit_count -lt 1 -or
            $sample.capsule_other_blocking_hit_count -ne 0 -or $sample.capsule_initial_penetration_count -ne 0 -or
            $null -eq $sample.capsule_hit_time -or [double]::IsNaN([double]$sample.capsule_hit_time) -or
            [double]::IsInfinity([double]$sample.capsule_hit_time) -or $sample.capsule_hit_time -lt 0 -or $sample.capsule_hit_time -gt 1) {
            throw 'A named building capsule contact is absent, penetrating or blocked by another component.'
        }
        foreach ($field in @('cooked_hit_normal', 'capsule_hit_cm', 'capsule_hit_normal')) {
            $vector = @($sample.$field)
            if ($vector.Count -ne 3) { throw 'A measured building capsule/ray vector is absent.' }
            $squaredLength = 0.0
            foreach ($value in $vector) {
                if ($null -eq $value -or $value -is [bool] -or [double]::IsNaN([double]$value) -or [double]::IsInfinity([double]$value)) {
                    throw 'A measured building capsule/ray vector is invalid.'
                }
                $squaredLength += [double]$value * [double]$value
            }
            if ($field -ne 'capsule_hit_cm' -and [Math]::Abs($squaredLength - 1) -gt 0.0001) {
                throw 'A measured building contact normal is invalid.'
            }
        }
        foreach ($field in @('capsule_source_plane_error_cm', 'capsule_reference_plane_error_cm')) {
            if ($null -eq $sample.$field -or [double]::IsNaN([double]$sample.$field) -or
                [double]::IsInfinity([double]$sample.$field) -or [double]$sample.$field -lt 0) {
                throw 'A measured building capsule plane bias is missing or invalid.'
            }
        }
    }
}
