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
	Progress.bCenterUnloadedAtEdge = true;
	Progress.bEdgeLoaded = true;
	Progress.bCenterReloaded = true;
	TestFalse(TEXT("Interaction remains required by default."), Progress.IsAccepted());
	TestTrue(TEXT("An explicit non-interactive world may omit gameplay interaction."), Progress.IsAccepted(false));
	TestTrue(TEXT("The non-interactive policy has no missing gate."), Progress.FirstMissingGate(false).IsEmpty());

	Progress.bGameplayInteraction = true;
	Progress.bCenterReloaded = false;
	TestEqual(TEXT("A missing reload cannot pass."), Progress.FirstMissingGate(), FString(TEXT("center_reloaded")));
	TestFalse(TEXT("Partial product evidence is rejected."), Progress.IsAccepted());

	Progress.bCenterReloaded = true;
	TestTrue(TEXT("The complete product route is accepted."), Progress.IsAccepted());
	TestTrue(TEXT("Complete evidence has no missing gate."), Progress.FirstMissingGate().IsEmpty());
	return true;
}

#endif
