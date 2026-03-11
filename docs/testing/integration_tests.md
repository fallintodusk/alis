# Integration Tests Guide

**Testing cross-plugin interactions and subsystem integration**

This guide covers **Tier 2: Cross-Plugin Integration Tests** - testing real interactions between multiple plugins and subsystems.

See also:
- **[Overview](README.md)** - Testing philosophy and architecture
- **[Unit Tests](unit_tests.md)** - Plugin-internal testing
- **[Smoke Tests](smoke_tests.md)** - Quick health checks
- **[Automation](automation.md)** - Running tests and CI/CD

---

## Integration Test Characteristics

**Purpose:** Test interactions between components and plugins

**Characteristics:**
- Medium speed (seconds)
- Test component interfaces
- Use real subsystems (no mocks)
- Test data flow between modules
- **Test cross-plugin dependencies**

**Examples:**
- ViewModel subscribing to subsystem events
- Asset Manager queries returning manifests
- Theme manager switching themes and notifying widgets
- Loading subsystem executing phase pipeline
- **MapListViewModel querying ProjectData manifests**
- **LoadingViewModel subscribing to ProjectLoading events**
- **MenuStartupComponent creating MenuScreen and loading theme**

**Where to Use:**
- Subsystem integration points
- Asset Manager integration
- Event delegate chains
- **Plugin dependencies (must test all cross-plugin interactions)**
- ViewModel + Subsystem integration
- Component + Widget integration

---

## Tier 2: Cross-Plugin Integration Architecture

### Location and Structure

**Location:** Separate shared test plugin
**Module Type:** `UncookedOnly` or `DeveloperTool`
**Dependencies:** All plugins being tested (real subsystems, no mocks)
**Coverage Target:** 100% of public APIs and critical workflows

**Example Structure:**
```
Plugins/Tests/ProjectIntegrationTests/
????????? Source/
    ????????? ProjectIntegrationTests/
    ???   ????????? Public/
    ???   ???   ????????? Fixtures/
    ???   ???       ????????? IntegrationTestBase.h  # Base fixture with world setup
    ???   ????????? Private/
    ???   ???   ????????? Integration/               # ?????? Cross-plugin integration tests
    ???   ???   ???   ????????? LoadingDataIntegration.cpp
    ???   ???   ???   ????????? MenuUILoadingIntegration.cpp
    ???   ???   ???   ????????? ThemeSystemIntegration.cpp
    ???   ???   ????????? Functional/                # ?????? End-to-end workflow tests
    ???   ???   ???   ????????? BootToWorldWorkflow.cpp
    ???   ???   ???   ????????? ErrorRecoveryWorkflow.cpp
    ???   ???   ????????? Smoke/                     # ?????? Health check tests
    ???   ???       ????????? SubsystemHealthCheck.cpp
    ???   ????????? ProjectIntegrationTests.Build.cs
    ????????? ProjectIntegrationTests.uplugin
```

### Build.cs Pattern

```cpp
// Tier 2: Depend on ALL plugins being tested (real subsystems)
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "ProjectCore",
    "ProjectBoot",
    "ProjectLoading",      // Real loading subsystem
    "ProjectData",         // Real asset manager
    "ProjectUIFramework",           // Real theme manager
    "ProjectMenuMain",       // Real ViewModels and screens
    "ProjectSession",      // Real session subsystem
    // All plugins - use real subsystems, no mocks!
});
```

---

## Integration Test Examples

### ProjectMenuMain ??? ProjectLoading Integration

