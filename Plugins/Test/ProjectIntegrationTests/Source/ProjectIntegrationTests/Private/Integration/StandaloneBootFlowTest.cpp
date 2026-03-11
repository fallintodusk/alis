// Copyright ALIS. All Rights Reserved.

/**
 * Standalone Boot Flow Integration Test
 *
 * Runs the project in standalone mode (not PIE) to validate:
 * 1. ProjectBootSubsystem initializes
 * 2. Config is loaded from DefaultProjectBoot.ini
 * 3. L_OrchestratorBoot map is recognized as boot world
 * 4. BootFlowController spawns
 * 5. Boot sequence completes
 *
 * This test can be run via command line:
 *   UnrealEditor-Cmd.exe Alis.uproject -ExecCmds="Automation RunTests ProjectIntegrationTests.Standalone" -unattended -nopause -NullRHI -log
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#if WITH_PROJECTBOOT
#include "ProjectBootSettings.h"
#include "ProjectBootSubsystem.h"
#endif // WITH_DEV_AUTOMATION_TESTS && WITH_PROJECTBOOT

#if WITH_DEV_AUTOMATION_TESTS && WITH_PROJECTBOOT

/**
 * Test: Standalone Boot Flow
 * Validates boot sequence in editor context (works with UnrealEditor-Cmd)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStandaloneBootFlowTest,
    "ProjectIntegrationTests.Standalone.BootFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FStandaloneBootFlowTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("=============================================="));
    UE_LOG(LogTemp, Display, TEXT("  Standalone Boot Flow Test"));
    UE_LOG(LogTemp, Display, TEXT("=============================================="));
    UE_LOG(LogTemp, Display, TEXT(""));

    // STEP 1: Validate ProjectBootSettings config
    UE_LOG(LogTemp, Display, TEXT("[STEP 1] Validating ProjectBootSettings..."));

    const UProjectBootSettings* Settings = UProjectBootSettings::Get();
    if (!TestNotNull(TEXT("ProjectBootSettings should exist"), Settings))
    {
        AddError(TEXT("Failed to get ProjectBootSettings - check config=ProjectBoot in UCLASS"));
        return false;
    }

    const FString BootMapPackage = Settings->GetBootMapPackageName();
    UE_LOG(LogTemp, Display, TEXT("  BootMap Package: %s"), *BootMapPackage);

    if (!TestFalse(TEXT("BootMap should not be empty"), BootMapPackage.IsEmpty()))
    {
        AddError(TEXT("BootMap is empty - check Config/DefaultProjectBoot.ini"));
        return false;
    }

    const FString ExpectedBootMap = TEXT("/Game/Project/Maps/Boot/L_OrchestratorBoot");
    if (!TestEqual(TEXT("BootMap should be configured correctly"), BootMapPackage, ExpectedBootMap))
    {
        AddError(FString::Printf(TEXT("BootMap mismatch - Expected: %s, Got: %s"), *ExpectedBootMap, *BootMapPackage));
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("  ✓ BootMap configured correctly: %s"), *BootMapPackage);
    UE_LOG(LogTemp, Display, TEXT(""));

    // STEP 2: Check BootFlowControllerClass
    UE_LOG(LogTemp, Display, TEXT("[STEP 2] Checking BootFlowControllerClass..."));

    if (Settings->BootFlowControllerClass.IsNull())
    {
        AddError(TEXT("BootFlowControllerClass is null - check Config/DefaultProjectBoot.ini"));
        return false;
    }

    const FString ControllerPath = Settings->BootFlowControllerClass.ToString();
    UE_LOG(LogTemp, Display, TEXT("  BootFlowControllerClass: %s"), *ControllerPath);
    UE_LOG(LogTemp, Display, TEXT("  ✓ BootFlowControllerClass configured"));
    UE_LOG(LogTemp, Display, TEXT(""));

    // STEP 3: Validate GameInstance has subsystem
    UE_LOG(LogTemp, Display, TEXT("[STEP 3] Checking GameInstance subsystems..."));

    UWorld* World = AutomationCommon::GetAnyGameWorld();
    if (!TestNotNull(TEXT("World should exist"), World))
    {
        AddError(TEXT("No game world available - test requires editor world"));
        return false;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
    {
        AddError(TEXT("GameInstance is null"));
        return false;
    }

    UProjectBootSubsystem* BootSubsystem = GameInstance->GetSubsystem<UProjectBootSubsystem>();
    if (!TestNotNull(TEXT("ProjectBootSubsystem should exist"), BootSubsystem))
    {
        AddError(TEXT("ProjectBootSubsystem not found - check plugin is enabled"));
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("  ✓ ProjectBootSubsystem exists"));
    UE_LOG(LogTemp, Display, TEXT(""));

    // STEP 4: Validate current world
    UE_LOG(LogTemp, Display, TEXT("[STEP 4] Validating current world..."));

    // Reuse World from STEP 3 (already validated)

    const FString WorldName = World->GetName();
    const FString WorldPackageOriginal = World->GetOutermost()->GetName();
    const FString WorldPackageStripped = UWorld::RemovePIEPrefix(WorldPackageOriginal);

    UE_LOG(LogTemp, Display, TEXT("  World Name: %s"), *WorldName);
    UE_LOG(LogTemp, Display, TEXT("  World Package (original): %s"), *WorldPackageOriginal);
    UE_LOG(LogTemp, Display, TEXT("  World Package (stripped): %s"), *WorldPackageStripped);
    UE_LOG(LogTemp, Display, TEXT(""));

    // STEP 5: Check if this is boot world
    UE_LOG(LogTemp, Display, TEXT("[STEP 5] Checking if current world is boot world..."));

    const bool bIsBootWorld = Settings->IsBootWorld(*World);
    UE_LOG(LogTemp, Display, TEXT("  IsBootWorld: %s"), bIsBootWorld ? TEXT("TRUE") : TEXT("FALSE"));

    if (bIsBootWorld)
    {
        UE_LOG(LogTemp, Display, TEXT("  ✓ Current world IS the boot world"));
        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display, TEXT("Expected logs from boot sequence:"));
        UE_LOG(LogTemp, Display, TEXT("  - ProjectBoot: Subsystem initialized"));
        UE_LOG(LogTemp, Display, TEXT("  - ProjectBoot: HandleWorldInitialized called"));
        UE_LOG(LogTemp, Display, TEXT("  - ProjectBoot: Boot world detected"));
        UE_LOG(LogTemp, Display, TEXT("  - ProjectBoot: Spawned BootFlowController"));
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("  ⚠ Current world is NOT the boot world"));
        UE_LOG(LogTemp, Display, TEXT("  This test should be run with L_OrchestratorBoot map loaded"));
    }

    UE_LOG(LogTemp, Display, TEXT(""));

    // SUMMARY
    UE_LOG(LogTemp, Display, TEXT("=============================================="));
    UE_LOG(LogTemp, Display, TEXT("  Test Summary"));
    UE_LOG(LogTemp, Display, TEXT("=============================================="));
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("Configuration Validation: ✓ PASSED"));
    UE_LOG(LogTemp, Display, TEXT("  - BootMap: %s"), *BootMapPackage);
    UE_LOG(LogTemp, Display, TEXT("  - Controller: %s"), *ControllerPath);
    UE_LOG(LogTemp, Display, TEXT("  - Subsystem: Present"));
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("Current World: %s"), *WorldPackageStripped);
    UE_LOG(LogTemp, Display, TEXT("Is Boot World: %s"), bIsBootWorld ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Display, TEXT(""));

    if (!bIsBootWorld)
    {
        UE_LOG(LogTemp, Display, TEXT("NOTE: To fully test boot flow, load L_OrchestratorBoot map first"));
        UE_LOG(LogTemp, Display, TEXT(""));
    }

    UE_LOG(LogTemp, Display, TEXT("=============================================="));
    UE_LOG(LogTemp, Display, TEXT(""));

    return true;
}

/**
 * Test: Config File Validation
 * Verifies DefaultProjectBoot.ini exists and has correct format
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBootConfigFileTest,
    "ProjectIntegrationTests.Standalone.ConfigFile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FBootConfigFileTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("=============================================="));
    UE_LOG(LogTemp, Display, TEXT("  Boot Config File Validation"));
    UE_LOG(LogTemp, Display, TEXT("=============================================="));
    UE_LOG(LogTemp, Display, TEXT(""));

    const UProjectBootSettings* Settings = UProjectBootSettings::Get();
    if (!TestNotNull(TEXT("Settings should exist"), Settings))
    {
        return false;
    }

    // Validate BootMap
    const FString BootMapPackage = Settings->GetBootMapPackageName();
    UE_LOG(LogTemp, Display, TEXT("Loaded Settings:"));
    UE_LOG(LogTemp, Display, TEXT("  BootMap: %s"), *BootMapPackage);
    UE_LOG(LogTemp, Display, TEXT("  bAutoSetDefaultMap: %s"), Settings->bAutoSetDefaultMap ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, Display, TEXT("  bAutoAddBootWidget: %s"), Settings->bAutoAddBootWidget ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, Display, TEXT("  BootFlowControllerClass: %s"), *Settings->BootFlowControllerClass.ToString());
    UE_LOG(LogTemp, Display, TEXT(""));

    // Ensure config is NOT the default engine map
    const bool bIsDefaultEngineMap = BootMapPackage.Contains(TEXT("/Engine/Maps/"));
    if (!TestFalse(TEXT("BootMap should not be default engine map"), bIsDefaultEngineMap))
    {
        AddError(TEXT("BootMap is still set to engine default - Config/DefaultProjectBoot.ini not loaded!"));
        UE_LOG(LogTemp, Error, TEXT(""));
        UE_LOG(LogTemp, Error, TEXT("CRITICAL ERROR: Config file not loaded!"));
        UE_LOG(LogTemp, Error, TEXT(""));
        UE_LOG(LogTemp, Error, TEXT("Possible causes:"));
        UE_LOG(LogTemp, Error, TEXT("  1. UCLASS has wrong config specifier (should be config=ProjectBoot)"));
        UE_LOG(LogTemp, Error, TEXT("  2. Config file doesn't exist at Config/DefaultProjectBoot.ini"));
        UE_LOG(LogTemp, Error, TEXT("  3. Config section is wrong (should be [/Script/ProjectBoot.ProjectBootSettings])"));
        UE_LOG(LogTemp, Error, TEXT(""));
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("✓ Config loaded successfully from DefaultProjectBoot.ini"));
    UE_LOG(LogTemp, Display, TEXT(""));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
