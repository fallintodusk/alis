# Sanitize .uproject by removing non-core plugins
# Creates a temporary sanitized .uproject for base release builds

param(
    [string]$ProjectRoot = "<project-root>",
    [string]$UProjectPath = "<project-root>\Alis.uproject",
    [string]$OutputPath = "<project-root>\Alis_sanitized.uproject"
)

# Allowed plugins (boot + temporary marketplace dependencies)
$AllowedPlugins = @(
    "Orchestrator",
    "DialoguePlugin",
    "InstanceArrayTool",
    "LowEntryExtStdLib"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  .uproject Sanitization" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Project Root: $ProjectRoot"
Write-Host "Input:        $UProjectPath"
Write-Host "Output:       $OutputPath"
Write-Host ""

if (!(Test-Path $UProjectPath)) {
    Write-Host "[X] Cannot find .uproject at $UProjectPath" -ForegroundColor Red
    exit 1
}

# Read and parse .uproject
$json = Get-Content $UProjectPath -Raw | ConvertFrom-Json
$originalCount = $json.Plugins.Count

Write-Host "Original plugins: $originalCount"

$keptPlugins = @()
$strippedPlugins = @()

foreach ($plugin in $json.Plugins) {
    $pluginName = $plugin.Name
    if ($AllowedPlugins -contains $pluginName) {
        $keptPlugins += $plugin
        Write-Host "  [OK] Keeping: $pluginName" -ForegroundColor Green
    } else {
        $strippedPlugins += $pluginName
        Write-Host "  [ ] Stripping: $pluginName" -ForegroundColor Yellow
    }
}

$json.Plugins = $keptPlugins
$json | ConvertTo-Json -Depth 10 | Set-Content $OutputPath

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Sanitization Complete" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Sanitized plugins: $($keptPlugins.Count)" -ForegroundColor Green
Write-Host "Stripped plugins:  $($strippedPlugins.Count)" -ForegroundColor Yellow
Write-Host ""
Write-Host "Stripped list:"
foreach ($plugin in $strippedPlugins) {
    Write-Host "  - $plugin"
}
Write-Host ""
Write-Host "[OK] Sanitized .uproject created: $OutputPath" -ForegroundColor Green
Write-Host "Use this file for base release builds."