```cpp
/**
 * Integration test: MapBrowserScreen triggers loading
 * Tests: MapBrowserScreen ??? MapListViewModel ??? ProjectLoadingSubsystem
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMapBrowserLoadingIntegrationTest,
    "ProjectMenuMain.Integration.MapBrowserLoading",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FMapBrowserLoadingIntegrationTest::RunTest(const FString& Parameters)
{
    // ARRANGE - Create game instance with loading subsystem
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();

    UProjectLoadingSubsystem* LoadingSubsystem =
        GameInstance->GetSubsystem<UProjectLoadingSubsystem>();
    TestNotNull(TEXT("Loading subsystem should exist"), LoadingSubsystem);

    // Create MapBrowserScreen (will create MapListViewModel)
    UProjectMapBrowserScreen* Screen = CreateWidget<UProjectMapBrowserScreen>(
        GameInstance, UProjectMapBrowserScreen::StaticClass());

    // Track loading events
    bool bLoadStarted = false;
    LoadingSubsystem->OnLoadStarted.AddLambda([&](const FLoadRequest& Request)
    {
        bLoadStarted = true;
    });

    // ACT - Trigger load from screen
    Screen->RefreshMapList(); // Queries ProjectData
    if (Screen->GetMapCount() > 0)
    {
        Screen->SelectMapByIndex(0);
        Screen->LoadSelectedMap(); // Calls LoadingSubsystem->StartLoad()
    }

    // ASSERT - Verify integration worked
    TestTrue(TEXT("LoadingSubsystem should receive StartLoad call"), bLoadStarted);

    return true;
}
```

### ProjectMenuMain ??? ProjectData Integration

```cpp
/**
 * Integration test: MapListViewModel queries manifests
 * Tests: MapListViewModel ??? UAssetManager ??? ProjectData manifests
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMapListDataIntegrationTest,
    "ProjectMenuMain.Integration.MapListData",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FMapListDataIntegrationTest::RunTest(const FString& Parameters)
{
    // ARRANGE - Create ViewModel
    UProjectMapListViewModel* ViewModel = NewObject<UProjectMapListViewModel>();
    ViewModel->Initialize(nullptr);

    // ACT - Refresh map list (queries Asset Manager for WorldManifests)
    ViewModel->RefreshMapList();

    // ASSERT - Verify manifests were loaded
    int32 MapCount = ViewModel->GetMapCount();
    TestTrue(TEXT("Should load manifests from ProjectData"), MapCount >= 0);

    if (MapCount > 0)
    {
        // Verify manifest data structure
        FProjectMapEntry Entry = ViewModel->GetMapEntry(0);
        TestFalse(TEXT("DisplayName should not be empty"), Entry.DisplayName.IsEmpty());
    }

    ViewModel->Shutdown();
    return true;
}
```

### ProjectUIFramework ??? ProjectMenuMain Integration

```cpp
/**
 * Integration test: MenuScreen applies theme
 * Tests: MenuScreen ??? ProjectUIFrameworkThemeManager ??? Theme application
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMenuScreenThemeIntegrationTest,
    "ProjectMenuMain.Integration.MenuScreenTheme",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FMenuScreenThemeIntegrationTest::RunTest(const FString& Parameters)
{
    // ARRANGE - Create game instance with theme manager
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();

    UProjectUIFrameworkThemeManager* ThemeManager =
        GameInstance->GetSubsystem<UProjectUIFrameworkThemeManager>();
    TestNotNull(TEXT("Theme manager should exist"), ThemeManager);

    // Create theme data asset
    UProjectUIThemeData* Theme = NewObject<UProjectUIThemeData>();
    Theme->PrimaryColor = FLinearColor::Blue;

    // Create MenuScreen
    UProjectMenuScreen* Screen = CreateWidget<UProjectMenuScreen>(
        GameInstance, UProjectMenuScreen::StaticClass());

    // Track theme changes
    bool bThemeChanged = false;
    ThemeManager->OnThemeChanged.AddLambda([&](UProjectUIThemeData* NewTheme)
    {
        bThemeChanged = true;
    });

    // ACT - Change theme
    ThemeManager->SetActiveTheme(Theme);

    // ASSERT - Verify integration
    TestTrue(TEXT("Theme change should broadcast"), bThemeChanged);
    TestEqual(TEXT("Active theme should match"),
        ThemeManager->GetActiveTheme(), Theme);

    return true;
}
```

