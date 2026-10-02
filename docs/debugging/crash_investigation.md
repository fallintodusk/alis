# Crash Investigation for Agents

Guide for AI agents to investigate crashes, hangs, and deadlocks in Alis.

---

## Quick Reference

| Situation | What to do |
|-----------|------------|
| Editor crashed | Check `Saved/Crashes/` for latest folder |
| Stuck at breakpoint | Save dump in VS, run analysis script |
| Need agent analysis | Extract to text with `analyze_dump.ps1` |
| Editor hung in `BlockOnShaderMapCompletion` / `DrawMaterialToRenderTarget` | See [cases/shader_compile_worker_hang.md](cases/shader_compile_worker_hang.md) |

---

## Crash Artifacts Location

```
Saved/
  Crashes/
    UECC-Windows-<GUID>_0000/
      CrashContext.runtime-xml    <-- Agent-readable: callstack, error
      UEMinidump.dmp              <-- Binary: needs extraction
      Alis.log                    <-- Log at crash time
  Logs/
    Alis.log                      <-- Current session log
```

**Agent workflow:** Read `CrashContext.runtime-xml` first - it has callstack in text.

---

## Reading CrashContext.runtime-xml

This XML file is auto-generated on crash and contains:

```xml
<ErrorMessage>EXCEPTION_ACCESS_VIOLATION 0x00000056</ErrorMessage>
<CallStack>
UnrealEditor_ProjectUI!UInventoryWidget::Refresh() [Line 142]
UnrealEditor_Slate!SWidget::Paint()
...
</CallStack>
```

**Key fields:**
- `ErrorMessage` - Exception type and address
- `CrashType` - Crash, Assert, Ensure, Stall
- `CallStack` - Text callstack with file:line info
- `IsRequestingExit` - true = shutdown crash (often benign)

---

## Dump Analysis Script

For deeper analysis when CrashContext isn't enough.

### Prerequisites

Install **Windows SDK Debugging Tools** (one-time):

```
1. Download: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
   (Any Windows 10 SDK version works, e.g. 10.0.19041.0)

2. Run installer (the default location is fine; the script finds any)

3. Click Next until feature selection

4. Uncheck everything EXCEPT "Debugging Tools for Windows"

5. Finish (~100MB download)
```

**Installed paths** (under the Windows Kits root):
```
<Windows Kits root>\Debuggers\
  x64\
    cdb.exe       <-- Console debugger (what we use)
    windbg.exe    <-- GUI debugger
    dbghelp.dll   <-- Symbol handling
    symsrv.dll    <-- Symbol server client
  x86\            <-- 32-bit versions (rarely needed)
```

**Verify installation:**
```powershell
(Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots").KitsRoot10
```

`analyze_dump.ps1` looks for `cdb.exe` on `PATH`, then under that Windows Kits
root, then in the standard Program Files locations. For symbols it uses your
`_NT_SYMBOL_PATH` when set, and otherwise caches Microsoft symbols in
`%LOCALAPPDATA%\Symbols`.

**Why not WinDbg Preview (Store)?**

| Version | GUI Debugging | Command-line (cdb.exe) |
|---------|---------------|------------------------|
| Windows SDK | Yes | Yes |
| WinDbg Preview (Store) | Yes | No (sandboxed) |

The Store version runs in a sandbox - cdb.exe cannot be invoked from scripts.
For automated dump analysis, you need the SDK version.

### Usage

```powershell
# Analyze crash dump
.\scripts\debug\analyze_dump.ps1 "Saved\Crashes\UECC-Windows-XXX\UEMinidump.dmp"

# Custom output path
.\scripts\debug\analyze_dump.ps1 -DumpFile "crash.dmp" -OutputFile "analysis.txt"
```

### Output

Creates `<DumpFile>.analysis.txt` with comprehensive state information:

| Section | What It Contains |
|---------|------------------|
| **CRASH SUMMARY** | Root cause, faulting module, exception type, probable culprit |
| **EXCEPTION DETAILS** | Exception record, address, code |
| **REGISTERS AT CRASH** | All CPU registers (RAX, RBX, RCX, etc.) + extended (SSE/AVX) |
| **CRASH CALLSTACK** | 100 frames with parameters and frame numbers |
| **LOCAL VARIABLES** | All variables in crash frame with types and values |
| **THIS POINTER** | Object state if crash is in a method |
| **MEMORY AT FAULT** | Memory region info around crash address |
| **STACK FRAMES 0-4** | Local variables for first 5 stack frames |
| **ALL THREADS** | Every thread's callstack (detect deadlocks) |
| **LOADED MODULES** | Project DLLs with addresses |
| **MEMORY REGIONS** | Memory usage summary |
| **HEAP SUMMARY** | Heap state (detect corruption) |

**Key fields to look for:**

```
EXCEPTION_RECORD:  -- What exception occurred
FAULTING_IP:       -- Exact instruction that crashed
STACK_TEXT:        -- Call sequence leading to crash
LOCAL VARIABLES:   -- Variable values at crash
this = 0x...       -- Object pointer (null = problem!)
GridPanel = 0x0    -- Null pointer found!
```

**Example analysis output:**
```
=== LOCAL VARIABLES (crash frame) ===
this = 0x000001a2f3b800     <-- Valid object
ItemCount = 47              <-- Normal value
GridPanel = 0x0000000000    <-- NULL! This caused the crash
bIsInitialized = true
```

---

## Analyzing Hangs/Deadlocks

When the process is stuck (not crashed):

### In Visual Studio

1. **Break All** - `Ctrl+Alt+Break`
2. **View threads** - Debug -> Windows -> Threads
3. **Save dump** - Debug -> Save Dump As -> `hang.dmp`

### Common Deadlock Patterns

**Game thread waiting on render thread:**
```
Thread 0 (Main):
  FlushRenderingCommands()
  WaitForRenderThread()

Thread 2 (Render):
  WaitForGameThread()  <-- Deadlock!
```

**Async loading blocking main:**
```
Thread 0 (Main):
  LoadSynchronous()
  WaitForAsyncLoading()

Thread 5 (AsyncLoad):
  WaitForGameThread()  <-- Deadlock!
```

### What to Look For

| Pattern | Meaning |
|---------|---------|
| `WaitForSingleObject` | Thread waiting on mutex/event |
| `EnterCriticalSection` | Waiting for lock |
| `FlushRenderingCommands` | Game waiting on render |
| `IsInGameThread() check` | Wrong thread access |
| `BlockOnShaderMapCompletion` -> `Sleep` | Shader compile worker stuck - see [cases/shader_compile_worker_hang.md](cases/shader_compile_worker_hang.md) |

---

## Common UE Crash Patterns

### Access Violation at Low Address (0x00-0xFF)

```
EXCEPTION_ACCESS_VIOLATION 0x0000000000000056
```

**Cause:** Null pointer + member offset. The 0x56 is the offset to a member.

**Fix:** Check for null before accessing:
```cpp
if (Widget && Widget->GetParent())
{
    Widget->GetParent()->DoSomething();
}
```

### Shutdown Crashes (IsRequestingExit=true)

Crashes during `PurgeAllUObjectsOnExit` or `StaticExit` are usually:
- Subsystem accessing destroyed object
- Delegate bound to destroyed object
- Often engine bugs, not your code

**Verdict:** Usually safe to ignore unless reproducible at runtime.

### GC Crash (IncrementalPurgeGarbage)

```
IncrementalPurgeGarbage()
  UObject::~UObject()
    YourClass::SomeCallback()  <-- Accessing dead object
```

**Cause:** Raw pointer to UObject that got garbage collected.

**Fix:** Use `TWeakObjectPtr` or ensure proper prevent garbage collection.

### Slate Crash During Tick

```
SWidget::Paint()
  SCompoundWidget::OnPaint()
    YourWidget::Tick()  <-- Crash
```

**Cause:** Widget accessing invalid data during UI tick.

**Fix:** Validate all data in widget tick, use `IsValid()` checks.

---

## Autonomous Agent Workflow

**Agents can investigate crashes fully autonomously.** No user intervention required.

### Timing Expectations

| Method | Time | When to Use |
|--------|------|-------------|
| CrashContext.runtime-xml | **Instant** | Always first - has callstack |
| Dump analysis (Quick) | 2-5 min | Need local variables, threads |
| Dump analysis (Full) | 5-15 min first run | Need MS symbol details |
| Dump analysis (cached) | 1-3 min | After first full run |

