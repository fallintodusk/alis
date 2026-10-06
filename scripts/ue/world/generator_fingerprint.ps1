# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '..\..\config\Resolve-UEConfig.ps1')

function Get-ProjectWorldManifestProducerId {
    param([Parameter(Mandatory = $true)][object]$Manifest)

    $owner = [string]$Manifest.owning_layer
    if ($owner -eq 'map') { return 'map:v1' }
    if ($owner -eq 'presentation') { return 'presentation:v1' }
    if (-not ($Manifest.PSObject.Properties.Name -contains 'layer_contract') -or
        $null -eq $Manifest.layer_contract) {
        throw "Layer manifest has no producer contract: $($Manifest.scope_id)"
    }
    return "$($Manifest.layer_contract.generator_id):v$($Manifest.layer_contract.generator_version)"
}

function Assert-ProjectWorldDescriptorFields {
    param([object]$Descriptor, [string[]]$Required, [string[]]$Optional, [string]$Path)

    $names = @($Descriptor.PSObject.Properties.Name)
    foreach ($name in $Required) {
        if ($names -cnotcontains $name) { throw "Producer descriptor missing $name`: $Path" }
    }
    foreach ($name in $names) {
        if ($Required -cnotcontains $name -and $Optional -cnotcontains $name) {
            throw "Producer descriptor has unknown field $name`: $Path"
        }
    }
}

function Assert-ProjectWorldRevision {
    param([object]$Value, [string]$Name, [string]$Path)

    if ($Value -isnot [int] -and $Value -isnot [long]) {
        throw "Producer descriptor $Name must be an integer: $Path"
    }
    if ($Value -lt 1) { throw "Producer descriptor $Name must be positive: $Path" }
}

