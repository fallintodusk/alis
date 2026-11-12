// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldStoredSurface.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldStoredSurfaceTest,
	"Project.World.Realization.Constructed.StoredSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldStoredSurfaceTest::RunTest(const FString&)
{
	const FVector Origin(-232459,278955,370),Target=Origin+FVector(50000,40000,10);
	const FProjectWorldStoredSurface Native{Origin,
		{{{FVector(static_cast<double>(.1f),0,0),FVector(100000,10,0),FVector(100000,20,0)}}}};
	TArray<FProjectWorldSurfaceTriangle> Out; FString Error;
	if (!TestTrue(TEXT("Actual native geometry admits an exact double comparison frame."),ProjectWorldStoredSurface::Reframe(Native,Target,Out,Error))) return false;
	TestNotEqual(TEXT("A shared float frame would lose the captured native position."),Out[0].Points[0],FVector(FVector3f(Out[0].Points[0])));
	for (int32 I=0; I<3; ++I) TestEqual(TEXT("Comparison preserves each actual world position exactly."),Target+Out[0].Points[I],Origin+Native.Triangles[0].Points[I]);
	auto Unknown=Native; Unknown.Triangles[0].Points[0].X=1./3.;
	TestFalse(TEXT("Unknown double coordinates cannot claim stored native fidelity."),ProjectWorldStoredSurface::Reframe(Unknown,Target,Out,Error));
	TestTrue(TEXT("Refusal exposes no stale comparison geometry."),Out.IsEmpty());
	return !HasAnyErrors();
}
#endif
