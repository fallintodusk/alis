// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
//
// Phase 5 / separate-correctness-track smoke test: prove that
// IAutomationDriverModule + CreateDriver() are reachable from ALIS test
// code. No widget interaction in v1 - just API plumbing. Widget-driven
// drag/drop migration stays deferred until this smoke passes on CI.
//
// Why this test exists:
//   Our drag/drop E2E coverage today uses synthetic FDragDropEvent calls
//   that bypass Slate's real bubble routing. AutomationDriver hooks the
//   platform message handler so Press/Move/Release flow through the
//   actual FEventRouter bubble policy. That is the correctness gap the
//   separate track closes. Before porting any real drag/drop test, we
//   need to be sure the module loads and the driver factory returns a
//   non-null driver in this target - that is what this test asserts.
//
// See: docs/agents/canonical.md "AutomationDriver migration" under
//      "Testing architecture decisions" for the dual-run migration rule.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "IAutomationDriverModule.h"
#include "IAutomationDriver.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAutomationDriverModuleLoadsSmokeTest,
    "ProjectIntegrationTests.Correctness.AutomationDriver.ModuleLoadsAndCreatesDriver",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FAutomationDriverModuleLoadsSmokeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    // Module load - the plugin ships with the engine and should be
    // available in editor + client contexts. If this fails we want to
    // know immediately, not when a later drag test tries to use the API.
    IAutomationDriverModule* Module = &IAutomationDriverModule::Get();
    TestNotNull(TEXT("IAutomationDriverModule::Get() must return a module pointer"), Module);
    if (!Module)
    {
        return false;
    }

    // Enable + create + drop. The driver owns the platform message
    // handler hook, so we must pair Enable/Disable to avoid leaving the
    // hook installed for subsequent unrelated tests. CreateDriver returns
    // a TSharedRef<IAutomationDriver, ESPMode::ThreadSafe>; keeping it in
    // a TSharedPtr lets us drop it before Disable().
    Module->Enable();
    {
        TSharedPtr<IAutomationDriver, ESPMode::ThreadSafe> Driver = Module->CreateDriver();
        TestTrue(TEXT("IAutomationDriverModule::CreateDriver() must return a valid driver"), Driver.IsValid());
    }
    Module->Disable();

    // Re-check that the module still reports as loaded after the
    // enable/disable round-trip (no accidental module unload). This
    // catches regressions where disable would tear down the module.
    TestTrue(TEXT("IAutomationDriverModule must remain loaded after enable/disable round-trip"),
        FModuleManager::Get().IsModuleLoaded(TEXT("AutomationDriver")));

    return true;
}

// Taxonomy (canonical.md Tag taxonomy): Speed=Fast, Kind=Integration
// (touches UE module manager + driver factory), Area=AutomationDriver.
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FAutomationDriverModuleLoadsSmokeTest,
    "ProjectIntegrationTests.Correctness.AutomationDriver.ModuleLoadsAndCreatesDriver",
    "[Fast][Integration][AutomationDriver]")

#endif // WITH_DEV_AUTOMATION_TESTS