function Get-ProjectWorldProducerCatalog {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    $root = [System.IO.Path]::GetFullPath($ProjectRoot)
    $schemaPath = [System.IO.Path]::GetFullPath((Join-Path $root 'Plugins\World\ProjectWorld\Data\Schemas\project_world_producer_descriptor.schema.json'))
    $pluginsRoot = Join-Path $root 'Plugins'
    if (-not (Test-Path -LiteralPath $schemaPath -PathType Leaf) -or
        -not (Test-Path -LiteralPath $pluginsRoot -PathType Container)) {
        throw "Producer descriptor catalog is incomplete: $root"
    }
    $paths = [System.Collections.Generic.List[string]]::new()
    foreach ($category in Get-ChildItem -LiteralPath $pluginsRoot -Directory) {
        foreach ($plugin in Get-ChildItem -LiteralPath $category.FullName -Directory) {
            $directory = Join-Path $plugin.FullName 'Data\Producers'
            if (-not (Test-Path -LiteralPath $directory -PathType Container)) { continue }
            foreach ($entry in Get-ChildItem -LiteralPath $directory) {
                if ($entry.PSIsContainer -or $entry.Extension -cne '.json') {
                    throw "Invalid producer descriptor entry: $($entry.FullName)"
                }
                $paths.Add($entry.FullName)
            }
        }
    }
    $orderedPaths = $paths.ToArray()
    [Array]::Sort($orderedPaths, [System.StringComparer]::Ordinal)
    $producers = [ordered]@{}
    $generatorIds = @{}
    $moduleOwners = @{}
    $pipeline = $null
    foreach ($path in $orderedPaths) {
        try {
            $utf8 = [System.Text.UTF8Encoding]::new($false, $true)
            $descriptorText = $utf8.GetString([System.IO.File]::ReadAllBytes($path))
            if ($descriptorText.Length -gt 0 -and [int][char]$descriptorText[0] -eq 0xFEFF) {
                $descriptorText = $descriptorText.Substring(1)
            }
            $document = $descriptorText | ConvertFrom-Json
        }
        catch { throw "Invalid producer descriptor JSON: $path`: $_" }
        if ($null -eq $document -or $document -isnot [pscustomobject]) {
            throw "Producer descriptor must be an object: $path"
        }
        $kind = [string]$document.kind
        if ($kind -ceq 'pipeline') {
            Assert-ProjectWorldDescriptorFields $document @('$schema', 'schema_version', 'kind', 'pipeline_revision', 'modules') @() $path
        }
        elseif ($kind -ceq 'producer') {
            Assert-ProjectWorldDescriptorFields $document @('$schema', 'schema_version', 'kind', 'generator_id', 'generator_version', 'output_revision', 'data_inputs', 'modules') @('runtime_acceptance') $path
        }
        else { throw "Unknown producer descriptor kind: $path" }
        if ($document.schema_version -ne 1) { throw "Unsupported producer descriptor schema version: $path" }
        $resolvedSchema = [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $path) ([string]$document.'$schema')))
        if (-not $resolvedSchema.Equals($schemaPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Producer descriptor schema path is wrong: $path"
        }
        $pluginRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $path))
        $pluginName = Split-Path -Leaf $pluginRoot
        $upluginPath = Join-Path $pluginRoot "$pluginName.uplugin"
        if (-not (Test-Path -LiteralPath $upluginPath -PathType Leaf)) { throw "Missing owning uplugin: $path" }
        $uplugin = Get-Content -LiteralPath $upluginPath -Raw | ConvertFrom-Json
        $declaredModules = @($uplugin.Modules | ForEach-Object { [string]$_.Name })
        $modules = @($document.modules)
        if ($kind -ceq 'pipeline') { Assert-ProjectWorldRevision $document.pipeline_revision 'pipeline_revision' $path }
        else {
            Assert-ProjectWorldRevision $document.generator_version 'generator_version' $path
            Assert-ProjectWorldRevision $document.output_revision 'output_revision' $path
        }
        if ($kind -ceq 'pipeline' -and $modules.Count -eq 0) { throw "Pipeline has no modules: $path" }
        $seenModules = @{}
        foreach ($module in $modules) {
            if ($module -isnot [string] -or $module -cnotmatch '^[A-Z][A-Za-z0-9]*$' -or
                $declaredModules -cnotcontains $module -or $seenModules.ContainsKey($module) -or
                $moduleOwners.ContainsKey($module)) {
                throw "Invalid or duplicate producer module $module`: $path"
            }
            $seenModules[$module] = $true
            $moduleOwners[$module] = $path
        }
        if ($kind -ceq 'pipeline') {
            $expectedPath = Join-Path $root 'Plugins\World\ProjectWorld\Data\Producers\realization_pipeline.json'
            if ($null -ne $pipeline -or -not $path.Equals($expectedPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Duplicate or misplaced pipeline descriptor: $path"
            }
            $pipeline = $document
            continue
        }
        $generatorId = [string]$document.generator_id
        if ($generatorId -cnotmatch '^[a-z][a-z0-9_]*$' -or $generatorId -ceq 'realization_pipeline' -or
            [System.IO.Path]::GetFileNameWithoutExtension($path) -cne $generatorId) {
            throw "Invalid producer id or filename: $path"
        }
        if ($generatorIds.ContainsKey($generatorId)) { throw "Duplicate producer id: $path" }
        $generatorIds[$generatorId] = $true
        $inputs = @($document.data_inputs)
        $orderedInputs = [string[]]$inputs
        [Array]::Sort($orderedInputs, [System.StringComparer]::Ordinal)
        if (@($inputs | Select-Object -Unique).Count -ne $inputs.Count) { throw "Duplicate producer data input: $path" }
        foreach ($input in $inputs) {
            if ($input -isnot [string] -or $input -cnotmatch '^Plugins/[A-Za-z0-9_][A-Za-z0-9_.-]*(/[A-Za-z0-9_][A-Za-z0-9_.-]*)+\.(uasset|umap|json)$') {
                throw "Invalid producer data input $input`: $path"
            }
            $full = Join-Path $root $input.Replace('/', [System.IO.Path]::DirectorySeparatorChar)
            if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "Missing producer data input $input`: $path" }
            $bytes = [System.IO.File]::ReadAllBytes($full)
            $prefix = [System.Text.Encoding]::ASCII.GetBytes('version https://git-lfs.github.com/spec/v1')
            if ($bytes.Length -ge $prefix.Length -and
                [System.Text.Encoding]::ASCII.GetString($bytes, 0, $prefix.Length) -ceq
                    'version https://git-lfs.github.com/spec/v1') {
                throw "LFS pointer is not producer data: $input`: $path"
            }
        }
        $id = "${generatorId}:v$($document.generator_version)"
        $producers[$id] = [pscustomobject]@{
            Path = $path; GeneratorId = $generatorId; GeneratorVersion = [int]$document.generator_version
            OutputRevision = [int]$document.output_revision; DataInputs = $orderedInputs; Modules = $modules
        }
    }
    if ($null -eq $pipeline) { throw "Missing pipeline descriptor: $root" }
    foreach ($entry in $producers.Values) {
        foreach ($module in $entry.Modules) {
            if ($pipeline.modules -ccontains $module) { throw "Producer names pipeline module: $($entry.Path)" }
        }
    }
    return [pscustomobject]@{
        PipelineRevision = [int]$pipeline.pipeline_revision
        PipelineModules = @($pipeline.modules)
        Producers = $producers
    }
}

