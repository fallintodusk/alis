#!/bin/bash
# Squash merge AI work branches into current branch with clean history
# Usage: scripts/git/squash_merge.sh <source-branch>

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
error() {
    echo -e "${RED}✗ Error: $1${NC}" >&2
    exit 1
}

warning() {
    echo -e "${YELLOW}⚠ Warning: $1${NC}"
}

info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Validate input
if [ $# -eq 0 ]; then
    error "Missing branch parameter. Usage: make merge-ai BRANCH=<branch-name>"
fi

SOURCE_BRANCH="$1"
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

echo ""
info "Squash Merge: ${SOURCE_BRANCH} → ${CURRENT_BRANCH}"
echo ""

# Safety checks
info "Running safety checks..."

# Check 1: Working directory clean
if ! git diff-index --quiet HEAD -- 2>/dev/null; then
    error "Working directory is not clean. Commit or stash changes first.\n       Run: git status"
fi
success "Working directory clean"

# Check 2: Source branch exists
if ! git rev-parse --verify "${SOURCE_BRANCH}" >/dev/null 2>&1; then
    error "Source branch '${SOURCE_BRANCH}' does not exist"
fi
success "Source branch exists"

# Check 3: Not on protected branch
if [[ "${CURRENT_BRANCH}" == "main" || "${CURRENT_BRANCH}" == "master" ]]; then
    warning "You are on protected branch '${CURRENT_BRANCH}'"
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        info "Aborted by user"
        exit 0
    fi
fi

# Check 4: Source branch is different from current
if [ "${SOURCE_BRANCH}" == "${CURRENT_BRANCH}" ]; then
    error "Cannot merge branch into itself"
fi

# Show branch comparison
echo ""
info "Branch comparison:"
COMMIT_COUNT=$(git rev-list --count ${CURRENT_BRANCH}..${SOURCE_BRANCH} 2>/dev/null || echo "0")
FILES_CHANGED=$(git diff --name-only ${CURRENT_BRANCH}...${SOURCE_BRANCH} | wc -l)

echo "  Commits to merge: ${COMMIT_COUNT}"
echo "  Files changed: ${FILES_CHANGED}"
echo ""

# Show recent commits from source branch
if [ "${COMMIT_COUNT}" -gt "0" ]; then
    info "Recent commits from ${SOURCE_BRANCH}:"
    git log --oneline --graph --max-count=5 ${CURRENT_BRANCH}..${SOURCE_BRANCH}
    echo ""
fi

# Confirm merge
read -p "Proceed with squash merge? [Y/n] " -n 1 -r
echo
if [[ $REPLY =~ ^[Nn]$ ]]; then
    info "Aborted by user"
    exit 0
fi

# Perform squash merge
info "Performing squash merge..."
if ! git merge --squash "${SOURCE_BRANCH}"; then
    error "Squash merge failed. Resolve conflicts and run:\n       git commit"
fi
success "Squash merge completed"

# Generate commit message
COMMIT_MSG_FILE=$(mktemp)
trap "rm -f ${COMMIT_MSG_FILE}" EXIT

# Pre-fill commit message with summary
cat > "${COMMIT_MSG_FILE}" <<EOF
Merge AI work from ${SOURCE_BRANCH}

Summary of changes:
- ${COMMIT_COUNT} commits squashed
- ${FILES_CHANGED} files changed

# Edit this message as needed. Lines starting with # are ignored.
#
# Commits included:
EOF

# Append commit list
git log --oneline ${CURRENT_BRANCH}..${SOURCE_BRANCH} | sed 's/^/# - /' >> "${COMMIT_MSG_FILE}"

# Open editor for commit message
info "Opening editor for commit message..."
${EDITOR:-nano} "${COMMIT_MSG_FILE}"

# Remove comment lines
FINAL_MSG=$(grep -v '^#' "${COMMIT_MSG_FILE}" | grep -v '^$' || true)

if [ -z "${FINAL_MSG}" ]; then
    warning "Commit message is empty"
    read -p "Use default message? [Y/n] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        git reset --mixed HEAD
        error "Commit aborted by user. Run: git reset --mixed HEAD (already done)"
    fi
    FINAL_MSG="Merge AI work from ${SOURCE_BRANCH}"
fi

# Commit the squash merge
if ! git commit -m "${FINAL_MSG}"; then
    error "Commit failed"
fi
success "Changes committed"

# Show result
echo ""
info "Merge result:"
git log --oneline --graph --max-count=3
echo ""

# Prompt for branch deletion
read -p "Delete source branch '${SOURCE_BRANCH}'? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if git branch -D "${SOURCE_BRANCH}"; then
        success "Branch '${SOURCE_BRANCH}' deleted"
    else
        warning "Failed to delete branch (may not be fully merged)"
    fi
else
    info "Branch '${SOURCE_BRANCH}' kept (delete manually with: git branch -D ${SOURCE_BRANCH})"
fi

echo ""
success "Squash merge complete!"
info "Current branch: ${CURRENT_BRANCH}"
echo ""
