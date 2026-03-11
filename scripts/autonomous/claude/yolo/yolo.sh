#!/bin/bash
# YOLO Mode - Autonomous CI with --dangerously-skip-permissions (WSL/Linux version)
#
# WARNING: This script runs Claude Code in fully autonomous mode without permission prompts.
# Only use on isolated branches with git push disabled.
#
# Safety features:
# - Git push explicitly blocked
# - Can execute all scripts within repository
# - Unlimited turns (requires unlimited tariff)
# - Restrictive tool whitelist

set -e

echo "========================================"
echo "YOLO Mode - Autonomous CI"
echo "========================================"
echo ""

# Check current branch
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
echo "Current branch: $CURRENT_BRANCH"

# Check if push is disabled (safety check)
PUSH_URL=$(git remote get-url --push origin 2>/dev/null || echo "")
if [ "$PUSH_URL" = "DISABLED" ]; then
    echo "✅ Git push is DISABLED (safe)"
else
    echo "⚠️  WARNING: Git push is NOT disabled!"
    echo "   Recommend running: git remote set-url --push origin DISABLED"
    echo ""
    read -p "Continue anyway? (y/N): " confirm
    if [ "$confirm" != "y" ]; then
        echo "Aborted."
        exit 1
    fi
fi

echo ""
echo "Starting YOLO mode (unlimited turns)..."
echo ""

# Run Claude in YOLO mode
claude \
  --dangerously-skip-permissions \
  --allowedTools "Read" "Write" "Edit" "Grep" "Glob" "Task" \
  --allowedTools "Bash(git status:*)" "Bash(git diff:*)" "Bash(git add:*)" "Bash(git commit:*)" \
  --allowedTools "Bash(git log:*)" "Bash(git rev-parse:*)" "Bash(git for-each-ref:*)" \
  --allowedTools "Bash(powershell.exe:*)" \
  --allowedTools "Bash(cmd:*)" \
  --allowedTools "Bash(timeout:*)" "Bash(taskkill:*)" "Bash(python:*)" \
  --allowedTools "Bash(*.bat:*)" "Bash(*.ps1:*)" "Bash(*.sh:*)" \
  --allowedTools "Bash(<ue-path>/Engine/Build/BatchFiles/Build.bat:*)" \
  --allowedTools "Bash(<ue-path>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe:*)" \
  --disallowedTools "Bash(git push:*)" "Bash(rm -rf:*)" "Bash(del /s /q:*)" \
  -p "Use the ue-gamefeature-ci skill. Do all tasks by priority, keep todos tiny, compact, consistent, mark done tasks. For manual todo leave one file that consist only critical doing in editor, all others tasks such as widget overridden should be optional - hot reload configs should support it. Then main goal to achieve it's like ROM system - check docs architecture - it's about not updatable module like BIOS with tiny UI that could update core game feature. So system shouldn't have by default links to GF that could restrict its updating, removing or adding. As the result we must have tiny core plugin, and at the first start it proposes to user add core game feature, that after loading proposes update dependencies. On the further start user could be proposed update some dependency in case outdated from core game feature."

EXIT_CODE=$?

echo ""
echo "========================================"
if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ YOLO session completed successfully"
else
    echo "❌ YOLO session failed (exit code: $EXIT_CODE)"
fi
echo "========================================"

exit $EXIT_CODE
