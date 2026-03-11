# ProjectLoadingTests

**Tier 1 Unit Tests for ProjectLoading Plugin**

## Overview

This test module provides comprehensive unit test coverage for the ProjectLoading plugin following the two-tier test architecture. Tests verify loading subsystem functionality, phase execution, retry logic, cancellation, progress tracking, and error handling.

## Test Organization

### Unit Tests (Tier 1: Plugin-Internal)

All tests are located in `Private/Unit/`:

#### 1. Subsystem Tests (`ProjectLoadingSubsystemTests.cpp`)
- **8 tests** verifying subsystem lifecycle and core API
- Subsystem initialization
- StartLoad request handling
- CancelActiveLoad (graceful vs force)
- IsLoadInProgress state tracking
- Telemetry tracking
- Event delegate signatures
- Load request validation
- Phase state tracking

#### 2. Phase Tests (`ProjectLoadPhaseTests.cpp`)
- **9 tests** verifying phase state management
- Phase state initialization
- State transitions (Pending -> InProgress -> Completed/Failed/Skipped)
- Progress tracking (0.0-1.0)
- Duration calculation
- All 6 phase types (ResolveAssets, MountContent, PreloadCriticalAssets, ActivateFeatures, Travel, Warmup)
- Phase state types
- Error tracking per phase
- Status messages
- Debug string generation (ToString)

#### 3. Retry Tests (`ProjectLoadRetryTests.cpp`)
- **7 tests** verifying retry logic and cancellation
- Retry counter (3 attempts max)
- Exponential backoff calculation
- Cancellation token behavior
- Graceful vs force cancellation modes
- Retry state machine transitions
- Cancellation checks between phases
- Error code range validation (100-999)

#### 4. Progress Tests (`ProjectLoadProgressTests.cpp`)
- **9 tests** verifying progress tracking and error handling
- Progress normalization (0.0-1.0 clamping)
- Overall progress calculation (6 phases)
- Partial phase progress
- Telemetry progress tracking
- Error message handling
- Loading state enum
- Progress update frequency (throttling)
- Monotonic progress increase
- Error context preservation

## Test Coverage

**Total: 33 unit tests**

- Subsystem: 8 tests
- Phase System: 9 tests
- Retry/Cancellation: 7 tests
- Progress/Error Handling: 9 tests

**Coverage Areas:**
- [OK] Subsystem lifecycle (initialization, shutdown, state tracking)
- [OK] Load request handling (validation, priority, transitions)
- [OK] 6 phase executors (all phase types)
- [OK] Phase state transitions (5 states: Pending, InProgress, Completed, Failed, Skipped)
- [OK] Retry logic (3 attempts, exponential backoff)
- [OK] Cancellation (graceful vs force, token management)
- [OK] Progress tracking (normalization, overall calculation, monotonic increase)
- [OK] Error handling (error codes 100-999, error messages, context)
- [OK] Telemetry (timing, progress snapshots)
- [OK] Event broadcasting (5 delegates: OnLoadStarted, OnPhaseChanged, OnProgress, OnCompleted, OnFailed)

**Target Coverage:** 80%+ of ProjectLoading plugin code

## Running Tests

### Via Makefile (Recommended)

```bash
# Run all unit tests (includes ProjectLoading)
make test-unit

# Run all tests
make test

# Run quick smoke tests
make test-quick
```

### Via Session Frontend (Editor)

1. Open Unreal Editor
2. Window -> Developer Tools -> Session Frontend
3. Go to Automation tab
4. Filter for `ProjectLoading`
5. Select tests to run
6. Click "Start Tests"

### Via Command Line

```bash
# Run all ProjectLoading tests
UnrealEditor-Cmd.exe Alis.uproject -ExecCmds="Automation RunTests ProjectLoading" -unattended -nopause -NullRHI -log

# Run specific test category
UnrealEditor-Cmd.exe Alis.uproject -ExecCmds="Automation RunTests ProjectLoading.Phase" -unattended -nopause -NullRHI -log
```

## Test Naming Convention

Tests follow hierarchical naming: `Plugin.Category.TestName`

**Examples:**
- `ProjectLoading.Subsystem.Initialization` - Subsystem init test
- `ProjectLoading.Phase.StateTransitions` - Phase state transition test
- `ProjectLoading.Retry.ExponentialBackoff` - Exponential backoff test
- `ProjectLoading.Progress.Overall` - Overall progress calculation test

## Dependencies

This is a Tier 1 test module - dependencies are minimal:

- `ProjectCore` - For types (FLoadRequest, FLoadPhaseState, ILoadingHandle)
- `ProjectLoading` - Module being tested

**No external plugin dependencies** - all external interactions are mocked.

## Integration Tests (Tier 2)

Cross-plugin integration tests will be added to `ProjectIntegrationTests` plugin:

- Integration: ProjectLoading <-> ProjectData (manifest resolution)
- Integration: ProjectLoading <-> ProjectSession (milestone recording)
- Functional: BootMap -> KazanMain workflow
- Smoke: Subsystem initializes in real GameInstance

See [docs/testing/index.md](../../../../docs/testing/index.md) for complete testing standards.

## Adding New Tests

1. Create test file in `Private/Unit/`
2. Include necessary headers (`ProjectLoadingTypes.h`, `Misc/AutomationTest.h`)
3. Wrap tests in `#if WITH_DEV_AUTOMATION_TESTS` guard
4. Use `IMPLEMENT_SIMPLE_AUTOMATION_TEST` macro
5. Follow naming convention: `ProjectLoading.Category.TestName`
6. Run `make test-unit` to verify tests compile and pass

**Example:**
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadMyFeatureTest,
    "ProjectLoading.Category.MyFeature",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadMyFeatureTest::RunTest(const FString& Parameters)
{
    // ARRANGE - Setup test data
    // ACT - Execute code being tested
    // ASSERT - Verify results
    TestTrue(TEXT("Description"), Condition);
    return true;
}
```

## Documentation

- **[docs/testing/index.md](../../../../docs/testing/index.md)** - Complete testing standards
- **[docs/loading_pipeline.md](../../../../docs/loading_pipeline.md)** - Loading system documentation
- **[todo/create_testing.md](../../../../todo/create_testing.md)** - Testing implementation plan
