#Requires -Version 5.1

param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,

    [Parameter(Mandatory = $true)]
    [string]$RequiredPackage,

    [Parameter(Mandatory = $true)]
    [string]$ResultPath,

    [string]$EngineRoot
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
if (-not $EngineRoot) {
    . (Join-Path $ProjectRoot "scripts\config\Resolve-UEConfig.ps1")
    $config = Resolve-UEConfig -ConfigDir (Join-Path $ProjectRoot "scripts\config")
    $EngineRoot = $config.UE_PATH
}

$UnrealPak = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealPak.exe"
if (-not (Test-Path -LiteralPath $UnrealPak -PathType Leaf)) {
    throw "UnrealPak.exe is unavailable under the configured engine."
}
$resolvedPackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$containers = @(
    Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -Filter *.utoc -File |
        Where-Object { $_.BaseName -ne 'global' }
)
if ($containers.Count -eq 0) {
    throw "No IoStore containers exist under the package root."
}
if ($RequiredPackage -notmatch '^/(?<Plugin>[A-Za-z][A-Za-z0-9_]*)/Generated/[A-Za-z0-9_/-]+$') {
    throw "RequiredPackage is outside a generated plugin mount."
}

$pluginName = $Matches.Plugin
$relativePackage = $RequiredPackage.Substring(("/$pluginName/").Length)
$expectedSuffix = (
    "Alis/Plugins/World/{0}/Content/{1}.umap" -f $pluginName, $relativePackage
).Replace('\', '/')
$resolvedResult = [System.IO.Path]::GetFullPath($ResultPath)
$resultParent = Split-Path -Parent $resolvedResult
New-Item -ItemType Directory -Path $resultParent -Force | Out-Null
$entries = @()
$listingLogs = @()
foreach ($container in $containers) {
    $output = @(& $UnrealPak $container.FullName -List 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "UnrealPak failed to list $($container.FullName)."
    }
    $listingLog = Join-Path $resultParent ("{0}.list.log" -f $container.BaseName)
    [System.IO.File]::WriteAllLines(
        [System.IO.Path]::GetFullPath($listingLog),
        [string[]]$output,
        [System.Text.UTF8Encoding]::new($false))
    $listingLogs += [System.IO.Path]::GetFileName($listingLog)
    foreach ($line in $output) {
        if ($line -notmatch '"(?<path>[^"]+)" offset:\s*(?<offset>\d+), size:\s*(?<size>\d+) bytes, hash:\s*(?<hash>[0-9a-f]+), compression:\s*(?<compression>[^.]+)\.') {
            continue
        }
        $relativeContainer = $container.FullName.Substring($resolvedPackageRoot.Length).TrimStart('\', '/').Replace('\', '/')
        $entries += [pscustomobject]@{
            container = $relativeContainer
            path = $Matches.path.Replace('\', '/')
            offset = [int64]$Matches.offset
            size_bytes = [int64]$Matches.size
            hash = $Matches.hash
            compression = $Matches.compression.Trim()
        }
    }
}

$requiredEntries = @($entries | Where-Object { $_.path.EndsWith($expectedSuffix, [System.StringComparison]::OrdinalIgnoreCase) })
$projectWorldEntries = @($entries | Where-Object { $_.path -match '/Plugins/World/ProjectWorld(?:TestData|Data)?/' })
$testDataEntries = @($entries | Where-Object { $_.path -match '/Plugins/World/ProjectWorldTestData/' })
$accepted = $requiredEntries.Count -eq 1 -and $testDataEntries.Count -eq 0
$receiptErrors = @()
if (-not $accepted) {
    if ($requiredEntries.Count -ne 1) {
        $receiptErrors += "Expected exactly one required map entry; found $($requiredEntries.Count)."
    }
    if ($testDataEntries.Count -ne 0) {
        $receiptErrors += "Editor-only ProjectWorldTestData leaked into IoStore; found $($testDataEntries.Count) entries."
    }
}
$receipt = [ordered]@{
    '$schema' = 'https://alis.world/schemas/iostore/inspection-result-v1.json'
    schema_version = 1
    status = if ($accepted) { 'accepted' } else { 'rejected' }
    required_package = $RequiredPackage
    expected_entry_suffix = $expectedSuffix
    entries = @($requiredEntries)
    listed_entry_count = $entries.Count
    project_world_entry_count = $projectWorldEntries.Count
    project_world_listed_bytes = [int64](($projectWorldEntries | Measure-Object -Property size_bytes -Sum).Sum)
    project_world_test_data_entry_count = $testDataEntries.Count
    largest_entries = @($entries | Sort-Object size_bytes -Descending | Select-Object -First 20)
    listing_logs = $listingLogs
    errors = @($receiptErrors)
}
[System.IO.File]::WriteAllText(
    $resolvedResult,
    ($receipt | ConvertTo-Json -Depth 6) + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false))

Write-Output ($receipt | ConvertTo-Json -Depth 6 -Compress)
if (-not $accepted) {
    exit 1
}