function Get-ProjectWorldEngineBuildIdentity {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    $configRoot = Join-Path $ProjectRoot 'scripts\config'
    try { $config = Resolve-UEConfig -ConfigDir $configRoot }
    catch { throw "Engine identity is unknown: $_" }
    $buildVersion = Join-Path $config.UE_PATH 'Engine\Build\Build.version'
    if (-not (Test-Path -LiteralPath $buildVersion -PathType Leaf)) {
        throw "Engine identity is unknown: missing Build.version at $buildVersion"
    }
    return (Get-FileHash -LiteralPath $buildVersion -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-ProjectWorldDataInputDigest {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ([System.IO.Path]::GetExtension($Path).ToLowerInvariant() -eq '.json') {
        $utf8 = [System.Text.UTF8Encoding]::new($false, $true)
        $value = $utf8.GetString($bytes)
        if ($value.Length -gt 0 -and [int][char]$value[0] -eq 0xFEFF) {
            $value = $value.Substring(1)
        }
        $value = $value.Replace("`r`n", "`n").Replace("`r", "`n")
        $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($value)
    }
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

function Get-ProjectWorldGeneratorFingerprint {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$ProducerId,
        [object]$Catalog,
        [string]$EngineIdentity
    )

    if ($null -eq $Catalog) { $Catalog = Get-ProjectWorldProducerCatalog -ProjectRoot $ProjectRoot }
    if (-not $Catalog.Producers.Contains($ProducerId)) { throw "Unknown ProjectWorld manifest producer: $ProducerId" }
    if ([string]::IsNullOrEmpty($EngineIdentity)) {
        $EngineIdentity = Get-ProjectWorldEngineBuildIdentity -ProjectRoot $ProjectRoot
    }
    if ($EngineIdentity -cnotmatch '^[a-f0-9]{64}$') { throw "Engine identity is unknown: $EngineIdentity" }
    $producer = $Catalog.Producers[$ProducerId]
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add('project_world_producer_fingerprint_v4')
    $lines.Add("producer`0$ProducerId")
    $lines.Add("output_revision`0$($producer.OutputRevision)")
    $lines.Add("pipeline_revision`0$($Catalog.PipelineRevision)")
    $lines.Add("engine_build_identity`0$EngineIdentity")
    foreach ($input in $producer.DataInputs) {
        $full = Join-Path $ProjectRoot $input.Replace('/', [System.IO.Path]::DirectorySeparatorChar)
        $digest = Get-ProjectWorldDataInputDigest -Path $full
        $lines.Add("data_input`0$input`0$digest")
    }
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes(($lines -join "`n"))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
