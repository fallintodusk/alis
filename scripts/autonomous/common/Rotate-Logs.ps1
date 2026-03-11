<#
.SYNOPSIS
  Rotate and cleanup overnight CI log files.

.DESCRIPTION
  Manages log file retention with three cleanup strategies:
  1. Delete files older than MaxAgeDays
  2. Keep only the newest MaxFiles files
  3. Enforce total size limit (MaxSizeMB) by deleting oldest files

  Processes both *.log and *.json files recursively.

.PARAMETER LogDir
  Log directory relative to project root. Default: "artifacts/overnight"

.PARAMETER MaxAgeDays
  Maximum age of log files in days. Files older than this are deleted. Default: 7 days.
  Valid range: 1-365 days.

.PARAMETER MaxFiles
  Maximum number of log files to keep. Oldest files are deleted first. Default: 100.
  Valid range: 10-10000 files.

.PARAMETER MaxSizeMB
  Maximum total size of log directory in MB. Oldest files deleted to stay under limit. Default: 50 MB.
  Valid range: 10-10000 MB.

.EXAMPLE
  # Rotate with defaults (7 days, 100 files, 50 MB)
  .\Rotate-Logs.ps1

.EXAMPLE
  # Aggressive rotation for low disk space
  .\Rotate-Logs.ps1 -MaxAgeDays 3 -MaxFiles 50 -MaxSizeMB 20

.EXAMPLE
  # Long retention for debugging
  .\Rotate-Logs.ps1 -MaxAgeDays 30 -MaxFiles 500 -MaxSizeMB 500

.NOTES
  - Files are deleted oldest-first to preserve recent logs
  - Use -Verbose to see detailed deletion log
  - Creates log directory if it doesn't exist
  - Safe to run multiple times (idempotent)
#>

param(
    [Parameter(HelpMessage = "Log directory relative to project root")]
    [ValidateNotNullOrEmpty()]
    [string]$LogDir = "artifacts/overnight",

    [Parameter(HelpMessage = "Maximum age of log files in days")]
    [ValidateRange(1, 365)]
    [int]$MaxAgeDays = 7,

    [Parameter(HelpMessage = "Maximum number of log files to keep")]
    [ValidateRange(10, 10000)]
    [int]$MaxFiles = 100,

    [Parameter(HelpMessage = "Maximum total size of log directory in MB")]
    [ValidateRange(10, 10000)]
    [int]$MaxSizeMB = 50
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$fullLogDir = Join-Path $ProjectRoot $LogDir

if (-not (Test-Path $fullLogDir)) {
    Write-Host "Log directory does not exist: $fullLogDir" -ForegroundColor Yellow
    exit 0
}

Write-Host "Log Rotation Started" -ForegroundColor Cyan
Write-Host "Directory: $fullLogDir"
Write-Host "Max Age: $MaxAgeDays days"
Write-Host "Max Files: $MaxFiles"
Write-Host "Max Size: $MaxSizeMB MB"
Write-Host ""

# Get all log files
$logFiles = Get-ChildItem -Path $fullLogDir -Recurse -File -Include "*.log", "*.json" | Sort-Object LastWriteTime

$totalFiles = $logFiles.Count
$totalSizeMB = ($logFiles | Measure-Object -Property Length -Sum).Sum / 1MB
$deletedCount = 0
$deletedSizeMB = 0

Write-Host "Found $totalFiles log files ($([Math]::Round($totalSizeMB, 2)) MB)" -ForegroundColor Gray
Write-Host ""

# Delete by age
$cutoffDate = (Get-Date).AddDays(-$MaxAgeDays)
$oldFiles = $logFiles | Where-Object { $_.LastWriteTime -lt $cutoffDate }

if ($oldFiles.Count -gt 0) {
    Write-Host "Deleting $($oldFiles.Count) files older than $MaxAgeDays days..." -ForegroundColor Yellow
    foreach ($file in $oldFiles) {
        try {
            $sizeMB = $file.Length / 1MB
            Write-Verbose "  Deleting: $($file.Name) ($([Math]::Round($sizeMB, 2)) MB, age: $((Get-Date) - $file.LastWriteTime).Days days)"
            Remove-Item $file.FullName -Force -ErrorAction Stop
            $deletedCount++
            $deletedSizeMB += $sizeMB
        } catch {
            Write-Warning "Failed to delete $($file.Name): $($_.Exception.Message)"
        }
    }
}

# Delete if too many files (keep newest)
$remainingFiles = $logFiles | Where-Object { $_.LastWriteTime -ge $cutoffDate }
if ($remainingFiles.Count -gt $MaxFiles) {
    $filesToDelete = $remainingFiles | Sort-Object LastWriteTime | Select-Object -First ($remainingFiles.Count - $MaxFiles)
    Write-Host "Deleting $($filesToDelete.Count) files (exceeds max count of $MaxFiles)..." -ForegroundColor Yellow
    foreach ($file in $filesToDelete) {
        try {
            $sizeMB = $file.Length / 1MB
            Write-Verbose "  Deleting: $($file.Name) ($([Math]::Round($sizeMB, 2)) MB)"
            Remove-Item $file.FullName -Force -ErrorAction Stop
            $deletedCount++
            $deletedSizeMB += $sizeMB
        } catch {
            Write-Warning "Failed to delete $($file.Name): $($_.Exception.Message)"
        }
    }
}

# Delete if total size too large (delete oldest first)
$remainingFiles = Get-ChildItem -Path $fullLogDir -Recurse -File -Include "*.log", "*.json" | Sort-Object LastWriteTime
$remainingSizeMB = ($remainingFiles | Measure-Object -Property Length -Sum).Sum / 1MB

if ($remainingSizeMB -gt $MaxSizeMB) {
    Write-Host "Total size ($([Math]::Round($remainingSizeMB, 2)) MB) exceeds $MaxSizeMB MB, deleting oldest..." -ForegroundColor Yellow
    foreach ($file in $remainingFiles) {
        if ($remainingSizeMB -le $MaxSizeMB) {
            break
        }
        try {
            $sizeMB = $file.Length / 1MB
            Write-Verbose "  Deleting: $($file.Name) ($([Math]::Round($sizeMB, 2)) MB)"
            Remove-Item $file.FullName -Force -ErrorAction Stop
            $deletedCount++
            $deletedSizeMB += $sizeMB
            $remainingSizeMB -= $sizeMB
        } catch {
            Write-Warning "Failed to delete $($file.Name): $($_.Exception.Message)"
            # Don't subtract size if delete failed
        }
    }
}

# Summary
Write-Host ""
Write-Host "Log Rotation Complete" -ForegroundColor Green
Write-Host "Deleted: $deletedCount files ($([Math]::Round($deletedSizeMB, 2)) MB)" -ForegroundColor Gray
Write-Host "Remaining: $($totalFiles - $deletedCount) files ($([Math]::Round($totalSizeMB - $deletedSizeMB, 2)) MB)" -ForegroundColor Gray
