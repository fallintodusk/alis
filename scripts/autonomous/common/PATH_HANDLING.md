# Windows vs WSL Path Handling Guide

Comprehensive guide for handling paths across Windows PowerShell and WSL (Windows Subsystem for Linux) in the overnight CI system.

---

## Quick Reference

| Scenario | Windows | WSL | Solution |
|----------|---------|-----|----------|
| **Project root** | `<project-root>` | `<project-root>` | Use `Resolve-Path` |
| **Path separator** | `\` (backslash) | `/` (forward slash) | Use `Join-Path` |
| **UBT paths** | `<ue-path>` | `/mnt/c/UnrealEngine/UE_5.5` | Config + auto-detect |
| **Git paths** | Works natively | Works natively | Both understand same format |
| **Script invocation** | `powershell.exe script.ps1` | `powershell.exe script.ps1` | Identical |

---

## Common Gotchas

### 1. Drive Letter vs Mount Point

**Issue:** Windows uses `E:` drive letters, WSL uses `/mnt/e` mount points.

**Example:**
```powershell
# Windows
$windowsPath = "<project-root>"

# WSL
$wslPath = "<project-root>"
```

**Solution:** Use PowerShell's `$PSScriptRoot` and relative paths:
```powershell
# Works in both Windows and WSL
$ProjectRoot = (Resolve-Path "$PSScriptRoot/../../..").Path
```

**Testing:** See `scripts/autonomous/overnight/test/PathHandling.Tests.ps1`

---

### 2. Path Separator Differences

**Issue:** Windows uses backslash `\`, WSL uses forward slash `/`.

**Example:**
```powershell
# Windows
"<project-root>\scripts\ci"

# WSL
"<project-root>/scripts/ci"
```

**Solution:** Use `Join-Path` (cross-platform):
```powershell
# Works in both
$artifactsDir = Join-Path $ProjectRoot "artifacts/overnight"
```

**Anti-pattern (fragile):**
```powershell
# DON'T DO THIS - breaks on WSL
$artifactsDir = "$ProjectRoot\artifacts\overnight"
```

---

### 3. UE Build Tool Path Resolution

**Issue:** UBT.bat expects Windows-style paths even when called from WSL.

**Example:**
```bash
# WSL calling Windows UBT
cmd.exe /c "<ue-path>\Engine\Build\BatchFiles\Build.bat" \
  AlisEditor Win64 Development \
  "<project-root>\Alis.uproject"  # Must be Windows path!
```

**Solution:** Convert WSL paths to Windows when needed:
```powershell
# In PowerShell (works in WSL)
function ConvertTo-WindowsPath {
    param([string]$Path)

    # Convert /mnt/e/foo/bar to E:\foo\bar
    if ($Path -match '^/mnt/([a-z])/(.+)$') {
        $drive = $Matches[1].ToUpper()
        $rest = $Matches[2] -replace '/', '\'
        return "$drive`:\$rest"
    }

    return $Path  # Already Windows path
}
```

**Testing:** PathHandling.Tests.ps1 validates this conversion:
```powershell
It "Should convert between Windows and WSL paths" {
    $windowsPath = "<project-root>"
    $expectedWSL = "<project-root>"

    $wslPath = $windowsPath -replace '^([A-Z]):', '/mnt/$1' -replace '\\', '/'
    $wslPath = $wslPath.ToLower()

    $wslPath | Should -Be $expectedWSL.ToLower()
}
```

---

### 4. Script Path Calculation

**Issue:** Scripts need to find project root from their location (e.g., `scripts/autonomous/overnight/`).

**Example:**
```
scripts/autonomous/overnight/main.ps1
└─ Need to find: <project-root>\ (3 levels up)
```

**Solution:** Go up 3 levels using `Split-Path`:
```powershell
# In main.ps1
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
```

**Or use `Resolve-Path` (cleaner):**
```powershell
# In any script under scripts/autonomous/overnight/
$ProjectRoot = (Resolve-Path "$PSScriptRoot/../../..").Path
```

**Testing:** PathHandling.Tests.ps1 validates this:
```powershell
It "Should resolve project root from scripts directory" {
    $root = (Resolve-Path "$PSScriptRoot/../../..").Path
    Test-Path $root | Should -Be $true
    Test-Path (Join-Path $root "Alis.uproject") | Should -Be $true
}
```

---

### 5. Absolute vs Relative Paths

**Issue:** Some UE tools require absolute paths, scripts may use relative.

**Example:**
```powershell
# Relative (fragile)
$projectPath = "./Alis.uproject"

# Absolute (robust)
$projectPath = "<project-root>\Alis.uproject"
```

**Solution:** Always resolve to absolute:
```powershell
$ProjectRoot = (Resolve-Path "$PSScriptRoot/../../..").Path
$projectPath = Join-Path $ProjectRoot "Alis.uproject"

