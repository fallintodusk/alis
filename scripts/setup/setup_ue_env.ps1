# scripts/setup/setup_ue_env.ps1
# Sets UE_PATH environment variable for VS Code workspace integration.
# Run once after cloning. Requires PowerShell (not Git Bash).

param(
    [switch]$Force
)

$ConfigPath = Join-Path $PSScriptRoot "..\config\ue_path.conf"
$UePath = $null

# Parse UE_PATH from ue_path.conf (single source of truth)
if (Test-Path $ConfigPath) {
    Get-Content $ConfigPath | ForEach-Object {
        $line = $_.Trim()
        if (-not $line -or $line.StartsWith("#")) { return }
        if ($line -match "^\s*UE_PATH\s*(:=|=)\s*(.+)$") {
            $val = $Matches[2].Trim('"', "'")
            if (-not $val.StartsWith("#")) {
                $UePath = $val
            }
        }
    }
}

if (-not $UePath) {
    Write-Error "Could not parse UE_PATH from $ConfigPath"
    exit 1
}

# Normalize path for Windows
$UePath = $UePath -replace '/', '\'

Write-Host "UE_PATH from config: $UePath"

# Check if already set (User or Machine level)
$UserValue = [Environment]::GetEnvironmentVariable("UE_PATH", "User")
$MachineValue = [Environment]::GetEnvironmentVariable("UE_PATH", "Machine")

if ($UserValue -eq $UePath) {
    Write-Host "[OK] UE_PATH already set correctly (User)." -ForegroundColor Green
    exit 0
}

if ($MachineValue -eq $UePath) {
    Write-Host "[OK] UE_PATH already set correctly (Machine)." -ForegroundColor Green
    exit 0
}

if ($MachineValue) {
    Write-Host "UE_PATH is set at Machine level: $MachineValue" -ForegroundColor Yellow
    Write-Host "To change, edit System Environment Variables or run as Admin."
    exit 1
}

if ($UserValue -and -not $Force) {
    Write-Host "UE_PATH is currently set to: $UserValue" -ForegroundColor Yellow
    Write-Host "Use -Force to override, or run:"
    Write-Host "  [Environment]::SetEnvironmentVariable('UE_PATH', `$null, 'User')" -ForegroundColor Cyan
    exit 1
}

# Set persistent User environment variable
[Environment]::SetEnvironmentVariable("UE_PATH", $UePath, "User")

Write-Host "[OK] UE_PATH set to: $UePath" -ForegroundColor Green

# Update .vscode files to use ${env:UE_PATH} instead of hardcoded paths
$ProjectRoot = Join-Path $PSScriptRoot "..\.." | Resolve-Path
$VsCodeDir = Join-Path $ProjectRoot ".vscode"

if (Test-Path $VsCodeDir) {
    Write-Host ""
    Write-Host "Updating .vscode configs..." -ForegroundColor Cyan

    # Common UE path patterns to replace (forward and backslash variants)
    $Patterns = @(
        'G:\\UnrealEngine-5\.7',
        '<ue-path>\.7',
        'C:\\UnrealEngine\\UE_5\.7',
        '<ue-path>\.7'
    )

    # Files that support VS Code variable expansion
    $VsCodeFiles = @("launch.json", "tasks.json", "settings.json")
    foreach ($file in $VsCodeFiles) {
        $filePath = Join-Path $VsCodeDir $file
        if (Test-Path $filePath) {
            $content = Get-Content $filePath -Raw
            $modified = $false
            foreach ($pattern in $Patterns) {
                if ($content -match $pattern) {
                    $content = $content -replace $pattern, '${env:UE_PATH}'
                    $modified = $true
                }
            }
            if ($modified) {
                Set-Content $filePath $content -NoNewline
                Write-Host "  [OK] $file - updated to use `${env:UE_PATH}" -ForegroundColor Green
            }
        }
    }

    # compileCommands files - use actual path (clangd doesn't expand VS Code vars)
    Get-ChildItem $VsCodeDir -Filter "compileCommands*.json" | ForEach-Object {
        $content = Get-Content $_.FullName -Raw
        $modified = $false
        foreach ($pattern in $Patterns) {
            if ($content -match $pattern) {
                $content = $content -replace $pattern, ($UePath -replace '\\', '\\')
                $modified = $true
            }
        }
        if ($modified) {
            Set-Content $_.FullName $content -NoNewline
            Write-Host "  [OK] $($_.Name) - updated path" -ForegroundColor Green
        }
    }
}

Write-Host ""
Write-Host "Restart VS Code to pick up the new environment variable." -ForegroundColor Yellow
