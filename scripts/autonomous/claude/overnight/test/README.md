# Overnight CI Unit Tests

PowerShell Pester unit tests for overnight CI scripts and utilities.

---

## Quick Start

### Run Tests

```powershell
# From project root
powershell scripts/autonomous/overnight/test/Run-Tests.ps1
```

### Install Pester (if not already installed)

```powershell
powershell scripts/autonomous/overnight/test/Run-Tests.ps1 -InstallPester
```

### Detailed Output

```powershell
powershell scripts/autonomous/overnight/test/Run-Tests.ps1 -Detailed
```

---

## Test Files

### Common.Tests.ps1

Tests for `scripts/autonomous/overnight/Common.psm1`:

**Invoke-WithExponentialBackoff:**
- ✅ Success on first attempt
- ✅ Retry and succeed on second attempt
- ✅ Exhaust retries on persistent failures
- ✅ Exponential backoff delays (2s → 4s → 8s → ...)
- ✅ Cap delays at MaxDelay

**Circuit Breaker:**
- ✅ Get/Set circuit breaker state
- ✅ Allow operation when CLOSED
- ✅ Block operation when OPEN (cooldown not expired)
- ✅ Transition OPEN → HALF_OPEN after cooldown
- ✅ Reset on success (HALF_OPEN → CLOSED)
- ✅ Increment failures below threshold (stay CLOSED)
- ✅ Open circuit at threshold (CLOSED → OPEN)
- ✅ Complete success flow integration test
- ✅ Complete failure flow integration test (CLOSED → OPEN → HALF_OPEN → CLOSED)

---

## Writing Tests

### Pester Test Template

```powershell
# MyModule.Tests.ps1

BeforeAll {
    Import-Module "$PSScriptRoot/../MyModule.psm1" -Force
}

Describe "MyFunction" {
    It "Should do something" {
        $result = MyFunction -Param "value"
        $result | Should -Be "expected"
    }
}
```

### Best Practices

1. **Use BeforeAll** for module imports (runs once per Describe block)
2. **Use BeforeEach** for test setup (runs before each It block)
3. **Use $TestDrive** for temporary files (auto-cleanup)
4. **Mock external dependencies** (Start-Sleep, Write-Host, etc.)
5. **Test integration flows** (not just units)

---

## Test Coverage

**Current Coverage:**
- ✅ Exponential backoff retry logic (100%)
- ✅ Circuit breaker state machine (100%)
- ✅ Integration flows (success and failure paths)

**Needed Coverage:**
- 🔲 discover_todos.ps1 (TODO detection)
- 🔲 main.ps1 (entry point orchestration)
- 🔲 rebuild_module_safe.ps1 integration (with Common.psm1)

---

## CI Integration

### Run Tests in CI Pipeline

```powershell
# CI script example
powershell scripts/autonomous/overnight/test/Run-Tests.ps1

# Check exit code
if ($LASTEXITCODE -ne 0) {
    Write-Error "Unit tests failed"
    exit 1
}
```

### Pre-Commit Hook

```bash
# .git/hooks/pre-commit
#!/bin/bash
powershell.exe scripts/autonomous/overnight/test/Run-Tests.ps1
```

---

## Troubleshooting

### "Pester module not found"

**Solution:**
```powershell
Install-Module -Name Pester -Force -SkipPublisherCheck -Scope CurrentUser
```

Or use the `-InstallPester` flag.

### "Module already loaded"

**Solution:** Pester uses `Import-Module -Force` to reload modules. If issues persist, close all PowerShell windows and re-run tests.

### Tests fail with "Mock not found"

**Solution:** Ensure Pester version ≥ 5.0. Check version:
```powershell
Get-Module -ListAvailable -Name Pester
```

Update if needed:
```powershell
Update-Module -Name Pester -Force
```

---

## References

- **Pester Docs:** https://pester.dev/docs/quick-start
- **PowerShell Modules:** https://learn.microsoft.com/en-us/powershell/scripting/developer/module/writing-a-windows-powershell-module
- **Mocking Guide:** https://pester.dev/docs/usage/mocking
