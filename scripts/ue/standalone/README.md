# Standalone Game Scripts

Scripts for running and testing the game in standalone mode (not PIE).

**Fast incremental build by default** - uses UE's smart build system (only rebuilds what changed)

## Scripts

| Script | Purpose | Use With | Time |
|--------|---------|----------|------|
| `build.ps1` | Build AlisEditor target | **Tabby/PowerShell/Windows Terminal** | ~5-10 min first, ~10-30 sec incremental |
| `run.ps1` | Run game (UnrealEditor.exe -game) | **Tabby/PowerShell/Windows Terminal** | Manual |
| `test.py` | Build + test + validate boot flow | Python 3 | ~30-40 sec total |

**All scripts use PowerShell** - works in Tabby, Windows Terminal, VSCode, PowerShell ISE.

## IMPORTANT: Build Target Explanation

**These scripts use `AlisEditor` target for fast iterative testing:**

| Target | What It Builds | Use Case | Speed | Output |
|--------|---------------|----------|-------|--------|
| **AlisEditor** | `UnrealEditor-Alis.dll` | Daily development testing | **FAST** (1-2 min incremental) | Run with `UnrealEditor.exe -game` |
| **Alis** | `UnrealGame-Alis.dll` | Packaging for distribution | **SLOW** (5-15 min + cooking) | Creates `Alis.exe` after packaging |

**Common Mistake:** Using `Alis` target for testing

[X] **WRONG for these scripts:**
```ini
BUILD_TARGET=Alis  # Creates UnrealGame-Alis.dll (NO .exe during dev!)
```
- Builds game DLLs for packaging only
- Does NOT create runnable executable during development
- Requires additional Cook/Package step to get Alis.exe
- Meant for final distribution, not daily testing

[OK] **CORRECT for these scripts:**
```ini
BUILD_TARGET=AlisEditor  # Creates UnrealEditor-Alis.dll
```
- Fast incremental builds (10-30 seconds)
- Run with `UnrealEditor.exe Alis.uproject -game`
- Same game behavior as packaged Alis.exe
- Perfect for fast iteration during development

**For true Alis.exe packaging:** Use `scripts/ue/package/package_release.ps1` or see `scripts/ue/package/README.md`.

## Requirements

- **Python 3.x** - Required for `test.py`
- **Unreal Engine 5.5** - Path configured in `scripts/config/ue_path.conf`

### Finding Python on Windows

```powershell
# Check if Python is installed
python --version

# Find Python path
(Get-Command python).Path
```

Common locations:
- PowerShell: `python`
- Git Bash: `/c/Program Files/Python311/python.exe`
- User install: `<local-app-data>\Programs\Python\Python3xx\python.exe`

---

## Execution Modes & Boot Flow

### Understanding UE Execution Modes

**Standalone Game Mode (these scripts):**
- Uses `-game` flag with editor executable
- **Full Orchestrator boot** (same as packaged game)
- **Full ProjectLoading pipeline** (6 phases)
- Command line: `UnrealEditor.exe ... L_OrchestratorBoot -game -windowed`
- Equivalent modes:
  - File -> Play -> Standalone Game (in Editor UI)
  - `test.py` (CLI automated)
  - Packaged game executable

**PIE Mode (Play In Editor):**
- Single-process in editor viewport OR separate window
- **Skips Orchestrator** (by design - plugins already loaded by editor)
- **Partial ProjectLoading** (subsystem initializes but content path differs)
- Use for: Visual/gameplay experiments, Blueprint iteration
- **NOT** representative of real boot flow

**Commandlet Mode (Build/Cook):**
- `-run=Cook`, `-run=GenerateProjectFiles`, etc.
- **Skips Orchestrator** (explicitly checked via `IsRunningCommandlet()`)
- No gameplay, rendering, or UI systems

### Boot Flow Decision Tree

