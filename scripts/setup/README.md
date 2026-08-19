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

**Purpose:** Derive machine-local engine state from the repository SOT.

**What it does:**
- Reads UE_PATH from `scripts/config/ue_path.conf` (single source of truth)
- Sets persistent User environment variable in Windows registry
- Updates `.vscode`, local agent settings, and MCP configuration
- Moves only the previously selected engine root; unrelated grants are preserved
- Prepares and validates every file, rolls back partial replacements, then
  updates the environment cache last
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

**Windows link primitives (read before creating any link in a script):**

| Primitive | Needs admin? | Notes |
|-----------|--------------|-------|
| `New-Item -ItemType SymbolicLink` (PS 5.1) | YES | Fails with "Administrator privilege required" even when Developer Mode is ON - PS 5.1 does not pass the unprivileged-create flag |
| `New-Item -ItemType Junction` | no | Directory only, same volume, local path. Correct default for a local directory link |
| `os.symlink` (Python) / `ln -s` (Git Bash) | no (Developer Mode) | These DO pass the unprivileged flag, so they succeed where PS 5.1 fails |

Prefer a junction for local directory links, and probe the link by reading
through it - creation can succeed while the link does not resolve.

**[!] Deleting a junction - measured behaviour, not folklore.**

A junction is a reparse point, and detection differs from deletion. Verified on
this workstation (PowerShell 5.1.19041, UE bundled Python 3):

| Operation on a junction | Result |
|-------------------------|--------|
| `os.path.islink()` | returns **False** - a junction is not a symlink |
| `shutil.rmtree()` | **refuses** with `OSError: Cannot call rmtree on a symbolic link` |
| `Remove-Item -Recurse -Force` | removes the link, **target intact** on 5.1.19041 |
| `[System.IO.Directory]::Delete($p, $false)` / `os.rmdir()` | removes the link, target intact - **use this** |

The widely repeated claim that a recursive delete destroys the junction target
did NOT reproduce here; it describes older PowerShell (<= 5.0) reparse-point
handling. Do not depend on that either way: the version-specific behaviour is
not a contract, so delete a junction explicitly with
`[System.IO.Directory]::Delete($path, $false)` (or `os.rmdir`) rather than
relying on a recursive delete happening to stop at the reparse point.

The `os.path.islink()` False result is the genuine trap: code that branches on
it will treat a junction as an ordinary directory. Test with the
`ReparsePoint` file attribute instead.

Live example: `.agents/skills` is a junction to `.claude/skills`
(see [link_codex_skills.ps1](../agents/link_codex_skills.ps1)).

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
