#Requires -Version 5.1
# License terms: see repository root LICENSE.

function Get-ProjectReleaseDefinitionAssets {
    param([string]$Root)

    $contract = Get-Content -LiteralPath (Join-Path $Root `
        'scripts\git\mirror\developer_asset_release.json') -Raw | ConvertFrom-Json
    $assets = [Collections.Generic.List[object]]::new()
    foreach ($authority in @($contract.asset_authorities | Where-Object {
            [string]$_.authority_kind -ceq 'generated_definition_manifest'
        })) {
        $manifestPath = ConvertTo-ProjectIsolatedRelativePath -ProjectRoot $Root `
            -Path ([string]$authority.manifest_path)
        $manifest = Get-Content -LiteralPath (Join-Path $Root $manifestPath) -Raw | ConvertFrom-Json
        foreach ($asset in @($manifest.assets)) {
            $path = ConvertTo-ProjectIsolatedRelativePath -ProjectRoot $Root `
                -Path ([string]$asset.artifact_path)
            if (-not $path.EndsWith('.uasset', [StringComparison]::Ordinal) -or
                [string]$asset.artifact_sha256 -notmatch '^[0-9a-f]{64}$') {
                throw "Invalid release definition artifact: $path"
            }
            $assets.Add([pscustomobject]@{
                path = $path
                sha256 = [string]$asset.artifact_sha256
            })
        }
    }
    return @($assets)
}

function Assert-ProjectReleaseDefinitionAssets {
    param([string]$Root, [object[]]$Assets)

    foreach ($asset in $Assets) {
        $file = Join-Path $Root ([string]$asset.path)
        if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or
            (Get-ProjectIsolatedFileSha256 -Path $file) -cne [string]$asset.sha256) {
            throw "Release definition artifact differs from its accepted manifest: $($asset.path)"
        }
    }
}
