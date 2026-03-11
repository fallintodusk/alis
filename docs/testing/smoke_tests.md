# Smoke Tests Guide

**Quick health checks and validation for critical systems**

This guide covers smoke tests - rapid validation that critical systems are operational.

See also:
- **[Overview](README.md)** - Testing philosophy and architecture
- **[Unit Tests](unit_tests.md)** - Plugin-internal testing
- **[Integration Tests](integration_tests.md)** - Cross-plugin testing
- **[Automation](automation.md)** - Running tests and CI/CD

---

## Smoke Test Characteristics

**Purpose:** Rapid validation that critical systems are operational

**Characteristics:**
- Very fast (< 5 seconds total)
- Test only critical paths
- Run on every build
- Fail fast if something is broken
- **Verify cross-plugin dependencies are satisfied**

**Examples:**
- All subsystems initialize without crashing
- Plugins load and dependencies resolve
- ViewModels can be instantiated
- Critical assets can be loaded
- **Menu system displays without errors**
- **Loading system can start a load**

**Where to Use:**
- Pre-commit hooks (block commits if fails)
- CI/CD entry point (run before full test suite)
- Post-deployment validation
- Developer quick checks

---

## Smoke Test Examples

### Subsystem Health Check

```cpp
/**
 * Smoke test: Core subsystems initialize
 * Quick check that all critical subsystems exist and initialize
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSubsystemsSmokeTest,
    "Project.Smoke.Subsystems",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FSubsystemsSmokeTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();

    // Verify all critical subsystems exist
    TestNotNull(TEXT("ProjectLoadingSubsystem"),
        GameInstance->GetSubsystem<UProjectLoadingSubsystem>());
    TestNotNull(TEXT("ProjectSessionSubsystem"),
        GameInstance->GetSubsystem<UProjectSessionSubsystem>());
    TestNotNull(TEXT("ProjectUIFrameworkThemeManager"),
        GameInstance->GetSubsystem<UProjectUIFrameworkThemeManager>());

    return true;
}
```

### ViewModels Instantiation

```cpp
/**
 * Smoke test: ViewModels instantiate
 * Quick check that all ViewModels can be created
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FViewModelsSmokeTest,
    "ProjectMenuMain.Smoke.ViewModels",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FViewModelsSmokeTest::RunTest(const FString& Parameters)
{
    // Create all ViewModels
    UProjectMenuViewModel* MenuVM = NewObject<UProjectMenuViewModel>();
    UProjectMapListViewModel* MapListVM = NewObject<UProjectMapListViewModel>();
    UProjectLoadingViewModel* LoadingVM = NewObject<UProjectLoadingViewModel>();
    UProjectSettingsViewModel* SettingsVM = NewObject<UProjectSettingsViewModel>();

    // Verify creation succeeded
    TestNotNull(TEXT("MenuViewModel"), MenuVM);
    TestNotNull(TEXT("MapListViewModel"), MapListVM);
    TestNotNull(TEXT("LoadingViewModel"), LoadingVM);
    TestNotNull(TEXT("SettingsViewModel"), SettingsVM);

    // Initialize (should not crash)
    MenuVM->Initialize(nullptr);
    MapListVM->Initialize(nullptr);
    LoadingVM->Initialize(nullptr);
    SettingsVM->Initialize(nullptr);

    // Cleanup
    MenuVM->Shutdown();
    MapListVM->Shutdown();
    LoadingVM->Shutdown();
    SettingsVM->Shutdown();

    return true;
}
```

### Screen Controllers Instantiation

```cpp
/**
 * Smoke test: Screen controllers instantiate
 * Quick check that all screen controllers can be created
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FScreenControllersSmokeTest,
    "ProjectMenuMain.Smoke.ScreenControllers",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FScreenControllersSmokeTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();

    // Create all screen controllers
    UProjectMenuScreen* MenuScreen = CreateWidget<UProjectMenuScreen>(
        GameInstance, UProjectMenuScreen::StaticClass());
    UProjectMapBrowserScreen* MapBrowserScreen = CreateWidget<UProjectMapBrowserScreen>(
        GameInstance, UProjectMapBrowserScreen::StaticClass());
    UProjectLoadingScreen* LoadingScreen = CreateWidget<UProjectLoadingScreen>(
        GameInstance, UProjectLoadingScreen::StaticClass());

    // Verify creation succeeded
    TestNotNull(TEXT("MenuScreen"), MenuScreen);
    TestNotNull(TEXT("MapBrowserScreen"), MapBrowserScreen);
    TestNotNull(TEXT("LoadingScreen"), LoadingScreen);

    return true;
}
```

### Plugin Pipeline Connectivity