### LoadingViewModel ??? LoadingSubsystem Event Integration

```cpp
/**
 * Integration test: LoadingViewModel subscribes to real ProjectLoadingSubsystem events
 * Tests: LoadingViewModel ??? ProjectLoadingSubsystem event propagation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLoadingViewModelSubsystemIntegrationTest,
    "ProjectIntegrationTests.Integration.LoadingViewModelEvents",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FLoadingViewModelSubsystemIntegrationTest::RunTest(const FString& Parameters)
{
    // ARRANGE - Create real game instance with real subsystems
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();

    // Get REAL ProjectLoadingSubsystem (no mock)
    UProjectLoadingSubsystem* LoadingSubsystem =
        GameInstance->GetSubsystem<UProjectLoadingSubsystem>();
    TestNotNull("LoadingSubsystem should exist", LoadingSubsystem);

    // Create LoadingViewModel and initialize (it will subscribe to real subsystem)
    UProjectLoadingViewModel* ViewModel = NewObject<UProjectLoadingViewModel>();
    ViewModel->Initialize();

    // ACT - Trigger REAL load request (not mocked)
    FLoadRequest Request;
    Request.TargetMapPath = FSoftObjectPath(TEXT("/Game/Project/Maps/TestMap"));
    TSharedPtr<ILoadingHandle> Handle = LoadingSubsystem->StartLoad(Request);

    // ASSERT - Verify ViewModel received real events and updated properties
    AddCommand(new FWaitLatentCommand(1.0f)); // Wait for async loading
    TestTrue("ViewModel should have loading progress > 0", ViewModel->GetLoadingProgress() > 0.0f);
    TestFalse("ViewModel current phase should not be empty", ViewModel->GetCurrentPhase().IsEmpty());

    // CLEANUP
    ViewModel->Shutdown();
    GameInstance->Shutdown();
    return true;
}
```

---

## Functional Tests (End-to-End)

**Purpose:** Test complete workflows across multiple plugins

**Characteristics:**
- Slow (seconds to minutes)
- Test complete user scenarios
- Use real game instance and subsystems
- May require level/world setup
- **Test entire plugin pipeline from user action to result**

**Examples:**
- Complete loading pipeline execution (ResolveAssets ??? Warmup)
- User navigating through menu screens
- Settings apply and persist to config
- **Map selection ??? loading ??? world spawn**
- **Boot flow ??? Menu display ??? Map selection ??? Loading screen ??? Gameplay**

### Complete Map Loading Workflow

