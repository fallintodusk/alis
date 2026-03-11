# YOLO Mode - Fully Autonomous CI

powershell.exe -File scripts/autonomous/claude/yolo/yolo.ps1

or under wsl run directly without powershell:
claude \
  --dangerously-skip-permissions \
  --verbose \
  --allowedTools "Read" "Write" "Edit" "Grep" "Glob" \
  --allowedTools "Bash(git status:*)" "Bash(git diff:*)" "Bash(git add:*)" "Bash(git commit:*)" \
  --allowedTools "Bash(powershell.exe:*)" "Bash(cmd:*)" \
  --disallowedTools "Bash(git push:*)" \
  --max-turns 2000 \
  "Use the ue-gamefeature-ci skill. do all tasks by priority, keep todos tiny, compact, consistent, mark done tasks. for manual todo leave one file that consist only critical doing in editor, all others tasks such as widget overriden should be optional - hot reload configs should support it. Then main goal to achieve it's like rom system - check docs architecture - it's about not updatable module like bios with tiny ui that could update core game feature. so system shouldn't have by default links to gf that could restrict it's updating , removing or adding. as the result we must have tiny core plugin, and at the first start it's propose to user add core game feature, that after loading propose update dependencies. on the further start user could be proposed update some dependency in case out dated from core game feature"

**⚠️ WARNING: This mode bypasses ALL permission prompts. Use with caution.**

## What is YOLO Mode?

YOLO (You Only Live Once) mode runs Claude Code with `--dangerously-skip-permissions`, enabling fully autonomous operation without any user prompts. This is useful for:

- ✅ Overnight autonomous development
- ✅ Unattended CI/CD pipelines
- ✅ Long-running refactoring tasks
- ✅ Batch processing tasks

## Safety Features

The script includes multiple layers of protection:

1. **Git Push Blocked** - Cannot push to remote repository
2. **Repository Scripts** - Can execute any script within repository (.bat, .ps1, .sh)
3. **Restricted Tools** - Whitelist of safe operations only
4. **Branch Verification** - Checks current branch before running
5. **Unlimited Turns** - No API budget limit (requires unlimited tariff)

## Prerequisites

Before running YOLO mode:

1. **Create isolated branch:**
   ```powershell
   git switch -c ai/yolo-$(Get-Date -Format yyyyMMdd)
   ```

2. **Block git push (CRITICAL):**
   ```powershell
   git remote set-url --push origin DISABLED
   ```

3. **Verify settings:**
   ```powershell
   # Check that .claude/settings.local.json exists and has permissions configured
   Get-Content .claude/settings.local.json
   ```

## Usage

### Basic Usage

```powershell
# From project root
cd <project-root>
.\scripts\autonomous\claude\yolo\yolo.ps1
```

### With Branch Verification

```powershell
# Only run if on specific branch
.\scripts\autonomous\claude\yolo\yolo.ps1 -Branch "ai/yolo-20251030"
```

## What the Script Does

1. **Pre-flight checks:**
   - Verifies current branch
   - Checks if git push is disabled
   - Prompts for confirmation if push is enabled

2. **Runs Claude with:**
   - `--dangerously-skip-permissions` (no prompts)
   - Restrictive tool whitelist
   - Unlimited turns (no budget limit)
   - ROM system architecture prompt

3. **Post-execution:**
   - Reports success/failure
   - Returns exit code

## Allowed Operations

The YOLO mode whitelist allows:

- ✅ Read/Write/Edit files in repository
- ✅ Git operations (status, diff, add, commit, log)
- ✅ Run any scripts in repository (.bat, .ps1, .sh files)
- ✅ PowerShell and cmd commands
- ✅ Build UE modules (via UE Build Tool)
- ✅ Run UE tests (via UnrealEditor-Cmd.exe)
- ✅ Process management (timeout, taskkill)
- ✅ Python scripts

## Blocked Operations

The YOLO mode explicitly blocks:

