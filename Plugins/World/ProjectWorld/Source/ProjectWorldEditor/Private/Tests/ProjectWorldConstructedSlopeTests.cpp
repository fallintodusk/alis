// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedMesh.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedSlopeTest,
	"Project.World.Realization.Constructed.Slopes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedSlopeTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedMesh;
	FSlopeMetrics Metrics; FString Error;
	const FVector2D Axis(583322.21-583300.93,4508127.52-4508130.52);
	const FProjectWorldSurfaceTriangle Captured{{FVector(583322.3431372071,4508128.51113266,3.7),
		FVector(583322.39,4508128.86,3.7),FVector(583322.3431359864,4508128.511132813,3.6999996948242186)}};
	TestFalse(TEXT("The captured height-close skinny face must fail frozen grade."),ValidateSlopes({Captured},Axis,.125,.02,Metrics,Error));
	TestTrue(TEXT("The captured bad grade is measured on actual output vertices."),Metrics.MaximumGrade>.24 && Metrics.FaceCount==1);
	const FProjectWorldSurfaceTriangle Thin{{FVector(0,0,0),FVector(1,0,0),FVector(0,.000001,.000001)}};
	TestFalse(TEXT("Submillimetre height error cannot qualify a steep thin face."),ValidateSlopes({Thin},FVector2D(1,0),.125,.02,Metrics,Error));
	const FProjectWorldSurfaceTriangle Good{{FVector(0,0,0),FVector(1,0,.12),FVector(0,1,.01)}};
	TestTrue(TEXT("A surface meeting the predeclared limits qualifies."),ValidateSlopes({Good},FVector2D(1,0),.125,.02,Metrics,Error));
	TestTrue(TEXT("The expected grade and cross-slope are measured independently."),FMath::Abs(Metrics.MaximumGrade-.12)<1e-12 && FMath::Abs(Metrics.MaximumCrossSlope-.01)<1e-12);
	TestFalse(TEXT("An empty output cannot claim slope acceptance."),ValidateSlopes({},Axis,.125,.02,Metrics,Error));
	return !HasAnyErrors();
}
#endif
