# Developer Setup Scripts

One-time setup scripts for new developers. Run these after cloning the repository.

---

## Quick Start

```powershell
# After cloning, run from PowerShell:
.\scripts\setup\setup_ue_env.ps1

# Then from Git Bash:
./scripts/setup/install_hooks.sh
```

---

## Available Scripts

### setup_ue_env.ps1

**Purpose:** Set UE_PATH environment variable for VS Code workspace.

**What it does:**
- Reads UE_PATH from `scripts/config/ue_path.conf` (single source of truth)
- Sets persistent User environment variable in Windows registry
- Updates `.vscode/*.json` files to use `${env:UE_PATH}` (or actual path for compileCommands)
- Safe to run multiple times (skips if already set correctly)

**Usage:**
```powershell
.\scripts\setup\setup_ue_env.ps1        # Normal setup
.\scripts\setup\setup_ue_env.ps1 -Force # Override existing value
```

**Why needed:** The `Alis.code-workspace` uses `${env:UE_PATH}` for the UE5 folder path, which requires a system environment variable.

---

### install_hooks.sh

**Purpose:** Install git hooks that automate build cache management.

**What it does:**
- Creates symlinks from `.git/hooks/` to `scripts/git/hooks/`
- Falls back to copying if symlinks unavailable (Windows without Developer Mode)
- Backs up any existing hooks before replacing
- Safe to run multiple times (skips already installed hooks)

**Details:** See [scripts/git/README.md](../git/README.md#installed-hooks) for:
- Installed hooks and triggers
- Why UBT doesn't handle Git natively
- Nuclear option (`ALIS_HOOK_CLEAN_BINARIES=1`)

---

## Troubleshooting

**"Permission denied" on Windows:**
```bash
# Run from Git Bash (not CMD/PowerShell)
bash scripts/setup/install_hooks.sh
```

**Symlink fails (Windows without Developer Mode):**
The installer automatically falls back to copying if symlinks are unavailable.
If you see "[OK] Installed (copy)", hooks work but won't auto-update.
Re-run `install_hooks.sh` after pulling hook updates.

To enable symlinks (optional):
```bash
# Enable Developer Mode in Windows Settings
# Settings -> Update & Security -> For developers -> Developer Mode
```

**Hook not running:**
```bash
# Check if hook exists and is executable
ls -la .git/hooks/post-merge

# Verify symlink target
readlink .git/hooks/post-merge
# Should show: ../../scripts/git/hooks/post-merge
```

---

## For CI/CD

CI environments typically don't need these hooks (they do clean builds).
Skip hook installation in CI by checking environment:

```bash
if [ -z "$CI" ]; then
    ./scripts/setup/install_hooks.sh
fi
```
