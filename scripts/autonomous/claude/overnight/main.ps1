<#
.SYNOPSIS
  Non-stop entry script for Overnight Autonomous CI.

.DESCRIPTION
  Sets up branch/logs and prints a NON-STOP prompt aligned with SKILL.md. Use this as the primary entry.
  Runs until all agent-compatible TODOs are complete (manual/editor tasks are optional by design).

.USAGE
  powershell scripts/autonomous/claude/overnight/main.ps1

.NOTES
  - Creates branch: ai/autonomous-dev-YYYYMMDD (push blocked by pre-push hook)
  - Logs to: artifacts/overnight/run-YYYYMMDD-HHMMSS.log
  - Helper: scripts/autonomous/common/discover_todos.ps1
#>

param(
    [switch]$NewBranch = $false,
    [switch]$DryRun = $false
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir)))
$Date = Get-Date -Format "yyyyMMdd"
$Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$LogFile = "$ProjectRoot\artifacts\overnight\run-$Timestamp.log"

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $LogMessage = "[$((Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))] [$Level] $Message"
    Write-Host $LogMessage
    if (-not $DryRun) { Add-Content -Path $LogFile -Value $LogMessage }
}

function Ensure-ArtifactsDir {
    $ArtifactsDir = "$ProjectRoot\artifacts\overnight"
    if (-not (Test-Path $ArtifactsDir)) { New-Item -ItemType Directory -Path $ArtifactsDir -Force | Out-Null }
}

function Test-Prerequisites {
    Write-Log "Pre-flight checks..."
    if (-not (Test-Path "$ProjectRoot\.git")) { throw "Not a git repository: $ProjectRoot" }
    if (-not (Test-Path "$ProjectRoot\.claude\skills\ue-gamefeature-ci\SKILL.md")) { throw "Skill file missing: .claude/skills/ue-gamefeature-ci/SKILL.md" }
    Ensure-ArtifactsDir
}

function Initialize-GitBranch {
    Push-Location $ProjectRoot
    try {
        if ($NewBranch) {
            $BranchName = "ai/autonomous-dev-$Date"
            Write-Log "Creating new branch: $BranchName"
            if (-not $DryRun) {
                $exists = git branch --list $BranchName
                if ($exists) {
                    Write-Log "Branch $BranchName already exists, switching to it" "WARN"
                    git switch $BranchName
                }
                else {
                    git switch -c $BranchName
                    Write-Log "Created and switched to $BranchName"
                }
            }
            else {
                Write-Log "[DryRun] Would switch/create branch $BranchName"
            }
        }
        else {
            $CurrentBranch = git branch --show-current
            Write-Log "Working on current branch: $CurrentBranch"
        }

        Write-Log "Disabling git push to origin (Safeguard)"
        if (-not $DryRun) {
            git remote set-url --push origin DISABLED
        }
        else {
            Write-Log "[DryRun] Would set --push origin DISABLED"
        }
    }
    finally { Pop-Location }
}

function Show-NonStopPrompt {
    Push-Location $ProjectRoot
    $CurrentBranch = git branch --show-current
    Pop-Location

    $Prompt = @"
Use the UE GameFeature Autonomous CI skill in INFINITE LOOP mode.

========================================
INFINITE LOOP PROTOCOL:
========================================

1. Check TODOs status:
   powershell scripts/autonomous/common/discover_todos.ps1

2. If TODOs > 0:
   - Pick highest priority unchecked task from todo/*.md
   - Implement (C++ code, tests, configs, docs)
   - Build -> Test -> Commit
   - Update artifacts\overnight\progress.md
   - GOTO step 1 (loop continues)

3. If TODOs = 0:
   - Create new extension todos based on architecture analysis
   - Review docs/architecture/*.md for gaps
   - Identify missing tests, error handling, logging improvements
   - Propose new features aligned with roadmap
   - Create todo/extend_[feature].md with new checkboxes
   - GOTO step 1 (loop continues with new tasks)

========================================
NEVER STOP CONDITIONS:
========================================
- [X] NEVER say "all done" and stop
- [X] NEVER wait for user input
- [OK] ALWAYS find next improvement
- [OK] ALWAYS commit progress

Build & Test Commands:
1) powershell scripts\ue\build\rebuild_module_safe.ps1 -Clean
2) powershell scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "Project.*"

Branch: $CurrentBranch (push blocked, safe for overnight)
Log:    $LogFile

Infinite loop = continuous improvement until external termination.
"@
    Write-Log "--- INFINITE LOOP PROMPT ---"
    Write-Log $Prompt
    Write-Log "--- END PROMPT ---"
}

function Main {
    Write-Log "========================================"
    Write-Log "Overnight Autonomous CI (Infinite Loop)"
    Write-Log "========================================"
    Write-Log "Timestamp: $Timestamp"
    try {
        Test-Prerequisites
        Initialize-GitBranch
        Show-NonStopPrompt
        Write-Log ""
        Write-Log "Setup complete!"
        Write-Log "Copy the INFINITE LOOP PROMPT above and paste it into Claude Code."
        Write-Log "Agent will run continuously until you stop it (Ctrl+C or close window)."
    }
    catch {
        Write-Log $_.Exception.Message "ERROR"
        exit 1
    }
}

Main