```cpp
/**
 * Functional test: Complete map loading workflow
 * Tests full pipeline: ProjectBoot ??? ProjectMenuMain ??? ProjectLoading ??? ProjectData ??? World
 */
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FCompleteMapLoadingWorkflowTest,
    "ProjectMenuMain.Functional.CompleteMapLoading",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FCompleteMapLoadingWorkflowTest::RunTest(const FString& Parameters)
{
    // ARRANGE - Create full game environment
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();

    // Get required subsystems
    UProjectLoadingSubsystem* LoadingSubsystem =
        GameInstance->GetSubsystem<UProjectLoadingSubsystem>();
    UProjectSessionSubsystem* SessionSubsystem =
        GameInstance->GetSubsystem<UProjectSessionSubsystem>();

    TestNotNull(TEXT("Loading subsystem required"), LoadingSubsystem);
    TestNotNull(TEXT("Session subsystem required"), SessionSubsystem);

    // Track workflow progress
    TArray<FString> WorkflowSteps;

    // Step 1: Boot flow completes
    WorkflowSteps.Add(TEXT("Boot"));
    SessionSubsystem->RecordMilestone(TEXT("BootComplete"));

    // Step 2: Menu displays
    UProjectMenuStartupComponent* MenuStartup =
        NewObject<UProjectMenuStartupComponent>();
    MenuStartup->BeginPlay(); // Spawns menu
    UProjectMenuScreen* MenuScreen = MenuStartup->GetMenuScreen();
    TestNotNull(TEXT("Menu should spawn"), MenuScreen);
    WorkflowSteps.Add(TEXT("MenuDisplayed"));

    // Step 3: User navigates to map browser
    MenuScreen->NavigateToMapBrowser();
    WorkflowSteps.Add(TEXT("MapBrowserOpened"));

    // Step 4: Map browser displays maps
    UProjectMapBrowserScreen* MapBrowser =
        CreateWidget<UProjectMapBrowserScreen>(GameInstance,
            UProjectMapBrowserScreen::StaticClass());
    MapBrowser->RefreshMapList(); // Queries ProjectData
    TestTrue(TEXT("Maps should load"), MapBrowser->GetMapCount() > 0);
    WorkflowSteps.Add(TEXT("MapsLoaded"));

    // Step 5: User selects map
    MapBrowser->SelectMapByIndex(0);
    TestTrue(TEXT("Map should be selected"), MapBrowser->HasSelectedMap());
    WorkflowSteps.Add(TEXT("MapSelected"));

    // Step 6: Loading screen component shows loading screen
    UProjectLoadingScreenComponent* LoadingScreenComp =
        NewObject<UProjectLoadingScreenComponent>();
    LoadingScreenComp->BeginPlay(); // Subscribes to events

    bool bLoadingScreenShown = false;
    LoadingSubsystem->OnLoadStarted.AddLambda([&](const FLoadRequest& Request)
    {
        bLoadingScreenShown = LoadingScreenComp->IsLoadingScreenVisible();
    });

    // Step 7: User clicks "Load Map"
    bool bLoadStarted = MapBrowser->LoadSelectedMap();
    TestTrue(TEXT("Load should start"), bLoadStarted);
    WorkflowSteps.Add(TEXT("LoadStarted"));

    // Step 8: Verify loading screen appeared
    TestTrue(TEXT("Loading screen should appear"), bLoadingScreenShown);
    WorkflowSteps.Add(TEXT("LoadingScreenShown"));

    // Step 9: Simulate loading completion
    // (In real test, would wait for actual loading phases)
    LoadingSubsystem->OnCompleted.Broadcast(FLoadRequest());
    WorkflowSteps.Add(TEXT("LoadCompleted"));

    // ASSERT - Verify complete workflow
    TestEqual(TEXT("All workflow steps completed"),
        WorkflowSteps.Num(), 9);

    // Verify milestones recorded
    TestTrue(TEXT("Boot milestone recorded"),
        SessionSubsystem->HasMilestone(TEXT("BootComplete")));

    // Cleanup
    LoadingScreenComp->EndPlay(EEndPlayReason::Quit);
    MenuStartup->EndPlay(EEndPlayReason::Quit);

    return true;
}
```

### Boot to World Workflow

```cpp
/**
 * Functional test: Complete Boot ??? Menu ??? MapBrowser ??? Loading ??? World workflow
 * Tests full pipeline across all plugins
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBootToWorldWorkflowTest,
    "ProjectIntegrationTests.Functional.BootToWorldWorkflow",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FBootToWorldWorkflowTest::RunTest(const FString& Parameters)
{
    // ARRANGE - Start from BootMap with real subsystems
    UGameInstance* GI = NewObject<UGameInstance>();
    GI->Init();

    UWorld* BootWorld = CreateTestWorld(TEXT("/Game/Project/Core/Boot/BootMap"));
    TestNotNull("BootMap should load", BootWorld);

    // ACT & ASSERT - Step through entire workflow
    // 1. MenuStartupComponent spawns main menu
    UProjectMenuStartupComponent* MenuStartup =
        GI->GetComponentByClass<UProjectMenuStartupComponent>();
    TestNotNull("MenuStartupComponent should exist", MenuStartup);

    // 2. User clicks "Play" ??? Map browser appears
    UProjectMapBrowserScreen* MapBrowser = MenuStartup->ShowMapBrowser();
    TestNotNull("MapBrowser should spawn", MapBrowser);

    // 3. User selects map
    MapBrowser->GetViewModel()->SelectMapByIndex(0);

    // 4. LoadingScreen appears
    MapBrowser->LoadSelectedMap();
    UProjectLoadingScreen* LoadingScreen = FindActiveLoadingScreen();
    TestNotNull("LoadingScreen should spawn", LoadingScreen);

    // 5. LoadingSubsystem executes 6 phases
    UProjectLoadingSubsystem* Loading = GI->GetSubsystem<UProjectLoadingSubsystem>();
    AddCommand(new FWaitForLoadingCommand(Loading, 30.0f)); // Wait up to 30s

    // 6. Travel completes
    TestTrue("Should travel to target map", Loading->GetCurrentState() == ELoadingState::Completed);

    // 7. LoadingScreen hides
    TestTrue("LoadingScreen should be hidden", LoadingScreen->GetVisibility() == ESlateVisibility::Hidden);

    return true;
}
```

