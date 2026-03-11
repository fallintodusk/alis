# DEPRECATED: Python Test Scripts

**Status:** ❌ **Do Not Use**

## Replacement

Use native Windows batch scripts instead:

```batch
REM Unit tests
scripts\win\test.bat --unit

REM Integration tests
scripts\win\test.bat --integration

REM Specific module
scripts\win\test.bat --filter ProjectMenuUI
```

## Why Deprecated

1. **Redundant:** Batch scripts provide same functionality
2. **Complexity:** Python adds unnecessary layer for Windows-native project
3. **Agent-Friendly:** Batch scripts are simpler to debug/maintain
4. **Native Integration:** Direct UE automation command invocation

## Migration Guide

| Old (Python) | New (Batch) |
|--------------|-------------|
| `python scripts/test/autonomous_boot_test.py` | `scripts\win\test.bat --integration` |
| `python scripts/test/*.py` | `scripts\win\test.bat --filter <name>` |

## Documentation

See [docs/testing_guide.md](../../docs/testing_guide.md) for complete testing documentation.

---

**Files Deprecated:**
- `autonomous_boot_test.py` ❌
- `autonomous_boot_test.bat` (wrapper) ❌

**Use Instead:**
- `scripts/win/test.bat` ✅
- `scripts/test/standalone.bat` ✅
