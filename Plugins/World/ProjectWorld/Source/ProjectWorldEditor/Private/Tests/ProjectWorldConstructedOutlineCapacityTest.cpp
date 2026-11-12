// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedOutline.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedOutlineCapacityTest,
	"Project.World.Realization.Constructed.OutlineCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedOutlineCapacityTest::RunTest(const FString&)
{
	using namespace UE::Geometry;
	TArray<FGeneralPolygon2d> Pieces,Out; FString Error;
	for (int32 Y=0; Y<64; ++Y) for (int32 X=0; X<64; ++X)
	{
		const FVector2D A(X,Y),B(X+1,Y),C(X,Y+1),D(X+1,Y+1);
		Pieces.Add(FGeneralPolygon2d(FPolygon2d({A,B,C})));
		Pieces.Add(FGeneralPolygon2d(FPolygon2d({B,D,C})));
	}
	// A clipped source triangle may emit up to five native triangles; the source limit is not the stored-face limit.
	const double Started=FPlatformTime::Seconds();
	if (!TestTrue(TEXT("A valid stored cover may exceed the 4096 source-triangle limit."),
		ProjectWorldConstructedOutline::Extract(Pieces,Out,Error))) AddError(Error);
	else if (TestEqual(TEXT("The larger conforming cover retains one component."),Out.Num(),1))
	{
		TestEqual(TEXT("The exact source area is retained."),Out[0].SignedArea(),4096.);
		TestTrue(TEXT("No hole is invented at higher subdivision count."),Out[0].GetHoles().IsEmpty());
	}
	AddInfo(FString::Printf(TEXT("ConstructedOutlineCapacity pieces=%d elapsed_seconds=%.6f; topology-only local fixture, not city performance"),
		Pieces.Num(),FPlatformTime::Seconds()-Started));
	Pieces.SetNum(5*4096+1);
	TestFalse(TEXT("The derived worst-case clipped-face bound still refuses before topology construction."),
		ProjectWorldConstructedOutline::Extract(Pieces,Out,Error));
	TestTrue(TEXT("The oversized census refuses at preflight, before its uninitialized pieces can be inspected."),
		Error.Contains(TEXT("bounded clipped-cover census")));
	TestTrue(TEXT("Capacity refusal exposes no partial outline."),Out.IsEmpty());
	return !HasAnyErrors();
}
#endif
