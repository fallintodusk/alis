// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductRouteCollision.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldProductRouteGroundPlacementPolicyTest,
	"Project.World.Presentation.ProductRouteGroundPlacementPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldProductRouteGroundPlacementPolicyTest::RunTest(const FString& Parameters)
{
	FHitResult Overlap;
	Overlap.bBlockingHit = false;
	Overlap.ImpactPoint = FVector(0.0, 0.0, 300.0);
	FHitResult Roof;
	Roof.bBlockingHit = true;
	Roof.ImpactPoint = FVector(0.0, 0.0, 200.0);
	FHitResult Terrain;
	Terrain.bBlockingHit = true;
	Terrain.ImpactPoint = FVector(0.0, 0.0, 100.0);
	FVector GroundLocation = FVector::ZeroVector;
	TestTrue(TEXT("An ordered blocking hit produces a ground location."),
		ProjectWorldProductRouteCollision::SelectFirstBlockingGroundLocation(
			{Overlap, Roof, Terrain}, 50.0f, GroundLocation));
	TestEqual(TEXT("Ground placement stops on the first blocker instead of tunnelling to terrain."),
		GroundLocation, FVector(0.0, 0.0, 260.0));

	GroundLocation = FVector::ZeroVector;
	TestFalse(TEXT("Overlaps alone cannot produce a grounded placement."),
		ProjectWorldProductRouteCollision::SelectFirstBlockingGroundLocation(
			{Overlap}, 50.0f, GroundLocation));
	return true;
}

#endif
