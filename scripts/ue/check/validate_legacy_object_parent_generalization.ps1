# Validates LEGACY_OBJECT_PARENT_GENERALIZATION ID mapping across:
# - code markers
# - todo/done/generalize_placeable_actor_parent.md registry
# - required plugin README Legacy Paths sections

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$todoPath = Join-Path $projectRoot "todo\done\generalize_placeable_actor_parent.md"

$requiredReadmes = @(
    "Plugins\Resources\ProjectObject\README.md",
    "Plugins\Editor\ProjectPlacementEditor\README.md",
    "Plugins\Gameplay\ProjectInteraction\README.md",
    "Plugins\World\ProjectWorld\README.md"
) | ForEach-Object { Join-Path $projectRoot $_ }

if (-not (Test-Path $todoPath))
{
    Write-Error "TODO file not found: $todoPath"
}

function Get-LegacyIdsFromText {
    param([string]$Text)
    return [regex]::Matches($Text, '\bL\d{3}\b') | ForEach-Object { $_.Value } | Sort-Object -Unique
}

function Get-CodeLegacyIds {
    param([string]$RootPath)
    $patterns = @("*.h", "*.hpp", "*.cpp", "*.cxx", "*.inl")
    $files = Get-ChildItem -Path (Join-Path $RootPath "Plugins") -Recurse -File -Include $patterns
    $ids = New-Object System.Collections.Generic.HashSet[string]

    foreach ($file in $files)
    {
        $text = Get-Content $file.FullName -Raw
        $matches = [regex]::Matches($text, 'LEGACY_OBJECT_PARENT_GENERALIZATION\((L\d{3})\)')
        foreach ($m in $matches)
        {
            [void]$ids.Add($m.Groups[1].Value)
        }
    }

    return @($ids) | Sort-Object
}

$todoText = Get-Content $todoPath -Raw
$todoIds = Get-LegacyIdsFromText -Text $todoText
$codeIds = Get-CodeLegacyIds -RootPath $projectRoot

$missingReadmes = @()
$docIdsSet = New-Object System.Collections.Generic.HashSet[string]
$readmesMissingLegacySection = @()

foreach ($readme in $requiredReadmes)
{
    if (-not (Test-Path $readme))
    {
        $missingReadmes += $readme
        continue
    }

    $text = Get-Content $readme -Raw
    if ($text -notmatch '(?m)^## Legacy Paths\s*$')
    {
        $readmesMissingLegacySection += $readme
    }

    foreach ($id in (Get-LegacyIdsFromText -Text $text))
    {
        [void]$docIdsSet.Add($id)
    }
}

$docIds = @($docIdsSet) | Sort-Object

$codeMissingInTodo = @($codeIds | Where-Object { $_ -notin $todoIds })
$todoMissingInDocs = @($todoIds | Where-Object { $_ -notin $docIds })

$hasError = $false

Write-Host "Legacy Object Parent Generalization Audit"
Write-Host "  TODO IDs: $($todoIds -join ', ')"
Write-Host "  Code IDs: $($codeIds -join ', ')"
Write-Host "  Doc IDs:  $($docIds -join ', ')"

if ($missingReadmes.Count -gt 0)
{
    $hasError = $true
    Write-Host ""
    Write-Host "ERROR: Missing required README files:"
    $missingReadmes | ForEach-Object { Write-Host "  - $_" }
}

if ($readmesMissingLegacySection.Count -gt 0)
{
    $hasError = $true
    Write-Host ""
    Write-Host "ERROR: Required README missing '## Legacy Paths' section:"
    $readmesMissingLegacySection | ForEach-Object { Write-Host "  - $_" }
}

if ($codeMissingInTodo.Count -gt 0)
{
    $hasError = $true
    Write-Host ""
    Write-Host "ERROR: Code marker IDs missing from TODO registry:"
    $codeMissingInTodo | ForEach-Object { Write-Host "  - $_" }
}

if ($todoMissingInDocs.Count -gt 0)
{
    $hasError = $true
    Write-Host ""
    Write-Host "ERROR: TODO registry IDs missing from required plugin docs:"
    $todoMissingInDocs | ForEach-Object { Write-Host "  - $_" }
}

if ($hasError)
{
    exit 1
}

Write-Host ""
Write-Host "OK: Legacy marker/doc registry audit passed."
exit 0
