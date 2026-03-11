# Troubleshooting Flowchart - Overnight CI

Quick diagnosis flowchart for common overnight CI issues.

---

## 🔍 Issue Detection Flow

```
START: Is overnight CI failing?
  |
  ├─ YES → What type of failure?
  |  |
  |  ├─ Build Failures → Go to [BUILD FAILURES]
  |  ├─ Test Failures → Go to [TEST FAILURES]
  |  ├─ Script Errors → Go to [SCRIPT ERRORS]
  |  └─ Resource Issues → Go to [RESOURCE ISSUES]
  |
  └─ NO → Monitoring only?
     |
     ├─ Performance degradation → Go to [PERFORMANCE]
     └─ Metrics anomalies → Go to [METRICS]
```

---

## 🔨 BUILD FAILURES

```
Build failed?
  |
  ├─ Check circuit breaker status
  |  └─ artifacts/overnight/step-*/circuit_breaker.json
  |     |
  |     ├─ State: OPEN?
  |     |  └─ Wait for cooldown (30min default) OR reset manually:
  |     |     rm artifacts/overnight/step-*/circuit_breaker.json
  |     |
  |     └─ State: CLOSED?
  |        └─ Check build logs
  |
  ├─ Check disk space
  |  └─ Is free space < 50GB?
  |     ├─ YES → Free up space:
  |     |  - Run: powershell scripts/autonomous/common/Rotate-Logs.ps1
  |     |  - Delete Intermediate/, Binaries/
  |     |  - Check for zombie UE processes
  |     |
  |     └─ NO → Continue to error logs
  |
  ├─ Check error logs
  |  └─ artifacts/overnight/step-*/build.log (last 50 lines)
  |     |
  |     ├─ "Cannot open file" → Zombie process holding lock
  |     |  └─ Run: Get-Process | Where-Object {$_.Name -like "Unreal*"} | Stop-Process -Force
  |     |
  |     ├─ "Linker error" → Missing API macro or dependency
  |     |  └─ Check module .Build.cs dependencies
  |     |     Check API macro is UPPERCASE
  |     |
  |     ├─ "Timeout" → Build took > 5 minutes
  |     |  └─ Increase timeout: -TimeoutSeconds 600
  |     |     OR use module-only rebuild
  |     |
  |     └─ Unknown error → Try graceful degradation
  |        └─ Degraded build attempted automatically if enabled
  |           Check build.log.degraded for details
  |
  └─ Still failing after 3 retries?
     └─ Circuit breaker will open automatically
        Manual intervention required
```

---

## 🧪 TEST FAILURES

```
Tests failing?
  |
  ├─ Check test type
  |  |
  |  ├─ Unit tests (Common.Tests.ps1)
  |  |  └─ Run manually: powershell scripts/autonomous/claude/overnight/test/Run-Tests.ps1 -Detailed
  |  |     └─ Check mocks and test data
  |  |
  |  ├─ Integration tests
  |  |  └─ Check prerequisites:
  |  |     - Git repository initialized?
  |  |     - UE path configured?
  |  |     - Artifacts directory exists?
  |  |
  |  └─ Path handling tests
  |     └─ Check E: vs /mnt/e path resolution
  |        └─ Verify project root calculation (3 levels up)
  |
  └─ Check test logs
     └─ Saved/Logs/Alis.log (for UE tests)
        scripts/autonomous/overnight/test/*.log (for Pester tests)
```

---

## 📜 SCRIPT ERRORS

```
Script error?
  |
  ├─ PowerShell syntax error?
  |  └─ Check syntax: powershell -Command "$null = [System.Management.Automation.PSParser]::Tokenize((Get-Content 'script.ps1' -Raw), [ref]$null)"
  |
  ├─ Path not found?
  |  └─ Check path calculation
  |     - Scripts should go up 3 levels: ../../..
  |     - Use Resolve-Path for absolute paths
  |     - Test both E:\ and /mnt/e formats
  |
  ├─ Git error?
  |  └─ Check git remote push URL
  |     - Should be: DISABLED (for safety)
  |     - Reset: git remote set-url --push origin DISABLED
  |
  └─ Module not found?
     └─ Import-Module scripts/autonomous/common/Common.psm1 -Force
        Check exported functions: Get-Command -Module Common
```

---

## 💾 RESOURCE ISSUES

```
Resource problem?
  |
  ├─ Disk space
  |  └─ Check: Get-PSDrive C | Select-Object Free
  |     └─ If < 50GB:
  |        - Run log rotation: scripts/autonomous/common/Rotate-Logs.ps1
  |        - Clean build artifacts: rm -rf Intermediate/, Binaries/
  |        - Check for large files: Get-ChildItem -Recurse | Sort-Object Length -Descending | Select-Object -First 20
  |
  ├─ Memory (RAM)
  |  └─ Check: Get-SystemResources (from Common.psm1)
  |     └─ If > 90% used:
  |        - Close unnecessary applications
  |        - Kill zombie UE processes
  |        - Restart system if persistent
  |
  ├─ CPU
  |  └─ Check: Get-SystemResources
  |     └─ If > 95% sustained:
  |        - Check for runaway processes
  |        - Reduce parallel build workers
  |        - Add timeout to prevent infinite loops
  |
  └─ Zombie processes
     └─ Find: powershell "Import-Module scripts/autonomous/common/Common.psm1; Find-ZombieProcesses -MaxAgeMinutes 30"
        Kill: Stop-ZombieProcesses -MaxAgeMinutes 30 -Force
```

