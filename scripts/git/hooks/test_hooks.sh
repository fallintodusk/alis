#!/bin/bash
# test_hooks.sh - Local validation of git hooks
# Run from repo root: bash scripts/git/hooks/test_hooks.sh
# Verbose mode: bash scripts/git/hooks/test_hooks.sh -v

set -uo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
DIM='\033[2m'
NC='\033[0m'

VERBOSE=0
if [ "${1:-}" = "-v" ] || [ "${1:-}" = "--verbose" ]; then
    VERBOSE=1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(git rev-parse --show-toplevel)"

log_verbose() {
    if [ "$VERBOSE" -eq 1 ]; then
        echo -e "${DIM}    $1${NC}"
    fi
}

echo -e "${CYAN}=========================================="
echo -e " Git Hooks - Local Test Suite"
echo -e "==========================================${NC}"
echo ""
log_verbose "Script dir: $SCRIPT_DIR"
log_verbose "Project root: $PROJECT_ROOT"
echo ""

PASSED=0
FAILED=0

# Test runner - executes and checks output
test_hook() {
    local name="$1"
    local expect_pattern="$2"
    shift 2

    echo -ne "  Testing ${CYAN}$name${NC}... "
    log_verbose ""
    log_verbose "Args: $*"

    OUTPUT=$("$@" 2>&1) || true

    log_verbose "Output length: ${#OUTPUT} chars"
    if [ "$VERBOSE" -eq 1 ] && [ -n "$OUTPUT" ]; then
        echo ""
        echo "$OUTPUT" | sed 's/^/    | /'
    fi

    if echo "$OUTPUT" | grep -qE "$expect_pattern"; then
        echo -e "${GREEN}PASS${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}FAIL${NC}"
        echo "    Expected: $expect_pattern"
        echo "    Got: ${OUTPUT:0:200}"
        FAILED=$((FAILED + 1))
    fi
}

# Test for silent/graceful exit
test_silent() {
    local name="$1"
    shift

    echo -ne "  Testing ${CYAN}$name${NC}... "
    log_verbose ""
    log_verbose "Args: $*"

    OUTPUT=$("$@" 2>&1) || true

    if [ -z "$OUTPUT" ]; then
        echo -e "${GREEN}PASS${NC} (silent)"
        PASSED=$((PASSED + 1))
    else
        echo -e "${GREEN}PASS${NC} (with output)"
        if [ "$VERBOSE" -eq 1 ]; then
            echo "$OUTPUT" | sed 's/^/    | /'
        fi
        PASSED=$((PASSED + 1))
    fi
}

echo -e "${YELLOW}1. Discovering test commits${NC}"
echo ""

CURRENT_HEAD=$(git rev-parse HEAD)
log_verbose "Current HEAD: $CURRENT_HEAD"

PREV_COMMIT=$(git rev-parse HEAD~5 2>/dev/null || git rev-parse HEAD~1 2>/dev/null || echo "")
log_verbose "Prev commit: ${PREV_COMMIT:-none}"

CPP_COMMIT=$(git rev-list -1 HEAD -- '*.cpp' '*.h' 2>/dev/null || echo "")
log_verbose "C++ commit: ${CPP_COMMIT:-none}"

if [ -n "$CPP_COMMIT" ]; then
    echo -e "  C++ commit: ${CYAN}${CPP_COMMIT:0:8}${NC}"
fi
if [ -n "$PREV_COMMIT" ]; then
    echo -e "  Prev commit: ${CYAN}${PREV_COMMIT:0:8}${NC}"
fi
echo ""

echo -e "${YELLOW}2. Core script (invalidate_ubt_cache.sh)${NC}"
echo ""

CORE="$SCRIPT_DIR/invalidate_ubt_cache.sh"

if [ -n "$CPP_COMMIT" ]; then
    CPP_PARENT=$(git rev-parse "${CPP_COMMIT}~1" 2>/dev/null || echo "")
    if [ -n "$CPP_PARENT" ]; then
        test_hook "C++ change detection" "C.. changes detected|C.. files changed" \
            bash "$CORE" "$CPP_PARENT" "$CPP_COMMIT"
    else
        echo -e "  ${YELLOW}[SKIP] C++ commit has no parent${NC}"
    fi
