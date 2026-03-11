# Scripts & Utilities Architecture

> **Document Type:** Reference Guide + Decision Tree
> **Target Audience:** AI Agents (Primary), Human Developers (Secondary)
> **Last Updated:** 2025-10-24
> **Version:** 2.0 (Agent-Optimized)

---

## Purpose

This document defines the **SOLID-compliant scripts architecture** for Unreal Engine 5.5 project automation.

**For AI Agents:**
- ✅ Decision trees for autonomous script placement
- ✅ Copy-paste templates for common tasks
- ✅ Explicit rules and constraints (MUST/MUST NOT)
- ✅ Quick reference tables and checklists
- ✅ Example implementations with full code

**For Humans:**
- Architecture rationale and design principles
- Best practices and naming conventions
- Migration guide from old structure
- Maintenance guidelines

---

## Quick Reference for Agents

### Decision Tree: Where Does This Script Belong?

```
START: I need to create/move a script
│
├─ Does it BUILD C++ code or packages?
│  └─ YES → scripts/ue/build/
│
├─ Does it VALIDATE without building (fast checks)?
│  └─ YES → scripts/ue/check/
│
├─ Does it TEST functionality?
│  ├─ Critical path, <5 min? → scripts/ue/test/smoke/
│  ├─ Multi-system, 5-30 min? → scripts/ue/test/integration/
│  ├─ Single system, <1 min? → scripts/ue/test/unit/
│  └─ Shared by multiple tests? → scripts/ue/test/lib/
│
├─ Is it CI/CD orchestration (runs other scripts)?
│  └─ YES → scripts/autonomous/
│
├─ Does it CONFIGURE the environment?
│  └─ YES → scripts/config/
│
├─ Is it a general utility (NOT UE-specific)?
│  └─ YES → scripts/utils/
│
├─ Does it RUN the editor/game?
│  └─ YES → scripts/ue/run/
│
└─ Is it a standalone APPLICATION (not automation)?
   └─ YES → tools/
```

### Script Naming Rules

| Rule | Pattern | Example | ❌ Wrong |
|------|---------|---------|----------|
| Use verbs | `<verb>_<target>.bat` | `build_editor.bat` | `editor.bat` |
| Be specific | Describe exact action | `validate_blueprints.bat` | `check.bat` |
| No abbreviations | Full words | `Feature_registration.bat` | `gf_reg.bat` |
| Windows-native | `.bat` or `.ps1` | `build.bat`, `test.ps1` | `build.sh` |
| Lowercase + underscores | `snake_case` | `run_cpp_tests.bat` | `RunCppTests.bat` |

### File Extension Rules

| Extension | When to Use | Example |
|-----------|-------------|---------|
| `.bat` | Simple logic, linear execution | `build.bat` |
| `.ps1` | Complex logic, error handling, loops | `overnight_ci.ps1` |
| `.py` | Cross-platform helpers, data processing | `autonomous_boot_test.py` |
| ❌ `.sh` | **NEVER** (Windows development only) | - |

---

