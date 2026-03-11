#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "OrchestratorRegistry.h"
#include "OrchestratorVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOrchestratorVersionComparisonTest,
    "Orchestrator.Registry.VersionComparison",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorVersionComparisonTest::RunTest(const FString& Parameters)
{
    FOrchestratorVersion v1(1,0,0), v2(2,0,0), v3(1,1,0), v4(1,0,1), v5(1,0,0);
    TestTrue("v2 > v1", v2 > v1);
    TestTrue("v3 > v1", v3 > v1);
    TestTrue("v4 > v1", v4 > v1);
    TestFalse("v1 > v2", v1 > v2);
    TestFalse("v1 > v5", v1 > v5);
    TestTrue("v1 < v2", v1 < v2);
    TestTrue("v1 < v3", v1 < v3);
    TestTrue("v1 < v4", v1 < v4);
    TestFalse("v2 < v1", v2 < v1);
    TestFalse("v1 < v5", v1 < v5);
    TestTrue("v1 == v5", v1 == v5);
    TestFalse("v1 == v2", v1 == v2);
    TestTrue("v2 >= v1", v2 >= v1);
    TestTrue("v1 >= v5", v1 >= v5);
    TestTrue("v1 <= v2", v1 <= v2);
    TestTrue("v1 <= v5", v1 <= v5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOrchestratorVersionStringTest,
    "Orchestrator.Registry.VersionString",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorVersionStringTest::RunTest(const FString& Parameters)
{
    FOrchestratorVersion v1(1,2,3);
    TestEqual("ToString", v1.ToString(), TEXT("1.2.3"));
    FOrchestratorVersion v2 = FOrchestratorVersion::FromString(TEXT("4.5.6"));
    TestEqual("FromString Major", v2.Major, 4);
    TestEqual("FromString Minor", v2.Minor, 5);
    TestEqual("FromString Patch", v2.Patch, 6);
    FOrchestratorVersion v3 = FOrchestratorVersion::FromString(TEXT("1.2"));
    TestEqual("FromString incomplete Major", v3.Major, 1);
    TestEqual("FromString incomplete Minor", v3.Minor, 2);
    TestEqual("FromString incomplete Patch", v3.Patch, 0);
    FOrchestratorVersion vZero(0,0,0), vValid(0,0,1);
    TestFalse("IsValid (0.0.0)", vZero.IsValid());
    TestTrue("IsValid (0.0.1)", vValid.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOrchestratorRegistryDefaultTest,
    "Orchestrator.Registry.DefaultRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorRegistryDefaultTest::RunTest(const FString& Parameters)
{
    TArray<FOrchestratorRegistryEntry> DefaultEntries = FOrchestratorRegistry::CreateDefaultRegistry();
    TestTrue("Default registry not empty", DefaultEntries.Num() > 0);
    const FOrchestratorRegistryEntry* CoreEntry = FOrchestratorRegistry::FindEntry(DefaultEntries, TEXT("ProjectCoreFeature"));
    TestNotNull("ProjectCoreFeature entry exists", CoreEntry);
    const FOrchestratorRegistryEntry* FallbackEntry = FOrchestratorRegistry::FindEntry(DefaultEntries, TEXT("ProjectMenuExperience"));
    TestNotNull("ProjectMenuExperience entry exists", FallbackEntry);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOrchestratorRegistrySaveLoadTest,
    "Orchestrator.Registry.SaveLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorRegistrySaveLoadTest::RunTest(const FString& Parameters)
{
    TArray<FOrchestratorRegistryEntry> OriginalEntries;
    FOrchestratorRegistryEntry Entry1; Entry1.PluginName=TEXT("TestOrchestrator1"); Entry1.InstalledVersion={1,2,3}; Entry1.LatestVersion={1,3,0}; Entry1.LastUpdateCheckTimestamp=123456789; Entry1.bUpdateAvailable=true; OriginalEntries.Add(Entry1);
    FOrchestratorRegistryEntry Entry2; Entry2.PluginName=TEXT("TestOrchestrator2"); Entry2.InstalledVersion={2,0,0}; Entry2.LatestVersion={2,0,0}; Entry2.LastUpdateCheckTimestamp=987654321; Entry2.bUpdateAvailable=false; OriginalEntries.Add(Entry2);
    TestTrue("Registry saved", FOrchestratorRegistry::SaveRegistry(OriginalEntries));
    TArray<FOrchestratorRegistryEntry> LoadedEntries; TestTrue("Registry loaded", FOrchestratorRegistry::LoadRegistry(LoadedEntries));
    TestEqual("Loaded entry count", LoadedEntries.Num(), OriginalEntries.Num());
    const FOrchestratorRegistryEntry* LoadedEntry1 = FOrchestratorRegistry::FindEntry(LoadedEntries, TEXT("TestOrchestrator1"));
    TestNotNull("Entry1 loaded", LoadedEntry1);
    const FOrchestratorRegistryEntry* LoadedEntry2 = FOrchestratorRegistry::FindEntry(LoadedEntries, TEXT("TestOrchestrator2"));
    TestNotNull("Entry2 loaded", LoadedEntry2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOrchestratorRegistryUpdateEntryTest,
    "Orchestrator.Registry.UpdateEntry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorRegistryUpdateEntryTest::RunTest(const FString& Parameters)
{
    TArray<FOrchestratorRegistryEntry> Entries;
    FOrchestratorRegistryEntry InitialEntry; InitialEntry.PluginName=TEXT("TestPlugin"); InitialEntry.InstalledVersion={1,0,0}; InitialEntry.LatestVersion={1,0,0}; Entries.Add(InitialEntry);
    TestEqual("Initial entry count", Entries.Num(), 1);
    FOrchestratorRegistryEntry UpdatedEntry; UpdatedEntry.PluginName=TEXT("TestPlugin"); UpdatedEntry.InstalledVersion={1,1,0}; UpdatedEntry.LatestVersion={1,1,0};
    FOrchestratorRegistry::UpdateEntry(Entries, UpdatedEntry);
    TestEqual("Updated entry count", Entries.Num(), 1);
    const FOrchestratorRegistryEntry* Found = FOrchestratorRegistry::FindEntry(Entries, TEXT("TestPlugin"));
    TestNotNull("Found entry", Found);
    if (Found)
    {
        TestEqual("Updated version (major)", Found->InstalledVersion.Major, 1);
        TestEqual("Updated version (minor)", Found->InstalledVersion.Minor, 1);
        TestEqual("Updated version (patch)", Found->InstalledVersion.Patch, 0);
    }
    return true;
}

