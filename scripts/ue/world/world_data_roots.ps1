# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Resolve-ProjectWorldDataRoots {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$PluginName
    )
    if ($PluginName -notmatch '^Project[A-Za-z0-9]+$') {
        throw "World-data owner is not a valid Project* plugin token: $PluginName"
    }
    $pluginsRoot = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot 'Plugins'))
    $matches = @(Get-ChildItem -LiteralPath $pluginsRoot -Filter "$PluginName.uplugin" -Recurse -File |
        Where-Object { $_.BaseName -ceq $PluginName })
    if ($matches.Count -ne 1) {
        throw "Expected exactly one world-data plugin descriptor for '$PluginName'; found $($matches.Count)."
    }
    $descriptor = Get-Content -LiteralPath $matches[0].FullName -Raw | ConvertFrom-Json
    if ($descriptor.CanContainContent -ne $true) {
        throw "World-data plugin must set CanContainContent=true: $PluginName"
    }
    $pluginRoot = [System.IO.Path]::GetFullPath($matches[0].DirectoryName)
    if (-not ($pluginRoot + [System.IO.Path]::DirectorySeparatorChar).StartsWith(
        $pluginsRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "World-data plugin escapes the project Plugins root: $PluginName"
    }
    return [pscustomobject]@{
        PluginName = $PluginName
        MountRoot = "/$PluginName/"
        GeneratedPackageRoot = "/$PluginName/Generated/"
        AuthoredPackageRoot = "/$PluginName/Authored/"
        PluginRoot = $pluginRoot
        ContentRoot = Join-Path $pluginRoot 'Content'
        DataRoot = Join-Path $pluginRoot 'Data'
        ManifestRoot = Join-Path $pluginRoot 'Data\Manifests'
    }
}
