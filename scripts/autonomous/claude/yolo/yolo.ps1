# YOLO Mode - Autonomous CI with --dangerously-skip-permissions
#
# WARNING: This script runs Claude Code in fully autonomous mode without permission prompts.
# Only use on isolated branches with git push disabled.
#
# Safety features:
# - Git push explicitly blocked
# - Can execute all scripts within repository
# - Unlimited turns (requires unlimited tariff)
# - Restrictive tool whitelist

param(
    [string]$Branch = ""
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "YOLO Mode - Autonomous CI" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check current branch
$currentBranch = git rev-parse --abbrev-ref HEAD
Write-Host "Current branch: $currentBranch" -ForegroundColor Yellow

if ($Branch -ne "" -and $currentBranch -ne $Branch) {
    Write-Host "ERROR: Expected branch '$Branch', but on '$currentBranch'" -ForegroundColor Red
    exit 1
}

# Check if push is disabled (safety check)
$pushUrl = git remote get-url --push origin 2>$null
if ($pushUrl -eq "DISABLED") {
    Write-Host "✅ Git push is DISABLED (safe)" -ForegroundColor Green
} else {
    Write-Host "⚠️  WARNING: Git push is NOT disabled!" -ForegroundColor Yellow
    Write-Host "   Recommend running: git remote set-url --push origin DISABLED" -ForegroundColor Yellow
    Write-Host ""
    $confirm = Read-Host "Continue anyway? (y/N)"
    if ($confirm -ne "y") {
        Write-Host "Aborted." -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "Starting YOLO mode (unlimited turns)..." -ForegroundColor Cyan
Write-Host ""

# Run Claude in YOLO mode
claude `
  --dangerously-skip-permissions `
  --allowedTools "Read" "Write" "Edit" "Grep" "Glob" "Task" `
  --allowedTools "Bash(git status:*)" "Bash(git diff:*)" "Bash(git add:*)" "Bash(git commit:*)" `
  --allowedTools "Bash(git log:*)" "Bash(git rev-parse:*)" "Bash(git for-each-ref:*)" `
  --allowedTools "Bash(powershell.exe:*)" `
  --allowedTools "Bash(cmd:*)" `
  --allowedTools "Bash(timeout:*)" "Bash(taskkill:*)" "Bash(python:*)" `
  --allowedTools "Bash(*.bat:*)" "Bash(*.ps1:*)" "Bash(*.sh:*)" `
  --allowedTools "Bash(<ue-path>/Engine/Build/BatchFiles/Build.bat:*)" `
  --allowedTools "Bash(<ue-path>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe:*)" `
  --disallowedTools "Bash(git push:*)" "Bash(rm -rf:*)" "Bash(del /s /q:*)" `
  -p "Use the ue-gamefeature-ci skill. Do all tasks by priority, keep todos tiny, compact, consistent, mark done tasks. For manual todo leave one file that consist only critical doing in editor, all others tasks such as widget overridden should be optional - hot reload configs should support it. Then main goal to achieve it's like ROM system - check docs architecture - it's about not updatable module like BIOS with tiny UI that could update core game feature. So system shouldn't have by default links to GF that could restrict its updating, removing or adding. As the result we must have tiny core plugin, and at the first start it proposes to user add core game feature, that after loading proposes update dependencies. On the further start user could be proposed update some dependency in case outdated from core game feature."

$exitCode = $LASTEXITCODE

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
if ($exitCode -eq 0) {
    Write-Host "✅ YOLO session completed successfully" -ForegroundColor Green
} else {
    Write-Host "❌ YOLO session failed (exit code: $exitCode)" -ForegroundColor Red
}
Write-Host "========================================" -ForegroundColor Cyan

exit $exitCode
