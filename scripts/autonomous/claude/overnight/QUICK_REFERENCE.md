powershell.exe -ExecutionPolicy Bypass -File "<project-root>\scripts\ci\overnight\main.ps1" -NewBranch

# Overnight CI Quick Reference Card

One-page cheat sheet for the autonomous overnight CI system.

---

## 🚀 Start Autonomous Session

```powershell
# Work on current branch
powershell scripts/autonomous/overnight/main.ps1

# Create new dated branch (ai/autonomous-dev-YYYYMMDD)
powershell scripts/autonomous/overnight/main.ps1 -NewBranch

# Dry run (setup only, no agent prompt)
powershell scripts/autonomous/overnight/main.ps1 -DryRun
```

**Output:** Copy the INFINITE LOOP PROMPT → Paste into Claude Code

---

## 📊 Check Status

| Command | Purpose | Output |
|---------|---------|--------|
| `powershell scripts/autonomous/overnight/discover_todos.ps1` | Count remaining TODOs | `Agent-compatible TODOs remaining: X` |
| `powershell scripts/autonomous/overnight/Export-Dashboard.ps1` | Generate HTML dashboard | `dashboard.html` in artifacts/ |
| `git log --oneline -10` | Recent commits | Last 10 commit messages |
| `git branch --show-current` | Current branch | Branch name |

---

## 🛠️ Manual Build Commands

```powershell
# Full editor build (15-30 min first time, 5-15 min incremental)
scripts/ue/build/build.bat AlisEditor Win64 Development

# Single module rebuild (2-5 min) - RESILIENT
powershell scripts/ue/build/rebuild_module_safe.ps1 -ModuleName ProjectMenuUI

# Without resilience features
powershell scripts/ue/build/rebuild_module_safe.ps1 -ModuleName ProjectMenuUI `
  -DisableRetries -DisableCircuitBreaker
```

---

## 🧪 Test Commands

### UE Test Orchestrator (Recommended)
```powershell
# Smoke tests (2-5 min) - GameFeature + 15s boot test
powershell scripts/autonomous/overnight/test/UE_Tests.ps1 -TestSuite smoke

# Unit tests (C++ tests)
powershell scripts/autonomous/overnight/test/UE_Tests.ps1 -TestSuite unit

# Integration tests (full boot flow)
powershell scripts/autonomous/overnight/test/UE_Tests.ps1 -TestSuite integration

# Full test suite (all tests)
powershell scripts/autonomous/overnight/test/UE_Tests.ps1 -TestSuite full

# With custom timeout and no retry
powershell scripts/autonomous/overnight/test/UE_Tests.ps1 -TestSuite smoke -TimeoutSeconds 180 -EnableRetry:$false
```

### Direct UE Test Scripts
```powershell
# Smoke tests (<5 min) - RUN BEFORE COMMITS
scripts/ue/test/smoke/boot_test.bat
scripts/ue/test/smoke/gamefeature_registration.bat

# C++ unit tests (headless)
powershell scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "Project.*"

# Autonomous boot flow (headless)
scripts/ue/test/integration/autonomous_boot_test.bat
```

### CI Infrastructure Tests (Pester)
```powershell
# Integration tests (git, PowerShell infrastructure)
Invoke-Pester scripts/autonomous/overnight/test/Integration.Tests.ps1

# Path handling tests (Windows/WSL)
Invoke-Pester scripts/autonomous/overnight/test/PathHandling.Tests.ps1

# Unit tests (Common.psm1 utilities)
Invoke-Pester scripts/autonomous/overnight/test/Common.Tests.ps1
```

---

## 🔧 Utilities (Common.psm1)

```powershell
# Import utilities
Import-Module scripts/autonomous/overnight/Common.psm1

# Check disk space (requires 50GB by default)
Test-DiskSpace -RequiredGB 50

# Find zombie processes (age > 60 min)
Find-ZombieProcesses -MaxAgeMinutes 60

# Kill zombie processes
Stop-ZombieProcesses -MaxAgeMinutes 60 -Force

# Kill ALL UE processes (emergency)
Stop-AllUEProcesses

# Check system resources
Get-SystemResources  # Returns @{CPU, RAM, Disk}

# Circuit breaker state
$stateFile = "artifacts/overnight/step-manual/circuit_breaker.json"
Get-CircuitBreakerState -StateFile $stateFile

