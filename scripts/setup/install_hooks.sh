#!/bin/bash
# install_hooks.sh - Install git hooks for UBT cache invalidation
# Run once after cloning. Safe to re-run (skips existing, backs up different).
# Usage: bash scripts/setup/install_hooks.sh

set -uo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}"
echo "=========================================="
echo " Alis Project - Git Hooks Setup"
echo "=========================================="
echo -e "${NC}"

# Worktree-safe: .git can be file (worktree) or dir (normal clone)
PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
HOOKS_TARGET="$(git rev-parse --git-path hooks 2>/dev/null || true)"

if [ -z "$PROJECT_ROOT" ] || [ -z "$HOOKS_TARGET" ] || [ ! -d "$HOOKS_TARGET" ]; then
    echo -e "${RED}[ERROR] Not a git repository (or hooks dir missing)${NC}"
    exit 1
fi

HOOKS_SOURCE="$PROJECT_ROOT/scripts/git/hooks"

if [ ! -d "$HOOKS_SOURCE" ]; then
    echo -e "${RED}[ERROR] Hooks source not found: $HOOKS_SOURCE${NC}"
    exit 1
fi

echo -e "Source: ${CYAN}$HOOKS_SOURCE${NC}"
echo -e "Target: ${CYAN}$HOOKS_TARGET${NC}"
echo ""

HOOK_NAMES="post-merge post-checkout post-rewrite"
INSTALLED=0 SKIPPED=0 COPIED=0

for HOOK_NAME in $HOOK_NAMES; do
    HOOK="$HOOKS_SOURCE/$HOOK_NAME"
    TARGET="$HOOKS_TARGET/$HOOK_NAME"

    [ ! -f "$HOOK" ] && continue

    # Skip if already installed (symlink to our script)
    if [ -L "$TARGET" ]; then
        LINK=$(readlink "$TARGET" 2>/dev/null || true)
        if [[ "$LINK" == *"scripts/git/hooks/$HOOK_NAME"* ]]; then
            echo -e "${YELLOW}[SKIP] $HOOK_NAME (symlink exists)${NC}"
            SKIPPED=$((SKIPPED + 1))
            continue
        fi
    fi

    # Skip if copy matches (compare header comment)
    if [ -f "$TARGET" ] && [ ! -L "$TARGET" ]; then
        if [ "$(head -2 "$TARGET" | tail -1)" = "$(head -2 "$HOOK" | tail -1)" ]; then
            echo -e "${YELLOW}[SKIP] $HOOK_NAME (copy exists)${NC}"
            SKIPPED=$((SKIPPED + 1))
            continue
        fi
        # Backup different existing hook
        mv "$TARGET" "$TARGET.backup.$(date +%Y%m%d%H%M%S)"
    fi

    [ -L "$TARGET" ] && rm "$TARGET"

    # Symlink preferred (auto-updates); copy fallback for Windows without Developer Mode
    if ln -s "../../scripts/git/hooks/$HOOK_NAME" "$TARGET" 2>/dev/null; then
        chmod +x "$TARGET"
        echo -e "${GREEN}[OK] $HOOK_NAME (symlink)${NC}"
        INSTALLED=$((INSTALLED + 1))
    else
        cp "$HOOK" "$TARGET" && chmod +x "$TARGET"
        echo -e "${GREEN}[OK] $HOOK_NAME (copy)${NC}"
        COPIED=$((COPIED + 1))
    fi
done

# Shared script always copied (hooks prefer repo version anyway)
cp "$HOOKS_SOURCE/invalidate_ubt_cache.sh" "$HOOKS_TARGET/" && chmod +x "$HOOKS_TARGET/invalidate_ubt_cache.sh"
echo -e "${GREEN}[OK] invalidate_ubt_cache.sh${NC}"

echo ""
echo -e "${CYAN}==========================================${NC}"
TOTAL=$((INSTALLED + COPIED))
[ "$TOTAL" -gt 0 ] && echo -e "${GREEN}Installed: $TOTAL${NC}"
[ "$SKIPPED" -gt 0 ] && echo -e "${YELLOW}Skipped: $SKIPPED (already installed)${NC}"
[ "$COPIED" -gt 0 ] && echo -e "${YELLOW}Note: Re-run after pulling hook updates${NC}"
echo -e "${CYAN}==========================================${NC}"
