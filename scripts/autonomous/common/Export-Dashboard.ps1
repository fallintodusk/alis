<#
.SYNOPSIS
  Generate HTML progress dashboard from overnight CI logs.

.DESCRIPTION
  Parses JSON-formatted CI logs and generates an interactive HTML dashboard with metrics:
  - Total events, errors, warnings, info messages
  - Average build time
  - Commit frequency (last 24 hours)
  - Recent events timeline
  - Recent commits list
  - System status

.PARAMETER LogDir
  Directory containing CI log files (*.json). Default: "artifacts/overnight"

.PARAMETER OutputFile
  Output HTML file path. Default: "artifacts/overnight/dashboard.html"

.EXAMPLE
  # Generate dashboard with defaults
  .\Export-Dashboard.ps1

.EXAMPLE
  # Custom log directory and output
  .\Export-Dashboard.ps1 -LogDir "custom/logs" -OutputFile "reports/ci-dashboard.html"

.NOTES
  - Requires git for commit history
  - Skips malformed JSON lines automatically
  - Dashboard auto-refreshes timestamp on each generation
  - Open in browser: file:///path/to/dashboard.html
#>

param(
    [Parameter(HelpMessage = "Directory containing CI log files")]
    [ValidateNotNullOrEmpty()]
    [string]$LogDir = "artifacts/overnight",

    [Parameter(HelpMessage = "Output HTML file path")]
    [ValidateNotNullOrEmpty()]
    [string]$OutputFile = "artifacts/overnight/dashboard.html"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))

# Parse JSON logs
$logFiles = Get-ChildItem -Path (Join-Path $ProjectRoot $LogDir) -Filter "*.json" -ErrorAction SilentlyContinue
$events = @()

foreach ($file in $logFiles) {
    Get-Content $file.FullName | ForEach-Object {
        try {
            $events += $_ | ConvertFrom-Json
        } catch {
            # Skip malformed JSON lines
        }
    }
}

# Calculate metrics
$totalEvents = $events.Count
$errorCount = ($events | Where-Object { $_.level -eq "ERROR" }).Count
$warnCount = ($events | Where-Object { $_.level -eq "WARN" }).Count
$infoCount = ($events | Where-Object { $_.level -eq "INFO" }).Count

# Build time metrics (if available)
$buildEvents = $events | Where-Object { $_.message -like "*build*" }
$avgBuildTime = if ($buildEvents.Count -gt 0) {
    $times = $buildEvents | Where-Object { $_.duration } | ForEach-Object { $_.duration }
    if ($times) { ($times | Measure-Object -Average).Average } else { 0 }
} else { 0 }

# Commit frequency (last 24 hours)
$recentCommits = @()
$commitCount = 0
Push-Location $ProjectRoot
try {
    # Check if git is available
    $gitAvailable = Get-Command git -ErrorAction SilentlyContinue
    if ($gitAvailable) {
        $since = (Get-Date).AddDays(-1).ToString("yyyy-MM-dd")
        $recentCommits = git log --since=$since --oneline --no-merges 2>&1

        # Filter out error messages if git command failed
        if ($LASTEXITCODE -eq 0) {
            $commitCount = ($recentCommits | Measure-Object).Count
        } else {
            Write-Warning "Git log command failed: $recentCommits"
            $recentCommits = @()
        }
    } else {
        Write-Warning "Git not available - skipping commit history"
    }
} catch {
    Write-Warning "Failed to retrieve git history: $($_.Exception.Message)"
    $recentCommits = @()
} finally {
    Pop-Location
}

# Generate HTML
$html = @"
<!DOCTYPE html>
<html>
<head>
    <title>Overnight CI Dashboard</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }
        .header { background: #2c3e50; color: white; padding: 20px; border-radius: 8px; margin-bottom: 20px; }
        .header h1 { margin: 0; }
        .header .timestamp { opacity: 0.8; margin-top: 5px; }
        .metrics { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-bottom: 20px; }
        .metric-card { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .metric-card .value { font-size: 36px; font-weight: bold; color: #3498db; }
        .metric-card .label { color: #7f8c8d; margin-top: 5px; }
        .metric-card.error .value { color: #e74c3c; }
        .metric-card.warn .value { color: #f39c12; }
        .metric-card.success .value { color: #27ae60; }
        .section { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin-bottom: 20px; }
        .section h2 { margin-top: 0; color: #2c3e50; }
        .event-list { max-height: 400px; overflow-y: auto; }
        .event { padding: 10px; border-left: 4px solid #3498db; margin-bottom: 10px; background: #ecf0f1; }
        .event.ERROR { border-left-color: #e74c3c; }
        .event.WARN { border-left-color: #f39c12; }
        .event .time { font-size: 12px; color: #7f8c8d; }
        .commit-list { list-style: none; padding: 0; }
        .commit-list li { padding: 8px; border-bottom: 1px solid #ecf0f1; }
        .commit-list li:last-child { border-bottom: none; }
    </style>
</head>
<body>
    <div class="header">
        <h1>🤖 Overnight CI Dashboard</h1>
        <div class="timestamp">Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")</div>
    </div>

    <div class="metrics">
        <div class="metric-card">
            <div class="value">$totalEvents</div>
            <div class="label">Total Events</div>
        </div>
        <div class="metric-card error">
            <div class="value">$errorCount</div>
            <div class="label">Errors</div>
        </div>
        <div class="metric-card warn">
            <div class="value">$warnCount</div>
            <div class="label">Warnings</div>
        </div>
        <div class="metric-card success">
            <div class="value">$infoCount</div>
            <div class="label">Info Messages</div>
        </div>
        <div class="metric-card">
            <div class="value">$([Math]::Round($avgBuildTime, 1))s</div>
            <div class="label">Avg Build Time</div>
        </div>
        <div class="metric-card success">
            <div class="value">$commitCount</div>
            <div class="label">Commits (24h)</div>
        </div>
    </div>

    <div class="section">
        <h2>Recent Events</h2>
        <div class="event-list">
"@

# Add recent events (last 20)
$recentEvents = $events | Sort-Object -Property timestamp -Descending | Select-Object -First 20
foreach ($event in $recentEvents) {
    $level = if ($event.level) { $event.level } else { "INFO" }
    $time = if ($event.timestamp) { $event.timestamp } else { "Unknown" }
    $message = if ($event.message) { $event.message } else { "No message" }

    $html += @"
            <div class="event $level">
                <div class="time">$time</div>
                <div>$message</div>
            </div>
"@
}

$html += @"
        </div>
    </div>

    <div class="section">
        <h2>Recent Commits (Last 24 Hours)</h2>
        <ul class="commit-list">
"@

foreach ($commit in $recentCommits | Select-Object -First 10) {
    $html += "            <li>$commit</li>`n"
}

if ($recentCommits.Count -eq 0) {
    $html += "            <li>No commits in the last 24 hours</li>`n"
}

$html += @"
        </ul>
    </div>

    <div class="section">
        <h2>System Status</h2>
        <p><strong>Project Root:</strong> $ProjectRoot</p>
        <p><strong>Log Directory:</strong> $LogDir</p>
        <p><strong>Log Files Processed:</strong> $($logFiles.Count)</p>
    </div>
</body>
</html>
"@

# Write HTML to file
$outputPath = Join-Path $ProjectRoot $OutputFile
$outputDir = Split-Path -Parent $outputPath
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

Set-Content -Path $outputPath -Value $html

Write-Host "Dashboard exported to: $outputPath" -ForegroundColor Green
Write-Host "Open in browser: file:///$($outputPath -replace '\\', '/')" -ForegroundColor Cyan