- ❌ Git push
- ❌ Recursive file deletion (`rm -rf`, `del /s /q`)
- ❌ Any operations outside project directory
- ❌ SSH key access
- ❌ Environment variable files (.env)

## Monitoring Progress

While YOLO mode is running, monitor in another terminal:

```powershell
# Watch git commits
Get-Content -Wait (git rev-parse --show-toplevel)\.git\logs\HEAD

# Or check git log periodically
while ($true) {
    Clear-Host
    git log --oneline -10
    Start-Sleep -Seconds 60
}
```

## After Completion

1. **Review changes:**
   ```powershell
   git log --oneline -20
   git diff HEAD~10
   ```

2. **Check for issues:**
   ```powershell
   # Look for error logs
   Get-ChildItem Saved\Logs\Alis.log
   ```

3. **Re-enable push (if merging to main):**
   ```powershell
   git remote set-url --push origin $(git config --get remote.origin.url)
   ```

## Current Task: ROM System Architecture

The script is configured to work on the **ROM System Architecture** goal:

### Goal
Implement a minimal "BIOS-like" core plugin with tiny UI that can update/manage GameFeatures dynamically.

### Requirements
- ✅ Tiny core plugin (ProjectBoot) - immutable loader
- ✅ No hardcoded GameFeature dependencies in core
- ✅ First start: propose adding core GameFeature
- ✅ After loading: propose dependency updates
- ✅ Support hot-reload configs (JSON-based)
- ✅ Manual tasks: only editor-critical operations
- ✅ Widget overrides: optional, hot-reloadable

### Architecture References
- See: [docs/architecture/plugin_architecture.md](../../../docs/architecture/plugin_architecture.md)
- Status: [todo/create_architecture.md](../../../todo/create_architecture.md)

## Risk Assessment

| Risk | Mitigation |
|------|-----------|
| Accidental push | Git push URL set to DISABLED |
| Malicious script execution | Only scripts within repository allowed |
| API cost runaway | N/A (unlimited tariff) |
| File corruption | Git branch isolation, easy rollback |
| Credential theft | SSH keys blocked, no .env access |

## Comparison to Regular Mode

| Feature | Regular Mode | YOLO Mode |
|---------|--------------|-----------|
| Permission prompts | ✅ Yes | ❌ No |
| Human oversight | ✅ Yes | ❌ No |
| Overnight capable | ⚠️ Maybe | ✅ Yes |
| API efficiency | ⚠️ Lower (prompts) | ✅ Higher |
| Risk level | ✅ Low | ⚠️ Moderate |
| Recommended for | Development | Overnight CI |

## Troubleshooting

### Script Won't Start
**Check branch:**
```powershell
git rev-parse --abbrev-ref HEAD
```

**Check push status:**
```powershell
git remote get-url --push origin
# Should show: DISABLED
```

### Claude Not Found
**Install Claude CLI:**
```bash
npm install -g @anthropic-ai/claude
```

### Permission Errors
**Verify settings file exists:**
```powershell
Test-Path .claude\settings.local.json
```

## See Also

- [Overnight CI Setup](../overnight/README.md)
- [Autonomous CI README](../../../AUTONOMOUS_CI_README.md)
- [Architecture Roadmap](../../../todo/create_architecture.md)
- [Claude Code Permissions](https://docs.claude.com/en/docs/claude-code/permissions)
4. **Verify Unreal Engine path (CRITICAL):**
   - Ensure `scripts/config/ue_path.conf` points to your UE 5.5 install
   - Example:
     ```bash
     # scripts/config/ue_path.conf
     UE_PATH="<ue-path>"
     # or
     UE_PATH="<ue-path>"
     ```
   - WSL note: run builds from PowerShell/CMD, or wrap calls with `cmd.exe /C`
   - Quick check (PowerShell):
     ```powershell
     Test-Path "$env:UE_PATH/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe"
     ```