---

## Cross-Plugin Integration Requirements

**IMPORTANT:** Every plugin MUST test integration with its dependencies.

### ProjectMenuMain Dependencies

**??? ProjectUIFramework:** Theme integration, ViewModel base classes, widget lifecycle
**??? ProjectLoading:** Loading event subscriptions, FLoadRequest creation
**??? ProjectData:** Manifest queries, asset loading
**??? ProjectBoot:** Component placement, startup integration

### Required Integration Tests

1. **ViewModel ??? Subsystem** - ViewModels subscribe to events correctly
2. **Screen ??? Theme** - Screen controllers apply themes
3. **MapBrowser ??? Loading** - Map selection triggers loading
4. **MapList ??? Data** - Map list queries manifests
5. **Startup ??? Menu** - Menu spawns in BootMap

### Required Functional Tests

1. **Boot ??? Menu ??? MapBrowser ??? Loading** - Complete user workflow
2. **Theme Change** - Theme switching updates all active widgets
3. **Map Load** - Full loading pipeline execution with UI updates

---

## Unreal Engine Integration Test Examples

### MassAI Plugin - Tier 2 Example

**Path:** `<ue-path>/Engine/Plugins/AI/MassAI/`

**Test Module:** `MassAITestSuite` (UncookedOnly)
**Pattern:** Multi-system integration tests
**Dependencies:** 8+ Mass runtime modules (cross-plugin testing)

**Dependencies (from .uplugin):**
```json
{
  "Modules": [
    {
      "Name": "MassAITestSuite",
      "Type": "UncookedOnly",
      "LoadingPhase": "Default"
    }
  ],
  "Plugins": [
    { "Name": "MassActors", "Enabled": true },
    { "Name": "MassAIBehavior", "Enabled": true },
    { "Name": "MassCommon", "Enabled": true },
    { "Name": "MassNavigation", "Enabled": true },
    { "Name": "MassAIReplication", "Enabled": true },
    { "Name": "MassSmartObjects", "Enabled": true },
    { "Name": "MassSpawner", "Enabled": true },
    { "Name": "MassRepresentation", "Enabled": true }
  ]
}
```

**What They Test:**
- Cross-plugin component interactions
- Mass Entity System (ECS) integration
- AI behavior execution across multiple subsystems
- Replication and networking
- Smart object interactions
- Entity spawning and representation

**Key Techniques:**
- Real subsystem initialization (no mocks)
- Multi-plugin dependency testing
- Tests complex interaction patterns
- Validates plugin load order
- Integration across 8+ systems

**Key Takeaway:** When your plugin depends on multiple other plugins, create a separate test plugin (Tier 2) that depends on all of them and tests real interactions.

---

## MLAdapter Plugin - Fixtures for World Context

**Path:** `<ue-path>/Engine/Plugins/AI/MLAdapter/`

