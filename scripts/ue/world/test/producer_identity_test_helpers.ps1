# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function ConvertTo-ProjectWorldFixtureRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $prefix = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $full = [System.IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside ${Root}: $Path"
    }
    return $full.Substring($prefix.Length).Replace('\', '/')
}

function Write-ProjectWorldFixtureFile {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Text
    )

    $full = Join-Path $Root $RelativePath.Replace('/', [System.IO.Path]::DirectorySeparatorChar)
    New-Item -ItemType Directory -Path (Split-Path -Parent $full) -Force | Out-Null
    [System.IO.File]::WriteAllText($full, $Text)
    return $full
}

# Finds each named implementation source at its single location under a World
# plugin's Source tree, wherever its module lives, so a fixture can mirror it.
function Resolve-ProjectWorldImplementationSources {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string[]]$FileName
    )

    $sources = @(foreach ($plugin in Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'Plugins\World') -Directory) {
        $sourceRoot = Join-Path $plugin.FullName 'Source'
        if (Test-Path -LiteralPath $sourceRoot -PathType Container) {
            Get-ChildItem -LiteralPath $sourceRoot -Recurse -File |
                Where-Object { $_.FullName -notmatch '[\\/](Tests|Intermediate)[\\/]' }
        }
    })
    $resolved = @{}
    foreach ($name in $FileName) {
        $found = @($sources | Where-Object { $_.Name -ceq $name })
        if ($found.Count -ne 1) {
            throw "Expected exactly one World implementation source named $name; found $($found.Count)."
        }
        $resolved[$name] = ConvertTo-ProjectWorldFixtureRelativePath -Root $RepositoryRoot -Path $found[0].FullName
    }
    return $resolved
}

# Builds an isolated producer-identity root: copies of every repository producer
# descriptor, the descriptor schema, and the owning plugins' .uplugin files;
# placeholder bytes for each declared data input and each given source path; and
# a fixture engine that the root's UE config resolves to. Tracked files are only
# read. Descriptors maps each producer id to its descriptor copy.
function New-ProjectWorldProducerIdentityFixture {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [string[]]$SourcePath = @()
    )

    $descriptors = [ordered]@{}
    $pluginRoots = @{}
    foreach ($category in Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'Plugins') -Directory) {
        foreach ($plugin in Get-ChildItem -LiteralPath $category.FullName -Directory) {
            $producerRoot = Join-Path $plugin.FullName 'Data\Producers'
            if (-not (Test-Path -LiteralPath $producerRoot -PathType Container)) { continue }
            $pluginRoots[$plugin.FullName] = $true
            foreach ($file in Get-ChildItem -LiteralPath $producerRoot -File -Filter '*.json') {
                $text = [System.IO.File]::ReadAllText($file.FullName)
                $copy = Write-ProjectWorldFixtureFile -Root $ProjectRoot -Text $text `
                    -RelativePath (ConvertTo-ProjectWorldFixtureRelativePath -Root $RepositoryRoot -Path $file.FullName)
                $descriptor = $text | ConvertFrom-Json
                if ([string]$descriptor.kind -cne 'producer') { continue }
                $descriptors["$([string]$descriptor.generator_id):v$([int]$descriptor.generator_version)"] = $copy
                foreach ($dataInput in @($descriptor.data_inputs)) {
                    Write-ProjectWorldFixtureFile -Root $ProjectRoot -RelativePath ([string]$dataInput) `
                        -Text "data-input:$dataInput" | Out-Null
                }
            }
        }
    }
    if ($descriptors.Count -eq 0) {
        throw 'The repository declares no producer descriptor under Plugins/<Category>/<Plugin>/Data/Producers.'
    }
    $copies = @(foreach ($pluginRoot in $pluginRoots.Keys) {
        Get-ChildItem -LiteralPath $pluginRoot -File -Filter '*.uplugin'
    }) + @(Get-Item -LiteralPath (Join-Path $RepositoryRoot `
        'Plugins\World\ProjectWorld\Data\Schemas\project_world_producer_descriptor.schema.json'))
    foreach ($file in $copies) {
        Write-ProjectWorldFixtureFile -Root $ProjectRoot -Text ([System.IO.File]::ReadAllText($file.FullName)) `
            -RelativePath (ConvertTo-ProjectWorldFixtureRelativePath -Root $RepositoryRoot -Path $file.FullName) | Out-Null
    }

    $engineRoot = Join-Path $ProjectRoot 'engine'
    Write-ProjectWorldFixtureFile -Root $engineRoot -RelativePath 'Engine/Build/Build.version' `
        -Text '{ "MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 0, "Changelist": 0, "BranchName": "producer-identity-fixture" }' |
        Out-Null
    Write-ProjectWorldFixtureFile -Root $ProjectRoot -RelativePath 'scripts/config/ue_path.conf' `
        -Text ("UE_PATH=$($engineRoot.Replace('\', '/'))`n") | Out-Null
    foreach ($relative in $SourcePath) {
        Write-ProjectWorldFixtureFile -Root $ProjectRoot -RelativePath $relative -Text "baseline:$relative" | Out-Null
    }

    return [pscustomobject]@{
        ProjectRoot = $ProjectRoot
        Descriptors = $descriptors
    }
}

function Get-ProjectWorldFixtureFingerprints {
    param([Parameter(Mandatory = $true)][object]$Fixture)

    $fingerprints = [ordered]@{}
    foreach ($producerId in @($Fixture.Descriptors.Keys)) {
        $fingerprints[$producerId] = Get-ProjectWorldGeneratorFingerprint `
            -ProjectRoot $Fixture.ProjectRoot -ProducerId $producerId
    }
    return $fingerprints
}

function Get-ProjectWorldMovedProducers {
    param(
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Before,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$After
    )

    return @(@($Before.Keys) | Where-Object { $After[$_] -cne $Before[$_] })
}

# Rewrites descriptor text by a pattern that must match exactly once: a pattern
# that misses is a fixture defect, never a passing or failing identity result.
function Edit-ProjectWorldDescriptorText {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Replacement
    )

    $text = [System.IO.File]::ReadAllText($Path)
    $count = [regex]::Matches($text, $Pattern).Count
    if ($count -ne 1) {
        throw "Expected one match of $Pattern in ${Path}; found $count."
    }
    [System.IO.File]::WriteAllText($Path, [regex]::Replace($text, $Pattern, $Replacement.Replace('$', '$$')))
}

function Step-ProjectWorldDescriptorOutputRevision {
    param([Parameter(Mandatory = $true)][string]$Path)

    $text = [System.IO.File]::ReadAllText($Path)
    $pattern = '"output_revision"\s*:\s*(?<revision>\d+)'
    $count = [regex]::Matches($text, $pattern).Count
    if ($count -ne 1) {
        throw "Expected one output_revision in ${Path}; found $count."
    }
    $bumped = [regex]::Replace($text, $pattern, [System.Text.RegularExpressions.MatchEvaluator]{
        param($match)
        return '"output_revision": ' + ([int]$match.Groups['revision'].Value + 1)
    })
    [System.IO.File]::WriteAllText($Path, $bumped)
}
