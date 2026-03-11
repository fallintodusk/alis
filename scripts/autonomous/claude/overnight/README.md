# Overnight Autonomous CI (Infinite Loop)

**Primary entry for autonomous infinite loop development.** Runs continuously until external termination.

## How It Works

### Infinite Loop Protocol
1. **Check TODOs:** `discover_todos.ps1` counts agent-compatible unchecked tasks
2. **If TODOs > 0:** Implement → Build → Test → Commit → GOTO 1
3. **If TODOs = 0:** Create new extension todos → GOTO 1
4. **Never stops autonomously** - continuous improvement until you stop it

### Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         OVERNIGHT CI WORKFLOW                           │
└─────────────────────────────────────────────────────────────────────────┘

                              ┌──────────────┐
                              │  main.ps1    │
                              │ Entry Point  │
                              └──────┬───────┘
                                     │
                                     ├─ Setup branch (optional -NewBranch)
                                     ├─ Block git push (set remote to DISABLED)
                                     ├─ Create artifacts/overnight/
                                     └─ Print INFINITE LOOP PROMPT
                                     │
                                     ▼
                     ┌───────────────────────────────┐
                     │   discover_todos.ps1          │
                     │   Count unchecked tasks       │
                     └───────────┬───────────────────┘
                                 │
                    ┌────────────┴────────────┐
                    │                         │
                    ▼ TODOs > 0               ▼ TODOs = 0
         ┌──────────────────────┐    ┌──────────────────────┐
         │  IMPLEMENT TASK      │    │  CREATE EXTENSION    │
         │  • Edit code         │    │  TODOS               │
         │  • Write tests       │    │  • Analyze codebase  │
         │  • Update docs       │    │  • Create new tasks  │
         └──────────┬───────────┘    └───────────┬──────────┘
                    │                            │
                    └────────────┬───────────────┘
                                 │
                                 ▼
                    ┌─────────────────────────────┐
                    │  rebuild_module_safe.ps1    │
                    │  BUILD WITH RESILIENCE      │
                    └─────────────┬───────────────┘
                                  │
                ┌─────────────────┼─────────────────┐
                │                 │                 │
                ▼                 ▼                 ▼
    ┌────────────────┐  ┌──────────────┐  ┌─────────────────┐
    │ Zombie Process │  │ Disk Space   │  │ Circuit Breaker │
    │ Detection      │  │ Check        │  │ Check           │
    │ (Common.psm1)  │  │ (Common.psm1)│  │ (Common.psm1)   │
    └────────┬───────┘  └──────┬───────┘  └────────┬────────┘
             │                 │                    │
             └─────────────────┼────────────────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │  UBT Build Process   │
                    │  (with retry logic)  │
                    └──────────┬───────────┘
                               │
                ┌──────────────┴──────────────┐
                │                             │
                ▼ Success                     ▼ Failure (after retries)
    ┌───────────────────────┐     ┌──────────────────────────┐
    │ Update Circuit        │     │ Graceful Degradation     │
    │ Breaker Success       │     │ • Try simpler build      │
    │ • Reset failures      │     │ • Log structured error   │
    │ • Update metrics      │     │ • Open circuit breaker   │
    └───────────┬───────────┘     └───────────┬──────────────┘
                │                             │
                └──────────────┬──────────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │   RUN TESTS          │
                    │   • Smoke tests      │
                    │   • Integration tests│
                    └──────────┬───────────┘
                               │
                ┌──────────────┴──────────────┐
                │                             │
                ▼ Pass                        ▼ Fail
    ┌───────────────────────┐     ┌──────────────────────────┐
    │ GIT COMMIT            │     │ LOG FAILURE              │
    │ • Stage changes       │     │ • Write structured log   │
    │ • Commit with msg     │     │ • Record metrics         │
    │ • Update progress.md  │     │ • Continue to next task  │
    └───────────┬───────────┘     └───────────┬──────────────┘
                │                             │
                └──────────────┬──────────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │  UPDATE METRICS      │
                    │  • Log structured    │
                    │  • Update dashboard  │
                    │  • Record timing     │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │  LOOP BACK           │
                    │  (GOTO discover)     │
                    └──────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                        STATE MANAGEMENT FILES                           │
└─────────────────────────────────────────────────────────────────────────┘

artifacts/overnight/
├── step-*/
│   ├── circuit_breaker.json       # Circuit breaker state (OPEN/CLOSED)
│   ├── build.log                  # Build output
│   ├── *.json                     # Structured logs (NDJSON)
│   └── metrics.json               # Performance metrics
├── dashboard.html                 # Visual progress dashboard
└── progress.md                    # Markdown progress report

┌─────────────────────────────────────────────────────────────────────────┐
│                        KEY COMPONENTS                                    │
└─────────────────────────────────────────────────────────────────────────┘

Common.psm1 (Shared Utilities)
├── Invoke-WithExponentialBackoff    # Retry with delays: 10s → 20s → 40s
├── Test-CircuitBreaker               # Check if circuit is OPEN
├── Update-CircuitBreakerOnSuccess    # Reset failure count
├── Update-CircuitBreakerOnFailure    # Increment failures, open if threshold
├── Find-ZombieProcesses              # Detect long-running UE processes
├── Stop-ZombieProcesses              # Kill zombies with age filter
├── Test-DiskSpace                    # Verify sufficient disk space
├── Write-StructuredLog               # JSON logging (timestamp, level, msg)
├── Add-Metric                        # Record metric with timestamp
├── Get-MetricStats                   # Analyze metrics (min/max/avg)
└── Get-SystemResources               # CPU/RAM/Disk monitoring

