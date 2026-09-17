// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Interfaces/IOrchestratorRegistry.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOrchestratorFeatureReadinessTest,
	"Orchestrator.Unit.FeatureReadiness.ManifestAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorFeatureReadinessTest::RunTest(const FString& Parameters)
{
	IOrchestratorRegistry* Registry = GetOrchestratorRegistry();
	if (!TestNotNull(TEXT("Orchestrator registry is installed."), Registry))
	{
		return false;
	}

	TestTrue(TEXT("A manifested plugin with its module loaded is available."),
		Registry->IsFeatureAvailable(FName(TEXT("Orchestrator"))));
	TestTrue(TEXT("A manifested production feature plugin is available by plugin name."),
		Registry->IsFeatureAvailable(FName(TEXT("ProjectCombat"))));
	TestFalse(TEXT("A gameplay feature identity is not an Orchestrator plugin identity."),
		Registry->IsFeatureAvailable(FName(TEXT("Combat"))));
	TestFalse(TEXT("An enabled plugin outside the Orchestrator manifest is not available."),
		Registry->IsFeatureAvailable(FName(TEXT("ProjectLoading"))));
	TestFalse(TEXT("An unknown plugin is not available."),
		Registry->IsFeatureAvailable(FName(TEXT("ProjectFeatureThatDoesNotExist"))));

	return true;
}

#endif
