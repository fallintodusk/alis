// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedMeshCoverage.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedCoverageTest,
	"Project.World.Realization.Constructed.Coverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedCoverageTest::RunTest(const FString&)
{
	using namespace UE::Geometry;
	using namespace ProjectWorldConstructedMeshCoverage;
	FString ClipError;
	const FPolygon2d LostArea({FVector2D(512,512),FVector2D(512.00000001,512),FVector2D(512,512.00000001)});
	TestTrue(TEXT("The native positive-area control reproduces exactly zero ordinary area."),LostArea.SignedArea()==0.);
	TestTrue(TEXT("A numerically lost positive source clip refuses instead of disappearing from Expected and output."),
		ClassifySourceClip(LostArea,ClipError)==ESourceClip::Invalid);
	TestTrue(TEXT("A genuinely collinear clipped source ring is empty."),
		ClassifySourceClip(FPolygon2d({FVector2D(0,0),FVector2D(1,1),FVector2D(2,2)}),ClipError)==ESourceClip::Empty);
	TestTrue(TEXT("A positive clipped source ring remains part of Expected."),
		ClassifySourceClip(FPolygon2d({FVector2D(0,0),FVector2D(1,0),FVector2D(0,1)}),ClipError)==ESourceClip::Area);
	const TArray<FGeneralPolygon2d> Expected={FGeneralPolygon2d(FPolygon2d({FVector2D(0,0),FVector2D(100,0),FVector2D(100,100),FVector2D(0,100)}))};
	const TArray<FProjectWorldSurfaceTriangle> Complete={
		{{FVector(0,0,0),FVector(100,0,0),FVector(0,100,0)}},{{FVector(100,0,0),FVector(100,100,0),FVector(0,100,0)}}};
	FString Error; FMetrics Metrics;
	TestTrue(TEXT("The original complete stored domain certifies with zero conversion."),Validate(Expected,Complete,0,Metrics,Error));
	TestFalse(TEXT("A missing positive-area face never certifies."),Validate(Expected,{Complete[0]},0,Metrics,Error));
	TestFalse(TEXT("Duplicate faces never certify."),Validate(Expected,{Complete[0],Complete[0]},0,Metrics,Error));
	auto Shifted=Complete; for (auto& T : Shifted) for (auto& P : T.Points) P.X+=10;
	TestFalse(TEXT("Geometry outside the original domain never certifies."),Validate(Expected,Shifted,0,Metrics,Error));
	TArray<FProjectWorldSurfaceTriangle> Hole;
	const FVector Outer[4]={FVector(0,0,0),FVector(100,0,0),FVector(100,100,0),FVector(0,100,0)};
	const FVector Inner[4]={FVector(45,45,0),FVector(55,45,0),FVector(55,55,0),FVector(45,55,0)};
	for (int32 I=0; I<4; ++I)
	{
		const int32 J=(I+1)%4;
		Hole.Add({{Outer[I],Outer[J],Inner[I]}}); Hole.Add({{Outer[J],Inner[J],Inner[I]}});
	}
	TestFalse(TEXT("Interior missing coverage refuses even within an aggregate boundary area allowance."),Validate(Expected,Hole,1,Metrics,Error));
	TestTrue(TEXT("The missing-hole control is smaller than the measured boundary area bound."),Metrics.MissingArea<Metrics.ConversionBound);
	auto Declared=Expected;
	TestTrue(TEXT("A genuine declared hole is well-formed."),Declared[0].AddHole(FPolygon2d({FVector2D(45,45),FVector2D(45,55),FVector2D(55,55),FVector2D(55,45)})));
	TestTrue(TEXT("The same hole is preserved when it belongs to the original domain."),Validate(Declared,Hole,0,Metrics,Error));
	TestFalse(TEXT("An outer boundary allowance cannot license filling a declared interior hole."),Validate(Declared,Complete,1,Metrics,Error));
	const TArray<FGeneralPolygon2d> Islands={Expected[0],FGeneralPolygon2d(FPolygon2d({FVector2D(120,0),FVector2D(220,0),FVector2D(220,100),FVector2D(120,100)}))};
	auto FalseCorrespondence=Complete;
	auto Rectangle = [&](double X0,double Y0,double X1,double Y1)
	{
		FalseCorrespondence.Add({{FVector(X0,Y0,0),FVector(X1,Y0,0),FVector(X0,Y1,0)}});
		FalseCorrespondence.Add({{FVector(X1,Y0,0),FVector(X1,Y1,0),FVector(X0,Y1,0)}});
	};
	Rectangle(120,0,220,100); Rectangle(100,49,120,51); Rectangle(300,0,301,1);
	TestFalse(TEXT("Equal component counts cannot hide two merged originals and a spurious replacement island."),Validate(Islands,FalseCorrespondence,1,Metrics,Error));
	TestTrue(TEXT("The component-correspondence control fits inside the aggregate exterior allowance."),Metrics.ExteriorArea<Metrics.ConversionBound);
	TestFalse(TEXT("A declared complete component cannot disappear into an empty result."),Validate(Expected,{},1,Metrics,Error));
	const TArray<FGeneralPolygon2d> Tiny={FGeneralPolygon2d(FPolygon2d({FVector2D(10000,10000),FVector2D(10000.00025,10000),FVector2D(10000,10000.00025)}))};
	TestTrue(TEXT("The point-collapse control has a positive original area before float conversion."),Tiny[0].SignedArea()>0);
	TestFalse(TEXT("A real complete source component cannot be treated as a point-collapsed subdivision."),Validate(Tiny,Complete,1,Metrics,Error));
	const TArray<FGeneralPolygon2d> Thin={FGeneralPolygon2d(FPolygon2d({FVector2D(10000,10000),
		FVector2D(10010,10000),FVector2D(10010,10000.0001),FVector2D(10000,10000.0001)}))};
	TestTrue(TEXT("The complete thin-component control has positive original area."),Thin[0].SignedArea()>0);
	TestFalse(TEXT("A complete source component cannot use the per-TIN two-vertex collapse allowance."),Validate(Thin,Complete,1,Metrics,Error));
	TestTrue(TEXT("The whole-component refusal is the native storage gate, not an unrelated area mismatch."),Error.Contains(TEXT("complete constructed component cannot survive")));
	return !HasAnyErrors();
}
#endif