- **rebuild_module_safe.ps1:** `scripts/ue/build/rebuild_module_safe.ps1` (example usage)
- **Integration.Tests.ps1:** `scripts/autonomous/claude/overnight/test/Integration.Tests.ps1` (workflow validation)
rebuild_module_safe.ps1 (Build Wrapper)
├── Kill zombie UE processes (age > 30 min)
├── Check disk space (require 50GB)
├── Test circuit breaker (bail if OPEN)
├── Invoke UBT with exponential backoff (max 3 attempts)
├── Graceful degradation (simpler build on repeated failures)
├── Update circuit breaker on success/failure
└── Structured logging for all events

discover_todos.ps1 (TODO Counter)
├── Scan todo/*.md for unchecked tasks
├── Filter out MANUAL tasks (editor assets)
└── Return count for agent decision

Export-Dashboard.ps1 (Monitoring)
├── Parse structured JSON logs
├── Calculate metrics (errors, warnings, timing)
├── Query git for recent commits
└── Generate responsive HTML dashboard

Rotate-Logs.ps1 (Maintenance)
├── Policy 1: Delete logs older than MaxAgeDays (default 7)
├── Policy 2: Keep only MaxFiles newest files (default 100)
└── Policy 3: Delete oldest if total size > MaxSizeMB (default 50)

┌─────────────────────────────────────────────────────────────────────────┐
│                        RESILIENCE FEATURES                               │
└─────────────────────────────────────────────────────────────────────────┘

1. EXPONENTIAL BACKOFF
   Retries: 0s → 10s → 20s → 40s → 80s → 120s (max)
   Use case: Transient build failures

2. CIRCUIT BREAKER
   CLOSED (normal) → 3 failures → OPEN (blocked) → 30min → HALF_OPEN → test
   Use case: Prevent cascading failures

3. GRACEFUL DEGRADATION
   Primary: UBT with -WaitMutex flag
   Fallback: UBT without -WaitMutex (if primary fails 3x)
   Use case: Build system issues

4. ZOMBIE DETECTION
   Pattern: UnrealEditor*, ShaderCompileWorker*, etc.
   Threshold: Processes running > 30 minutes
   Use case: Prevent process accumulation

5. DISK MONITORING
   Warning: < 50GB free space
   Action: Trigger log rotation, suggest cleanup
   Use case: Prevent disk exhaustion

6. STRUCTURED LOGGING
   Format: NDJSON (newline-delimited JSON)
   Fields: timestamp, level, message, data
   Use case: Machine-readable logs for dashboard

7. METRICS COLLECTION
   Tracked: build_time, test_time, commit_count, error_count
   Storage: metrics.json (NDJSON)
   Use case: Performance trends and anomaly detection
```

### Files
- **main.ps1** - Infinite loop entry script. Sets up branch, logs, prints INFINITE LOOP prompt
- **discover_todos.ps1** - Helper to count remaining agent-compatible TODOs (filters MANUAL lines)

## Usage

### Start Infinite Loop Session

**Default: Work on current branch**
```powershell
powershell scripts/autonomous/claude/overnight/main.ps1
# Works on whatever branch you're currently on
# Copy the INFINITE LOOP PROMPT from output
# Paste into Claude Code in VS Code
# Agent runs forever until you stop it (Ctrl+C or close window)
```

**Create new dated branch:**
```powershell
powershell scripts/autonomous/claude/overnight/main.ps1 -NewBranch
# Creates ai/autonomous-dev-YYYYMMDD branch
```

### Check TODOs Status (During Session)
```powershell
powershell scripts/autonomous/common/discover_todos.ps1
# Shows: "Agent-compatible TODOs remaining: X"
```

## Behavior

### What Agent Does
- ✅ Picks highest priority unchecked task from todo/*.md
- ✅ Implements C++ code, tests, configs, docs (no editor assets)
- ✅ Builds → Tests → Commits on green
- ✅ Updates artifacts/overnight/progress.md
- ✅ Loops back to check todos
- ✅ **When todos = 0:** Creates new extension todos and continues

### What Agent Skips
- ❌ Editor asset creation (marked MANUAL)
- ❌ Tasks requiring UE editor UI interaction
- ❌ Asking user for input (fully autonomous)

### Never Stop Conditions
- **Never says "all done"** - always finds next improvement
- **Never waits** - fully autonomous operation
- **Always extends** - creates new todos when current ones complete

## Safety

- **Branch:** `ai/autonomous-dev-YYYYMMDD` (push blocked for safety)
- **Logs:** `artifacts/overnight/run-YYYYMMDD-HHMMSS.log`
- **Review:** Manually review commits before merging to main

## Termination

**Agent runs infinitely. Stop it externally:**
- Ctrl+C in terminal
- Close VS Code window
- Kill process

Then review commits and merge:
```bash
git switch main
git merge --no-ff ai/autonomous-dev-YYYYMMDD
git push origin main
```

---

## Autonomous Operation Guarantees

**Verified Safe (Web Research 2024):**
- ✅ **No user prompts** - `$ErrorActionPreference = "Stop"` in all scripts
- ✅ **No UE popups** - Build.bat doesn't trigger UnrealVersionSelector
- ✅ **No git push** - Remote push URL set to "DISABLED"
- ✅ **Graceful errors** - Build/test failures logged, agent continues

**See [AUTONOMOUS_SAFEGUARDS.md](AUTONOMOUS_SAFEGUARDS.md) for complete safety analysis.**

