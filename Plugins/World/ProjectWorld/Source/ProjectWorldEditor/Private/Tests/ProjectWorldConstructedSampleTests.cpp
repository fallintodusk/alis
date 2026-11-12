// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedSurface.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedSampleTest,
	"Project.World.Realization.Constructed.Sample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedSampleTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedSurface;
	TArray<FProjectWorldSurfaceTriangle> Field={
		{{FVector(0,0,3),FVector(10,0,3),FVector(0,10,3)}},
		{{FVector(10,0,3),FVector(10,10,4),FVector(0,10,3)}}};
	FString Error; double Height;
	TestTrue(TEXT("The small piecewise field validates."),ValidateField(Field,Error));
	TestTrue(TEXT("Membership qualifies an interior sample."),Sample(Field,FVector2D(2,2),Height,Error));
	TestEqual(TEXT("Sampling does not extrapolate the other triangle."),Height,3.);
	TestTrue(TEXT("The other plane samples its own interior."),Sample(Field,FVector2D(8,8),Height,Error));
	TestTrue(TEXT("The sampled height follows the containing affine face."),FMath::Abs(Height-3.6)<1e-12);
	TestTrue(TEXT("Shared-edge membership agrees."),Sample(Field,FVector2D(5,5),Height,Error));
	TestEqual(TEXT("A shared-edge point has the common height."),Height,3.);
	Swap(Field[0],Field[1]);
	TestTrue(TEXT("An equivalent field order retains shared-edge sampling."),Sample(Field,FVector2D(5,5),Height,Error));
	TestEqual(TEXT("Shared-edge height is independent of clipping-loop order."),Height,3.);
	TestFalse(TEXT("Outside data has no nearest-face fallback."),Sample(Field,FVector2D(-1,2),Height,Error));
	TestTrue(TEXT("Unknown sample output cannot look like valid zero Ground."),FMath::IsNaN(Height));
	TestFalse(TEXT("An empty field remains unknown."),Sample({},FVector2D(2,2),Height,Error));
	TestFalse(TEXT("A nonfinite query remains unknown."),Sample(Field,FVector2D(std::numeric_limits<double>::quiet_NaN(),2),Height,Error));
	Field[0].Points[0].Z+=.1;
	TestFalse(TEXT("Contradictory containing faces cannot choose an arbitrary height."),Sample(Field,FVector2D(10,0),Height,Error));
	TestTrue(TEXT("Contradiction preserves the unknown output."),FMath::IsNaN(Height));
	return !HasAnyErrors();
}
#endif
