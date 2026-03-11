# Unit Tests Guide

**Testing individual classes and functions in isolation**

This guide covers **Tier 1: Plugin-Internal Tests** - testing a single plugin's code in isolation using mocks.

See also:
- **[Overview](README.md)** - Testing philosophy and architecture
- **[Integration Tests](integration_tests.md)** - Cross-plugin testing
- **[Smoke Tests](smoke_tests.md)** - Quick health checks
- **[Automation](automation.md)** - Running tests and CI/CD

---

## Unit Test Characteristics

**Purpose:** Test individual classes/functions in isolation

**Characteristics:**
- Fast (milliseconds)
- No external dependencies (use mocks)
- Test single units of logic
- Deterministic (same input = same output)

**Examples:**
- ViewModel property change notifications
- Data structure validation
- Utility function behavior
- State machine transitions

**Where to Use:**
- ViewModels (pure C++ logic)
- Utility classes (config helpers, math)
- Data structures (validation logic)
- Service locator registration/resolution

---

## Writing Unit Tests

### Test File Structure

**Naming Convention:**
```
[ModuleName]Tests/Private/[Feature]/[ClassName]Tests.cpp
```

**Example:**
```
ProjectMenuMainTests/Private/ViewModels/ProjectMapListViewModelTests.cpp
```

### Test Declaration

Use `IMPLEMENT_SIMPLE_AUTOMATION_TEST` for unit tests:

```cpp
/**
 * [Test Description]
 *
 * Verifies: [What this test verifies]
 * Setup: [Required setup]
 * Expected: [Expected behavior]
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    F[ClassName][TestName]Test,
    "[ModuleName].[Category].[TestName]",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool F[ClassName][TestName]Test::RunTest(const FString& Parameters)
{
    // Test implementation
    return true;
}
```

**Example:**
```cpp
/**
 * MapListViewModel Initialization Test
 *
 * Verifies: ViewModel initializes with correct default values
 * Setup: Create ViewModel, call Initialize()
 * Expected: MapCount=0, SelectedMapIndex=-1, bIsRefreshing=false
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProjectMapListViewModelInitTest,
    "ProjectMenuMain.MapListViewModel.Initialization",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
```

### Test Structure (Arrange-Act-Assert)

```cpp
bool FMyTestTest::RunTest(const FString& Parameters)
{
    // ARRANGE - Setup test conditions
    UMyViewModel* ViewModel = NewObject<UMyViewModel>();
    ViewModel->Initialize(nullptr);

    // ACT - Perform action being tested
    bool bResult = ViewModel->DoSomething(InputValue);

    // ASSERT - Verify expected behavior
    TestTrue(TEXT("DoSomething should return true for valid input"), bResult);
    TestEqual(TEXT("Property should be updated"), ViewModel->GetProperty(), ExpectedValue);

    // CLEANUP - Shutdown/destroy test objects
    ViewModel->Shutdown();

    return true; // Test passed
}
```

### Assertions

**Equality:**
```cpp
TestEqual(TEXT("Description"), Actual, Expected);
TestNotEqual(TEXT("Description"), Actual, NotExpected);
```

**Boolean:**
```cpp
TestTrue(TEXT("Description"), bCondition);
TestFalse(TEXT("Description"), bCondition);
```

**Null Checks:**
```cpp
TestNull(TEXT("Description"), Pointer);
TestNotNull(TEXT("Description"), Pointer);
```

**Validity:**
```cpp
TestValid(TEXT("Description"), Object);
TestInvalid(TEXT("Description"), Object);
```

**Floating Point:**
```cpp
TestEqual(TEXT("Description"), ActualFloat, ExpectedFloat, 0.001f); // Tolerance
```

### Test Documentation

**File Header:**
```cpp
// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for [ClassName]
 *
 * [Description of what is being tested]
 *
 * To run tests:
 * 1. Open Session Frontend (Window -> Developer Tools -> Session Frontend)
 * 2. Go to Automation tab
 * 3. Filter for "[ModuleName].[Category]"
 * 4. Select tests and click "Start Tests"
 */
```

**Test Comments:**
```cpp
/**
 * Test: [Test Name]
 * Verifies [what behavior is being tested]
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(...)
```

---

## Unit Test Patterns

### Pattern 1: Testing Property Change Notifications