# Verify it's absolute
[System.IO.Path]::IsPathRooted($projectPath)  # Should be $true
```

**Testing:** PathHandling.Tests.ps1 validates:
```powershell
It "Should preserve absolute paths through Resolve-Path" {
    $resolved = (Resolve-Path $script:ProjectRoot).Path
    [System.IO.Path]::IsPathRooted($resolved) | Should -Be $true
}
```

---

### 6. UBT Argument Quoting

**Issue:** Paths with spaces must be quoted in UBT arguments.

**Example:**
```powershell
# WRONG (breaks on spaces)
Start-Process "Build.bat" -ArgumentList "AlisEditor Win64 Development <project-root>\Alis.uproject"

# CORRECT (quoted)
Start-Process "Build.bat" -ArgumentList "AlisEditor Win64 Development `"<project-root>\Alis.uproject`""
```

**Solution:** Use `"`"` for nested quotes:
```powershell
$projectPath = Join-Path $ProjectRoot "Alis.uproject"
$quotedPath = "`"$projectPath`""

$buildArgs = "AlisEditor Win64 Development $quotedPath -WaitMutex"
```

**Testing:** PathHandling.Tests.ps1 validates:
```powershell
It "Should quote paths with spaces in UBT arguments" {
    $projectPath = Join-Path $script:ProjectRoot "Alis.uproject"
    $quotedPath = "`"$projectPath`""

    $quotedPath | Should -Match '^".*"$'
}
```

---

### 7. Forward Slashes for WSL UBT Paths

**Issue:** When running in WSL, even Windows tools may expect forward slashes.

**Example:**
```powershell
# WSL environment
if ($IsLinux) {
    $projectPath = "<project-root>/Alis.uproject"
    $projectPath | Should -Not -Match '\\'  # No backslashes
    $projectPath | Should -Match '/'        # Forward slashes
}
```

**Solution:** Detect environment and adjust:
```powershell
function Get-ProjectPath {
    param([string]$ProjectRoot)

    $projectFile = Join-Path $ProjectRoot "Alis.uproject"

    if ($IsLinux) {
        # WSL: use forward slashes, /mnt/ prefix
        return $projectFile -replace '\\', '/'
    } else {
        # Windows: backslashes OK
        return $projectFile
    }
}
```

**Testing:** PathHandling.Tests.ps1 validates:
```powershell
It "Should use forward slashes for WSL UBT paths" {
    if ($IsLinux) {
        $projectPath = "<project-root>/Alis.uproject"
        $projectPath | Should -Not -Match '\\'
        $projectPath | Should -Match '/'
    }
}
```

---

### 8. Artifacts Directory Creation

**Issue:** `New-Item` behavior differs slightly between Windows and WSL.

**Example:**
```powershell
# Both Windows and WSL support this
$artifactsDir = Join-Path $ProjectRoot "artifacts/overnight"

if (-not (Test-Path $artifactsDir)) {
    New-Item -ItemType Directory -Path $artifactsDir -Force | Out-Null
}
```

**Solution:** Use `-Force` flag (idempotent):
```powershell
# Creates parent directories if needed, no error if exists
New-Item -ItemType Directory -Path $artifactsDir -Force | Out-Null
```

**Testing:** PathHandling.Tests.ps1 validates:
```powershell
It "Should create artifacts directory with correct path" {
    $artifactsDir = Join-Path $script:ProjectRoot "artifacts/overnight"

    if (-not (Test-Path $artifactsDir)) {
        New-Item -ItemType Directory -Path $artifactsDir -Force | Out-Null
    }

    Test-Path $artifactsDir | Should -Be $true
}
```

---

## Best Practices

### 1. Use `Resolve-Path` for Project Root
✅ **DO:**
```powershell
$ProjectRoot = (Resolve-Path "$PSScriptRoot/../../..").Path
```

❌ **DON'T:**
```powershell
$ProjectRoot = "<project-root>"  # Hardcoded, breaks on WSL
```

### 2. Use `Join-Path` for Path Construction
✅ **DO:**
```powershell
$artifactsDir = Join-Path $ProjectRoot "artifacts/overnight"
```

❌ **DON'T:**
```powershell
$artifactsDir = "$ProjectRoot\artifacts\overnight"  # Breaks on WSL
```

### 3. Always Quote UBT Paths
✅ **DO:**
```powershell
$quotedPath = "`"$projectPath`""
```

❌ **DON'T:**
```powershell
$args = "AlisEditor Win64 Development $projectPath"  # Breaks on spaces
```

### 4. Test Both Environments
✅ **DO:**
```powershell
Describe "Cross-Platform Path Compatibility" {
    It "Should use platform-appropriate path separators" {
        $testPath = Join-Path "foo" "bar" "baz"

        if ($IsWindows -or $PSVersionTable.PSVersion.Major -le 5) {
            $testPath | Should -Match '\\'
        } else {
            $testPath | Should -Match '/'
        }
    }
}
```

### 5. Use `Test-Path` Before Operations
✅ **DO:**
```powershell
if (Test-Path $artifactsDir) {
    # Directory exists, proceed
} else {
    New-Item -ItemType Directory -Path $artifactsDir -Force | Out-Null
}
```

---

## Debugging Path Issues

### Check Current Environment
```powershell
# Detect OS
if ($IsLinux) {
    Write-Host "Running in WSL"
} elseif ($IsWindows) {
    Write-Host "Running in Windows"
} elseif ($PSVersionTable.PSVersion.Major -le 5) {
    Write-Host "Running in Windows PowerShell 5.1"
}

# Check path format
$PSScriptRoot
# Windows: <project-root>\scripts\ci\overnight
# WSL:     <project-root>/scripts/autonomous/overnight
```

### Validate Path Resolution
```powershell
# Test project root calculation
$ProjectRoot = (Resolve-Path "$PSScriptRoot/../../..").Path
Write-Host "Project Root: $ProjectRoot"
Write-Host "Is Absolute: $([System.IO.Path]::IsPathRooted($ProjectRoot))"
Write-Host "Alis.uproject exists: $(Test-Path (Join-Path $ProjectRoot 'Alis.uproject'))"
```

### Test Path Conversion
```powershell
# Windows to WSL
$windowsPath = "<project-root>"
$wslPath = $windowsPath -replace '^([A-Z]):', '/mnt/$1' -replace '\\', '/'
Write-Host "Windows: $windowsPath"
Write-Host "WSL:     $wslPath"

# WSL to Windows
$wslPath = "<project-root>"
if ($wslPath -match '^/mnt/([a-z])/(.+)$') {
    $drive = $Matches[1].ToUpper()
    $rest = $Matches[2] -replace '/', '\'
    $windowsPath = "$drive`:\$rest"
    Write-Host "WSL:     $wslPath"
    Write-Host "Windows: $windowsPath"
}
```

---

## Example: Cross-Platform Script

```powershell
# rebuild_module_safe.ps1 - Works in both Windows and WSL
param(
    [string]$ModuleName,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = "Stop"

# Step 1: Calculate project root (cross-platform)
$ProjectRoot = (Resolve-Path "$PSScriptRoot/../../..").Path

# Step 2: Construct paths using Join-Path
$projectPath = Join-Path $ProjectRoot "Alis.uproject"
$artifactsDir = Join-Path $ProjectRoot "artifacts/overnight"

# Step 3: Validate paths
if (-not (Test-Path $projectPath)) {
    throw "Project file not found: $projectPath"
}

# Step 4: Create directories (cross-platform)
if (-not (Test-Path $artifactsDir)) {
    New-Item -ItemType Directory -Path $artifactsDir -Force | Out-Null
}

# Step 5: Quote paths for UBT
$quotedProjectPath = "`"$projectPath`""

# Step 6: Build arguments
$buildArgs = "AlisEditor Win64 Development $quotedProjectPath -WaitMutex"

if ($ModuleName) {
    $buildArgs += " -Module=$ModuleName"
}

# Step 7: Invoke UBT (Windows paths even from WSL)
$ubtPath = "<ue-path>\Engine\Build\BatchFiles\Build.bat"

if ($IsLinux) {
    # WSL: Use cmd.exe to invoke Windows batch file
    cmd.exe /c "`"$ubtPath`" $buildArgs"
} else {
    # Windows: Direct invocation
    & $ubtPath $buildArgs
}
```

---

## Testing Path Handling

**Run smoke tests:**
```powershell
# From project root
cd <project-root>  # or <project-root>

# Run path handling tests
Invoke-Pester scripts/autonomous/overnight/test/PathHandling.Tests.ps1
```

**Expected output:**
```
Tests completed in 1.23s
Tests Passed: 11, Failed: 0, Skipped: 0 NotRun: 0
```

**Coverage:**
- Project root resolution
- Windows/WSL format handling
- Path conversion (bidirectional)
- Relative path resolution
- Absolute path preservation
- UBT argument quoting
- Forward slash usage in WSL
- Artifacts directory creation
- Platform-specific separators

---

## Reference

- **PathHandling.Tests.ps1:** `scripts/autonomous/overnight/test/PathHandling.Tests.ps1`
- **Common.psm1:** `scripts/autonomous/overnight/Common.psm1` (shared utilities)
- **rebuild_module_safe.ps1:** `scripts/ue/build/rebuild_module_safe.ps1` (example usage)
- **Integration.Tests.ps1:** `scripts/autonomous/overnight/test/Integration.Tests.ps1` (workflow validation)

---

## See Also

- [README.md](README.md) - Main overnight CI documentation
- [TROUBLESHOOTING_FLOWCHART.md](TROUBLESHOOTING_FLOWCHART.md) - Path not found errors
- [AUTONOMOUS_SAFEGUARDS.md](AUTONOMOUS_SAFEGUARDS.md) - Safety guarantees

