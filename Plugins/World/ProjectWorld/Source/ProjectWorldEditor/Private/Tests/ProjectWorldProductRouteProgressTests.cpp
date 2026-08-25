// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Presentation/ProjectWorldProductRouteProgress.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldProductRouteProgressTest,
	"Project.World.Presentation.ProductRouteProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldProductRouteProgressTest::RunTest(const FString& Parameters)
{
	FProjectWorldProductRouteProgress Progress;
	TestEqual(TEXT("Empty evidence fails at map identity."), Progress.FirstMissingGate(), FString(TEXT("map_identity")));
	TestFalse(TEXT("Empty evidence is rejected."), Progress.IsAccepted());

	Progress.bMapIdentity = true;
	Progress.bGameModeIdentity = true;
	Progress.bPossessedPlayer = true;
	Progress.bGroundedPlayer = true;
	Progress.bNormalMovement = true;
	Progress.bTerrainCollision = true;
	Progress.bRoadCollision = true;
	Progress.bBuildingCollision = true;
	Progress.bGameplayInteraction = true;
	Progress.bCenterUnloadedAtEdge = true;
	Progress.bEdgeLoaded = true;
	TestEqual(TEXT("A missing reload cannot pass."), Progress.FirstMissingGate(), FString(TEXT("center_reloaded")));
	TestFalse(TEXT("Partial product evidence is rejected."), Progress.IsAccepted());

	Progress.bCenterReloaded = true;
	TestTrue(TEXT("The complete product route is accepted."), Progress.IsAccepted());
	TestTrue(TEXT("Complete evidence has no missing gate."), Progress.FirstMissingGate().IsEmpty());
	return true;
}

#endif