```cpp
bool FViewModelPropertyNotificationTest::RunTest(const FString& Parameters)
{
    UMyViewModel* ViewModel = NewObject<UMyViewModel>();
    ViewModel->Initialize(nullptr);

    // Track property changes
    bool bPropertyChangedCalled = false;
    FName ChangedPropertyName;

    ViewModel->OnPropertyChanged.AddLambda([&](FName PropertyName)
    {
        bPropertyChangedCalled = true;
        ChangedPropertyName = PropertyName;
    });

    // Trigger change
    ViewModel->SetProperty(NewValue);

    // Verify notification
    TestTrue(TEXT("OnPropertyChanged should be called"), bPropertyChangedCalled);
    TestEqual(TEXT("Correct property name"), ChangedPropertyName.ToString(), TEXT("Property"));

    ViewModel->Shutdown();
    return true;
}
```

### Pattern 2: Testing State Machines

```cpp
bool FMenuNavigationTest::RunTest(const FString& Parameters)
{
    UProjectMenuViewModel* ViewModel = NewObject<UProjectMenuViewModel>();
    ViewModel->Initialize(nullptr);

    // Verify initial state
    TestEqual(TEXT("Should start at Main"), ViewModel->GetCurrentScreen(), EMenuScreen::Main);
    TestFalse(TEXT("Back button hidden"), ViewModel->CanNavigateBack());

    // Navigate forward
    ViewModel->NavigateToMapBrowser();
    TestEqual(TEXT("Should be at MapBrowser"), ViewModel->GetCurrentScreen(), EMenuScreen::MapBrowser);
    TestTrue(TEXT("Back button visible"), ViewModel->CanNavigateBack());

    // Navigate back
    ViewModel->NavigateBack();
    TestEqual(TEXT("Should return to Main"), ViewModel->GetCurrentScreen(), EMenuScreen::Main);
    TestFalse(TEXT("Back button hidden again"), ViewModel->CanNavigateBack());

    ViewModel->Shutdown();
    return true;
}
```

### Pattern 3: Testing Value Clamping

```cpp
bool FSettingsVolumeClampingTest::RunTest(const FString& Parameters)
{
    UProjectSettingsViewModel* ViewModel = NewObject<UProjectSettingsViewModel>();
    ViewModel->Initialize(nullptr);

    // Test upper bound
    ViewModel->SetMasterVolume(1.5f);
    TestEqual(TEXT("Volume should clamp to 1.0"), ViewModel->GetMasterVolume(), 1.0f);

    // Test lower bound
    ViewModel->SetMasterVolume(-0.5f);
    TestEqual(TEXT("Volume should clamp to 0.0"), ViewModel->GetMasterVolume(), 0.0f);

    // Test valid range
    ViewModel->SetMasterVolume(0.7f);
    TestEqual(TEXT("Valid volume should not clamp"), ViewModel->GetMasterVolume(), 0.7f);

    ViewModel->Shutdown();
    return true;
}
```

### Pattern 4: Testing with Mocks

```cpp
// Mock subsystem for testing
class UMockProjectLoadingSubsystem : public UProjectLoadingSubsystem
{
public:
    void SimulateLoadStarted(FLoadRequest Request)
    {
        OnLoadStarted.Broadcast(Request);
    }

    void SimulateProgress(float Progress, FText Status)
    {
        OnProgress.Broadcast(Progress, Status);
    }
};

bool FLoadingViewModelEventsTest::RunTest(const FString& Parameters)
{
    // Create mock subsystem
    UMockProjectLoadingSubsystem* MockSubsystem = NewObject<UMockProjectLoadingSubsystem>();

    // Create ViewModel with mock
    UProjectLoadingViewModel* ViewModel = NewObject<UProjectLoadingViewModel>();
    ViewModel->Initialize(MockSubsystem);

    // Simulate event
    MockSubsystem->SimulateLoadStarted(FLoadRequest());

    // Verify ViewModel reacted
    TestTrue(TEXT("ViewModel should be loading"), ViewModel->IsLoading());

    ViewModel->Shutdown();
    return true;
}
```

---

## ViewModel Testing Examples