```
START: Which execution mode am I in?

|-- Commandlet (-run=...)
|   \-- Orchestrator: SKIPPED
|       ProjectLoading: SKIPPED
|
|-- PIE (Play in Editor, single process)
|   \-- Orchestrator: SKIPPED (plugins already loaded)
|       ProjectLoading: RUNS (subsystem auto-init)
|       Use case: Quick Blueprint/visual iteration
|
\-- Standalone Game (-game flag OR packaged)
    \-- Orchestrator: RUNS FULLY (check)
        ProjectLoading: RUNS FULLY (check)
        Use case: Boot flow testing, CI/CD, production
```

### When to Use Each Mode

**Standalone Mode (these scripts OR File -> Play -> Standalone Game):**
- (check) Testing Orchestrator boot flow
- (check) Testing loading pipeline phases (6 phases)
- (check) Validating boot-time features
- (check) Automated CI/CD testing
- (check) Debugging with full boot sequence (attach VS debugger)
- (check) Representative of packaged game behavior

**PIE (Play in viewport/window):**
- (check) Quick Blueprint iteration
- (check) Visual experiments
- (check) Gameplay prototyping
- (X) **NOT for boot flow testing** (Orchestrator skipped)
- (X) **NOT representative of packaged game**

**Rule of thumb:**
- **Boot bugs, loading issues, Orchestrator problems** -> Standalone Game
- **Visual layout, Blueprint logic, quick iteration** -> PIE

---

## Usage

### Human: Interactive Run

```batch
REM Simple run - shows game window + UE log console
scripts\ue\standalone\run.bat

REM With extra UE flags (e.g., no sound)
scripts\ue\standalone\run.bat -NoSound

REM Auto-quit after 60 seconds (for quick validation)
scripts\ue\standalone\run.bat 60
```

**What you'll see:**
- **Game window** (1280x720, windowed)
- **UE log console** (showing ProjectLoading + later messages)
- **Command prompt** blocked until UE exits

**Log behavior:**
- Script blocks until UE exits (auto-quit or manual close)
- After UE closes, appends **complete UE log** (`Saved/Logs/Alis.log`) to combined log
- **Complete logs with Orchestrator + ProjectLoading** [OK]

**Important:** For complete logs, let UE exit normally (auto-quit recommended). If you forcefully kill the script, you still have raw `Saved/Logs/Alis.log`.

**Boot sequence (in log file):**
1. Uses `GameDefaultMap` from `Config/DefaultEngine.ini` (= `L_OrchestratorBoot`)
2. Orchestrator loads plugins: "=== Orchestrator Module Loading ==="
3. ProjectLoading starts initial experience: "Phase 1: Resolve Assets"
4. Travels to MainMenu map: "LoadMap(/MainMenuWorld/Maps/MainMenu_Persistent)"

**Verify boot success after running:**
```bash
# Check Orchestrator messages
grep "Orchestrator" tmp/run_standalone_*.log | head -20

# Check ProjectLoading messages
grep "Phase.*Completed" tmp/run_standalone_*.log
```

**Log file:**
- **Single combined log:** `tmp/run_standalone_YYYYMMDD_HHMMSS.log`
  - Script metadata (date, paths, args, exit code)
  - **Complete UE output** (Orchestrator + ProjectLoading + all boot messages)
- Also saved to: `Saved/Logs/Alis.log` (overwritten each run)

**Tip:** To watch UE output in real-time while game runs:
```powershell
Get-Content Saved/Logs/Alis.log -Wait -Tail 50
```

### Agent: Automated Test

**IMPORTANT:** Boot flow needs ~20-30 seconds to complete all phases (Orchestrator + ProjectLoading 5 phases). Default timeout is 30 seconds to ensure full validation. Tests with <20 sec timeout will fail with incomplete boot flow.

**Fast incremental test** - only rebuilds what changed, validates full boot flow (~30-40 seconds)

```batch
REM Default: Incremental build + test (FAST, recommended)
python scripts/ue/standalone/test.py

REM 45 second game runtime (for thorough testing)
python scripts/ue/standalone/test.py --timeout 45

REM Skip build entirely (use existing binaries)
python scripts/ue/standalone/test.py --no-build

REM Clean + full rebuild (slow, use when needed)
python scripts/ue/standalone/test.py --clean

REM Using full Python path if not in PATH
"python" scripts/ue/standalone/test.py
```

