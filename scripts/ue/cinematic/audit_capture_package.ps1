# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PackageRoot,
    [Parameter(Mandatory = $true)][string]$ResultPath
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$resolvedPackage = (Resolve-Path -LiteralPath $PackageRoot).Path
$resolvedResult = [IO.Path]::GetFullPath($ResultPath)
$resultParent = Split-Path -Parent $resolvedResult
New-Item -ItemType Directory -Path $resultParent -Force | Out-Null

. (Join-Path $projectRoot 'scripts\config\Resolve-UEConfig.ps1')
$config = Resolve-UEConfig -ConfigDir (Join-Path $projectRoot 'scripts\config')
$unrealPak = Join-Path $config.UE_PATH 'Engine\Binaries\Win64\UnrealPak.exe'
if (-not (Test-Path -LiteralPath $unrealPak -PathType Leaf)) {
    throw 'Launcher UnrealPak.exe is unavailable.'
}

$forbiddenPattern = [regex]::new(
    '(?i)(ProjectCinematic|MovieRenderPipeline|MoviePipelineMaskRenderPass|' +
    'VirtualProduction[/\\]Takes|AppleProResMedia|Alis[/\\]Content[/\\]Cinematics)')
$allowedManifestMetadata = @(
    'Engine/Plugins/VirtualProduction/Takes/Takes.uplugin',
    'Engine/Plugins/VirtualProduction/Takes/Config/DefaultTakes.ini'
)
$manifestEntries = [Collections.Generic.List[object]]::new()
$metadataEntries = [Collections.Generic.List[object]]::new()
foreach ($name in @('Manifest_UFSFiles_Win64.txt', 'Manifest_NonUFSFiles_Win64.txt')) {
    $manifest = Get-ChildItem -LiteralPath $resolvedPackage -Recurse -File -Filter $name |
        Select-Object -First 1
    if (-not $manifest) {
        continue
    }
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $manifest.FullName) {
        ++$lineNumber
        $entry = [string]$line
        if ($forbiddenPattern.IsMatch($entry)) {
            $path = ($entry -split "`t", 2)[0].Replace('\', '/')
            $record = [pscustomobject]@{
                    manifest = $manifest.FullName.Substring($resolvedPackage.Length).TrimStart('\', '/')
                    line = $lineNumber
                    entry = $entry
                }
            if ($allowedManifestMetadata -contains $path) {
                $metadataEntries.Add($record)
            }
            else {
                $manifestEntries.Add($record)
            }
        }
    }
}

$looseEntries = @(
    Get-ChildItem -LiteralPath $resolvedPackage -Recurse -File |
        Where-Object {
            $relative = $_.FullName.Substring($resolvedPackage.Length).TrimStart('\', '/')
            $forbiddenPattern.IsMatch($relative)
        } |
        ForEach-Object {
            [pscustomobject]@{
                path = $_.FullName.Substring($resolvedPackage.Length).TrimStart('\', '/').Replace('\', '/')
                size_bytes = $_.Length
            }
        }
)

$ioStoreEntries = [Collections.Generic.List[object]]::new()
$listingHashes = [Collections.Generic.List[object]]::new()
$containers = @(Get-ChildItem -LiteralPath $resolvedPackage -Recurse -File -Filter '*.utoc' |
    Where-Object { $_.BaseName -ne 'global' })
foreach ($container in $containers) {
    $output = @(& $unrealPak $container.FullName -List 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "UnrealPak could not list $($container.FullName)."
    }
    $listingText = ([string[]]$output) -join "`n"
    $listingBytes = [Text.Encoding]::UTF8.GetBytes($listingText)
    $listingSha = [Security.Cryptography.SHA256]::Create()
    try {
        $listingDigest = ([BitConverter]::ToString(
                $listingSha.ComputeHash($listingBytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $listingSha.Dispose()
    }
    $listingHashes.Add([pscustomobject]@{
            container = $container.FullName.Substring($resolvedPackage.Length).TrimStart('\', '/').Replace('\', '/')
            listing_sha256 = $listingDigest
            listing_entry_count = $output.Count
        })
    $lineNumber = 0
    foreach ($line in $output) {
        ++$lineNumber
        $entry = [string]$line
        if ($forbiddenPattern.IsMatch($entry)) {
            $ioStoreEntries.Add([pscustomobject]@{
                    container = $container.Name
                    line = $lineNumber
                    entry = $entry
                })
        }
    }
}

$errors = [Collections.Generic.List[string]]::new()
if ($manifestEntries.Count -gt 0) {
    $errors.Add("Staged manifests contain $($manifestEntries.Count) capture-only entries.")
}
if ($looseEntries.Count -gt 0) {
    $errors.Add("Staged tree contains $($looseEntries.Count) capture-only loose files.")
}
if ($ioStoreEntries.Count -gt 0) {
    $errors.Add("IoStore contains $($ioStoreEntries.Count) capture-only entries.")
}
$accepted = $errors.Count -eq 0
$receipt = [ordered]@{
    schema_version = 1
    status = if ($accepted) { 'accepted' } else { 'rejected' }
    package_root = $resolvedPackage
    forbidden_pattern = $forbiddenPattern.ToString()
    allowed_manifest_metadata = @($metadataEntries)
    manifest_entries = @($manifestEntries)
    loose_entries = @($looseEntries)
    iostore_entries = @($ioStoreEntries)
    iostore_listing_receipts = @($listingHashes)
    errors = @($errors)
}
[IO.File]::WriteAllText(
    $resolvedResult,
    ($receipt | ConvertTo-Json -Depth 8) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))
Write-Output ($receipt | ConvertTo-Json -Depth 8 -Compress)
if (-not $accepted) {
    exit 1
}