### Testing Initialization

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProjectMapListViewModelInitTest,
    "ProjectMenuMain.Unit.MapListViewModel.Initialization",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FProjectMapListViewModelInitTest::RunTest(const FString& Parameters)
{
    // ARRANGE & ACT
    UProjectMapListViewModel* ViewModel = NewObject<UProjectMapListViewModel>();
    ViewModel->Initialize(nullptr);

    // ASSERT - Verify default state
    TestEqual(TEXT("MapCount should be 0"), ViewModel->GetMapCount(), 0);
    TestEqual(TEXT("SelectedMapIndex should be -1"), ViewModel->GetSelectedMapIndex(), -1);
    TestFalse(TEXT("Should not be refreshing"), ViewModel->IsRefreshing());

    // CLEANUP
    ViewModel->Shutdown();
    return true;
}
```

### Testing Search Filtering

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMapListViewModelSearchTest,
    "ProjectMenuMain.Unit.MapListViewModel.Search",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FMapListViewModelSearchTest::RunTest(const FString& Parameters)
{
    // ARRANGE - Create ViewModel with hardcoded test data
    UProjectMapListViewModel* ViewModel = NewObject<UProjectMapListViewModel>();
    ViewModel->Initialize();

    // Manually populate with test entries (bypass real Asset Manager)
    TArray<FProjectMapEntry> TestEntries;
    TestEntries.Add({TEXT("KazanMain"), TEXT("Main world"), nullptr});
    TestEntries.Add({TEXT("Hrush"), TEXT("Forest area"), nullptr});
    ViewModel->SetTestEntries(TestEntries); // Mock method for testing

    // ACT - Apply search filter
    ViewModel->SetSearchFilter(TEXT("Kazan"));

    // ASSERT - Verify filtered results
    TestEqual("Should have 1 result", ViewModel->GetFilteredEntryCount(), 1);
    TestEqual("Should be KazanMain", ViewModel->GetFilteredEntry(0).DisplayName, TEXT("KazanMain"));

    ViewModel->Shutdown();
    return true;
}
```

### Testing Property Change Notifications

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMapListViewModelPropertyNotificationTest,
    "ProjectMenuMain.Unit.MapListViewModel.PropertyNotification",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FMapListViewModelPropertyNotificationTest::RunTest(const FString& Parameters)
{
    // ARRANGE
    UProjectMapListViewModel* ViewModel = NewObject<UProjectMapListViewModel>();
    ViewModel->Initialize();

    bool bPropertyChanged = false;
    FName ChangedProperty;

    ViewModel->OnPropertyChanged.AddLambda([&](FName PropertyName)
    {
        bPropertyChanged = true;
        ChangedProperty = PropertyName;
    });

    // ACT - Trigger property change
    ViewModel->SetSelectedMapIndex(5);

    // ASSERT
    TestTrue(TEXT("OnPropertyChanged should fire"), bPropertyChanged);
    TestEqual(TEXT("Property name should be SelectedMapIndex"),
        ChangedProperty.ToString(), TEXT("SelectedMapIndex"));

    ViewModel->Shutdown();
    return true;
}
```

---

## Utility Class Testing Examples

### Testing Config Helpers

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProjectConfigHelpersTest,
    "ProjectCore.Unit.ConfigHelpers.GetString",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FProjectConfigHelpersTest::RunTest(const FString& Parameters)
{
    // ARRANGE
    const FString SectionName = TEXT("TestSection");
    const FString KeyName = TEXT("TestKey");
    const FString DefaultValue = TEXT("Default");

    // ACT
    FString Value = FProjectConfigHelpers::GetString(SectionName, KeyName, DefaultValue);

    // ASSERT
    TestFalse(TEXT("Value should not be empty"), Value.IsEmpty());

    return true;
}
```

### Testing Data Validation

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProjectManifestValidationTest,
    "ProjectData.Unit.Manifest.Validation",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FProjectManifestValidationTest::RunTest(const FString& Parameters)
{
    // ARRANGE - Create manifest with invalid data
    UProjectWorldManifest* Manifest = NewObject<UProjectWorldManifest>();
    Manifest->WorldPath = FSoftObjectPath(); // Invalid path

    // ACT
    TArray<FString> Errors;
    bool bIsValid = Manifest->Validate(Errors);

    // ASSERT
    TestFalse(TEXT("Invalid manifest should fail validation"), bIsValid);
    TestTrue(TEXT("Should have validation errors"), Errors.Num() > 0);

    return true;
}
```

---

## Module Structure for Unit Tests

### Example: ProjectMenuMainTests

```
Plugins/UI/ProjectMenuMain/
+-- Source/
    +-- ProjectMenuMain/           # Runtime module
    +-- ProjectMenuMainTests/      # Test module
        +-- Public/
        |   +-- Mocks/
        |       +-- MockLoadingSubsystem.h
        +-- Private/
        |   +-- Unit/
        |       +-- MapListViewModelTests.cpp
        |       +-- MenuViewModelTests.cpp
        |       +-- SettingsViewModelTests.cpp
        |       +-- LoadingViewModelTests.cpp
        +-- ProjectMenuMainTests.Build.cs