### test.py Flags

- `--timeout N` - Run game for N seconds before auto-quit (default: 30)
- `--no-build` - Skip building (use existing binaries)
- `--clean` - Clean before building (slow, use when switching branches or artifacts corrupted)

**Default behavior (recommended):** Incremental build + test (~30-40 seconds total)

**Performance (typical times):**
- Default test (incremental build + 30 sec boot): ~30-40 seconds (FAST!)
- No-build test (existing binaries + 30 sec boot): ~30 seconds
- Clean rebuild: ~2-5 minutes (first time or after --clean)
- Full cold build: ~2-5 minutes (one-time cost)

### What test.py validates

**Boot flow success (6 indicators):**
1. `=== Orchestrator Module Loading ===`
2. `Boot plugins loaded:`
3. `Phase 1: Resolve Assets - Completed`
4. `Phase 5: Travel - Completed successfully`
5. `Pipeline completed successfully`
6. `LoadMap(/MainMenuWorld/Maps/MainMenu_Persistent)`

**Font errors (3 patterns):**
- `Failed to read file 'Roboto'`
- `GetFontFace failed to load or process 'Roboto'`
- `Last resort fallback font was requested`

**Crash detection (3 patterns):**
- `EXCEPTION_ACCESS_VIOLATION`
- `Fatal error!`
- `Assertion failed`

### Output

- Console: Real-time progress
- Report: `Saved/AutomatedTests/standalone_test_<timestamp>.txt`
- UE Log: `Saved/Logs/Alis.log`

---

## Debugging Standalone Game

### Method 1: Attach VS Debugger

1. Launch standalone game (File -> Play -> Standalone Game OR `run.bat`)
2. Visual Studio -> Debug -> Attach to Process
3. Find **second** `UnrealEditor-Win64-DebugGame.exe` process
4. Attach and set breakpoints in Orchestrator/ProjectLoading code

### Method 2: CLI with Debugger

```bash
# Launch with debugger attached
devenv /debugexe "<ue-path>\Engine\Binaries\Win64\UnrealEditor.exe" ^
  "<project-root>\Alis.uproject" ^
  "L_Boot" -game -windowed
```

### Boot Flow Log Markers

**Expected log sequence for successful boot:**

```
(check) Orchestrator markers:
  - "=== Orchestrator Module Loading ==="
  - "Boot plugins loaded: N, OnDemand plugins loaded (code only): M"
  - "=== Orchestrator Boot Complete ==="

(check) ProjectLoading markers:
  - "Phase 1: Resolve Assets - Completed"
  - "Phase 5: Travel - Completed successfully"
  - "Pipeline completed successfully"
  - "LoadMap(/MainMenuWorld/Maps/MainMenu_Persistent)"

(X) Failure indicators:
  - "Skipping Orchestrator" (wrong mode - commandlet or PIE)
  - "Phase N - Failed" (pipeline error)
  - "EXCEPTION_ACCESS_VIOLATION" (crash)
```

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success - all checks passed |
| 1 | Failure - see report for details |

---

## Example Workflows

```bash
# Quick test after code change (~30-40 seconds, incremental)
python scripts/ue/standalone/test.py

# Very fast test with existing binaries (~30 seconds)
python scripts/ue/standalone/test.py --no-build

# Thorough test with longer runtime (~45 seconds)
python scripts/ue/standalone/test.py --timeout 45

# Full clean rebuild (slow, use when switching branches)
python scripts/ue/standalone/test.py --clean

# CI/CD pipeline (incremental build, fast)
python scripts/ue/standalone/test.py
```

**Verification after test:**
```bash
# Check latest report
cat Saved/AutomatedTests/standalone_test_*.txt

# Search logs for boot markers
grep "Orchestrator" Saved/Logs/Alis.log
grep "Phase.*Completed" Saved/Logs/Alis.log
```
