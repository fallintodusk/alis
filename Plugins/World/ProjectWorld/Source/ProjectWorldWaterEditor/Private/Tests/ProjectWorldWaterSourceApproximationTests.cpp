// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldWaterCellMesh.h"
#include "ProjectWorldWaterSourceApproximation.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldWaterApproximationTest
{
    FProjectWorldCanonicalFeature Source(const FString& Id, const TArray<FVector>& Knots)
    {
        FProjectWorldCanonicalFeature Feature;
        Feature.FeatureId=Id; Feature.FeatureClass=TEXT("water"); Feature.WidthMeters=1;
        auto& Surface=Feature.WaterSurface;
        Surface.bValid=true; Surface.Geometry=TEXT("ribbon"); Surface.Behavior=TEXT("flowing");
        Surface.SurfaceGroupId=Id; Surface.FunctionId=TEXT("rolling_quantile_l1_isotonic");
        Surface.FunctionVersion=1; Surface.Knots=Knots;
        return Feature;
    }

    TArray<FProjectWorldSurfaceTriangle> Faces(const FMeshDescription& Mesh)
    {
        TArray<FProjectWorldSurfaceTriangle> Result;
        const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
        for (const auto Id : Mesh.Triangles().GetElementIDs())
        {
            FProjectWorldSurfaceTriangle Face;
            const auto Instances=Mesh.GetTriangleVertexInstances(Id);
            for (int32 I=0;I<3;++I)
                Face.Points[I]=FVector(Positions[Mesh.GetVertexInstanceVertex(Instances[I])]);
            Result.Add(Face);
        }
        return Result;
    }

    double Overlap(const FMeshDescription& Mesh)
    {
        const auto Triangles=Faces(Mesh);
        double Area=0;
        for (int32 A=0;A<Triangles.Num();++A) for (int32 B=A+1;B<Triangles.Num();++B)
        {
            TArray<FVector2D> Polygon; FString Error;
            if (!ProjectWorldTerrainSurface::IntersectXY(Triangles[A],Triangles[B],Polygon,Error))
                return TNumericLimits<double>::Max();
            Area+=ProjectWorldTerrainSurface::Area(Polygon)/10000;
        }
        return Area;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterSourceApproximationTest,
    "Project.World.Water.SourceApproximation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterSourceApproximationTest::RunTest(const FString&)
{
    using namespace ProjectWorldWaterApproximationTest;
    using namespace UE::Geometry;
    FProjectWorldCanonicalBundle Bundle;
    Bundle.CoordinateQuantizationMeters=.01; Bundle.HeightQuantizationMeters=.1;
    FProjectWorldCanonicalCell Cell;
    Cell.CellId=TEXT("approximation:x0:y0"); Cell.Bounds=FVector4d(0,0,100,100);
    const FString SlopingId=TEXT("water/transition"), FlatId=TEXT("water/local-flat");
    // Translated source-shaped junction: distinct raw functions have the same
    // quantized plateau locally. The high corner and its incident face remain real.
    Bundle.Features.Add(SlopingId,Source(SlopingId,{FVector(65.52,11.72,.4),
        FVector(48.31,17.78,.1),FVector(30,30,0),FVector(.36,32.16,0)}));
    Bundle.Features.Add(FlatId,Source(FlatId,{FVector(60,60,.8),
        FVector(48.27,38.72,0),FVector(30,30,0)}));
    TMap<FString,FProjectWorldPreparedWaterSurface> Prepared;
    Prepared.Add(SlopingId).TriangleVertices={
        FVector2D(30.28,30.42),FVector2D(29.83,29.51),FVector2D(48.54,18.23),
        FVector2D(29.83,29.51),FVector2D(48.03,17.36),FVector2D(48.54,18.23),
        FVector2D(30.04,30.5),FVector2D(29.83,29.51),FVector2D(30.28,30.42)};
    Prepared.Add(FlatId).TriangleVertices={
        FVector2D(48.49,38.27),FVector2D(48.05,39.17),FVector2D(30.22,29.55),
        FVector2D(48.05,39.17),FVector2D(29.78,30.45),FVector2D(30.22,29.55)};
    for (const auto& R : TArray<FVector4d>{FVector4d(80,50,85,65),FVector4d(90,50,95,65),
        FVector4d(85,50,90,55),FVector4d(85,60,90,65)})
        Prepared[SlopingId].TriangleVertices.Append({FVector2D(R.X,R.Y),FVector2D(R.Z,R.Y),FVector2D(R.Z,R.W),
            FVector2D(R.X,R.Y),FVector2D(R.Z,R.W),FVector2D(R.X,R.W)});
    const TArray<FString> Ids{SlopingId,FlatId};
    Cell.OwnedFeatureIds=Ids;
    FMeshDescription Mesh; FVector Origin; FString Semantic,Error; int32 Count=0;
    const auto Build=[&] {return ProjectWorldWaterCellMesh::Build(Bundle,Cell,Prepared,Ids,.25,
        Mesh,Origin,Semantic,Count,Error);};
    if (!TestTrue(TEXT("Source-shaped transition fixture builds"),Build())) {AddError(Error);return false;}
    const double Area=Overlap(Mesh);
    TestTrue(FString::Printf(TEXT("All positive-area mixed contacts are removed, not only near-depth contacts (%.12g m2)"),Area),Area<.000001);

    TArray<FGeneralPolygon2d> Inputs,United;
    for (const auto& Entry : Prepared) for (int32 I=0;I<Entry.Value.TriangleVertices.Num();I+=3)
    {
        FPolygon2d Polygon({FVector2d(Entry.Value.TriangleVertices[I]),
            FVector2d(Entry.Value.TriangleVertices[I+1]),FVector2d(Entry.Value.TriangleVertices[I+2])});
        if (Polygon.SignedArea()<0) Polygon.Reverse();
        Inputs.Add(FGeneralPolygon2d(MoveTemp(Polygon)));
    }
    if (!TestTrue(TEXT("Independent native coverage union succeeds"),PolygonsUnion(Inputs,United,false))) return false;
    double ExpectedArea=0,ActualArea=0;
    for (const auto& Polygon : United) ExpectedArea+=FMath::Abs(Polygon.SignedArea());
    bool HighCorner=false;int32 HoleHits=0;
    const FVector HoleWorld=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(87.5,57.5,0))-Origin;
    const FVector ExpectedHigh=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(48.54,18.23,.35))-Origin;
    for (const auto& Face : Faces(Mesh))
    {
        ActualArea+=FMath::Abs(FVector::CrossProduct(Face.Points[1]-Face.Points[0],Face.Points[2]-Face.Points[0]).Z)/20000;
        for (const auto& Point : Face.Points) HighCorner|=Point.Equals(ExpectedHigh,.001);
        HoleHits+=FPolygon2d({FVector2d(Face.Points[0].X,Face.Points[0].Y),
            FVector2d(Face.Points[1].X,Face.Points[1].Y),FVector2d(Face.Points[2].X,Face.Points[2].Y)}).Contains(FVector2d(HoleWorld.X,HoleWorld.Y));
    }
    TestTrue(TEXT("Refinement retains the legitimate high source corner"),HighCorner);
    TestTrue(TEXT("Exact projected union coverage is retained"),FMath::Abs(ExpectedArea-ActualArea)<.00001);
    TestEqual(TEXT("An unrelated source footprint hole remains uncovered"),HoleHits,0);
    const FString FirstSemantic=Semantic;
    TestTrue(TEXT("Identical source replay is deterministic"),Build() && Semantic==FirstSemantic);

    // Inspect the approximation boundary before horizontal union can remove
    // interior edges. A neighboring face must use the same source-evaluated split.
    FMeshDescription Intermediate;
    TArray<const FProjectWorldCanonicalFeature*> Sources;
    for (const auto& Id : Ids)
    {
        FProjectWorldWaterMeshBuildResult Part;
        if (!ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle,Cell,Bundle.Features[Id],Prepared[Id],.25,Part,Error))
        {AddError(Error);return false;}
        for (int32 I=0;I<Part.TriangleCount;++I) Sources.Add(&Bundle.Features[Id]);
        if (Intermediate.Triangles().Num()==0) Intermediate=MoveTemp(Part.MeshDescription);
        else FStaticMeshOperations::AppendMeshDescription(Part.MeshDescription,Intermediate,FStaticMeshOperations::FAppendSettings());
    }
    const FMeshDescription Coarse=Intermediate;
    bool Changed=false;FString Hash;
    if (!ProjectWorldWaterSourceApproximation::Refine(Intermediate,Sources,Bundle,Cell,Origin,.25,Changed,Hash,Error))
    {AddError(Error);return false;}
    TestTrue(TEXT("The source approximation boundary is actually exercised"),Changed && Hash.Len()==64);
    const FVector Split=FVector(FVector3f(FProjectWorldCanonicalLoader::CanonicalToUnreal(
        Bundle,FVector(34.5075,26.69,.25))-Origin));
    const auto InspectSplit=[&](const FMeshDescription& Output)
    {
        int32 IncidentFaces=0,UnsplitEdges=0,WrongHeights=0;
        for (const auto& Face : Faces(Output))
        {
            bool Incident=false;
            for (int32 I=0;I<3;++I)
            {
                const auto& A=Face.Points[I];const auto& B=Face.Points[(I+1)%3];
                const FVector2d AP(Split.X-A.X,Split.Y-A.Y),AB(B.X-A.X,B.Y-A.Y);
                if (AP==FVector2d::Zero()) {Incident=true;WrongHeights+=A.Z!=Split.Z;}
                else if (AP!=AB && FVector2d::CrossProduct(AP,AB)==0 && AP.Dot(AB)>0 && AP.Dot(AB)<AB.SquaredLength()) ++UnsplitEdges;
            }
            IncidentFaces+=Incident;
        }
        return FIntVector(IncidentFaces,UnsplitEdges,WrongHeights);
    };
    TestTrue(TEXT("The interface inspector rejects known-old unsplit source edges"),InspectSplit(Coarse).Y>0);
    const auto BeforeUnion=InspectSplit(Intermediate),AfterUnion=InspectSplit(Mesh);
    TestTrue(TEXT("Both sides of the shared source edge receive its new vertex before union"),BeforeUnion.X>=4);
    TestTrue(TEXT("The final plateau/transition interface retains its emitted split"),AfterUnion.X>=2);
    TestEqual(TEXT("No intermediate edge spans an inserted shared vertex"),BeforeUnion.Y,0);
    TestEqual(TEXT("No final edge spans the required interface vertex"),AfterUnion.Y,0);
    TestEqual(TEXT("Intermediate shared source vertices retain one exact evaluated height"),BeforeUnion.Z,0);
    TestEqual(TEXT("Final shared interface vertices retain one exact evaluated height"),AfterUnion.Z,0);

    // Different accepted local levels are not approximation defects.
    for (auto& Knot : Bundle.Features[FlatId].WaterSurface.Knots) Knot.Z+=.1;
    if (!TestTrue(TEXT("Unequal source-level control builds"),Build())) {AddError(Error);return false;}
    TestTrue(TEXT("Unequal source levels retain independent projected coverage"),Overlap(Mesh)>.4);
    TestEqual(TEXT("Unequal control does not refine or merge source faces or the separate ring"),Count,13);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterSurfaceRegionCertificateTest,
    "Project.World.Water.SurfaceRegionCertificate",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterSurfaceRegionCertificateTest::RunTest(const FString&)
{
    using namespace ProjectWorldWaterApproximationTest;
    auto Feature=Source(TEXT("water/function"),{FVector(0,0,0),FVector(10,0,1)});
    const TArray<FVector2D> Flat{FVector2D(0,0),FVector2D(.2,0),FVector2D(.1,1)};
    const TArray<FVector2D> Crossing{FVector2D(0,0),FVector2D(.2,0),FVector2D(1,1)};
    const TArray<FVector2D> Threshold{FVector2D(.49,0),FVector2D(.5,0),FVector2D(.5,1)};
    double Height=0;
    TestTrue(TEXT("A complete convex region inside one quantized level certifies"),
        ProjectWorldWaterMeshBuilder::ConstantSurfaceZ(Feature,Flat,.1,Height) && Height==0);
    TestFalse(TEXT("A region crossing a height threshold cannot borrow its low corner's value"),
        ProjectWorldWaterMeshBuilder::ConstantSurfaceZ(Feature,Crossing,.1,Height));
    TestFalse(TEXT("Floating uncertainty at a quantization threshold refuses certification"),
        ProjectWorldWaterMeshBuilder::ConstantSurfaceZ(Feature,Threshold,.1,Height));
    Feature.WaterSurface.Knots={FVector(0,0,0),FVector(10,0,0),FVector(10,1,1),FVector(0,1,1)};
    const TArray<FVector2D> Tie{FVector2D(1,.4),FVector2D(2,.4),FVector2D(1.5,.6)};
    TestFalse(TEXT("Nearest-segment competition keeps both possible levels, including ties"),
        ProjectWorldWaterMeshBuilder::ConstantSurfaceZ(Feature,Tie,.1,Height));
    Feature.WaterSurface.Behavior=TEXT("standing");Feature.WaterSurface.LevelMeters=.037;
    TestTrue(TEXT("Standing levels retain their exact unquantized authority"),
        ProjectWorldWaterMeshBuilder::ConstantSurfaceZ(Feature,Flat,.1,Height) && Height==.037);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterSourceApproximationRefusalTest,
    "Project.World.Water.SourceApproximationRefusal",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterSourceApproximationRefusalTest::RunTest(const FString&)
{
    using namespace ProjectWorldWaterApproximationTest;
    FProjectWorldCanonicalBundle Bundle;Bundle.HeightQuantizationMeters=.1;
    FProjectWorldCanonicalCell Cell;Cell.CellId=TEXT("refusal");Cell.Bounds=FVector4d(0,-10,100,10);
    auto Sloping=Source(TEXT("water/slope"),{FVector(0,0,0),FVector(10,0,.1)});
    auto Flat=Source(TEXT("water/flat"),{FVector(0,-2,0),FVector(0,2,0)});
    const auto Fixture=[&](double Start,double End,double Width)
    {
        FMeshDescription Mesh;FStaticMeshAttributes Attributes(Mesh);Attributes.Register();
        Attributes.GetVertexInstanceUVs().SetNumChannels(1);
        Attributes.GetPolygonGroupMaterialSlotNames()[Mesh.CreatePolygonGroup()]=TEXT("Water");
        const double High=(ProjectWorldWaterMeshBuilder::EvaluateSurfaceZ(Sloping,FVector2D(End,0),.1)+.25)*100;
        const FVector P[3]={FVector(Start*100,100,25),FVector(End*100,0,High),FVector(Start*100,-100,25)};
        const FVector Q[3]={FVector(Start*100,50,25),FVector((Start+Width)*100,0,25),FVector(Start*100,-50,25)};
        ProjectWorldWaterMeshBuilder::AppendTriangle(Mesh,P);ProjectWorldWaterMeshBuilder::AppendTriangle(Mesh,Q);
        return Mesh;
    };
    const auto Bytes=[](FMeshDescription& Mesh)
    {TArray<uint8> Data;FMemoryWriter Writer(Data);Mesh.Serialize(Writer);return Data;};
    auto Mesh=Fixture(0,10,.4);auto Original=Bytes(Mesh);
    bool Changed=false;FString Hash,Error;
    const TArray<const FProjectWorldCanonicalFeature*> Sources{&Sloping,&Flat};
    TestFalse(TEXT("Unproven new shared-cell seam vertices refuse"),ProjectWorldWaterSourceApproximation::Refine(
        Mesh,Sources,Bundle,Cell,FVector::ZeroVector,.25,Changed,Hash,Error));
    TestTrue(TEXT("Seam refusal is explicit"),Error.Contains(TEXT("seam context")));
    TestTrue(TEXT("Seam refusal preserves exact mesh bytes and no promoted identity"),
        Bytes(Mesh)==Original && !Changed && Hash.IsEmpty());
    TestFalse(TEXT("Missing source membership refuses"),ProjectWorldWaterSourceApproximation::Refine(
        Mesh,{},Bundle,Cell,FVector::ZeroVector,.25,Changed,Hash,Error));
    TestEqual(TEXT("Membership refusal preserves exact mesh bytes"),Bytes(Mesh),Original);
    Cell.Bounds=FVector4d(-100,-100,2000,100);
    Sloping.WaterSurface.Knots={FVector(10,0,0),FVector(11,0,1),FVector(1010,0,1)};
    Flat.WaterSurface.Knots={FVector(10,-2,0),FVector(10,2,0)};
    Mesh=Fixture(10,1010,.02);Original=Bytes(Mesh);
    TestFalse(TEXT("A source-shaped plateau below the bounded refinement resolution refuses"),
        ProjectWorldWaterSourceApproximation::Refine(Mesh,Sources,Bundle,Cell,FVector::ZeroVector,.25,Changed,Hash,Error));
    TestTrue(TEXT("Bounded refusal identifies its budget"),Error.Contains(TEXT("budget")));
    TestTrue(TEXT("Budget refusal preserves exact mesh bytes and no promoted identity"),
        Bytes(Mesh)==Original && !Changed && Hash.IsEmpty());
    return true;
}
#endif
