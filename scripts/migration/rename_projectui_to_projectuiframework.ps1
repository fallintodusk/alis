# Rename ProjectUI (legacy) to ProjectUIFramework (authoritative) - PLAN
# Default mode is dry-run. Pass -Execute to perform file operations.

param(
  [switch]$Execute
)

function Write-Step($msg){ Write-Host "[STEP] $msg" }
function Do-Move($src,$dst){
  if(-not $Execute){ Write-Host "DRY-RUN: move '$src' -> '$dst'"; return }
  New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
  Move-Item -Force $src $dst
}

Write-Step "Rename Systems/ProjectUI -> UI/ProjectUIFramework (folder layout)"
$src = "Plugins/Systems/ProjectUI"
$dst = "Plugins/UI/ProjectUIFramework/_legacy_port"
if(Test-Path $src){ Do-Move $src $dst } else { Write-Host "Skip: $src not found" }

Write-Step "Update references in docs and configs (already largely migrated)"
Write-Host "- Verify any remaining mentions of ProjectUI -> ProjectUIFramework"

Write-Step "Module rename (manual)"
Write-Host "- Update .uplugin module Name to ProjectUIFramework"
Write-Host "- Rename module API macro to PROJECTUIFRAMEWORK_API"
Write-Host "- Rename module directories under Source/ and fix Build.cs"

Write-Step "Dependencies"
Write-Host "- Update dependent modules to reference ProjectUIFramework in Build.cs"

Write-Step "Validation"
Write-Host "- Rebuild affected modules"
Write-Host "- Run smoke tests"