**Always start with CrashContext** - it's instant and usually sufficient.

### Step 1: Quick Triage (CrashContext)

```bash
# Find latest crash
CRASH_DIR=$(ls -td Saved/Crashes/UECC-* 2>/dev/null | head -1)
echo "Analyzing: $CRASH_DIR"

# Extract key fields (instant - text file)
grep -E "<ErrorMessage>|<CrashType>|<IsRequestingExit>" "$CRASH_DIR/CrashContext.runtime-xml"

# Get callstack
grep "<CallStack>" "$CRASH_DIR/CrashContext.runtime-xml" | head -20
```

**Decision point:**
- Clear callstack in YOUR code? -> Analyze source, suggest fix (DONE)
- Shutdown crash (`IsRequestingExit=true`)? -> Report benign (DONE)
- Need more context? -> Continue to Step 2

### Step 2: Deep Analysis (if needed)

Run dump extraction in background and monitor progress:

```bash
# Start analysis (background)
powershell.exe -ExecutionPolicy Bypass -File "scripts/debug/analyze_dump.ps1" \
    -Quick "$CRASH_DIR/UEMinidump.dmp" &

# Monitor progress every 30 seconds
while true; do
    SIZE=$(stat -c%s "$CRASH_DIR/UEMinidump.dmp.analysis.txt" 2>/dev/null || echo 0)
    LINES=$(wc -l < "$CRASH_DIR/UEMinidump.dmp.analysis.txt" 2>/dev/null || echo 0)
    echo "Progress: $SIZE bytes, $LINES lines"

    # Check for completion marker
    if grep -q "=== HEAP SUMMARY ===" "$CRASH_DIR/UEMinidump.dmp.analysis.txt" 2>/dev/null; then
        echo "Analysis complete!"
        break
    fi
    sleep 30
done

# Read results
cat "$CRASH_DIR/UEMinidump.dmp.analysis.txt"
```

### Script Modes

```powershell
# Quick mode (2-5 min) - local symbols only, faster
.\scripts\debug\analyze_dump.ps1 -Quick "Saved\Crashes\UECC-XXX\UEMinidump.dmp"

# Full mode (5-15 min first run, cached after) - MS symbols included
.\scripts\debug\analyze_dump.ps1 "Saved\Crashes\UECC-XXX\UEMinidump.dmp"
```

### Decision Tree

```
User reports crash
    |
    v
[Read CrashContext.runtime-xml] (instant)
    |
    +-- Has clear callstack in YOUR code?
    |       --> Read source at crash line
    |       --> Identify bug, suggest fix
    |       --> DONE
    |
    +-- Shutdown crash (IsRequestingExit=true)?
    |       --> Report: "Benign shutdown crash in engine code"
    |       --> DONE
    |
    +-- Engine-only callstack / need variables?
            --> Run analyze_dump.ps1 -Quick (background)
            --> Monitor progress every 30s
            --> When complete, analyze detailed output
            --> DONE
```

---

## What Agents Can Do Autonomously

| Task | Method |
|------|--------|
| Find crashes | `ls -lt Saved/Crashes/` |
| Read crash info | `cat CrashContext.runtime-xml` (text, no tools) |
| Extract dump details | `analyze_dump.ps1` (converts binary to text) |
| Read analysis | `cat UEMinidump.dmp.analysis.txt` |
| Correlate logs | `tail Saved/Logs/Alis.log` |
| Identify root cause | Analyze callstack + code |
| Suggest fix | Read source at crash location |

**No user action required** - agents have full autonomous access to crash investigation.

---

## Known Cases

Project-specific incidents with confirmed root cause + fix. Add a new
file under `cases/` after fixing a non-trivial crash or hang. Keep
entries here one line each; the case file owns the details.

- [cases/shader_compile_worker_hang.md](cases/shader_compile_worker_hang.md) - editor hangs in `BlockOnShaderMapCompletion` during `DrawMaterialToRenderTarget`; antivirus blocks `ShaderCompileWorker.exe` spawn

---

## See Also

- [troubleshooting.md](../testing/troubleshooting.md) - General troubleshooting
- [testing/README.md](../testing/README.md) - Running tests to reproduce issues
- [automation.md](../testing/automation.md) - Automated test runs