# Reset circuit breaker (if OPEN)
rm $stateFile

# Structured logging
Write-StructuredLog -Level "INFO" -Message "Build started" `
  -LogFile "artifacts/overnight/build.json"

# Add metric
Add-Metric -MetricName "build_time" -Value 123.45 `
  -MetricsFile "artifacts/overnight/metrics.json"

# Get metric stats
Get-MetricStats -MetricName "build_time" `
  -MetricsFile "artifacts/overnight/metrics.json"
```

---

## 🗂️ File Locations

| File/Dir | Purpose |
|----------|---------|
| `scripts/autonomous/overnight/main.ps1` | Entry point for infinite loop |
| `scripts/autonomous/overnight/discover_todos.ps1` | TODO counter |
| `scripts/autonomous/overnight/Common.psm1` | Shared utilities |
| `scripts/autonomous/overnight/test/UE_Tests.ps1` | **UE test orchestrator with resilience** |
| `scripts/ue/build/rebuild_module_safe.ps1` | Resilient build wrapper |
| `scripts/ue/test/smoke/` | Direct UE smoke tests (boot, gamefeature) |
| `scripts/ue/test/unit/` | Direct C++ unit tests |
| `scripts/ue/test/integration/` | Direct integration tests (autonomous boot) |
| `artifacts/overnight/` | Logs, metrics, circuit breaker state |
| `artifacts/overnight/dashboard.html` | Visual progress dashboard |
| `todo/*.md` | Task lists (checked = done, unchecked = pending) |

---

## 🔍 Troubleshooting

### Build Failing?
1. Check circuit breaker: `cat artifacts/overnight/step-*/circuit_breaker.json`
   - If `"State": "OPEN"` → Wait 30 min OR `rm circuit_breaker.json`
2. Check disk space: `Test-DiskSpace`
3. Kill zombies: `Stop-ZombieProcesses -MaxAgeMinutes 30 -Force`
4. Check logs: `tail -50 artifacts/overnight/step-*/build.log`

### Tests Failing?
1. Check logs: `cat Saved/Logs/Alis.log | tail -100`
2. Run in Session Frontend: Window → Developer Tools → Session Frontend → Automation
3. See [TROUBLESHOOTING_FLOWCHART.md](TROUBLESHOOTING_FLOWCHART.md)