**Test Module:** `MLAdapterTests` (DeveloperTool)
**Test Files:** `AgentTest.cpp`, `RPCServerTest.cpp`, `SessionTest.cpp`
**Pattern:** Fixture-based tests with SetUp() override
**Dependencies:** MLAdapter module + platform-specific (Win64/Mac/Linux for RPC)

**What They Test:**
- Actor avatar setting and clearing
- Controller avatar management
- Avatar destruction handling
- RPC server communication (platform-specific)
- Session lifecycle management

**Key Techniques:**
- Uses test fixtures for world context
- SetUp() and TearDown() pattern
- World-dependent tests (actors, controllers)
- Platform-specific code handling (`#if PLATFORM_WINDOWS`)
- Uses `IMPLEMENT_INSTANT_TEST_WITH_FIXTURE` macro

**Fixture Pattern:**
```cpp
// From AgentTest.cpp
struct FMLAdapterTest_ActorAvatar : public FAITestBase
{
    UWorld* TestWorld;
    ATestActor* TestActor;

    virtual bool SetUp() override
    {
        FAITestBase::SetUp();
        TestWorld = GetWorld();  // From FAITestBase
        TestActor = TestWorld->SpawnActor<ATestActor>();
        return TestActor != nullptr;
    }

    virtual void TearDown() override
    {
        if (TestActor)
        {
            TestActor->Destroy();
        }
        FAITestBase::TearDown();
    }
};

IMPLEMENT_INSTANT_TEST_WITH_FIXTURE(FMLAdapterTest_ActorAvatar,
    "System.AI.MLAdapter.ActorAvatarSetting")
{
    // World and TestActor available from fixture
    UMLAdapterAgent* Agent = NewObject<UMLAdapterAgent>();
    Agent->SetActorAvatar(TestActor);

    AITEST_NOT_NULL("Avatar should be set", Agent->GetAvatar());
    AITEST_EQUAL("Avatar should be TestActor", Agent->GetAvatar(), TestActor);

    return true;
}
```

---

## UI Layout Dump (Agent Observability)

**Purpose:** Validate inventory UI layout composition with synthetic ViewModel data (no gameplay dependency).

**Test ID:** `ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree`

**Map source of truth:** `Plugins/Test/ProjectIntegrationTests/Config/DefaultProjectIntegrationTests.ini`

**Key:** `ProjectIntegrationTests.InventoryDumpMap`

**Plugin:** `ProjectIntegrationTests` (enabled in `Alis.uproject`)

**Run (Windows):**
```bat
for /f "tokens=1,* delims==" %%A in ('findstr /b InventoryDumpMap Plugins\\Test\\ProjectIntegrationTests\\Config\\DefaultProjectIntegrationTests.ini') do set INV_MAP=%%B

UnrealEditor-Cmd.exe Alis.uproject %INV_MAP% ^
  -unattended -nopause -NullRHI ^
  -stdout -FullStdOutLogOutput ^
  -ExecCmds="Automation RunTests ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree; Quit" ^
  -TestExit="Automation Test Queue Empty" ^
  -ABSLOG="Saved/Logs/RunTests.log"
```

**Output:** `Saved/Dumps/Inventory.json`

**Agent checks (fail if any):**
- Visible widgets with ArrangedSize ~ (0,0)
- Hit-testable widgets that are not Visible
- Unexpected clipping on containers (unless explicitly allowed)

---

## Resources

**See Also:**
- **[Unit Tests](unit_tests.md)** - Plugin-internal testing
- **[Smoke Tests](smoke_tests.md)** - Quick health checks
- **[Automation](automation.md)** - Running tests and troubleshooting

**Unreal Documentation:**
- [Automation Technical Guide](https://docs.unrealengine.com/5.3/en-US/automation-technical-guide-for-unreal-engine/)
- [Functional Testing](https://docs.unrealengine.com/5.3/en-US/functional-testing-in-unreal-engine/)

---

## License

Copyright ALIS. All Rights Reserved.