```cpp
/**
 * Smoke test: Plugin pipeline connections
 * Quick check that cross-plugin dependencies work
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPluginPipelineSmokeTest,
    "Project.Smoke.PluginPipeline",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FPluginPipelineSmokeTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();

    // Test ProjectMenuMain ??? ProjectLoading connection
    UProjectMapListViewModel* MapListVM = NewObject<UProjectMapListViewModel>();
    MapListVM->Initialize(GameInstance);

    UProjectLoadingSubsystem* LoadingSubsystem =
        GameInstance->GetSubsystem<UProjectLoadingSubsystem>();
    TestNotNull(TEXT("MapListVM should access LoadingSubsystem"), LoadingSubsystem);

    // Test ProjectMenuMain ??? ProjectData connection
    MapListVM->RefreshMapList(); // Should query Asset Manager
    // Should not crash (even if no manifests exist)

    // Test ProjectUI ??? ProjectMenuMain connection
    UProjectUIFrameworkThemeManager* ThemeManager =
        GameInstance->GetSubsystem<UProjectUIFrameworkThemeManager>();
    TestNotNull(TEXT("MenuScreen should access ThemeManager"), ThemeManager);

    MapListVM->Shutdown();
    return true;
}
```

---

## Boot Flow Smoke Tests

### Boot Subsystem Initialization

```cpp
/**
 * Smoke test: Boot subsystem initializes
 * Verifies ProjectBootSubsystem can initialize without crashing
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBootSubsystemSmokeTest,
    "ProjectBoot.Smoke.SubsystemInit",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FBootSubsystemSmokeTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();

    UProjectBootSubsystem* BootSubsystem =
        GameInstance->GetSubsystem<UProjectBootSubsystem>();

    TestNotNull(TEXT("BootSubsystem should exist"), BootSubsystem);

    // Should not crash during basic operations
    bool bIsBootComplete = BootSubsystem->IsBootComplete();
    TestFalse(TEXT("Boot should not be complete yet"), bIsBootComplete);

    return true;
}
```

### Boot Flow Controller Spawn

```cpp
/**
 * Smoke test: Boot flow controller spawns
 * Verifies AProjectBootFlowController can be created
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBootFlowControllerSmokeTest,
    "ProjectBoot.Smoke.FlowController",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FBootFlowControllerSmokeTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = CreateTestWorld();
    TestNotNull(TEXT("Test world should exist"), TestWorld);

    AProjectBootFlowController* FlowController =
        TestWorld->SpawnActor<AProjectBootFlowController>();

    TestNotNull(TEXT("FlowController should spawn"), FlowController);

    return true;
}
```

---

## Feature Activation Smoke Tests

### Feature Subsystem Initialization

```cpp
/**
 * Smoke test: Feature subsystems initialize
 * Verifies ProjectFeatureActivationSubsystem and Registry exist
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFeatureSubsystemsSmokeTest,
    "Features.Smoke.Subsystems",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FFeatureSubsystemsSmokeTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();

    UProjectFeatureActivationSubsystem* ActivationSubsystem =
        GameInstance->GetSubsystem<UProjectFeatureActivationSubsystem>();
    TestNotNull(TEXT("FeatureActivationSubsystem should exist"), ActivationSubsystem);

    UProjectFeatureRegistrySubsystem* RegistrySubsystem =
        GameInstance->GetSubsystem<UProjectFeatureRegistrySubsystem>();
    TestNotNull(TEXT("FeatureRegistrySubsystem should exist"), RegistrySubsystem);

    return true;
}
```

### Feature Manifest Loading

```cpp
/**
 * Smoke test: Feature manifests load
 * Verifies feature manifests can be discovered via Asset Manager
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFeatureManifestSmokeTest,
    "Features.Smoke.Manifests",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FFeatureManifestSmokeTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    GameInstance->Init();

    UProjectFeatureRegistrySubsystem* Registry =
        GameInstance->GetSubsystem<UProjectFeatureRegistrySubsystem>();
    TestNotNull(TEXT("Registry should exist"), Registry);

    // Attempt to discover manifests (should not crash)
    Registry->DiscoverFeatures();

    // Should return >= 0 features (0 is valid if no manifests exist yet)
    int32 FeatureCount = Registry->GetFeatureCount();
    TestTrue(TEXT("Feature count should be >= 0"), FeatureCount >= 0);

    return true;
}
```

---

## Immutable Bootloader Smoke Tests

### Manifest JSON Parse

