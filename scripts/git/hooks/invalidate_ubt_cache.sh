#!/bin/bash
# invalidate_ubt_cache.sh - Force UBT to recompile after git operations
#
# PROBLEM: UE's build system assumes Perforce (read-only files until checkout).
# Git doesn't mark files read-only, so after pull/merge, timestamps can be
# OLDER than binaries - UBT skips recompilation. This script fixes that.
#
# Usage: ./invalidate_ubt_cache.sh OLD_REF NEW_REF
# Env:   ALIS_HOOK_CLEAN_BINARIES=1 to also delete Binaries/ (nuclear option)

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

OLD_REF="${1:-ORIG_HEAD}"
NEW_REF="${2:-HEAD}"

# git rev-parse works regardless of symlinks, worktrees, or submodules
PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [ -z "$PROJECT_ROOT" ] || [ ! -d "$PROJECT_ROOT" ]; then
    exit 0
fi

# Pathspec filtering: O(relevant files) not O(all files in diff)
# Fast even when merge touches 30k .uasset files
CPP_CHANGES="$(git diff --name-only "$OLD_REF" "$NEW_REF" -- \
    '*.cpp' '*.h' '*.hpp' '*.c' '*.inl' 2>/dev/null || true)"

# .uplugin affects module loading order - changes need rebuild
BUILD_CHANGES="$(git diff --name-only "$OLD_REF" "$NEW_REF" -- \
    '*.Build.cs' '*.Target.cs' '*.uproject' '*.uplugin' 2>/dev/null || true)"

# Silent exit for content-only merges (most common case)
if [ -z "$CPP_CHANGES" ] && [ -z "$BUILD_CHANGES" ]; then
    exit 0
fi

echo ""
echo -e "${CYAN}=========================================="
echo -e " Git Hook: C++ changes detected"
echo -e "==========================================${NC}"

CPP_COUNT=0
BUILD_COUNT=0
[ -n "$CPP_CHANGES" ] && CPP_COUNT=$(echo "$CPP_CHANGES" | wc -l | tr -d ' ')
[ -n "$BUILD_CHANGES" ] && BUILD_COUNT=$(echo "$BUILD_CHANGES" | wc -l | tr -d ' ')

[ "$CPP_COUNT" -gt 0 ] && echo -e "${YELLOW}  C++ files changed: ${CPP_COUNT}${NC}"
[ "$BUILD_COUNT" -gt 0 ] && echo -e "${YELLOW}  Build configs changed: ${BUILD_COUNT}${NC}"

INVALIDATED=0

if [ -d "$PROJECT_ROOT/Intermediate" ]; then
    # .ubtmakefile: UBT's cached dependency graph - delete to force re-scan
    if find "$PROJECT_ROOT/Intermediate" -name "*.ubtmakefile" 2>/dev/null | grep -q .; then
        find "$PROJECT_ROOT/Intermediate" -name "*.ubtmakefile" -delete 2>/dev/null || true
        INVALIDATED=1
    fi

    # ActionHistory.bin: tracks completed actions - stale = skipped rebuilds
    # UE5 places these under platform/arch subdirs (e.g. Build/Win64/x64/AlisEditor/)
    if find "$PROJECT_ROOT/Intermediate" -name "ActionHistory.bin" 2>/dev/null | grep -q .; then
        find "$PROJECT_ROOT/Intermediate" -name "ActionHistory.bin" -delete 2>/dev/null || true
        INVALIDATED=1
    fi
fi

# Touch .uproject so UE tools see "recent modification"
UPROJECT=$(find "$PROJECT_ROOT" -maxdepth 1 -name "*.uproject" 2>/dev/null | head -1)
[ -n "$UPROJECT" ] && touch "$UPROJECT" 2>/dev/null || true

# Nuclear option: full Binaries/ deletion (10+ min rebuild)
# Only for .Build.cs/.uplugin weirdness - makefile deletion handles 99% of cases
# -- prevents path injection if PROJECT_ROOT contains dashes
if [ -n "$BUILD_CHANGES" ] && [ "${ALIS_HOOK_CLEAN_BINARIES:-0}" = "1" ]; then
    rm -rf -- "$PROJECT_ROOT/Binaries" 2>/dev/null || true
    find "$PROJECT_ROOT/Plugins" -type d -name Binaries -prune -exec rm -rf -- {} + 2>/dev/null || true
    echo -e "${GREEN}  [OK] Binaries removed (ALIS_HOOK_CLEAN_BINARIES=1)${NC}"
fi

if [ "$INVALIDATED" -eq 1 ]; then
    echo -e "${GREEN}  [OK] UBT cache invalidated${NC}"
else
    echo -e "${YELLOW}  [!] No UBT cache found (clean state)${NC}"
fi

echo -e "${CYAN}  Next build will recompile changed modules.${NC}"
echo -e "${CYAN}==========================================${NC}"
echo ""
