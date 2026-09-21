#Requires -Version 5.1

param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,

    [string]$RequiredPackage,

    [Parameter(Mandatory = $true)]
    [string]$ResultPath,

    [Parameter(Mandatory = $true)]
    [string]$ObjectContractPath,

    [string]$EngineRoot
)

$ErrorActionPreference = "Stop"

function Get-ProjectContentEntrySuffix {
    param(
        [Parameter(Mandatory = $true)][string]$VirtualPath,
        [Parameter(Mandatory = $true)][string]$Root
    )

    if ($VirtualPath -notmatch '^/(?<Mount>[A-Za-z][A-Za-z0-9_]*)/(?<Relative>[^.]+)(?:\..*)?$') {
        return $null
    }

    $mount = $Matches.Mount
    $relative = $Matches.Relative
    if ($mount -ceq 'Game') {
        return "Alis/Content/$relative.uasset"
    }

    $descriptor = Get-ChildItem -LiteralPath (Join-Path $Root 'Plugins') `
        -Recurse -Filter "$mount.uplugin" -File -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $descriptor) {
        return $null
    }

    $pluginRoot = Split-Path -Parent $descriptor.FullName
    $relativePluginRoot = $pluginRoot.Substring($Root.Length).TrimStart('\', '/').Replace('\', '/')
    return "Alis/$relativePluginRoot/Content/$relative.uasset"
}

function Get-ObjectDefinitionShippingContract {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$ContractPath
    )

    if (-not (Test-Path -LiteralPath $ContractPath -PathType Leaf)) {
        throw "Generated ObjectDefinition cook contract is missing: $ContractPath"
    }
    try {
        $contract = Get-Content -LiteralPath $ContractPath -Raw | ConvertFrom-Json
    } catch {
        throw "Generated ObjectDefinition cook contract cannot be parsed: $ContractPath"
    }
    if ([int]$contract.schema_version -ne 1) {
        throw "Generated ObjectDefinition cook contract has an unsupported schema."
    }

    $definitionSuffixes = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    $referenceSuffixes = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)

    foreach ($asset in @($contract.assets)) {
        if ([string]$asset.asset_class -cne 'ObjectDefinition') {
            continue
        }

        $definitionSuffix = Get-ProjectContentEntrySuffix `
            -VirtualPath ([string]$asset.package_name) -Root $Root
        if (-not $definitionSuffix) {
            throw "Generated ObjectDefinition package cannot be mapped to Shipping: $($asset.package_name)"
        }
        $null = $definitionSuffixes.Add($definitionSuffix)

        foreach ($reference in @($asset.capability_cook_references)) {
            $suffix = Get-ProjectContentEntrySuffix `
                -VirtualPath ([string]$reference) -Root $Root
            if (-not $suffix) {
                throw "Capability cook reference cannot be mapped to Shipping: $reference"
            }
            $null = $referenceSuffixes.Add($suffix)
        }
    }

    if ($definitionSuffixes.Count -eq 0) {
        throw "Generated ObjectDefinition cook contract contains no ObjectDefinitions."
    }

    return [pscustomobject]@{
        DefinitionSuffixes = @($definitionSuffixes)
        ReferenceSuffixes = @($referenceSuffixes)
    }
}

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
$expectedSuffix = $null
if ($RequiredPackage) {
    if ($RequiredPackage -notmatch '^/(?<Plugin>[A-Za-z][A-Za-z0-9_]*)/Generated/[A-Za-z0-9_/-]+$') {
        throw "RequiredPackage is outside a generated plugin mount."
    }

    $pluginName = $Matches.Plugin
    $relativePackage = $RequiredPackage.Substring(("/$pluginName/").Length)
    $expectedSuffix = (
        "Alis/Plugins/World/{0}/Content/{1}.umap" -f $pluginName, $relativePackage
    ).Replace('\', '/')
}
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

$requiredEntries = @(if ($expectedSuffix) {
    $entries | Where-Object { $_.path.EndsWith($expectedSuffix, [System.StringComparison]::OrdinalIgnoreCase) }
})
$projectWorldEntries = @($entries | Where-Object { $_.path -match '/Plugins/World/ProjectWorld(?:TestData|Data)?/' })
$testDataEntries = @($entries | Where-Object { $_.path -match '/Plugins/World/ProjectWorldTestData/' })
$forbiddenMetaHumanEntries = @($entries | Where-Object {
    $_.path -match '/Engine/Plugins/MetaHuman/(?:MetaHumanAnimator/Content/(?:GenericTracker|Solver)|MetaHumanCoreTechLib/Content/(?:GenericTracker|RealtimeMono))/'
})
$forbiddenMutableSampleEntries = @($entries | Where-Object {
    $_.path -match '/Plugins/ThirdParty/MutableSample/' -or
    $_.path -match '/CO_Character(?:[./-]|$)'
})
$entryPaths = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($entry in $entries) {
    $null = $entryPaths.Add($entry.path.TrimStart('.', '/'))
}
$objectContract = Get-ObjectDefinitionShippingContract `
    -Root $ProjectRoot -ContractPath $ObjectContractPath
$missingDefinitions = @($objectContract.DefinitionSuffixes | Where-Object { -not $entryPaths.Contains($_) })
$missingCapabilityReferences = @($objectContract.ReferenceSuffixes | Where-Object { -not $entryPaths.Contains($_) })
$accepted = (
    (-not $expectedSuffix -or $requiredEntries.Count -eq 1) -and
    $testDataEntries.Count -eq 0 -and
    $forbiddenMetaHumanEntries.Count -eq 0 -and
    $forbiddenMutableSampleEntries.Count -eq 0 -and
    $missingDefinitions.Count -eq 0 -and
    $missingCapabilityReferences.Count -eq 0)
$receiptErrors = @()
if (-not $accepted) {
    if ($expectedSuffix -and $requiredEntries.Count -ne 1) {
        $receiptErrors += "Expected exactly one required map entry; found $($requiredEntries.Count)."
    }
    if ($testDataEntries.Count -ne 0) {
        $receiptErrors += "Editor-only ProjectWorldTestData leaked into IoStore; found $($testDataEntries.Count) entries."
    }
    if ($forbiddenMetaHumanEntries.Count -ne 0) {
        $receiptErrors += "MetaHuman authoring model data leaked into Shipping; found $($forbiddenMetaHumanEntries.Count) entries."
    }
    if ($forbiddenMutableSampleEntries.Count -ne 0) {
        $receiptErrors += "Mutable sample content leaked into Shipping; found $($forbiddenMutableSampleEntries.Count) entries."
    }
    if ($missingDefinitions.Count -ne 0) {
        $receiptErrors += "Generated ObjectDefinitions are missing from Shipping: $($missingDefinitions -join ', ')."
    }
    if ($missingCapabilityReferences.Count -ne 0) {
        $receiptErrors += "Declared capability asset references are missing from Shipping: $($missingCapabilityReferences -join ', ')."
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
    forbidden_metahuman_authoring_entry_count = $forbiddenMetaHumanEntries.Count
    forbidden_mutable_sample_entry_count = $forbiddenMutableSampleEntries.Count
    object_definition_expected_count = $objectContract.DefinitionSuffixes.Count
    object_definition_missing = @($missingDefinitions)
    capability_reference_expected_count = $objectContract.ReferenceSuffixes.Count
    capability_reference_missing = @($missingCapabilityReferences)
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