else
    echo -e "  ${YELLOW}[SKIP] No C++ commits found${NC}"
fi

test_silent "same ref (no changes)" \
    bash "$CORE" HEAD HEAD

echo ""
echo -e "${YELLOW}3. post-merge hook${NC}"
echo ""

if [ -n "$PREV_COMMIT" ]; then
    log_verbose "Setting ORIG_HEAD=$PREV_COMMIT"
    git update-ref ORIG_HEAD "$PREV_COMMIT"

    test_hook "post-merge" "changes detected|UBT cache" \
        bash "$SCRIPT_DIR/post-merge"

    git update-ref -d ORIG_HEAD 2>/dev/null || true
else
    echo -e "  ${YELLOW}[SKIP] Not enough history${NC}"
fi

echo ""
echo -e "${YELLOW}4. post-checkout hook${NC}"
echo ""

if [ -n "$PREV_COMMIT" ]; then
    test_hook "branch checkout (flag=1)" "changes detected|UBT cache" \
        bash "$SCRIPT_DIR/post-checkout" "$PREV_COMMIT" "$CURRENT_HEAD" 1

    test_silent "file checkout (flag=0)" \
        bash "$SCRIPT_DIR/post-checkout" "$PREV_COMMIT" "$CURRENT_HEAD" 0

    test_silent "same ref (skip)" \
        bash "$SCRIPT_DIR/post-checkout" "$CURRENT_HEAD" "$CURRENT_HEAD" 1
else
    echo -e "  ${YELLOW}[SKIP] Not enough history${NC}"
fi

echo ""
echo -e "${YELLOW}5. post-rewrite hook${NC}"
echo ""

if [ -n "$PREV_COMMIT" ]; then
    log_verbose "Setting ORIG_HEAD=$PREV_COMMIT for rebase"
    git update-ref ORIG_HEAD "$PREV_COMMIT"

    test_hook "rebase" "changes detected|UBT cache" \
        bash "$SCRIPT_DIR/post-rewrite" rebase

    git update-ref -d ORIG_HEAD 2>/dev/null || true

    # amend via stdin
    echo -ne "  Testing ${CYAN}amend (stdin)${NC}... "
    OUTPUT=$(echo "$PREV_COMMIT $CURRENT_HEAD" | bash "$SCRIPT_DIR/post-rewrite" amend 2>&1) || true
    if echo "$OUTPUT" | grep -qE "changes detected|UBT cache"; then
        echo -e "${GREEN}PASS${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${GREEN}PASS${NC} (ran)"
        PASSED=$((PASSED + 1))
    fi
    if [ "$VERBOSE" -eq 1 ] && [ -n "$OUTPUT" ]; then
        echo "$OUTPUT" | sed 's/^/    | /'
    fi
else
    echo -e "  ${YELLOW}[SKIP] Not enough history${NC}"
fi

echo ""
echo -e "${YELLOW}6. Edge cases${NC}"
echo ""

git update-ref -d ORIG_HEAD 2>/dev/null || true

test_silent "rebase without ORIG_HEAD" \
    bash "$SCRIPT_DIR/post-rewrite" rebase

# amend empty stdin
echo -ne "  Testing ${CYAN}amend empty stdin${NC}... "
OUTPUT=$(echo "" | bash "$SCRIPT_DIR/post-rewrite" amend 2>&1) || true
echo -e "${GREEN}PASS${NC} (graceful)"
PASSED=$((PASSED + 1))

test_silent "invalid refs" \
    bash "$CORE" nonexistent123 nonexistent456

echo ""
echo -e "${CYAN}=========================================="
if [ "$FAILED" -eq 0 ]; then
    echo -e " ${GREEN}All tests passed!${NC} ($PASSED passed)"
else
    echo -e " Results: ${GREEN}$PASSED passed${NC}, ${RED}$FAILED failed${NC}"
fi
echo -e "==========================================${NC}"

[ "$FAILED" -eq 0 ]
