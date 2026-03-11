<#
.SYNOPSIS
  Show remaining agent-compatible TODO checkboxes in todo/.

.DESCRIPTION
  Discovers unchecked TODOs across todo/*.md files with enhanced regex patterns.
  Supports multiple checkbox formats, priority detection, and structured output.

.PARAMETER Root
  Project root directory. Defaults to 3 levels up from script location.

.PARAMETER Format
  Output format: "text" (default), "json", or "detailed"

.PARAMETER ShowManual
  Include MANUAL tasks in output (default: false, agent-compatible only)

.PARAMETER ShowStatistics
  Show breakdown statistics by file and priority (default: true)

.USAGE
  # Basic usage (agent-compatible count)
  powershell scripts/autonomous/overnight/discover_todos.ps1

  # Detailed output with manual tasks
  powershell scripts/autonomous/overnight/discover_todos.ps1 -Format detailed -ShowManual

  # JSON output for parsing
  powershell scripts/autonomous/overnight/discover_todos.ps1 -Format json

.NOTES
  - Enhanced regex patterns handle multiple checkbox formats:
    - Bullet styles: -, *, +
    - Checkbox formats: [ ], [], [_], [.]
    - Whitespace: spaces and tabs
  - Priority detection: HIGH, MEDIUM, LOW markers
  - Validation: Warns on malformed TODO lines
  - Intended for non-stop overnight loop visibility
#>

param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\..\..\..").Path,
    [ValidateSet("text", "json", "detailed")]
    [string]$Format = "text",
    [switch]$ShowManual = $false,
    [switch]$ShowStatistics = $true
)

$ErrorActionPreference = "Stop"

# Validate todo directory
$todoDir = Join-Path $Root "todo"
if (-not (Test-Path $todoDir)) {
    Write-Error "todo/ directory not found at: $todoDir"
    exit 1
}

# Enhanced regex patterns for checkbox detection
# Matches: - [ ], * [ ], + [ ], - [], - [_], - [.], etc.
# Also handles multiple spaces/tabs before and after checkbox
$checkboxPatterns = @(
    '^\s*[-*+]\s+\[\s*\]',   # Standard: - [ ], * [ ], + [ ]
    '^\s*[-*+]\s+\[\]',      # Compact: - [], * [], + []
    '^\s*[-*+]\s+\[_\]',     # Underscore: - [_]
    '^\s*[-*+]\s+\[\.\]'     # Dot: - [.]
)

# Combine patterns with OR
$checkboxRegex = "($($checkboxPatterns -join '|'))"

# Discover all TODO markdown files
$todoFiles = Get-ChildItem $todoDir -Recurse -Include *.md

# Parse TODOs with metadata
$allTodos = @()
foreach ($file in $todoFiles) {
    $lines = Get-Content $file.FullName
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        $lineNumber = $i + 1  # 1-indexed

        # Check if line matches checkbox pattern
        if ($line -match $checkboxRegex) {
            # Extract metadata
            $isManual = $line -match '\bMANUAL\b'
            $priority = 'MEDIUM'  # Default priority

            if ($line -match '\b(HIGH|CRITICAL|URGENT)\b') {
                $priority = 'HIGH'
            } elseif ($line -match '\bLOW\b') {
                $priority = 'LOW'
            }

            # Validation: Warn on malformed TODOs (e.g., missing description)
            $hasDescription = ($line -replace $checkboxRegex, '').Trim().Length -gt 0
            if (-not $hasDescription) {
                Write-Warning "Malformed TODO at $($file.Name):$lineNumber - No description"
            }

            # Build TODO object
            $todo = [PSCustomObject]@{
                File        = $file.Name
                Path        = $file.FullName
                LineNumber  = $lineNumber
                Line        = $line.Trim()
                IsManual    = $isManual
                Priority    = $priority
            }

            $allTodos += $todo
        }
    }
}

# Filter based on -ShowManual flag
$filteredTodos = if ($ShowManual) {
    $allTodos
} else {
    $allTodos | Where-Object { -not $_.IsManual }
}

# Calculate statistics
$totalCount = $filteredTodos.Count
$statistics = @{
    Total = $totalCount
    ByFile = $filteredTodos | Group-Object File | ForEach-Object { @{ $_.Name = $_.Count } }
    ByPriority = @{
        HIGH = ($filteredTodos | Where-Object { $_.Priority -eq 'HIGH' }).Count
        MEDIUM = ($filteredTodos | Where-Object { $_.Priority -eq 'MEDIUM' }).Count
        LOW = ($filteredTodos | Where-Object { $_.Priority -eq 'LOW' }).Count
    }
}

# Output based on format
switch ($Format) {
    "json" {
        # JSON output for machine parsing
        $output = @{
            Count = $totalCount
            ShowManual = $ShowManual.IsPresent
            Statistics = $statistics
            Todos = $filteredTodos | ForEach-Object {
                @{
                    File = $_.File
                    LineNumber = $_.LineNumber
                    Line = $_.Line
                    IsManual = $_.IsManual
                    Priority = $_.Priority
                }
            }
        }
        $output | ConvertTo-Json -Depth 5
    }

    "detailed" {
        # Detailed text output
        Write-Host "======================================" -ForegroundColor Cyan
        Write-Host "  TODO Discovery Report" -ForegroundColor Cyan
        Write-Host "======================================" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Agent-compatible TODOs remaining: $totalCount" -ForegroundColor $(if ($totalCount -gt 0) { 'Yellow' } else { 'Green' })
        Write-Host ""

        if ($ShowStatistics -and $totalCount -gt 0) {
            Write-Host "--- Statistics ---" -ForegroundColor Cyan
            Write-Host "By Priority:"
            Write-Host "  HIGH:   $($statistics.ByPriority.HIGH)" -ForegroundColor Red
            Write-Host "  MEDIUM: $($statistics.ByPriority.MEDIUM)" -ForegroundColor Yellow
            Write-Host "  LOW:    $($statistics.ByPriority.LOW)" -ForegroundColor Gray
            Write-Host ""

            Write-Host "By File:"
            foreach ($fileGroup in ($filteredTodos | Group-Object File)) {
                Write-Host "  $($fileGroup.Name): $($fileGroup.Count)"
            }
            Write-Host ""
        }

        if ($totalCount -gt 0) {
            Write-Host "--- TODO List ---" -ForegroundColor Cyan
            $filteredTodos | Sort-Object Priority, File, LineNumber |
                Format-Table -Property @{
                    Label = 'Priority'; Expression = { $_.Priority }; Width = 8
                },
                @{
                    Label = 'File'; Expression = { $_.File }; Width = 30
                },
                @{
                    Label = 'Line'; Expression = { $_.LineNumber }; Width = 5
                },
                @{
                    Label = 'TODO'; Expression = { $_.Line }; Width = 80
                } -AutoSize
        }
    }

    default {
        # Default text output (compact, agent-compatible)
        Write-Host "Agent-compatible TODOs remaining: $totalCount"

        if ($ShowStatistics -and $totalCount -gt 0) {
            Write-Host "  HIGH: $($statistics.ByPriority.HIGH) | MEDIUM: $($statistics.ByPriority.MEDIUM) | LOW: $($statistics.ByPriority.LOW)"
        }

        if ($totalCount -gt 0) {
            $filteredTodos | Sort-Object Priority, File, LineNumber |
                Format-Table -Property File, LineNumber, Line -AutoSize
        }
    }
}
