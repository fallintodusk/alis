// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldWaterCellMesh.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	FProjectWorldCanonicalFeature Ribbon(const FString& Id, const TArray<FVector2D>& Points, double Z)
	{
		FProjectWorldCanonicalFeature Feature;
		Feature.FeatureId = Id;
		Feature.FeatureClass = TEXT("water");
		Feature.WidthMeters = 12;
		Feature.GeometryParts.Add(Points);
		auto& Water = Feature.WaterSurface;
		Water.bValid = true;
		Water.SurfaceGroupId = Id;
		Water.Geometry = TEXT("ribbon");
		Water.Behavior = TEXT("flowing");
		Water.FunctionId = TEXT("rolling_quantile_l1_isotonic");
		Water.FunctionVersion = 1;
		for (const FVector2D& P : Points) Water.Knots.Add(FVector(P.X, P.Y, Z));
		return Feature;
	}

	TArray<FProjectWorldSurfaceTriangle> Triangles(const FMeshDescription& Description)
	{
		FStaticMeshConstAttributes Attributes(Description);
		const auto Positions = Attributes.GetVertexPositions();
		TArray<FProjectWorldSurfaceTriangle> Result;
		for (const FTriangleID Id : Description.Triangles().GetElementIDs())
		{
			FProjectWorldSurfaceTriangle Triangle;
			const auto Instances = Description.GetTriangleVertexInstances(Id);
			for (int32 Corner = 0; Corner < 3; ++Corner)
				Triangle.Points[Corner] = FVector(Positions[Description.GetVertexInstanceVertex(Instances[Corner])]);
			Result.Add(Triangle);
		}
		return Result;
	}

	double OverlapArea(const FMeshDescription& Description)
	{
		const auto Surfaces = Triangles(Description);
		double Area = 0;
		for (int32 A = 0; A < Surfaces.Num(); ++A)
			for (int32 B = A + 1; B < Surfaces.Num(); ++B)
			{
				TArray<FVector2D> Polygon;
				FString Error;
                if (!ProjectWorldTerrainSurface::IntersectXY(Surfaces[A], Surfaces[B], Polygon, Error))
                    return TNumericLimits<double>::Max();
                Area += ProjectWorldTerrainSurface::Area(Polygon);
			}
		return Area / 10000;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterCoplanarCompositionTest,
	"Project.World.Water.CoplanarRibbonComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldWaterCoplanarCompositionTest::RunTest(const FString& Parameters)
{
    FMeshDescription Malformed;
    FStaticMeshAttributes BadAttributes(Malformed);
    BadAttributes.Register();
    const FPolygonGroupID BadGroup = Malformed.CreatePolygonGroup();
    for (const TArray<FVector3f>& Points : TArray<TArray<FVector3f>>{
        {FVector3f(0,0,0),FVector3f(100,0,0),FVector3f(200,0,0)},
        {FVector3f(0,0,0),FVector3f(100,0,0),FVector3f(0,100,0)}})
    {
        TArray<FVertexInstanceID> Instances;
        for (const FVector3f& P : Points)
        {
            const FVertexID Vertex = Malformed.CreateVertex();
            BadAttributes.GetVertexPositions()[Vertex] = P;
            Instances.Add(Malformed.CreateVertexInstance(Vertex));
        }
        Malformed.CreateTriangle(BadGroup, Instances);
    }
    TestTrue(TEXT("Malformed triangle intersections cannot certify disjointness."),
        OverlapArea(Malformed) == TNumericLimits<double>::Max());
	FProjectWorldCanonicalBundle Bundle;
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.HeightQuantizationMeters = 0.1;
	FProjectWorldCanonicalCell Cell;
	Cell.CellId = TEXT("composition:x0:y0");
	Cell.Bounds = FVector4d(0, 0, 100, 100);
	Bundle.Features.Add(TEXT("water/a"), Ribbon(TEXT("water/a"), {FVector2D(10,10), FVector2D(40,40)}, 0));
	Bundle.Features.Add(TEXT("water/b"), Ribbon(TEXT("water/b"), {FVector2D(20,60), FVector2D(40,40)}, 0));
	const TArray<FString> Ids{TEXT("water/a"), TEXT("water/b")};
	Cell.OwnedFeatureIds = Ids;
	TMap<FString, FProjectWorldPreparedWaterSurface> Prepared;
	FString Error;
	for (const FString& Id : Ids)
	{
		FProjectWorldPreparedWaterSurface Surface;
		if (!ProjectWorldWaterMeshBuilder::PrepareSurface(Bundle.Features[Id], Cell.Bounds, Surface, Error))
		{
			AddError(Error);
			return false;
		}
		Prepared.Add(Id, MoveTemp(Surface));
	}
	FMeshDescription Mesh;
	FVector Origin;
	FString Semantic;
	int32 Count = 0;
	if (!ProjectWorldWaterCellMesh::Build(Bundle, Cell, Prepared, Ids, 0.25, Mesh, Origin, Semantic, Count, Error))
	{
		AddError(Error);
		return false;
	}
	const double Area = OverlapArea(Mesh);
	TestTrue(FString::Printf(TEXT("Final quantized cell triangles have no positive-area overlap (%.9g m2)."), Area), Area < 0.00001);
	TestTrue(TEXT("Composition keeps visible coverage."), Count > 0);

	const FString InitialSemantic = Semantic;
	Bundle.Features.Add(TEXT("water/remote"), Ribbon(TEXT("water/remote"), {FVector2D(1000,1000), FVector2D(1100,1100)}, 0));
	TestTrue(TEXT("An unconsumed remote feature leaves the bounded cell unchanged."),
		ProjectWorldWaterCellMesh::Build(Bundle, Cell, Prepared, Ids, 0.25, Mesh, Origin, Semantic, Count, Error) && Semantic == InitialSemantic);

	// Coincident footprints still retain both authenticated source identities.
	Prepared[TEXT("water/b")] = Prepared[TEXT("water/a")];
	TestTrue(TEXT("Duplicate footprint composition succeeds."),
		ProjectWorldWaterCellMesh::Build(Bundle, Cell, Prepared, Ids, 0.25, Mesh, Origin, Semantic, Count, Error));
	TestTrue(TEXT("The duplicate emits one surface."), OverlapArea(Mesh) < 0.00001);
	const FString DuplicateSemantic = Semantic;
	Bundle.Features.Add(TEXT("water/c"), Bundle.Features[TEXT("water/b")]);
	Bundle.Features[TEXT("water/c")].FeatureId = TEXT("water/c");
	Prepared.Add(TEXT("water/c"), Prepared[TEXT("water/b")]);
	TestTrue(TEXT("Fully covered source identity remains part of semantics."),
		ProjectWorldWaterCellMesh::Build(Bundle, Cell, Prepared, {TEXT("water/a"),TEXT("water/c")}, 0.25, Mesh, Origin, Semantic, Count, Error)
		&& Semantic != DuplicateSemantic);

	// Unequal standing heights are not quantized to manufacture equality.
	auto& Standing = Bundle.Features[TEXT("water/b")].WaterSurface;
	Standing.Behavior = TEXT("standing");
	Standing.LevelMeters = 0.037;
	TestTrue(TEXT("Unequal constant heights retain their original independent surfaces."),
		ProjectWorldWaterCellMesh::Build(Bundle, Cell, Prepared, Ids, 0.25, Mesh, Origin, Semantic, Count, Error));
	const double UnequalArea = OverlapArea(Mesh);
	TestTrue(TEXT("Different-height projected coverage is not silently removed."),
		UnequalArea != TNumericLimits<double>::Max() && UnequalArea > 100);
	bool bExactStanding = false;
	for (const auto& Triangle : Triangles(Mesh))
		bExactStanding |= FMath::IsNearlyEqual(Triangle.Points[0].Z, 28.7, 0.0001);
	TestTrue(TEXT("Standing level 0.037 m survives without rounding to 0 m."), bExactStanding);
	Standing.Behavior = TEXT("flowing");
	Standing.Knots.Last().Z = 0.2;
	TestTrue(TEXT("Nonconstant functions build successfully."),
		ProjectWorldWaterCellMesh::Build(Bundle, Cell, Prepared, Ids, 0.25, Mesh, Origin, Semantic, Count, Error));
	const double NonconstantArea = OverlapArea(Mesh);
	TestTrue(TEXT("Nonconstant functions are preserved."),
		NonconstantArea != TNumericLimits<double>::Max() && NonconstantArea > 100);

	// Final quantized vertices, not only boolean area, own seam correctness.
	Standing.Knots.Last().Z = 0;
	TArray<double> Seam[2];
	for (int32 Side = 0; Side < 2; ++Side)
	{
		Cell.Bounds = Side == 0 ? FVector4d(0,0,40,100) : FVector4d(40,0,100,100);
		TestTrue(TEXT("A shared-border cell composes successfully."),
			ProjectWorldWaterCellMesh::Build(Bundle, Cell, Prepared, Ids, 0.25, Mesh, Origin, Semantic, Count, Error));
		TestTrue(TEXT("Shared-cell output remains disjoint."), OverlapArea(Mesh) < 0.00001);
		for (const auto& Triangle : Triangles(Mesh))
			for (const FVector& P : Triangle.Points)
				if (FMath::IsNearlyEqual((P + Origin).X, 4000, 0.0001)) Seam[Side].AddUnique((P + Origin).Y);
		Seam[Side].Sort();
	}
	TestTrue(TEXT("A visible shared seam is exercised."), !Seam[0].IsEmpty());
	TestEqual(TEXT("Both cells emit the same quantized seam vertices."), Seam[0], Seam[1]);

    Cell.Bounds = FVector4d(0,0,100,100);
    Prepared.Empty();
    for (int32 Side = 0; Side < 2; ++Side)
    {
        auto Feature = Ribbon(Ids[Side], {}, 0);
        Feature.WaterSurface.Geometry = TEXT("polygon");
        Feature.WaterSurface.Behavior = TEXT("standing");
        Feature.WaterSurface.FunctionId = TEXT("standing_polygon_quantile");
        Feature.WaterSurface.LevelMeters = 0.037;
        FProjectWorldCanonicalPolygon Polygon;
        const double MinX = Side == 0 ? 5 : 39.996;
        const double MaxX = Side == 0 ? 40 : 60;
        Polygon.Outer = {FVector2D(MinX,5),FVector2D(MaxX,5),FVector2D(MaxX,45),FVector2D(MinX,45)};
        if (Side == 0)
            Polygon.Holes.Add({FVector2D(20,20),FVector2D(20,25),FVector2D(25,25),FVector2D(25,20)});
        Feature.GeometryPolygons.Add(Polygon);
        Bundle.Features.Add(Ids[Side], Feature);
        FProjectWorldPreparedWaterSurface Surface;
        TestTrue(TEXT("Polygon/hole fixture preparation succeeds."),
            ProjectWorldWaterMeshBuilder::PrepareSurface(Feature, Cell.Bounds, Surface, Error));
        Prepared.Add(Ids[Side], MoveTemp(Surface));
    }
    TestTrue(TEXT("Sub-centimeter partial overlap composes."),
        ProjectWorldWaterCellMesh::Build(Bundle, Cell, Prepared, Ids, 0.25, Mesh, Origin, Semantic, Count, Error));
    TestTrue(TEXT("Final quantized polygon triangles are disjoint."), OverlapArea(Mesh) < 0.00001);
    double UnionArea = 0;
    for (const auto& Triangle : Triangles(Mesh))
    {
        const double TriangleArea = FMath::Abs(FVector::CrossProduct(Triangle.Points[1]-Triangle.Points[0], Triangle.Points[2]-Triangle.Points[0]).Z) / 20000;
        TestTrue(TEXT("Quantization creates no zero-area output triangles."), TriangleArea > 0);
        UnionArea += TriangleArea;
    }
    TestTrue(TEXT("Visible union coverage retains the 25 m2 hole."), FMath::Abs(UnionArea-2175) < 0.00001);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterLocalPlateauCompositionTest,
    "Project.World.Water.LocalPlateauComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterLocalPlateauCompositionTest::RunTest(const FString&)
{
    FProjectWorldCanonicalBundle Bundle;
    Bundle.CoordinateQuantizationMeters=.01; Bundle.HeightQuantizationMeters=.1;
    FProjectWorldCanonicalCell Cell;
    Cell.CellId=TEXT("plateau:x0:y0"); Cell.Bounds=FVector4d(0,-10,40,10);
    const TArray<FVector2D> Axis{FVector2D(-10,0),FVector2D(50,0),FVector2D(150,30)};
    auto Flat=Ribbon(TEXT("water/constant"),Axis,0);
    auto Sloping=Ribbon(TEXT("water/plateau"),Axis,0);
    Sloping.WaterSurface.Knots.Last().Z=2;
    Bundle.Features.Add(Flat.FeatureId,Flat); Bundle.Features.Add(Sloping.FeatureId,Sloping);
    const TArray<FString> Ids{Flat.FeatureId,Sloping.FeatureId};
    Cell.OwnedFeatureIds=Ids;
    TMap<FString,FProjectWorldPreparedWaterSurface> Prepared;
    FString Error,Semantic; FVector Origin; int32 Count=0;
    for (const auto& Id : Ids)
    {
        FProjectWorldPreparedWaterSurface Surface;
        if (!ProjectWorldWaterMeshBuilder::PrepareSurface(Bundle.Features[Id],Cell.Bounds,Surface,Error))
        {AddError(Error);return false;}
        Prepared.Add(Id,MoveTemp(Surface));
    }
    FMeshDescription Mesh;
    if (!ProjectWorldWaterCellMesh::Build(Bundle,Cell,Prepared,Ids,.25,Mesh,Origin,Semantic,Count,Error))
    {AddError(Error);return false;}
    TestTrue(TEXT("Global slope outside this cell does not retain duplicate local plateau faces"),
        OverlapArea(Mesh)<.00001);
    double Area=0;
    for (const auto& Triangle : Triangles(Mesh))
    {
        for (const auto& Point : Triangle.Points)
            TestEqual(TEXT("Native emitted plateau height is preserved exactly"),Point.Z,25.0);
        Area+=FMath::Abs(FVector::CrossProduct(Triangle.Points[1]-Triangle.Points[0],
            Triangle.Points[2]-Triangle.Points[0]).Z)/20000;
    }
    TestTrue(TEXT("Local union keeps the complete 40 m by 12 m footprint"),FMath::Abs(Area-480)<.00001);
    const FString BothSources=Semantic;
    TestTrue(TEXT("Single source control builds"),ProjectWorldWaterCellMesh::Build(
        Bundle,Cell,Prepared,{Flat.FeatureId},.25,Mesh,Origin,Semantic,Count,Error));
    TestNotEqual(TEXT("Covered source identity remains authenticated"),BothSources,Semantic);
    return true;
}
#endif
