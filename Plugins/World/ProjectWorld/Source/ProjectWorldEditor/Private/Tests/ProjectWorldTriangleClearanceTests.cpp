// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldTerrainSurface.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldTriangleClearanceTest,
	"Project.World.Realization.Alignment.TriangleClearanceOracle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldTriangleClearanceTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldTerrainSurface;
	FProjectWorldSurfaceTriangle Road{{FVector(0, 0, 1), FVector(10, 0, 1), FVector(0, 10, 1)}};
	FProjectWorldSurfaceTriangle Terrain{{FVector(2, 2, 2), FVector(12, 2, -2), FVector(2, 12, -2)}};
	TArray<FVector2D> Polygon;
	FString Error;
	TestTrue(TEXT("Native intersection includes a terrain vertex inside the road."), IntersectXY(Road, Terrain, Polygon, Error));
	TestEqual(TEXT("Independent triangular overlap area."), Area(Polygon), 18.0);
	double Clearance = DBL_MAX;
	for (const auto& P : Polygon)
	{
		double R = 0, T = 0;
		TestTrue(TEXT("Finite plane heights evaluate."), Height(Road, P, R, Error) && Height(Terrain, P, T, Error));
		Clearance = FMath::Min(Clearance, R - T);
	}
	TestTrue(TEXT("Interior overlap detects the 1 m burial."), FMath::Abs(Clearance + 1) < 1e-9);
	for (auto& P : Road.Points) P += FVector(1000000, 6000000, 0);
	for (auto& P : Terrain.Points) P += FVector(1000000, 6000000, 0);
	TestTrue(TEXT("Canonical-scale translated intersection succeeds."), IntersectXY(Road, Terrain, Polygon, Error));
	TestTrue(TEXT("Translation preserves overlap area."), FMath::Abs(Area(Polygon) - 18) < 1e-7);
	for (auto& P : Terrain.Points) P += FVector(100, 100, 0);
	TestTrue(TEXT("Disjoint triangles return an empty intersection."), IntersectXY(Road, Terrain, Polygon, Error) && Polygon.IsEmpty());
	Terrain = Road;
	for (auto& P : Terrain.Points) P += FVector(10, 0, 0);
	TestTrue(TEXT("A point touch has no positive-area coverage."), IntersectXY(Road, Terrain, Polygon, Error) && Area(Polygon) < 1e-12);
	Terrain = Road;
	Terrain.Points[2] = Terrain.Points[1];
	TestFalse(TEXT("Degenerate XY cannot certify clearance."), IntersectXY(Road, Terrain, Polygon, Error));
	TestTrue(TEXT("Rejected numerical geometry reports its error."), !Error.IsEmpty());
	Terrain = Road;
	Terrain.Points[0].X = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Non-finite geometry cannot certify clearance."), IntersectXY(Road, Terrain, Polygon, Error));
	return true;
}
#endif
