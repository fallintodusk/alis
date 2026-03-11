# Git Workflow Scripts

Automation scripts for git operations in the Alis project.

---

## Quick Setup

**New developers:** Install git hooks after cloning:

```bash
./scripts/setup/install_hooks.sh
```

See [../setup/README.md](../setup/README.md) for details.

---

## hooks/

Shared git hooks (installed via symlinks to `.git/hooks/`).

**Problem solved:** After pulling/switching C++ changes, UE's timestamp-based build system may not detect outdated binaries. This causes developers to run stale code until they manually clean and rebuild.

**Why UBT doesn't handle this:** UBT's `bUseAdaptiveUnityBuild` uses **read-only file flags** to identify the "working set" for adaptive non-unity builds - assumes Perforce-style writable checkout. **Git doesn't mark files read-only**, so this heuristic fails. No built-in UBT setting exists for Git workflows. See [Epic's Build Configuration docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-configuration-for-unreal-engine).

### Installed Hooks

| Hook | Trigger | When |
|------|---------|------|
| `post-merge` | `git pull`, `git merge` | After merge completes |
| `post-checkout` | `git checkout`, `git switch` | After branch switch |
| `post-rewrite` | `git rebase`, `git commit --amend` | After history rewrite |

### What They Do

1. Detect changes to `.cpp`, `.h`, `.hpp`, `.c`, `.inl` files (via git pathspec - fast)
2. Detect changes to `.Build.cs`, `.Target.cs`, `.uproject`, `.uplugin` files
3. Delete UBT makefile caches in `Intermediate/`
4. Delete `ActionHistory.bin` (stale action graphs)
5. Touch `.uproject` to update timestamp

### Nuclear Option: Delete Binaries

For risky merges with build config changes, opt-in to also delete `Binaries/`:

```bash
ALIS_HOOK_CLEAN_BINARIES=1 git pull
```

This removes all compiled DLLs - use when `.Build.cs`/`.uplugin` changes cause weirdness.

### Output Example

```
==========================================
 Git Hook: C++ changes detected
==========================================
  C++ files changed: 5
  Build configs changed: 1
  [OK] UBT cache invalidated
  Next build will recompile changed modules.
==========================================
```

### Architecture

```
scripts/git/hooks/
  invalidate_ubt_cache.sh   # Shared core logic
  post-merge                 # Calls shared script with ORIG_HEAD HEAD
  post-checkout              # Calls shared script with $1 $2 (old/new refs)
  post-rewrite               # Rebase: ORIG_HEAD..HEAD; Amend: stdin old/new pair
``` 

---

## mirror/

Publish a filtered public snapshot of the repository to a separate remote repository.

Documentation:
- [mirror/README.md](mirror/README.md)

Quick usage:
```bash
make mirror MIRROR_DRY_RUN=1 MIRROR_FORCE=1
make mirror MIRROR_REMOTE_URL=git@github.com:org/repo.git
```

The mirror flow:
1. reads committed `HEAD` only;
2. applies a blacklist from `scripts/git/mirror/mirror.exclude`;
3. runs hard validation for forbidden file types and text patterns;
4. commits in a temp repo;
5. pushes only to the mirror remote.

---

## squash_merge.sh

**Purpose:** Squash merge AI work branches into current branch with clean history.

**Use case:** After autonomous AI development sessions, merge all work from temporary branches (e.g., `ai/autonomous-dev-*`) into your main working branch with a single commit.

### Features

- ✅ **Safety checks**: Validates working directory, branch existence, protected branches
- ✅ **Clean history**: Single merge commit instead of full branch history
- ✅ **Interactive**: Prompts for commit message (pre-filled with summary)
- ✅ **Optional cleanup**: Prompts to delete source branch after merge
- ✅ **Clear feedback**: Colored output with progress indicators

### Usage

