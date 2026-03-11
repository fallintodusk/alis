# Script Development Guide

This guide covers how to create, debug, and maintain scripts in the Alis project.

---

## 🆕 Create New Script

### Decision Tree: Where Does This Script Belong?

```
START: I need to create a script

├─ Does it BUILD C++ code?
│  └─ scripts/ue/build/

├─ Does it VALIDATE without building?
│  └─ scripts/ue/check/

├─ Does it TEST functionality?
│  ├─ Critical path, <5 min? → scripts/ue/test/smoke/
│  ├─ Multi-system, 5-30 min? → scripts/ue/test/integration/
│  └─ Single system, <1 min? → scripts/ue/test/unit/

├─ Does it RUN the game/editor?
│  └─ scripts/ue/run/

├─ Is it AUTONOMOUS AGENT?
│  └─ scripts/autonomous/

└─ General utility?
   └─ scripts/utils/
```

### Naming Rules
- **Lowercase with underscores:** `boot_test.bat`, `bp_compile.bat`
- **Descriptive:** `gamefeature_registration.bat`, not `test1.bat`
- **Platform extension:** `.bat` (Windows), `.sh` (Linux/macOS), `.ps1` (PowerShell)

### Script Checklists
- [ ] Descriptive name (lowercase_underscore)
- [ ] Correct location (decision tree)
- [ ] Clear purpose comment
- [ ] Exit code handling
- [ ] Duration estimate

---

## 🔄 Move Existing Script

**When to move:**
- Script in wrong category (e.g., test script in build/)
- Script purpose changed
- Reorganizing for SOLID compliance

**Steps:**
1. Use decision tree to find correct location
2. Move file: `git mv old/path/script.bat new/path/script.bat`
3. Update references in other scripts (`grep -r` is your friend)
4. Update documentation
5. Test script in new location

---

## 🐛 Debug Script

### Common Issues

| Issue | Symptom | Fix |
|-------|---------|-----|
| **UE path not found** | "UE path not found" | Check `scripts/config/ue_path.conf` |
| **Exit code always 0** | Script reports success on failure | Add exit code checks |
| **Hangs indefinitely** | Script doesn't complete | Add timeout, check for interactive prompts |
| **Paths with spaces** | "File not found" errors | Quote all paths: `"C:/Program Files/..."` |

### Debugging Steps
1. Add `echo` statements to show progress
2. Check exit codes: `echo %ERRORLEVEL%` (Windows), `echo $?` (Linux)
3. Run with verbose logging: Add `-verbose` or `-AllowStdOutLogVerbosity`
4. Check logs: `Saved/Logs/Alis.log`
5. Test in isolation (not as part of larger script)

---

## 🔧 Common Script Patterns

### UE Path Detection
**All scripts should use central config:**

```batch
REM Windows batch
set "UE_PATH="
if exist "%~dp0..\..\config\ue_path.conf" (
    for /f "tokens=*" %%i in (%~dp0..\..\config\ue_path.conf) do set "UE_PATH=%%i"
)
if "%UE_PATH%"=="" (
    echo Error: UE path not configured
    exit /b 1
)
```

### Error Handling
**Always check exit codes:**

```batch
REM Windows
call some_command.bat
if %ERRORLEVEL% neq 0 (
    echo Error: Command failed with exit code %ERRORLEVEL%
    exit /b 1
)
```