### Path Issues?
1. Verify project root: `(Resolve-Path "$PSScriptRoot/../../..").Path`
2. Check format: Windows (`E:\`) vs WSL (`/mnt/e/`)
3. See [PATH_HANDLING.md](PATH_HANDLING.md)

### Agent Stuck?
- **Issue:** Agent says "continuing" but doesn't execute next task
- **Fix:** Use TodoWrite tool explicitly after each task completion
- **Pattern:** Edit → Commit → TodoWrite → Update TODO.md → Commit → Next task

---

## 🔄 Workflow Overview

```
┌─────────────────┐
│ discover_todos  │ ← Count agent-compatible TODOs
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
    ▼ >0      ▼ =0
┌────────┐  ┌──────────────┐
│ WORK   │  │ CREATE NEW   │
│ ON     │  │ EXTENSION    │
│ TASK   │  │ TODOS        │
└───┬────┘  └──────┬───────┘
    │              │
    └──────┬───────┘
           │
           ▼
    ┌──────────────┐
    │ BUILD        │ ← rebuild_module_safe.ps1
    │ (resilient)  │   • Zombie detection
    └──────┬───────┘   • Disk space check
           │           • Circuit breaker
           │           • Exponential backoff
           │           • Graceful degradation
           ▼
    ┌──────────────┐
    │ TEST         │ ← Smoke/Integration tests
    └──────┬───────┘
           │
           ▼
    ┌──────────────┐
    │ COMMIT       │ ← Git commit with message
    └──────┬───────┘
           │
           ▼
    ┌──────────────┐
    │ UPDATE       │ ← TodoWrite + TODO.md
    │ PROGRESS     │
    └──────┬───────┘
           │
           └──────► LOOP BACK TO discover_todos
```

---

## 🛡️ Safety Guarantees

| Feature | Status | Mechanism |
|---------|--------|-----------|
| **No git push** | ✅ Safe | Remote push URL set to "DISABLED" |
| **Branch isolation** | ✅ Safe | Works on `ai/autonomous-dev-*` branch |
| **No UE popups** | ✅ Safe | Build.bat doesn't trigger editor |
| **Error handling** | ✅ Safe | `$ErrorActionPreference = "Stop"` |
| **Graceful failures** | ✅ Safe | Circuit breaker prevents cascades |

**See [AUTONOMOUS_SAFEGUARDS.md](AUTONOMOUS_SAFEGUARDS.md) for full analysis**

---

## 📈 Metrics & Monitoring

```powershell
# Generate dashboard (HTML)
powershell scripts/autonomous/overnight/Export-Dashboard.ps1
# Opens: artifacts/overnight/dashboard.html

# View recent logs
cat artifacts/overnight/*.json | tail -50

# Check commit frequency (last 24h)
git log --since="1 day ago" --oneline --no-merges | wc -l

# Check system health
Import-Module scripts/autonomous/overnight/Common.psm1
Get-SystemResources
Test-DiskSpace
Find-ZombieProcesses
```

---

## 🧹 Maintenance

```powershell
# Rotate old logs (delete > 7 days, > 100 files, or > 50MB)
powershell scripts/autonomous/overnight/Rotate-Logs.ps1

# Clean build artifacts (free disk space)
rm -rf Intermediate/ Binaries/

# Reset circuit breaker (emergency)
rm artifacts/overnight/step-*/circuit_breaker.json

# Kill all UE processes (emergency)
Import-Module scripts/autonomous/overnight/Common.psm1
Stop-AllUEProcesses
```

---

## 🆘 Emergency Procedures

### Complete System Reset
```powershell
# 1. Kill all UE processes
Get-Process | Where-Object {$_.Name -like "Unreal*"} | Stop-Process -Force

# 2. Reset circuit breaker
rm artifacts/overnight/step-*/circuit_breaker.json

# 3. Clean build artifacts
rm -rf Intermediate/ Binaries/

# 4. Rotate logs
powershell scripts/autonomous/overnight/Rotate-Logs.ps1

# 5. Reset git push blocking
git remote set-url --push origin DISABLED

# 6. Verify system resources
Import-Module scripts/autonomous/overnight/Common.psm1
Get-SystemResources

# 7. Run smoke tests
scripts/ue/test/smoke/boot_test.bat

# 8. Restart overnight CI
powershell scripts/autonomous/overnight/main.ps1
```

---

## 📚 Full Documentation

| Topic | File |
|-------|------|
| Overview | [README.md](README.md) |
| Architecture | [README.md](README.md) (see "Architecture Diagram") |
| Path handling | [PATH_HANDLING.md](PATH_HANDLING.md) |
| Troubleshooting | [TROUBLESHOOTING_FLOWCHART.md](TROUBLESHOOTING_FLOWCHART.md) |
| Safety | [AUTONOMOUS_SAFEGUARDS.md](AUTONOMOUS_SAFEGUARDS.md) |
| Unit tests | [test/Common.Tests.ps1](test/Common.Tests.ps1) |
| Integration tests | [test/Integration.Tests.ps1](test/Integration.Tests.ps1) |

---

## 🎯 Key Concepts

### Exponential Backoff
Retries with increasing delays: 10s → 20s → 40s → 80s → 120s (max)

### Circuit Breaker
State machine: CLOSED (normal) → 3 failures → OPEN (blocked) → 30min → HALF_OPEN → test

### Graceful Degradation
Primary: UBT with `-WaitMutex` flag
Fallback: UBT without `-WaitMutex` (if primary fails 3x)

### Structured Logging
Format: NDJSON (newline-delimited JSON)
Fields: `timestamp`, `level`, `message`, `data`

### Zombie Detection
Pattern: `UnrealEditor*`, `ShaderCompileWorker*`, etc.
Threshold: Processes running > 30 minutes

---

## 🔗 Common Patterns

### Add TODO
```markdown
# In todo/*.md
- [ ] Task description (agent-compatible)
- [ ] MANUAL: Task requiring user action (skipped by agent)
```

### Commit Message Format
```
<type>(scope): <description>

🤖 N/M LOOP - <details>

<body>

Generated with Claude Code
```

**Types:** feat, fix, docs, test, refactor, perf, build, ci

---

**Version:** 2025-10-29
**Author:** Claude Code Autonomous Agent
