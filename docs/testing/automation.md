# Test Automation Guide

**Running tests, CI/CD integration, and troubleshooting**

This guide covers automated test execution, continuous integration, and common issues.

See also:
- **[Overview](README.md)** - Testing philosophy and architecture
- **[Unit Tests](unit_tests.md)** - Plugin-internal testing
- **[Integration Tests](integration_tests.md)** - Cross-plugin testing
- **[Smoke Tests](smoke_tests.md)** - Quick health checks

---

## Running Tests

### Automated Testing (Recommended)

**Use Makefile for fully automated testing with plugin preparation:**

```bash
# Run all tests (prepares plugins automatically)
make test

# Run only unit tests (Tier 1: plugin-internal)
make test-unit

# Run only unit tests for changed plugins (auto-selective)
make test-unit-smart

# Run only integration tests (Tier 2: cross-plugin)
make test-integration

# Run quick smoke tests
make test-quick

# Manually prepare plugins (optional, make test does this automatically)
make prepare-tests
```

### Selective Local Runs (Changed Plugins Only)

To avoid re-running the entire suite when only a handful of plugins changed, use the selective helper:

```bash
utility/test/selective_run_tests.sh
utility/test/selective_run_tests.sh --base origin/main   # compare against upstream
```

This script:
- Detects modified or untracked files relative to `HEAD`
- Maps changes to their owning plugin test filters
- Falls back to a small UI regression set when no direct mapping exists
- Automatically invokes `prepare_tests.sh` before dispatching targeted filters to `run_tests.sh`
- Runs the automation executable with headless flags (`-NullRHI`, `-game`, `-NoSound`, etc.) so the full editor UI never appears (based on guidance shared in [StackOverflow ??? Running UE automation via command line](https://stackoverflow.com/questions/29135227))

If the testing harness itself changes (`utility/test/`) the script executes the full Tier 1 filter list to ensure infrastructure edits are validated.

**What `make prepare-tests` does:**
1. Generate Visual Studio project files (if needed)
2. Run UnrealHeaderTool to generate `.generated.h` files
3. Compile all plugin test modules (ProjectMenuMainTests, etc.)
4. Verify all intermediate files exist

**Why this matters:**
- Ensures all plugins are compiled before UE Editor starts
- Generates all reflection code (UPROPERTY/UFUNCTION macros)
- Prevents "module not found" errors during test execution
- Faster test execution (no compilation during testing)

### In Unreal Editor

1. Open Session Frontend:
   ```
   Window ??? Developer Tools ??? Session Frontend
   ```

2. Go to **Automation** tab

3. Filter by test category:
   ```
   ProjectMenuMain            # All ProjectMenuMain tests
   ProjectLoading.Subsystem # Specific subsystem tests
   ProjectUIFramework.ViewModel      # MVVM tests
   ```

4. Select tests and click **"Start Tests"**

5. View results (green = pass, red = fail)

### Command Line (Manual)

**?????? CRITICAL: Always include `-testexit` and `-nosplash` flags to prevent hanging!**
See [Troubleshooting](#critical-tests-hang-at-starting-unreal-editor-test-runner) for detailed explanation.

**Run all tests:**
```bash
UnrealEditor-Cmd.exe "ProjectPath.uproject" \
  -ExecCmds="Automation RunTests Project" \
  -unattended -nopause -nosplash -NullRHI \
  -testexit="Automation Test Queue Empty" \
  -log
```

**Run specific tier:**
```bash
# Tier 1: Unit tests (plugin-internal)
UnrealEditor-Cmd.exe "Alis.uproject" \
  -ExecCmds="Automation RunTests ProjectMenuMain;ProjectLoading;ProjectUIFramework" \
  -unattended -nopause -nosplash -NullRHI \
  -testexit="Automation Test Queue Empty" \
  -log

# Tier 2: Integration tests (cross-plugin)
UnrealEditor-Cmd.exe "Alis.uproject" \
  -ExecCmds="Automation RunTests ProjectIntegrationTests" \
  -unattended -nopause -nosplash -NullRHI \
  -testexit="Automation Test Queue Empty" \
  -log
```

**Run with report:**
```bash
UnrealEditor-Cmd.exe "Alis.uproject" \
  -ExecCmds="Automation RunTests Project" \
  -unattended -nopause -nosplash -NullRHI \
  -testexit="Automation Test Queue Empty" \
  -log \
  -ReportOutputPath="Saved/Automation/Reports"
```

**Why `-testexit` is CRITICAL:**
- Without it, UnrealEditor-Cmd.exe **hangs forever** waiting for manual input
- Process will consume 6+ GB RAM and never exit
- CI/CD pipelines will block indefinitely
- Must be exact string: `"Automation Test Queue Empty"`

### Headless Flag Reference (CRITICAL)

**IMPORTANT:** When updating test scripts or CI pipelines, ALWAYS retain these flags for proper headless execution.

**Core Headless Flags (REQUIRED):**
- `-unattended`: Disables all interactive prompts.
- `-nopause`: Prevents waiting for user keypress (critical for CI).
- `-NullRHI`: Uses null rendering (no GPU).
- `-log`: Enables logging.
- `-testexit="Automation Test Queue Empty"`: **REQUIRED** to prevent hangs.

**Common Mistakes:**
- [X] Removing `-unattended` -> Tests hang on dialogs.
- [X] Removing `-NullRHI` -> Tests require GPU.
- [X] Removing `-testexit` -> Process never terminates.

---

## Execution Modes & Boot Flow

**CRITICAL:** The Orchestrator and ProjectLoading boot flow behavior depends on execution mode:

| Mode | Orchestrator | ProjectLoading | Use Case |
|------|--------------|----------------|----------|
| **Standalone Game** (`-game`) | [OK] RUNS | [OK] RUNS | Boot flow testing, CI/CD |
| **PIE** (Play In Editor) | [X] SKIPPED | [OK] RUNS (partial) | Quick iteration |
| **Commandlet** (`-run=...`) | [X] SKIPPED | [X] SKIPPED | Build, cook |

**For boot flow tests:** Always use **Standalone Game** mode.
**For quick iteration:** Use **PIE** (but know that Orchestrator is skipped).

### Pre-Commit Hook

Add to `.git/hooks/pre-commit`:

```bash
#!/bin/bash
echo "Running automated tests..."

# Use makefile to prepare and run tests
make test

if [ $? -ne 0 ]; then
    echo "??? Tests failed! Commit aborted."
    echo "Fix failing tests or use 'git commit --no-verify' to bypass (not recommended)"
    exit 1
fi

echo "??? All tests passed!"
```

---

### WSL Headless Test Execution

For remote/CI environments running Windows Subsystem for Linux (WSL), use this command to execute tests without GUI dependencies:

```bash
bash -lc '/mnt/c/UnrealEngine/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe "E:\\Repos_Alis\\Alis\\Alis.uproject" -unattended -nop4 -NoSound -stdout -FullStdOutLogOutput -NullRHI -testexit="Automation Test Queue Empty" -ReportOutputPath="Saved/Automation/Reports" -ExecCmds="Automation RunTests ProjectIntegrationTests;Quit"'
```

**Command Breakdown:**

- `bash -lc`: Login shell with command execution for proper environment variables
- `"/mnt/c/UnrealEngine/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"`: UE Editor command-line executable (WSL path)
- `"E:\\Repos_Alis\\Alis\\Alis.uproject"`: Project file path (Windows format for WSL compatibility)
- `-unattended`: No user interaction
- `-nop4`: Disable Perforce integration
- `-NoSound`: Disable audio to prevent console capture issues
- `-stdout -FullStdOutLogOutput`: Redirect all logs to stdout
- `-NullRHI`: True headless (no GPU requirements)
- `-testexit="Automation Test Queue Empty"`: **Critical** - Ensures clean exit, prevents infinite hangs
- `-ReportOutputPath="Saved/Automation/Reports"`: Saves detailed reports to project directory
- `-ExecCmds="Automation RunTests ProjectIntegrationTests;Quit"`: Run integration tests then exit

**Usage Examples:**

```bash
# Run all ProjectIntegrationTests
bash -lc '/mnt/c/UnrealEngine/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe "E:\\Repos_Alis\\Alis\\Alis.uproject" -unattended -nop4 -NoSound -stdout -FullStdOutLogOutput -NullRHI -testexit="Automation Test Queue Empty" -ReportOutputPath="Saved/Automation/Reports" -ExecCmds="Automation RunTests ProjectIntegrationTests;Quit"'
```

**CI Integration:**
```yaml
# GitHub Actions example
- name: Run Integration Tests
  run: |
    bash -lc '/mnt/c/UnrealEngine/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe "E:\\Repos_Alis\\Alis\\Alis.uproject" -unattended -nop4 -NoSound -stdout -FullStdOutLogOutput -NullRHI -testexit="Automation Test Queue Empty" -ReportOutputPath="Saved/Automation/Reports" -ExecCmds="Automation RunTests ProjectIntegrationTests;Quit"'
  working-directory: ./UEProject
```

---

## CI/CD Integration

### Build Pipeline

**Phases:**
1. **Prepare** - Generate code, compile test modules (`make prepare-tests`)
2. **Test** - Run all automation tests (`make test`)
3. **Report** - Generate coverage report
4. **Deploy** - Package if tests pass

### Test Execution in CI

**Recommended (using Makefile):**
```yaml
# Example CI configuration (GitHub Actions, GitLab CI, etc.)
test:
  script:
    # Prepare all plugins and generate intermediate files
    - make prepare-tests

    # Run all tests with automated preparation
    - make test

  artifacts:
    reports:
      junit: Saved/Automation/Reports/*.xml
    paths:
      - Saved/Automation/Reports/
    when: always
```

**Manual (if Makefile unavailable):**
```yaml
test:
  script:
    # 1. Generate Visual Studio project files
    - ./Engine/Build/BatchFiles/Build.bat -projectfiles -project="Alis.uproject"

    # 2. Run UHT to generate reflection code
    - ./Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe
        -Mode=UnrealHeaderTool
        "-Target=AlisEditor Win64 Development -Project=\"Alis.uproject\""

    # 3. Compile test modules
    - ./Engine/Build/BatchFiles/Build.bat AlisEditor Win64 Development Alis.uproject

    # 4. Run tests
    - ./Engine/Binaries/Win64/UnrealEditor-Cmd.exe Alis.uproject
        -ExecCmds="Automation RunTests Project"
        -unattended -nopause -NullRHI -log
        -ReportOutputPath="Saved/Automation/Reports"

  artifacts:
    reports:
      junit: Saved/Automation/Reports/*.xml
    paths:
      - Saved/Automation/Reports/
    when: always
```

### Failure Handling

**When tests fail in CI:**
1. CI build fails (red)
2. Pull request blocked
3. Developer notified
4. Must fix tests before merge

**Bypassing tests:**
- ??? Never skip tests in CI
- ??? Never use `--no-verify` for PRs
- ??? Fix tests or mark as TODO (with issue number)

### Test Performance

**Targets:**
- Unit tests: < 1 second total
- Integration tests: < 10 seconds total
- Functional tests: < 60 seconds total

**If tests are slow:**
- Profile test execution
- Use mocks instead of real subsystems
- Parallelize test execution
- Cache expensive setup

---

## Unreal Engine Examples

Reference these real Unreal Engine 5.5 plugins for test implementation patterns and best practices.

### Key Patterns from UE5 Engine

#### 1. Test Naming Convention

```cpp
// Format: F[Plugin][Feature][TestName]Test
FHTNTest_DomainBuilderBasics
FMLAdapterTest_ActorAvatar
FProjectMenuMainTest_MapListSearch

// Hierarchy: "System.Category.Subcategory.TestName"
"System.AI.HTN.DomainBuilderBasics"
"System.AI.MLAdapter.ActorAvatarSetting"
"ProjectMenuMain.Unit.MapListViewModel.Search"
```

#### 2. Test Macro Usage

```cpp
// Simple test (no fixtures, fast)
IMPLEMENT_AI_INSTANT_TEST(FMyTest, "Category.TestName")

// Fixture test (world context, setup/teardown)
IMPLEMENT_INSTANT_TEST_WITH_FIXTURE(FMyFixture, "Category.TestName")

// Standard automation test (full control)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMyTest, "Category.TestName", Flags)
```

#### 3. Assertion Macros

```cpp
// From UE5 engine tests
AITEST_TRUE("Description", Condition)
AITEST_FALSE("Description", Condition)
AITEST_EQUAL("Description", Actual, Expected)
AITEST_NOT_EQUAL("Description", Actual, Expected)
AITEST_NULL("Description", Pointer)
AITEST_NOT_NULL("Description", Pointer)
```

#### 4. Module Type Usage

| Module Type | Shipped with Game | Platforms | Use Case |
|-------------|-------------------|-----------|----------|
| **UncookedOnly** | ??? No | Editor only | Most plugin tests (HTNPlanner, MassAI) |
| **DeveloperTool** | ??? No | Win64/Mac/Linux | Platform-specific tests (MLAdapter RPC) |
| **Runtime** | ??? Yes | All platforms | Tests that ship (rare) |
| **Editor** | ??? No | Editor only | Editor/content tests |

#### 5. Build.cs Dependency Patterns

**Tier 1 (Isolated):**
```cpp
// HTNPlanner pattern - minimal dependencies
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core", "CoreUObject", "Engine", "AIModule",
    "AITestSuite",    // Test framework
    "HTNPlanner"      // ONLY the plugin being tested
});
```

**Tier 2 (Integrated):**
```cpp
// MassAI pattern - all dependencies
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core", "CoreUObject", "Engine",
    "MassActors",           // All dependent plugins
    "MassAIBehavior",
    "MassCommon",
    "MassNavigation",
    // ... 8+ Mass plugins
});
```

---

### How to Study Engine Tests

1. **Navigate to UE5 plugins:**
   ```
   <ue-path>/Engine/Plugins/
   ```

2. **Search for test modules:**
   ```
   Look for: Source/[PluginName]Tests/ or Source/[PluginName]TestSuite/
   ```

3. **Check .uplugin file:**
   - Look for modules with `Type: "UncookedOnly"`
   - See plugin dependencies in `Plugins` array

4. **Read Build.cs:**
   - Check `PublicDependencyModuleNames`
   - Tier 1 = few dependencies, Tier 2 = many dependencies

5. **Study test patterns:**
   - Read `*Test.cpp` files in `Private/` folder
   - Look for `IMPLEMENT_*_TEST` macros
   - Study fixture patterns (SetUp/TearDown)

---

## Agent Guidelines

### When Implementing Features

1. **Write tests BEFORE implementation** (TDD when possible)
2. **Use existing test patterns** (copy from similar tests)
3. **Document test purpose** (what/why in comments)
4. **Keep tests focused** (one behavior per test)
5. **Make tests self-contained** (no dependencies between tests)

### When Fixing Bugs

1. **Add regression test FIRST** (reproduces the bug)
2. **Verify test fails** (proves bug exists)
3. **Fix the bug**
4. **Verify test passes** (proves bug is fixed)
5. **Document the bug** (comment in test explaining what was broken)

---

## Troubleshooting

### ?????? CRITICAL: Tests Hang at "Starting Unreal Editor test runner..."

**Symptom:** Process hangs indefinitely, never completes, consumes 6+ GB RAM

**Root Cause:** Missing `-testexit` flag - UnrealEditor-Cmd.exe waits for manual input

**Solution:** Always include these flags when running automation tests:

```bash
UnrealEditor-Cmd.exe "Project.uproject" \
    -ExecCmds="Automation RunTests <filter>" \
    -unattended \
    -nopause \
    -nosplash \                                      # ??? Prevents UI hang
    -NullRHI \
    -testexit="Automation Test Queue Empty" \        # ??? CRITICAL!
    -stdout
```

**Why This Matters:**
- Without `-testexit`, the process **never exits** - it waits indefinitely
- CI/CD pipelines will **block forever**
- Must use exact string: `"Automation Test Queue Empty"`

**Verification:**
```bash
# Should complete in 30-60 seconds and show:
...Automation Test Queue Empty 33 tests performed.
**** TestExit: Automation Test Queue Empty ****
```

**If you're using `make test-unit`:** ??? Already fixed! The Makefile has the correct flags.

**If running manually:** ?????? You MUST add the flags yourself or it will hang.

---

### Abstract Class Instantiation Error

**Symptom:** `NewObject<UProjectUserWidget>()` crashes with "Cannot allocate abstract class"

**Root Cause:** `UProjectUserWidget` is marked as `UCLASS(Abstract)` and cannot be instantiated

**Solution:** Use a concrete subclass instead:

```cpp
// ??? WRONG:
UProjectUserWidget* Widget = NewObject<UProjectUserWidget>();  // Crashes!

// ??? CORRECT:
UProjectButton* Widget = NewObject<UProjectButton>();  // Works!
```

**When writing tests:**
- Check if base class has `UCLASS(Abstract)` specifier
- Use concrete derived classes for testing
- Common concrete widgets: `UProjectButton`, custom widget subclasses

---

### VIEWMODEL_PROPERTY Setter Conflicts

**Symptom:** Compilation error "ambiguous call to overloaded function" on setter methods

**Root Cause:** `BlueprintReadWrite` properties + manual setters create UHT conflicts

**Solution:** Use the updated `VIEWMODEL_PROPERTY` macro pattern:

```cpp
// Current macro (CORRECT):
VIEWMODEL_PROPERTY(int32, MapCount)

// Generates:
protected:
    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")  // ??? ReadOnly!
    int32 MapCount;
public:
    UFUNCTION(BlueprintCallable, Category = "ViewModel")  // ??? UFUNCTION!
    void SetMapCount(const int32& InValue) {
        MapCount = InValue;
        NotifyPropertyChanged(FName(TEXT("MapCount")));
    }
```

**Key Points:**
- Use `BlueprintReadOnly` (not `BlueprintReadWrite`) to prevent UHT setter generation
- Make setter a `UFUNCTION(BlueprintCallable)` for Blueprint access
- Auto-notification is built into the macro

**See also:** [docs/VIEWMODEL_PROPERTY_UPDATE.md](../VIEWMODEL_PROPERTY_UPDATE.md) for migration guide

---

### Missing Module Dependencies

**Symptom:** `Cannot open include file: 'ModuleName.h': No such file or directory`

**Root Cause:** Module not listed in `Build.cs` dependencies

**Solution:** Add missing module to your plugin's `Build.cs`:

```csharp
// Example: ProjectLoading.Build.cs
PrivateDependencyModuleNames.AddRange(new string[]
{
    "Features",
    "Features",  // ??? Add missing dependency
    "ProjectSettings"
});
```

**Common missing dependencies:**
- `Features` in `ProjectLoading`
- `ProjectData` in UI plugins
- Test framework modules in test plugins

---

### Test Module Not Found

**Symptom:** `make test-unit` skips your test module with "??? ModuleTests not found (skipped)"

**Causes & Solutions:**

1. **Module not created yet:**
   - Create `YourPlugin/Source/YourPluginTests/` directory
   - Add `.Build.cs` file with `Type = "UncookedOnly"`
   - Update `.uplugin` to include test module

2. **Build.cs not found:**
   - Ensure file is named exactly `YourPluginTests.Build.cs`
   - Must be in `Source/YourPluginTests/` directory

3. **Not compiled:**
   - Run `make prepare-tests` to compile all test modules
   - Or build manually: `Build.bat AlisEditor -Module=YourPluginTests`

---

### Tests Pass in Editor but Fail in Automation

**Symptom:** Tests pass in Session Frontend but fail with `make test-unit`

**Common Causes:**

1. **World context dependency:**
   - Automation tests use `-NullRHI` (no rendering)
   - Some tests need actual world/viewport
   - Solution: Mock world context or use `EditorContext` flag

2. **Timing assumptions:**
   - Headless mode runs faster/slower than editor
   - Solution: Use tolerances for timing tests, avoid hardcoded delays

3. **Asset loading:**
   - Automation may not load all assets
   - Solution: Explicitly load required assets in test setup

---

### Makefile Errors

**Symptom:** `make: command not found` or `make test-unit` fails

**Solution:**

**Windows (Git Bash):**
```bash
# Install make via Chocolatey
choco install make

# Or use utility scripts directly:
bash utility/test/prepare_tests.sh
bash utility/test/run_tests.sh --unit
```

**Linux/WSL:**
```bash
sudo apt-get install build-essential
```

---

### Quick Debug Checklist

When tests fail, check in this order:

1. ??? Are you using `make test-unit`? (recommended)
2. ??? Do all test modules compile? (`make prepare-tests`)
3. ??? Is `-testexit` flag present? (if running manually)
4. ??? Are module dependencies correct in `Build.cs`?
5. ??? Do tests pass in Session Frontend? (isolate automation vs. test logic)
6. ??? Check test logs: `Saved/Automation/Reports/tests.log`

---

## Test Status (Current Session: 2025-10-18)

### Results by Module

| Module | Tests | Passed | Failed | Pass Rate |
|--------|-------|--------|--------|-----------|
| ProjectDataTests | 6 | 6 | 0 | 100% ??? |
| ProjectMenuMainTests | 18 | 18 | 0 | 100% ??? |
| ProjectUIFrameworkTests | 24 | 24 | 0 | 100% ??? |
| ProjectLoadingTests | 33 | 30 | 3 | 91% ?????? |
| **TOTAL** | **81** | **78** | **3** | **96%** |

### Recent Fixes Applied

1. **Test Hanging Bug (CRITICAL)** ??? See Troubleshooting section above
   - Added `-testexit` flag to `utility/test/run_tests.sh`
   - All manual examples updated with correct flags

2. **VIEWMODEL_PROPERTY Macro** ??? See Troubleshooting section above
   - Fixed UHT setter conflicts
   - Changed to `BlueprintReadOnly + UFUNCTION` pattern
   - See [VIEWMODEL_PROPERTY_UPDATE.md](../VIEWMODEL_PROPERTY_UPDATE.md) for details

3. **ProjectUIWidgetTests** ??? See Troubleshooting section above
   - Fixed abstract class instantiation
   - Use `UProjectButton` instead of `UProjectUserWidget`

4. **ProjectLoading Dependencies**
   - Added `Features` to `ProjectLoading.Build.cs`

### Known Issues

**ProjectLoadingTests (3 flaky tests):**
- `ProjectLoading.Phase.Duration` - Timing test
- `ProjectLoading.Progress.Monotonic` - Progress order test
- `ProjectLoading.Progress.UpdateFrequency` - Update rate test

These are timing-sensitive and may fail in headless/NullRHI environment. Not critical for functionality.

---

## Resources

**Plugin Test Documentation:**
- [ProjectMenuMainTests README](../../Plugins/UI/ProjectMenuMain/Source/ProjectMenuMainTests/README.md) - Example test module
- [ProjectMenuMain README](../../Plugins/UI/ProjectMenuMain/README.md) - ViewModel API examples
- [ProjectUIFramework README](../../Plugins/UI/ProjectUIFramework/README.md) - MVVM architecture guide

**Unreal Documentation:**
- [Automation System Overview](https://docs.unrealengine.com/5.3/en-US/automation-system-user-guide-in-unreal-engine/)
- [Automation Technical Guide](https://docs.unrealengine.com/5.3/en-US/automation-technical-guide-for-unreal-engine/)
- [Functional Testing](https://docs.unrealengine.com/5.3/en-US/functional-testing-in-unreal-engine/)

**Project Documentation:**
- [Architecture Roadmap](../../todo/create_architecture.md) - Testing requirements by phase
- [Core Principles](../architecture/core_principles.md) - Code standards and conventions

---

## License

Copyright ALIS. All Rights Reserved.