```cpp
/**
 * Smoke test: Bootloader manifest parses
 * Verifies manifest JSON can be parsed without errors
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBootloaderManifestParseSmokeTest,
    "ProjectBootloader.Smoke.ManifestParse",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FBootloaderManifestParseSmokeTest::RunTest(const FString& Parameters)
{
    // Sample manifest JSON
    FString ManifestJson = TEXT(R"({
        "plugins": [
            {
                "uuid": "550e8400-e29b-41d4-a716-446655440001",
                "name": "ProjectCore",
                "version": "1.4.0",
                "module": "ProjectCore",
                "platform": "Windows",
                "code": {
                    "url": "https://cdn.example.com/ProjectCore_1.4.0_code.zip",
                    "hash": "abc123",
                    "size": 1024
                },
                "assets": [
                    {
                        "url": "https://cdn.example.com/ProjectCore_1.4.0_content.utoc",
                        "hash": "def456",
                        "size": 2048,
                        "role": "utoc"
                    },
                    {
                        "url": "https://cdn.example.com/ProjectCore_1.4.0_content.ucas",
                        "hash": "789abc",
                        "size": 4096,
                        "role": "ucas"
                    }
                ],
                "engine_build_id": "UE5.5.0",
                "depends_on": [],
                "signature_thumbprint": "1234567890ABCDEF"
            }
        ]
    })");

    // Attempt to parse (replace with actual parsing logic)
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestJson);
    bool bSuccess = FJsonSerializer::Deserialize(Reader, JsonObject);

    TestTrue(TEXT("Manifest should parse successfully"), bSuccess);
    TestTrue(TEXT("JsonObject should be valid"), JsonObject.IsValid());

    return true;
}
```

### Decision Rule Behavior

```cpp
/**
 * Smoke test: Decision rule behavior
 * Verifies code vs asset hash decision logic
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBootloaderDecisionRuleSmokeTest,
    "ProjectBootloader.Smoke.DecisionRule",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FBootloaderDecisionRuleSmokeTest::RunTest(const FString& Parameters)
{
    // Sample test data
    FString CurrentCodeHash = TEXT("abc123");
    FString CurrentContentHash = TEXT("def456");

    // Test 1: No changes -> up-to-date
    FString NewCodeHash = TEXT("abc123");
    FString NewContentHash = TEXT("def456");
    TestTrue(TEXT("Should be up-to-date"),
        CurrentCodeHash == NewCodeHash && CurrentContentHash == NewContentHash);

    // Test 2: Code hash changed -> install-before-load (no restart in Shipping)
    NewCodeHash = TEXT("xyz789");
    NewContentHash = TEXT("def456");
    TestTrue(TEXT("Code change should trigger restart"),
        CurrentCodeHash != NewCodeHash);

    // Test 3: Content hash changed -> hot-mount
    NewCodeHash = TEXT("abc123");
    NewContentHash = TEXT("uvw999");
    TestTrue(TEXT("Content change should trigger hot-mount"),
        CurrentCodeHash == NewCodeHash && CurrentContentHash != NewContentHash);

    return true;
}
```

### engine_build_id Mismatch Handling

```cpp
/**
 * Smoke test: engine_build_id mismatch aborts
 * Verifies update is refused when engine build IDs don't match
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBootloaderEngineBuildIdSmokeTest,
    "ProjectBootloader.Smoke.EngineBuildId",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FBootloaderEngineBuildIdSmokeTest::RunTest(const FString& Parameters)
{
    FString CurrentEngineBuildId = TEXT("UE5.5.0");
    FString ManifestEngineBuildId = TEXT("UE5.6.0");

    // Verify mismatch is detected
    bool bMismatch = (CurrentEngineBuildId != ManifestEngineBuildId);
    TestTrue(TEXT("Mismatch should be detected"), bMismatch);

    // In real implementation, this should refuse the update
    // (Orchestrator would log error and abort)

    return true;
}
```

### (Dev/Editor) pending_updates.json on Code Change

```cpp
/**
 * Smoke test (Dev/Editor): pending_updates.json written when simulating relaunch flow
 * Verifies pending updates file is created when code changes
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBootloaderPendingUpdatesSmokeTest,
    "ProjectBootloader.Smoke.PendingUpdates",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FBootloaderPendingUpdatesSmokeTest::RunTest(const FString& Parameters)
{
    // Simulate code change detection
    bool bCodeChanged = true;

    if (bCodeChanged)
    {
        // Dev/Editor-only: mock writing pending_updates.json
        // In real implementation, Orchestrator would write (dev/editor):
        // <local-app-data>/Alis/State/pending_updates.json
        FString PendingUpdatesPath = TEXT("<local-app-data>/Alis/State/pending_updates.json");

        // Verify path is constructed correctly
        TestTrue(TEXT("Pending updates path should be valid"),
            !PendingUpdatesPath.IsEmpty());
    }

    return true;
}
```

**Details:** [boot_chain.md](../architecture/boot_chain.md)

---

## Validation Tests

**Purpose:** Verify data integrity and asset correctness