## Table of Contents
- [Quick Reference for Agents](#quick-reference-for-agents)
- [Folder Structure](#folder-structure)
- [Script Placement Rules](#script-placement-rules)
- [Design Principles](#design-principles)
- [Script Categories](#script-categories)
- [Best Practices](#best-practices)
- [Usage Examples](#usage-examples)
- [Maintenance Guidelines](#maintenance-guidelines)

---

## Folder Structure

```
scripts/
├── config/                          # Configuration (Single Source of Truth)
│   ├── ue_path.conf                # UE installation path
│   ├── ue_path.conf.example        # Template for new developers
│   ├── ue_env.mk                   # Make environment variables
│   └── ue_env.sh                   # Shell environment (legacy/cross-platform)
│
├── ue/                              # ALL Unreal Engine Operations
│   ├── build/                      # Build operations ONLY
│   │   ├── build.bat               # Main build script (Windows)
│   │   ├── build_editor.bat        # Editor build
│   │   ├── build_module.bat        # Single module rebuild
│   │   └── rebuild_module_safe.ps1 # Safe module rebuild with validation
│   │
│   ├── check/                      # Validation WITHOUT full build
│   │   ├── validate_uht.bat        # C++ reflection markup check
│   │   ├── validate_syntax.bat     # Syntax validation
│   │   ├── validate_blueprints.bat # Blueprint compilation check
│   │   ├── validate_assets.bat     # Asset data validation
│   │   └── reports/                # Validation output logs
│   │
│   ├── test/                       # ALL Testing (Windows-native)
│   │   ├── smoke/                  # Smoke tests (<5 min)
│   │   │   ├── boot_test.bat       # Basic boot verification
│   │   │   └── Feature_registration.bat  # GF plugin registration
│   │   │
│   │   ├── integration/            # Integration tests (5-30 min)
│   │   │   ├── standalone_test.bat          # Standalone game test
│   │   │   ├── autonomous_boot_test.bat     # Automated boot sequence
│   │   │   └── autonomous_boot_test.py      # Python helper for automation
│   │   │
│   │   ├── unit/                   # Unit tests (fast <1 min each)
│   │   │   ├── run_cpp_tests.bat            # C++ unit tests
│   │   │   └── run_ue_tests_safe.ps1        # UE test framework runner
│   │   │
│   │   ├── lib/                    # Shared test libraries
│   │   │   ├── check_logs.bat      # Log parsing utilities
│   │   │   ├── cleanup.bat         # Test cleanup routines
│   │   │   └── launch_editor.bat   # Editor launch helper
│   │   │
│   │   └── config/                 # Test-specific configuration
│   │       └── test_config.bat     # Test environment setup
│   │
│   └── run/                        # Runtime operations
│       └── run_editor.bat          # Launch UE editor
│
├── autonomous/                      # Autonomous Agent Automation
│   ├── common/                     # Shared utilities
│   │   ├── Common.psm1             # Resilience library
│   │   └── discover_todos.ps1      # TODO counter
│   ├── claude/                     # Claude-specific workflows
│   │   ├── overnight/              # Overnight infinite loop
│   │   │   └── main.ps1            # Main orchestrator
│   │   └── yolo/                   # YOLO mode
│   │       └── yolo.ps1            # YOLO runner
│
└── utils/                           # General-purpose utilities
    ├── sanitize_text.py            # Text sanitization
    └── kill_ue_processes.bat       # Process cleanup

tools/                               # External Applications (NOT automation scripts)
└── agentic/                        # LangGraph agent tooling
    ├── graph.py                    # Agent graph definition
    ├── langgraph.json              # Agent configuration
    └── scripts/                    # Agent-specific scripts
        ├── run.py                  # Agent runner
        └── restart.bat             # Agent restart utility
```

---

## Script Placement Rules

**CRITICAL:** Every script MUST have exactly ONE correct location. Use this table to determine placement.

| Script Purpose | Location | Duration | Modifies Build? | Example Scripts |
|----------------|----------|----------|-----------------|-----------------|
| **Build C++ code/packages** | `scripts/ue/build/` | Minutes-Hours | ✅ YES | `build.bat`, `build_editor.bat`, `rebuild_module_safe.ps1` |
| **Validate without build** | `scripts/ue/check/` | <5 min | ❌ NO | `validate_uht.bat`, `validate_blueprints.bat`, `validate_assets.bat` |
| **Smoke test (critical path)** | `scripts/ue/test/smoke/` | <5 min | ❌ NO | `boot_test.bat`, `Feature_registration.bat` |
| **Integration test** | `scripts/ue/test/integration/` | 5-30 min | ❌ NO | `standalone_test.bat`, `autonomous_boot_test.bat` |
| **Unit test** | `scripts/ue/test/unit/` | <1 min | ❌ NO | `run_cpp_tests.bat`, `run_ue_tests_safe.ps1` |
| **Shared test utilities** | `scripts/ue/test/lib/` | N/A | ❌ NO | `check_logs.bat`, `cleanup.bat`, `launch_editor.bat` |
| **Test configuration** | `scripts/ue/test/config/` | N/A | ❌ NO | `test_config.bat` |
| **Run editor/game** | `scripts/ue/run/` | N/A | ❌ NO | `run_editor.bat` |
| **Autonomous Agents** | `scripts/autonomous/` | Hours | ✅ YES | `claude/overnight/main.ps1`, `claude/yolo/yolo.ps1` |
| **Environment config** | `scripts/config/` | N/A | ❌ NO | `ue_path.conf`, `ue_env.mk` |
| **General utilities** | `scripts/utils/` | Varies | ❌ NO | `sanitize_text.py`, `kill_ue_processes.bat` |
| **External applications** | `tools/` | N/A | ❌ NO | `tools/agentic/graph.py` |

### Key Constraints

**MUST:**
- ✅ All scripts reference `scripts/config/ue_path.conf` for UE path
- ✅ Use Windows-native formats (`.bat`, `.ps1`)
- ✅ Include header comment with purpose/usage/example
- ✅ Return exit code 0 (success) or 1 (failure)
- ✅ Check prerequisites before execution

**MUST NOT:**
- ❌ Create `.sh` files (Windows-only environment)
- ❌ Hardcode paths (use config files)
- ❌ Mix purposes (build + test in one script)
- ❌ Duplicate logic (use `lib/` for shared code)
- ❌ Create scripts in deprecated locations (`utility/`, `scripts/win/`)

---

## Design Principles

### 1. Single Responsibility Principle (SRP)

**Each folder has ONE clear purpose:**

- ✅ `scripts/ue/build/` → Build operations ONLY
- ✅ `scripts/ue/check/` → Validation WITHOUT build
- ✅ `scripts/ue/test/smoke/` → Quick smoke tests ONLY
- ✅ `scripts/config/` → Configuration ONLY

**Anti-pattern (OLD):**
```
utility/check/check_compile.sh  # Mixed: checks AND compiles
```

**Correct (NEW):**
```
scripts/ue/check/validate_syntax.bat  # ONLY validates
scripts/ue/build/build.bat            # ONLY builds
```

### 2. Open/Closed Principle

**Easy to extend without modifying existing structure:**

- Need E2E tests? → Create `scripts/ue/test/e2e/`
- Need Linux builds? → Create `scripts/ue/build/linux/`
- Need performance tests? → Create `scripts/ue/test/performance/`

The hierarchy is designed to accommodate growth without restructuring.

### 3. DRY (Don't Repeat Yourself)

**Single Source of Truth:**

- ❌ OLD: Config files duplicated in `utility/config/` AND `scripts/config/`
- ✅ NEW: Single config location: `scripts/config/`

**All scripts read from ONE location:**
```batch
REM All scripts reference the same config
call "%~dp0..\..\config\ue_path.conf"
```

### 4. Separation of Concerns

**Clear boundaries between categories:**

- `scripts/` → Automation scripts (build, test, validate)
- `tools/` → Standalone applications (agentic tools, external utilities)

**Tools vs Scripts:**
- **Scripts** = Automation for the project (build, test, CI)
- **Tools** = External applications that run independently

### 5. Platform Consistency

**Windows-first approach (agent-friendly):**

- ✅ `.bat` files for simple operations
- ✅ `.ps1` files for complex logic
- ❌ `.sh` files removed (Windows development environment)

**Rationale:**
- Primary development platform: Windows
- Claude Code agent compatibility: Better .bat/.ps1 support
- Cleaner execution without WSL/Git Bash translation layer

If cross-platform support needed later:
- Option A: PowerShell Core (.ps1 works on Windows/Linux/macOS)
- Option B: Separate platform folders (`scripts/ue/build/win/`, `scripts/ue/build/linux/`)

---

## Script Categories

### Configuration (`scripts/config/`)

**Purpose:** Central configuration for ALL scripts

**Key Files:**
- `ue_path.conf` - Unreal Engine installation path
- `ue_path.conf.example` - Template for new developers

**Usage:**
```batch
@echo off
call "%~dp0..\..\config\ue_path.conf"
echo Using UE at: %UE_PATH%
```

**Why centralized?**
- Change UE path in ONE place
- All scripts automatically pick up changes
- Prevents version drift across scripts

### Build (`scripts/ue/build/`)

**Purpose:** Compile C++ code, build plugins, package game

**Characteristics:**
- Long-running operations (minutes to hours)
- Modifies build artifacts
- Requires UBT (Unreal Build Tool)

**Examples:**
```batch
REM Build entire editor
scripts\ue\build\build.bat AlisEditor Win64 Development

REM Build specific module
scripts\ue\build\build_module.bat ProjectMenuMain
```

### Check (`scripts/ue/check/`)

**Purpose:** Fast validation WITHOUT full compilation

**Characteristics:**
- Fast execution (<5 minutes)
- Read-only operations
- Catches errors early (before build)

**Examples:**
```batch
REM Validate C++ reflection markup
scripts\ue\check\validate_uht.bat

REM Compile all Blueprints
scripts\ue\check\validate_blueprints.bat
```

**Why separate from build?**
- Faster feedback loop (seconds vs minutes)
- Pre-commit hooks can run checks quickly
- CI can fail fast on validation errors

### Test (`scripts/ue/test/`)

**Purpose:** Automated testing (unit, integration, smoke)

**Test Pyramid Structure:**

```
        /\
       /  \     Unit Tests (fast, many)
      /____\
     /      \   Integration Tests (medium)
    /________\
   /          \ Smoke Tests (slow, few)
  /____________\
```

#### Smoke Tests (`scripts/ue/test/smoke/`)
- **Duration:** <5 minutes each
- **Purpose:** Critical path verification
- **When to run:** Pre-commit, pre-deployment
- **Examples:**
  - `boot_test.bat` - Verify game boots
  - `Feature_registration.bat` - Verify plugins register

#### Integration Tests (`scripts/ue/test/integration/`)
- **Duration:** 5-30 minutes each
- **Purpose:** Multi-system interaction testing
- **When to run:** Nightly builds, PR validation
- **Examples:**
  - `standalone_test.bat` - Full game session test
  - `autonomous_boot_test.bat` - Automated boot flow

#### Unit Tests (`scripts/ue/test/unit/`)
- **Duration:** <1 minute each
- **Purpose:** Single-system verification
- **When to run:** Every build, TDD workflow
- **Examples:**
  - `run_cpp_tests.bat` - C++ unit tests
  - `run_ue_tests_safe.ps1` - UE Automation Framework tests

**Test Libraries (`scripts/ue/test/lib/`):**
- Shared utilities across test types
- Prevents code duplication
- Examples: Log parsing, cleanup, editor launch

### Autonomous (`scripts/autonomous/`)

**Purpose:** Autonomous Agent Automation (Claude)

**Characteristics:**
- Orchestrates other scripts
- Long-running (hours)
- Runs unattended

**Example Flow:**
```powershell
# scripts/autonomous/claude/overnight/main.ps1 orchestrates:
1. Cleanup old builds
2. Pull latest code
3. Run checks (validate_uht, validate_blueprints)
4. Build (build.bat)
5. Run tests (smoke → integration → unit)
6. Package build
7. Report results
```

### Utilities (`scripts/utils/`)

**Purpose:** General-purpose helpers (NOT UE-specific)

**Examples:**
- `sanitize_text.py` - Text processing
- `kill_ue_processes.bat` - Process management

**When to add here:**
- Script used by multiple categories
- No UE dependency
- General utility (not specific to build/test/check)

### Tools (`tools/`)

**Purpose:** Standalone applications (NOT automation scripts)

**Distinction:**
- Scripts = Called by other scripts, part of automation pipeline
- Tools = Standalone apps with their own lifecycle

**Example:**
- `tools/agentic/` - LangGraph agent application
  - Has own dependencies (Python, langgraph)
  - Runs independently
  - Not part of UE build/test pipeline

---

## Best Practices

### 1. Script Creation Checklist for Agents

When creating a new script, follow this exact sequence:

- [ ] **Determine location** using decision tree (see Quick Reference)
- [ ] **Choose extension**: `.bat` (simple) or `.ps1` (complex)
- [ ] **Name the script**: `<verb>_<target>.<ext>` (e.g., `validate_blueprints.bat`)
- [ ] **Add header** using template below
- [ ] **Source config**: Call `scripts/config/ue_path.conf` at the start
- [ ] **Check prerequisites**: Verify UE path, project file exist
- [ ] **Implement logic**: Single responsibility only
- [ ] **Handle errors**: Return exit code 1 on failure
- [ ] **Return success**: Return exit code 0 on success
- [ ] **Test execution**: Run script and verify behavior
- [ ] **Document usage**: Update this file if adding new category

### 2. Script Header Template

**Every script MUST start with this header:**

```batch
@echo off
REM ============================================================================
REM Script: <script_name>.bat
REM Purpose: <One-line description of what this script does>
REM Usage: <script_name>.bat [arguments]
REM Example: <script_name>.bat AlisEditor Win64 Development
REM Location: scripts/ue/<category>/<script_name>.bat
REM Exit Codes: 0=Success, 1=Failure
REM Author: <Your name or "Claude Code Agent">
REM Created: <YYYY-MM-DD>
REM Updated: <YYYY-MM-DD>
REM ============================================================================

REM Load UE configuration
call "%~dp0..\..\config\ue_path.conf"
if not exist "%UE_PATH%" (
    echo ERROR: UE_PATH not found: %UE_PATH%
    exit /b 1
)

REM Your script logic here...
```

**PowerShell Template:**

```powershell
# ============================================================================
# Script: <script_name>.ps1
# Purpose: <One-line description of what this script does>
# Usage: .<script_name>.ps1 [-Parameter Value]
# Example: .\<script_name>.ps1 -ModuleName ProjectMenuMain
# Location: scripts/ue/<category>/<script_name>.ps1
# Exit Codes: 0=Success, 1=Failure
# Author: <Your name or "Claude Code Agent">
# Created: <YYYY-MM-DD>
# Updated: <YYYY-MM-DD>
# ============================================================================

# Load UE configuration
. "$PSScriptRoot\..\..\config\ue_path.conf"
if (-not (Test-Path $UE_PATH)) {
    Write-Error "UE_PATH not found: $UE_PATH"
    exit 1
}

# Your script logic here...
```

### 3. Naming Conventions

**Script Names:**
- Use verbs: `build.bat`, `validate_blueprints.bat`, `run_tests.bat`
- Be specific: `validate_uht.bat` not `check.bat`
- Avoid abbreviations: `Feature_registration.bat` not `gf_reg.bat`

**Directory Names:**
- Plural for collections: `tests/`, `scripts/`, `utils/`
- Singular for categories: `build/`, `check/`, `ci/`
- Lowercase with underscores: `unit_tests/` not `UnitTests/`

### 4. Error Handling

**All scripts should:**
- Check prerequisites (UE path, project file)
- Validate arguments
- Return proper exit codes (0=success, 1=failure)
- Log errors clearly

**Example:**
```batch
if not exist "%UE_PATH%" (
    echo ERROR: UE_PATH not found: %UE_PATH%
    exit /b 1
)
```

### 5. Modularity

**Don't duplicate logic - create libraries:**

❌ **Bad (duplicated):**
```batch
REM In test1.bat
taskkill /F /IM UnrealEditor.exe

REM In test2.bat
taskkill /F /IM UnrealEditor.exe
```

✅ **Good (library):**
```batch
REM scripts/ue/test/lib/cleanup.bat
call cleanup.bat
```

### 6. Configuration Over Hardcoding

❌ **Bad:**
```batch
set UE_PATH=<hardcoded path>
```

✅ **Good:**
```batch
call "%~dp0..\..\config\ue_path.conf"
```

---

## Common Agent Tasks

### Task 1: Create a New Build Script

**Scenario:** Need to build a specific module quickly.

**Steps:**
1. Location: `scripts/ue/build/build_module.bat`
2. Use template with header
3. Implement:
```batch
@echo off
REM [Header from template above]

call "%~dp0..\..\config\ue_path.conf"
if not exist "%UE_PATH%" (echo ERROR: UE path not found && exit /b 1)

set MODULE_NAME=%1
if "%MODULE_NAME%"=="" (echo ERROR: Module name required && exit /b 1)

"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" AlisEditor Win64 Development "<project-root>\Alis.uproject" -Module=%MODULE_NAME%
exit /b %ERRORLEVEL%
```

### Task 2: Create a New Validation Check

**Scenario:** Need to validate Feature plugin registration.

**Steps:**
1. Location: `scripts/ue/check/validate_Features.bat`
2. Duration: <5 min (no build)
3. Implement:
```batch
@echo off
REM [Header from template]

call "%~dp0..\..\config\ue_path.conf"

echo Checking Feature plugin registration...
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<project-root>\Alis.uproject" -run=CheckFeatures -unattended -nopause
if %ERRORLEVEL% neq 0 (echo ERROR: Feature validation failed && exit /b 1)

echo SUCCESS: All Features validated
exit /b 0
```

### Task 3: Create a New Smoke Test

**Scenario:** Need to verify editor boots successfully.

**Steps:**
1. Location: `scripts/ue/test/smoke/editor_boot_test.bat`
2. Duration: <5 min
3. Implement:
```batch
@echo off
REM [Header from template]

call "%~dp0..\..\config\ue_path.conf"
call "%~dp0..\lib\cleanup.bat"

echo Starting editor boot test...
start /wait "" "%UE_PATH%\Engine\Binaries\Win64\UnrealEditor.exe" "<project-root>\Alis.uproject" -log

call "%~dp0..\lib\check_logs.bat" "Saved\Logs\Alis.log" "Error"
if %ERRORLEVEL% neq 0 (echo ERROR: Boot failed && exit /b 1)

echo SUCCESS: Editor boot test passed
exit /b 0
```

### Task 4: Move an Existing Script

**Scenario:** Found script in wrong location: `utility/test/old_test.sh`

**Agent Actions:**
```
1. Determine script purpose: It tests boot sequence → Integration test
2. Check duration: 10 minutes → Integration test confirmed
3. Target location: scripts/ue/test/integration/
4. Convert .sh to .bat (Windows-only environment)
5. Rename: old_test.sh → boot_sequence_test.bat
6. Add header template
7. Source config: call "%~dp0..\..\..\config\ue_path.conf"
8. Test execution
9. Delete old file: utility/test/old_test.sh
```

### Task 5: Debug a Failing Script

**Agent Checklist:**
- [ ] Verify script location is correct
- [ ] Check script sources `scripts/config/ue_path.conf`
- [ ] Verify UE_PATH exists and is valid
- [ ] Check exit codes (0=success, 1=failure)
- [ ] Review logs in `scripts/ue/check/reports/` or test output
- [ ] Ensure script has single responsibility
- [ ] Check for hardcoded paths (use config instead)
- [ ] Verify Windows-native format (.bat or .ps1, NOT .sh)

---

## Usage Examples

### Developer Workflow

**Quick pre-commit check:**
```batch
scripts\ue\check\validate_uht.bat
scripts\ue\check\validate_blueprints.bat
```

**Build and test:**
```batch
scripts\ue\build\build.bat AlisEditor Win64 Development
scripts\ue\test\smoke\boot_test.bat
```

**Rebuild single module (fast iteration):**
```batch
scripts\ue\build\rebuild_module_safe.ps1 -ModuleName ProjectMenuMain
```

### CI/CD Workflow

**Nightly build:**
```powershell
scripts\autonomous\claude\overnight\main.ps1
```

**PR validation:**
```batch
REM Fast checks
scripts\ue\check\validate_uht.bat
scripts\ue\check\validate_blueprints.bat

REM Smoke tests only (fast)
for %%T in (scripts\ue\test\smoke\*.bat) do call %%T
```

### Debugging Tests

**Check logs:**
```batch
scripts\ue\test\lib\check_logs.bat Saved\Logs\Alis.log "Error"
```

**Cleanup after failed test:**
```batch
scripts\ue\test\lib\cleanup.bat
```

---

## Maintenance Guidelines

### Adding New Scripts

1. **Determine category:**
   - Does it build? → `scripts/ue/build/`
   - Does it validate? → `scripts/ue/check/`
   - Does it test? → `scripts/ue/test/{smoke|integration|unit}/`
   - Is it autonomous? → `scripts/autonomous/`
   - Is it general? → `scripts/utils/`

2. **Follow naming conventions:**
   - Verb-based: `validate_X.bat`, `run_X.bat`, `build_X.bat`
   - Specific and descriptive

3. **Add documentation header:**
   - Purpose, usage, example, author, date

4. **Use central config:**
   - Source `scripts/config/ue_path.conf`
   - Don't hardcode paths

5. **Update this document:**
   - Add script to appropriate section
   - Document usage example

### Refactoring Old Scripts

**When you encounter old scripts:**

1. **Identify purpose** (build/check/test/CI/util)
2. **Check for duplication** (can it use lib/?)
3. **Move to correct location**
4. **Update documentation**
5. **Test thoroughly**
6. **Update references** (in CLAUDE.md, other scripts)

### Deprecation Process

**When removing old scripts:**

1. Move to `scripts/deprecated/` (don't delete immediately)
2. Add deprecation notice:
   ```batch
   @echo off
   echo WARNING: This script is DEPRECATED
   echo Use: scripts\ue\build\build.bat instead
   pause
   ```
3. Update all documentation
4. Delete after 1 month if no issues

---

## Migration Guide

### From Old Structure to New

**Old → New mapping:**

```
utility/check/check_compile.sh       → scripts/ue/check/validate_syntax.bat
utility/config/ue_path.conf          → scripts/config/ue_path.conf
utility/test/run_tests.sh            → scripts/ue/test/unit/run_cpp_tests.bat
utility/test/run_integration_tests.sh→ scripts/ue/test/integration/standalone_test.bat

scripts/test/autonomous_boot_test.bat→ scripts/ue/test/integration/autonomous_boot_test.bat
scripts/test/standalone.bat          → scripts/ue/test/integration/standalone_test.bat
scripts/test/lib/                    → scripts/ue/test/lib/ (keep)
scripts/win/build.bat                → scripts/ue/build/build.bat
scripts/win/check.bat                → scripts/ue/check/validate_uht.bat
scripts/check/validate.sh            → scripts/ue/check/validate_blueprints.bat
scripts/build/build.sh               → scripts/ue/build/build.bat
scripts/overnight_ci.ps1             → scripts/autonomous/claude/overnight/main.ps1
scripts/sanitize_text.py             → scripts/utils/sanitize_text.py

tools/agentic/                       → tools/agentic/ (keep)
```

### Updating References

**Files that reference scripts:**
- `CLAUDE.md` - Update all script paths
- `Makefile` - Update build/check/test targets
- `.claude/settings.json` - Update approved Bash commands
- CI configuration files
- Other scripts that call these scripts

**Search and replace:**
```batch
REM Find old references
findstr /S /I "utility/check" *.md *.bat *.ps1

REM Update to new paths
REM utility/check → scripts/ue/check
```

---

## Rationale Summary

### Why This Structure is Best

1. **SOLID Principles Applied**
   - Single Responsibility: Each folder has ONE purpose
   - Open/Closed: Easy to extend without modifying
   - DRY: No duplication, single config source

2. **Industry Standards**
   - Test pyramid (smoke/integration/unit)
   - Separation of build/check/test
   - Clear CI/CD organization

3. **Agent-Friendly**
   - Windows-native scripts (.bat/.ps1)
   - Claude Code better compatibility
   - Consistent conventions

4. **Scalability**
   - Easy to add new test types
   - Clear where new scripts belong
   - Supports multi-platform later

5. **Maintainability**
   - Self-documenting structure
   - Reduced cognitive load
   - Easy onboarding for new developers

6. **Performance**
   - Fast checks separate from slow builds
   - Test pyramid enables quick feedback
   - Parallel test execution possible

---

## References

**Related Documentation:**
- [CLAUDE.md](../../CLAUDE.md) - Project overview and conventions
- [testing/overview.md](../testing/overview.md) - Detailed testing guidelines
- [architecture/core_principles.md](../architecture/core_principles.md) - Architecture principles

**External Resources:**
- [Unreal Engine Build Operations](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine)
- [UE Automation Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine)
- [SOLID Principles](https://en.wikipedia.org/wiki/SOLID)
- [Test Pyramid](https://martinfowler.com/bliki/TestPyramid.html)

---

## Quick Reference Card for Agents

**Copy this section for fast decision-making:**

### Script Placement (One-Liner Rules)

```
Build/Package? → scripts/ue/build/
Fast validation (<5min, no build)? → scripts/ue/check/
Critical path test (<5min)? → scripts/ue/test/smoke/
Multi-system test (5-30min)? → scripts/ue/test/integration/
Single-system test (<1min)? → scripts/ue/test/unit/
Shared test code? → scripts/ue/test/lib/
CI orchestration? → scripts/ci/
General utility (not UE-specific)? → scripts/utils/
External app? → tools/
```

### File Extensions

```
Simple logic → .bat
Complex logic/loops/error handling → .ps1
NO .sh files (Windows-only)
```

### Every Script Must

```
✅ Include header with purpose/usage/example
✅ Source scripts/config/ue_path.conf
✅ Check prerequisites (UE path exists)
✅ Return 0 (success) or 1 (failure)
✅ Use verb_target.ext naming
✅ Have single responsibility
```

### Every Script Must NOT

```
❌ Hardcode paths
❌ Create .sh files
❌ Mix purposes (build+test)
❌ Duplicate logic (use lib/)
❌ Use deprecated locations (utility/, scripts/win/)
```

### Config Path Patterns

```batch
# From scripts/ue/build/ or scripts/ue/check/ or scripts/ue/run/
call "%~dp0..\..\config\ue_path.conf"

# From scripts/ue/test/smoke/ or scripts/ue/test/integration/ or scripts/ue/test/unit/
call "%~dp0..\..\..\config\ue_path.conf"

# From scripts/ue/test/lib/
call "%~dp0..\..\..\..\config\ue_path.conf"

# From scripts/ci/ or scripts/utils/
call "%~dp0..\config\ue_path.conf"
```

### Common UE Commands

```batch
# Build
"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" <Target> <Platform> <Config> "<ProjectPath>"

# Run editor
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor.exe" "<ProjectPath>"

# Run automation tests
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<ProjectPath>" -ExecCmds="Automation RunTests <TestName>" -unattended -nopause -NullRHI -log

# Validate
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<ProjectPath>" -run=<Command> -unattended
```

### Decision Tree (Copy-Paste)

```
Q: What does this script do?
├─ Compiles C++ → scripts/ue/build/
├─ Checks without compiling → scripts/ue/check/
├─ Tests critical path (<5min) → scripts/ue/test/smoke/
├─ Tests integration (5-30min) → scripts/ue/test/integration/
├─ Tests single system (<1min) → scripts/ue/test/unit/
├─ Shared test utility → scripts/ue/test/lib/
├─ Orchestrates CI → scripts/ci/
├─ Configures environment → scripts/config/
├─ General purpose utility → scripts/utils/
├─ Runs editor/game → scripts/ue/run/
└─ Standalone application → tools/
```

---

**Last Updated:** 2025-10-24
**Version:** 2.0 (Agent-Optimized)
**Maintainer:** Development Team