```

### Build.cs for Unit Tests

```cpp
// ProjectMenuMainTests.Build.cs
public class ProjectMenuMainTests : ModuleRules
{
    public ProjectMenuMainTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "ProjectMenuMain",    // Module being tested - ONLY dependency
            // NO other plugins - use mocks for external dependencies!
        });
    }
}
```

### .uplugin Configuration

```json
{
  "Modules": [
    {
      "Name": "ProjectMenuMain",
      "Type": "Runtime",
      "LoadingPhase": "Default"
    },
    {
      "Name": "ProjectMenuMainTests",
      "Type": "UncookedOnly",
      "LoadingPhase": "Default"
    }
  ]
}
```

---

## Unreal Engine Unit Test Examples

### HTNPlanner Plugin - Best Practices

**Path:** `<ue-path>/Engine/Plugins/AI/HTNPlanner/`

**Test Module:** `HTNTestSuite` (UncookedOnly)
**Test File:** `Source/HTNTestSuite/Private/HTNTest.cpp`
**Pattern:** Instant tests with FAITestBase inheritance
**Test Count:** 20+ unit tests
**Dependencies:** HTNPlanner module only (isolated)

**What They Test:**
- Domain builder basics (adding tasks, conditions, effects)
- Planning algorithm correctness (goal decomposition)
- World state validation (preconditions, postconditions)
- Condition evaluation logic (state matching)
- HTN compiler (domain validation)

**Key Techniques:**
- Fast unit tests with no world context
- All external dependencies mocked
- Excellent test isolation
- Uses `IMPLEMENT_AI_INSTANT_TEST` macro
- Hierarchical test naming: `System.AI.HTN.DomainBuilderBasics`

**Build.cs Pattern:**
```cpp
// HTNTestSuite.Build.cs
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "AIModule",
    "AITestSuite",      // Test framework
    "HTNPlanner",       // Module being tested - ONLY dependency
});
```

**Example Test:**
```cpp
// From HTNTest.cpp
IMPLEMENT_AI_INSTANT_TEST(FHTNTest_DomainBuilderBasics,
    "System.AI.HTN.DomainBuilderBasics")
{
    FHTNBuilder_Domain DomainBuilder;

    // Test: Domain starts empty
    AITEST_EQUAL("Initial domain should have 0 tasks",
        DomainBuilder.GetTasks().Num(), 0);

    // Test: Add task
    DomainBuilder.AddTask(TEXT("MoveTo"));
    AITEST_EQUAL("Domain should have 1 task",
        DomainBuilder.GetTasks().Num(), 1);

    return true;
}
```

---

## Agent Guidelines for Unit Tests

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

### Test-Driven Development (TDD)

**Red-Green-Refactor Cycle:**

1. **RED** - Write failing test
   ```cpp
   // Test for feature that doesn't exist yet
   TestEqual(TEXT("New feature should work"), ViewModel->NewFeature(), ExpectedValue);
   // Fails: NewFeature() doesn't exist
   ```

2. **GREEN** - Write minimal code to pass
   ```cpp
   int32 UViewModel::NewFeature()
   {
       return ExpectedValue; // Simplest implementation
   }
   ```

3. **REFACTOR** - Improve code quality
   ```cpp
   int32 UViewModel::NewFeature()
   {
       // Proper implementation with validation, error handling, etc.
       if (!IsValid()) return -1;
       return CalculateValue();
   }
   ```

### Code Comments for Tests

**Good:**
```cpp
/**
 * Test: MapListViewModel Selection
 * Verifies that selecting a map updates SelectedMapIndex and broadcasts OnPropertyChanged.
 * This is critical for reactive UI updates.
 */
```

**Bad:**
```cpp
// Test selection
```

**Good:**
```cpp
// Set invalid index (should fail and not change state)
bool bResult = ViewModel->SetSelectedMapIndex(999);
TestFalse(TEXT("Invalid index should return false"), bResult);
```

**Bad:**
```cpp
ViewModel->SetSelectedMapIndex(999);
TestFalse(TEXT("Test"), bResult);
```

---

## Resources

**See Also:**
- **[Integration Tests](integration_tests.md)** - Testing cross-plugin interactions
- **[Smoke Tests](smoke_tests.md)** - Quick health checks
- **[Automation](automation.md)** - Running tests and troubleshooting

**Plugin Documentation:**
- [ProjectMenuMainTests README](../../Plugins/UI/ProjectMenuMain/Source/ProjectMenuMainTests/README.md)
- [ProjectUIFramework README](../../Plugins/UI/ProjectUIFramework/README.md)

---

## License

Copyright ALIS. All Rights Reserved.