---

## 📊 PERFORMANCE

```
Slow performance?
  |
  ├─ Check metrics
  |  └─ powershell scripts/autonomous/common/Export-Dashboard.ps1
  |     └─ Open: artifacts/overnight/dashboard.html
  |        |
  |        ├─ Build time increasing?
  |        |  └─ Check for:
  |        |     - Disk fragmentation
  |        |     - Incremental build issues
  |        |     - Growing intermediate files
  |        |
  |        └─ Commit frequency decreasing?
  |           └─ Agent may be stuck
  |              Check current branch: git branch --show-current
  |              Check TODOs: scripts/autonomous/common/discover_todos.ps1
  |
  ├─ System resources
  |  └─ Monitor during build: Get-SystemResources
  |     └─ Record baseline and compare over time
  |
  └─ Log file growth
     └─ Check size: Get-ChildItem artifacts/overnight -Recurse | Measure-Object -Property Length -Sum
        └─ If > 500MB: Run Rotate-Logs.ps1
```

---

## 📈 METRICS

```
Metric anomalies?
  |
  ├─ Circuit breaker opened frequently?
  |  └─ Review circuit_breaker.json history
  |     └─ If ConsecutiveFailures > 3 repeatedly:
  |        - Investigate root cause in build.log
  |        - May need to adjust threshold or fix persistent issue
  |
  ├─ No commits in 24h?
  |  └─ Check if agent is running
  |     └─ Check TODOs: discover_todos.ps1
  |        └─ If 0 TODOs: Agent should create extension todos
  |           If not happening: Check SKILL.md infinite loop protocol
  |
  └─ Test pass rate dropping?
     └─ Check test logs for new failures
        └─ Run tests manually to reproduce:
           - Unit: scripts/autonomous/claude/overnight/test/Run-Tests.ps1
           - Integration: scripts/autonomous/claude/overnight/test/Integration.Tests.ps1
```

---

## 🚨 EMERGENCY PROCEDURES

### Complete System Reset

```bash
# 1. Kill all UE processes
Get-Process | Where-Object {$_.Name -like "Unreal*"} | Stop-Process -Force

# 2. Reset circuit breaker
rm artifacts/overnight/step-*/circuit_breaker.json

# 3. Clean build artifacts
rm -rf Intermediate/, Binaries/

# 4. Rotate logs
powershell scripts/autonomous/common/Rotate-Logs.ps1

# 5. Reset git push blocking
git remote set-url --push origin DISABLED

# 6. Verify system resources
powershell "Import-Module scripts/autonomous/common/Common.psm1; Get-SystemResources"

# 7. Run smoke tests
powershell scripts/autonomous/claude/overnight/test/Run-Tests.ps1

# 8. Restart overnight CI
powershell scripts/autonomous/claude/overnight/main.ps1
```

### Check System Health

```powershell
# Import utilities
Import-Module scripts/autonomous/common/Common.psm1

# Check disk space
Test-DiskSpace -RequiredGB 50

# Check for zombies
Find-ZombieProcesses -MaxAgeMinutes 30

# Check system resources
Get-SystemResources

# Check circuit breaker
Get-CircuitBreakerState -StateFile "artifacts/overnight/step-manual/circuit_breaker.json"

# Check TODOs
powershell scripts/autonomous/common/discover_todos.ps1

# Generate dashboard
powershell scripts/autonomous/common/Export-Dashboard.ps1
```

---

## 📞 Quick Reference

| Issue | Command | Expected Result |
|-------|---------|-----------------|
| Check disk space | `Test-DiskSpace` | `$true` if > 50GB |
| Find zombies | `Find-ZombieProcesses` | Empty array if none |
| Check resources | `Get-SystemResources` | CPU/RAM/Disk % |
| Check circuit breaker | `Get-CircuitBreakerState` | `State: CLOSED` |
| Count TODOs | `discover_todos.ps1` | Number > 0 |
| Rotate logs | `Rotate-Logs.ps1` | Summary report |
| View dashboard | `Export-Dashboard.ps1` | HTML file |
| Run tests | `test/Run-Tests.ps1` | All PASSED |

---

## 🔗 See Also

- [overnight/README.md](README.md) - Main documentation
- [AUTONOMOUS_SAFEGUARDS.md](AUTONOMOUS_SAFEGUARDS.md) - Safety guarantees
- [test/README.md](test/README.md) - Testing guide
- [Common.psm1](Common.psm1) - Utility functions