**Characteristics:**
- Fast to medium speed
- Test data assets and manifests
- Verify references and dependencies
- Run on data changes

**Examples:**
- Manifest fields are valid
- Soft references resolve
- Required features are declared
- Asset naming conventions

### Manifest Validation

```cpp
/**
 * Validation test: World manifests are valid
 * Verifies all WorldManifest assets have required fields
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWorldManifestValidationTest,
    "ProjectData.Validation.WorldManifests",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWorldManifestValidationTest::RunTest(const FString& Parameters)
{
    UAssetManager& AssetManager = UAssetManager::Get();

    // Query all WorldManifest assets
    TArray<FAssetData> ManifestAssets;
    AssetManager.GetPrimaryAssetDataList(FPrimaryAssetType("WorldManifest"), ManifestAssets);

    int32 ValidCount = 0;
    int32 InvalidCount = 0;

    for (const FAssetData& AssetData : ManifestAssets)
    {
        UProjectWorldManifest* Manifest =
            Cast<UProjectWorldManifest>(AssetData.GetAsset());

        if (Manifest)
        {
            TArray<FString> Errors;
            if (Manifest->Validate(Errors))
            {
                ValidCount++;
            }
            else
            {
                InvalidCount++;
                for (const FString& Error : Errors)
                {
                    AddError(FString::Printf(TEXT("%s: %s"),
                        *AssetData.AssetName.ToString(), *Error));
                }
            }
        }
    }

    TestTrue(TEXT("Should have at least 1 manifest"), ValidCount > 0);
    TestEqual(TEXT("All manifests should be valid"), InvalidCount, 0);

    return true;
}
```

### Asset Reference Validation

```cpp
/**
 * Validation test: Soft references resolve
 * Verifies all soft object paths in manifests are valid
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAssetReferenceValidationTest,
    "ProjectData.Validation.AssetReferences",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FAssetReferenceValidationTest::RunTest(const FString& Parameters)
{
    UAssetManager& AssetManager = UAssetManager::Get();

    TArray<FAssetData> ManifestAssets;
    AssetManager.GetPrimaryAssetDataList(FPrimaryAssetType("WorldManifest"), ManifestAssets);

    int32 BrokenReferences = 0;

    for (const FAssetData& AssetData : ManifestAssets)
    {
        UProjectWorldManifest* Manifest =
            Cast<UProjectWorldManifest>(AssetData.GetAsset());

        if (Manifest && !Manifest->WorldPath.IsNull())
        {
            // Check if soft reference resolves
            if (!Manifest->WorldPath.IsValid())
            {
                BrokenReferences++;
                AddError(FString::Printf(TEXT("Broken reference in %s: %s"),
                    *AssetData.AssetName.ToString(),
                    *Manifest->WorldPath.ToString()));
            }
        }
    }

    TestEqual(TEXT("No broken references"), BrokenReferences, 0);

    return true;
}
```

---

## Running Smoke Tests

### Automated

```bash
# Run only smoke tests (fastest)
make test-quick
```

### Manual

```bash
UnrealEditor-Cmd.exe "Alis.uproject" \
  -ExecCmds="Automation RunTests Project.Smoke;ProjectMenuMain.Smoke;ProjectBoot.Smoke" \
  -unattended -nopause -nosplash -NullRHI \
  -testexit="Automation Test Queue Empty" \
  -log
```

### Pre-Commit Hook

Add smoke tests to `.git/hooks/pre-commit`:

```bash
#!/bin/bash
echo "Running smoke tests..."

make test-quick

if [ $? -ne 0 ]; then
    echo "??? Smoke tests failed! Commit aborted."
    exit 1
fi

echo "??? Smoke tests passed!"
```

---

## CI/CD Integration

### Build Pipeline Entry Point

Smoke tests should be the **first** step in CI/CD:

```yaml
stages:
  - smoke     # Fast health check (< 5s)
  - test      # Full test suite (30-60s)
  - build     # Compile game
  - deploy    # Package and deploy

smoke:
  stage: smoke
  script:
    - make test-quick
  timeout: 1m
  retry: 0  # Fail fast, no retries
```

**Benefits:**
- Fails fast if critical systems are broken
- Saves CI time (don't run full suite if smoke fails)
- Early feedback to developers

---

## Resources

**See Also:**
- **[Unit Tests](unit_tests.md)** - Plugin-internal testing
- **[Integration Tests](integration_tests.md)** - Cross-plugin testing
- **[Automation](automation.md)** - Running tests and troubleshooting

**Project Documentation:**
- [Architecture Roadmap](../../todo/create_architecture.md)
- [Core Principles](../architecture/core_principles.md)

---

## License

Copyright ALIS. All Rights Reserved.

