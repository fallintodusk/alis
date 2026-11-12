// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedBoundary.h"
#include "Misc/AutomationTest.h"
#include <cmath>
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedBoundaryTest,
	"Project.World.Realization.Constructed.Boundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedBoundaryTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedBoundary;
	using ProjectWorldConstructedSurface::ESampleResult;
	const TArray<FProjectWorldSurfaceTriangle> Source={{{FVector(0,0,0),FVector(.1,0,0),FVector(0,1,0)}}};
	FString Error; double Height;
	TestEqual(TEXT("Nearest float rounding can move a source-boundary point outside its field."),
		ProjectWorldConstructedSurface::TrySample(Source,FVector2D(.1f,0),Height,Error),ESampleResult::Outside);
	FConversion Conversion([&](const FVector2f& P,FString& E)
		{ double Z; return ProjectWorldConstructedSurface::TrySample(Source,FVector2D(P),Z,E); });
	FVector2f Selected,Repeated;
	TestTrue(TEXT("An immediate inward float neighbor retains genuine source support."),Conversion.Select(FVector2f(.1f,0),Selected,Error));
	TestTrue(TEXT("The selected point differs from the unsupported nearest node."),Selected.X<.1f);
	TestEqual(TEXT("An existing adjacent-grid success retains its exact representation."),Selected,FVector2f(std::nextafter(.1f,0.f),0.f));
	TestEqual(TEXT("The result samples the complete source field without extrapolation."),
		ProjectWorldConstructedSurface::TrySample(Source,FVector2D(Selected),Height,Error),ESampleResult::Sampled);
	TestTrue(TEXT("A shared node can be sampled again."),Conversion.Select(FVector2f(.1f,0),Repeated,Error));
	TestEqual(TEXT("Shared boundary nodes retain one exact stored XY."),Selected,Repeated);
	TestFalse(TEXT("Source support cannot be invented outside the immediate float neighborhood."),Conversion.Select(FVector2f(1,1),Selected,Error));
	FConversion Broken([](const FVector2f&,FString& E) { E=TEXT("invalid source control"); return ESampleResult::Invalid; });
	TestFalse(TEXT("Invalid source queries refuse without a height fallback."),Broken.Select(FVector2f(0,0),Selected,Error));
	TestTrue(TEXT("Invalid query refusal retains its diagnostic."),Error.Contains(TEXT("invalid source control")));
	TestFalse(TEXT("Nonfinite source nodes refuse."),Conversion.Select(FVector2f(std::numeric_limits<float>::infinity(),0),Selected,Error));
	TestTrue(TEXT("Refusal clears the previously successful output."),Selected.ContainsNaN());
	const FVector A(583372.4469119443,4508183.905848016,3),B(583373.608825579,4508182.277979254,3);
	const FVector C(583360.9811744209,4508175.722020746,3),D(583362.1430880557,4508174.094151984,3);
	const TArray<FProjectWorldSurfaceTriangle> Captured={{{A,C,B}},{{C,D,B}}};
	auto Domain = [&](const FVector2f& P,FString& E)
	{
		double Z;
		return ProjectWorldConstructedSurface::TrySample(Captured,FVector2D(583303.41+P.X*.01,4508532.45-P.Y*.01),Z,E);
	};
	const FVector2f Nearest((B.X-583303.41)*100,(4508532.45-B.Y)*100);
	const float Infinity=std::numeric_limits<float>::infinity(); int32 OldSupport=0;
	for (const float X : {std::nextafter(Nearest.X,-Infinity),Nearest.X,std::nextafter(Nearest.X,Infinity)})
		for (const float Y : {std::nextafter(Nearest.Y,-Infinity),Nearest.Y,std::nextafter(Nearest.Y,Infinity)})
			OldSupport+=Domain(FVector2f(X,Y),Error)==ESampleResult::Sampled;
	TestEqual(TEXT("The captured rotated corner has no supported point in the old anisotropic 3x3 search."),OldSupport,0);
	FConversion Projected(Domain);
	if (!TestTrue(TEXT("A storage-spacing-bounded conversion retains the captured rotated source corner."),Projected.Select(Nearest,Selected,Error))) AddError(Error);
	else
	{
		TestEqual(TEXT("The converted projected corner has actual source support."),Domain(Selected,Error),ESampleResult::Sampled);
		const double Spacing=std::nextafter(Nearest.Y,Infinity)-Nearest.Y;
		TestTrue(TEXT("The new corner stays within one coarsest physical storage interval per axis."),
			FMath::Abs(static_cast<double>(Selected.X)-Nearest.X)<=Spacing && FMath::Abs(static_cast<double>(Selected.Y)-Nearest.Y)<=Spacing);
	}
	for (const float SignX : {-1.f,1.f}) for (const float SignY : {-1.f,1.f})
	{
		const FVector2f Edge(std::nextafter(SignX*1024.f,-SignX*Infinity),SignY*2048.f);
		const double Spacing=FMath::Max(FMath::Abs(static_cast<double>(std::nextafter(Edge.Y,Infinity))-Edge.Y),
			FMath::Abs(static_cast<double>(std::nextafter(Edge.Y,-Infinity))-Edge.Y));
		const float Escape=Edge.X+SignX*static_cast<float>(Spacing);
		TestTrue(TEXT("The power-of-two witness really rounds beyond its intended interval."),FMath::Abs(static_cast<double>(Escape)-Edge.X)>Spacing);
		FConversion Bound([&](const FVector2f& P,FString&)
			{ return P.X==Escape && P.Y==Edge.Y ? ESampleResult::Sampled : ESampleResult::Outside; });
		TestFalse(TEXT("A source-feasible sample beyond the actual storage interval cannot escape the bound."),Bound.Select(Edge,Selected,Error));
	}
	return !HasAnyErrors();
}
#endif
