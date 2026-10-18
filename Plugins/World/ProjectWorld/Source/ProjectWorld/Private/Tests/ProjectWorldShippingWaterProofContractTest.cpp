// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Presentation/ProjectWorldShippingWaterProofGate.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldShippingWaterMapContractTest,
	"ProjectWorld.PlayableTour.ShippingWater.ProductMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldShippingWaterMapContractTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldShippingWaterProofContract;
	TestTrue(TEXT("Kazan production map is admitted."), IsProductMapPackage(
		TEXT("/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory")));
	TestTrue(TEXT("Manhattan production map is admitted by the same owner contract."), IsProductMapPackage(
		TEXT("/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase")));
	TestFalse(TEXT("Fixture ownership cannot satisfy production Water proof."), IsProductMapPackage(
		TEXT("/ProjectWorldTestData/Generated/Twin/L_ProjectWorldTerritoryTwin")));
	TestFalse(TEXT("Authored content is outside generated product ownership."), IsProductMapPackage(
		TEXT("/ProjectWorldData/Authored/Fixtures/L_ProjectWorldMarker")));
	TestFalse(TEXT("Traversal-like package paths refuse."), IsProductMapPackage(
		TEXT("/ProjectWorldData/Generated/../Authored/L_Invalid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldShippingWaterProofContractTest,
	"ProjectWorld.PlayableTour.ShippingWater.TargetRelativeTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldShippingWaterProofContractTest::RunTest(const FString& Parameters)
{
	FString Error;
	TestTrue(
		TEXT("A 550.29 m target accepts the geometrically required real-input travel."),
		ProjectWorldShippingWaterProofContract::ValidateTargetRelativeTravel(
			55029.0, 19287.0, 55028.0, Error));

	Error.Reset();
	TestFalse(
		TEXT("Travel shorter than target distance minus the arrival radius is rejected."),
		ProjectWorldShippingWaterProofContract::ValidateTargetRelativeTravel(
			80000.0, 20000.0, 54998.0, Error));
	TestTrue(TEXT("Insufficient travel reports its exact contract failure."),
		Error.Contains(TEXT("target-relative minimum")));

	Error.Reset();
	TestTrue(
		TEXT("Changing the requested distance changes the derived minimum without another constant."),
		ProjectWorldShippingWaterProofContract::ValidateTargetRelativeTravel(
			40000.0, 10000.0, 15000.0, Error));

	Error.Reset();
	TestFalse(
		TEXT("A target inside the arrival radius cannot masquerade as traversal evidence."),
		ProjectWorldShippingWaterProofContract::ValidateTargetRelativeTravel(
			20000.0, 1000.0, 20000.0, Error));
	return true;
}

#endif
