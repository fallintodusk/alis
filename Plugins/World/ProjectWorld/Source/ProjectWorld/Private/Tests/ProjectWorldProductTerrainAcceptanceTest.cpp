// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductTerrainAcceptance.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldProductTerrainNavigationPolicyTest,
	"Project.World.Presentation.ProductTerrainNavigationPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldProductTerrainNavigationPolicyTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldProductTerrainAcceptancePolicy;
	TestTrue(TEXT("Collision that contributes to navigation is accepted."),
		IsMeshComponentAccepted(true, false, true));
	TestTrue(TEXT("A non-main-pass helper excluded from navigation is accepted."),
		IsMeshComponentAccepted(false, true, false));
	TestFalse(TEXT("A collision component excluded from navigation is rejected."),
		IsMeshComponentAccepted(true, false, false));
	TestFalse(TEXT("A non-main-pass helper contributing to navigation is rejected."),
		IsMeshComponentAccepted(false, true, true));
	return true;
}

#endif