```bash
# Basic usage
make merge-ai BRANCH=ai/autonomous-dev-20251029

# Example workflow
git checkout main                          # Switch to target branch
make merge-ai BRANCH=ai/task-123           # Merge AI work
# Script will:
# 1. Check safety (clean working dir, branch exists, etc.)
# 2. Show commit/file counts
# 3. Perform squash merge
# 4. Open editor for commit message (pre-filled)
# 5. Commit changes
# 6. Prompt to delete source branch
```

### Safety Features

**Pre-merge checks:**
- Working directory must be clean (no uncommitted changes)
- Source branch must exist
- Warns if on protected branch (main/master)
- Cannot merge branch into itself

**If checks fail:** Script aborts with clear error message

### Commit Message

**Interactive editor** opens with pre-filled template:

```
Merge AI work from ai/autonomous-dev-20251029

Summary of changes:
- 15 commits squashed
- 23 files changed

# Edit this message as needed. Lines starting with # are ignored.
#
# Commits included:
# - abc1234 Fix build errors in ProjectMenuUI
# - def5678 Add unit tests for ViewModels
# - ...
```

**Editor:** Uses `$EDITOR` environment variable (defaults to `nano`)

### Branch Deletion

**After successful merge**, script prompts:

```
Delete source branch 'ai/autonomous-dev-20251029'? [y/N]
```

- `y` - Delete branch immediately
- `n` (default) - Keep branch (delete manually later with `git branch -D <branch>`)

### Output Example

```
ℹ Squash Merge: ai/autonomous-dev-20251029 → main

ℹ Running safety checks...
✓ Working directory clean
✓ Source branch exists

ℹ Branch comparison:
  Commits to merge: 15
  Files changed: 23

ℹ Recent commits from ai/autonomous-dev-20251029:
* abc1234 Fix build errors in ProjectMenuUI
* def5678 Add unit tests for ViewModels
* ...

Proceed with squash merge? [Y/n] y

ℹ Performing squash merge...
✓ Squash merge completed

ℹ Opening editor for commit message...
✓ Changes committed

ℹ Merge result:
* hij9012 (HEAD -> main) Merge AI work from ai/autonomous-dev-20251029
* abc1234 Previous commit
* def5678 Earlier commit

Delete source branch 'ai/autonomous-dev-20251029'? [y/N] y
✓ Branch 'ai/autonomous-dev-20251029' deleted

✓ Squash merge complete!
ℹ Current branch: main
```

### Troubleshooting

**Error: "Working directory is not clean"**
```bash
# Check status
git status

# Commit or stash changes
git stash
# or
git add . && git commit -m "WIP"
```

**Error: "Source branch does not exist"**
```bash
# List all branches
git branch -a

# Fetch remote branches if needed
git fetch --all
```

**Merge conflicts**
```bash
# If squash merge fails with conflicts
git status                    # See conflicted files
# Edit files to resolve conflicts
git add .                     # Stage resolved files
git commit                    # Complete the merge
```

---

## Best Practices

1. **Before merge**: Ensure source branch is up to date
   ```bash
   git checkout ai/task-123
   git pull origin ai/task-123  # If remote tracking
   git checkout main
   make merge-ai BRANCH=ai/task-123
   ```

2. **Commit message**: Write clear summary of what was accomplished
   - Focus on WHAT was achieved, not HOW (git history has the details)
   - Example: "Implement overnight CI system with autonomous testing"

3. **Branch naming**: Use consistent patterns
   - `ai/autonomous-dev-YYYYMMDD` - Overnight autonomous work
   - `ai/task-123` - Specific task/feature work
   - `ai/fix-issue-456` - Bug fixes

4. **After merge**: Push to remote if needed
   ```bash
   git push origin main
   ```

---

## SOLID Principles

**Single Responsibility**: Script only handles squash merge workflow
**Open/Closed**: Extensible via parameters, closed to modification
**Dependency Inversion**: Delegates to git commands, not tied to implementation

**Location rationale**:
- ❌ Not `scripts/autonomous/` (developer workflow, not autonomous agent)
- ❌ Not `scripts/ue/` (git operation, not UE-specific)
- ✅ `scripts/git/` (dedicated git workflow automation)
