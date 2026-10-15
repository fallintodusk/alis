// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldWaterHorizontalComposition.h"
#include "ProjectWorldWaterMeshBuilder.h"
#include "ProjectWorldTerrainSurface.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"
#include "StaticMeshAttributes.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterHorizontalPiecesTest,
    "Project.World.Water.HorizontalPieces",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterHorizontalPiecesTest::RunTest(const FString&)
{
    using namespace UE::Geometry;
    const TArray<FVector4d> Strips{FVector4d(0,0,40,100),FVector4d(60,0,100,100),
        FVector4d(40,0,60,40),FVector4d(40,60,60,100)};
    const auto Fixture=[&](const FAxisAlignedBox2d& Bounds)
    {
        FMeshDescription Mesh;
        FStaticMeshAttributes Attributes(Mesh); Attributes.Register();
        Attributes.GetVertexInstanceUVs().SetNumChannels(1);
        Attributes.GetPolygonGroupMaterialSlotNames()[Mesh.CreatePolygonGroup()]=TEXT("Water");
        for (int32 Copy=0;Copy<2;++Copy)
            for (const auto& R : Strips)
            {
                FPolygon2d Polygon({FVector2d(R.X,R.Y),FVector2d(R.Z,R.Y),FVector2d(R.Z,R.W),FVector2d(R.X,R.W)});
                Polygon.ClipConvex(Bounds);
                for (int32 I=1;I+1<Polygon.VertexCount();++I)
                {
                    FVector Points[3]={FVector(Polygon[0].X,Polygon[0].Y,25),
                        FVector(Polygon[I+1].X,Polygon[I+1].Y,25),FVector(Polygon[I].X,Polygon[I].Y,25)};
                    if (FVector::CrossProduct(Points[1]-Points[0],Points[2]-Points[0]).Z!=0)
                        ProjectWorldWaterMeshBuilder::AppendTriangle(Mesh,Points);
                }
            }
        const FVector Slope[3]={FVector(200,0,25),FVector(200,100,25),FVector(300,0,50)};
        ProjectWorldWaterMeshBuilder::AppendTriangle(Mesh,Slope);
        return Mesh;
    };
    const auto Bytes=[](FMeshDescription& Mesh)
    {TArray<uint8> Result;FMemoryWriter Writer(Result);Mesh.Serialize(Writer);return Result;};
    auto Mesh=Fixture(FAxisAlignedBox2d(FVector2d(0,0),FVector2d(100,100)));
    bool Changed=false;FString Hash,Error;
    if (!TestTrue(TEXT("Native final-piece union succeeds"),
        ProjectWorldWaterHorizontalComposition::Compose(Mesh,Changed,Hash,Error))) {AddError(Error);return false;}
    TestTrue(TEXT("Positive overlap actually exercises final-piece replacement"),Changed && Hash.Len()==64);
    double Area=0;int32 Slopes=0,HoleHits=0;
    const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
    for (const auto Id : Mesh.Triangles().GetElementIDs())
    {
        FProjectWorldSurfaceTriangle Triangle;const auto Instances=Mesh.GetTriangleVertexInstances(Id);
        for (int32 I=0;I<3;++I) Triangle.Points[I]=FVector(Positions[Mesh.GetVertexInstanceVertex(Instances[I])]);
        const auto& P=Triangle.Points;
        const double Cross=FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).Z;
        TestTrue(TEXT("Every final face retains clockwise front-face winding"),Cross<0);
        Area+=FMath::Abs(Cross)*.5;
        if (P[0].Z!=P[1].Z || P[0].Z!=P[2].Z)
        {
            ++Slopes;
            TestTrue(TEXT("In-cell sloping geometry remains exact"),P[0]==FVector(200,0,25) &&
                P[1]==FVector(200,100,25) && P[2]==FVector(300,0,50));
        }
        // The central hole cannot be certified by a total-area check alone.
        const FVector2d A(P[0].X,P[0].Y),B(P[1].X,P[1].Y),C(P[2].X,P[2].Y),Q(50,50);
        const double S0=FVector2d::CrossProduct(B-A,Q-A),S1=FVector2d::CrossProduct(C-B,Q-B),
            S2=FVector2d::CrossProduct(A-C,Q-C);
        HoleHits+=(S0<=0 && S1<=0 && S2<=0);
    }
    TestEqual(TEXT("One true sloped face is retained"),Slopes,1);
    TestEqual(TEXT("The 20 by 20 cm hole remains uncovered"),HoleHits,0);
    TestTrue(TEXT("Float replacement coverage equals ring plus slope"),FMath::Abs(Area-14600)<1e-5);
    auto Once=Bytes(Mesh);
    TestTrue(TEXT("Repeated composition succeeds"),ProjectWorldWaterHorizontalComposition::Compose(Mesh,Changed,Hash,Error));
    TestFalse(TEXT("Already disjoint geometry does not churn"),Changed);
    TestEqual(TEXT("No-op keeps exact native mesh bytes"),Bytes(Mesh),Once);
    TArray<float> Seam[2];
    for (int32 Side=0;Side<2;++Side)
    {
        auto Half=Fixture(FAxisAlignedBox2d(FVector2d(Side==0?0:50,0),FVector2d(Side==0?50:100,100)));
        if (!ProjectWorldWaterHorizontalComposition::Compose(Half,Changed,Hash,Error)) {AddError(Error);return false;}
        const auto Points=FStaticMeshConstAttributes(Half).GetVertexPositions();
        for (const auto Id : Half.Vertices().GetElementIDs())
            if (Points[Id].X==50) Seam[Side].AddUnique(Points[Id].Y);
        Seam[Side].Sort();
    }
    TestTrue(TEXT("A visible shared border is exercised"),Seam[0].Num()>=4);
    TestEqual(TEXT("Both sides preserve the exact hole/border vertices"),Seam[0],Seam[1]);
    auto Bad=Fixture(FAxisAlignedBox2d(FVector2d(0,0),FVector2d(100,100)));
    auto BadPositions=FStaticMeshAttributes(Bad).GetVertexPositions();
    BadPositions[FVertexID(0)].X=std::numeric_limits<float>::quiet_NaN();
    const auto Rejected=Bytes(Bad);
    TestFalse(TEXT("Invalid final input refuses"),ProjectWorldWaterHorizontalComposition::Compose(Bad,Changed,Hash,Error));
    TestEqual(TEXT("Refusal preserves every source mesh byte"),Bytes(Bad),Rejected);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterHorizontalFloatPrecisionTest,
    "Project.World.Water.HorizontalFloatPrecision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterHorizontalFloatPrecisionTest::RunTest(const FString&)
{
    FMeshDescription Mesh;FStaticMeshAttributes Attributes(Mesh);Attributes.Register();
    Attributes.GetVertexInstanceUVs().SetNumChannels(1);
    Attributes.GetPolygonGroupMaterialSlotNames()[Mesh.CreatePolygonGroup()]=TEXT("Water");
    const FVector Points[3]={FVector(0,0,25),FVector(10000,9999,25),FVector(9999,9998,25)};
    const FVector3f A(Points[0]),B(Points[1]),C(Points[2]);
    TestEqual(TEXT("Known-bad float products erase this valid thin face"),FVector3f::CrossProduct(B-A,C-A).Z,0.f);
    ProjectWorldWaterMeshBuilder::AppendTriangle(Mesh,Points);
    ProjectWorldWaterMeshBuilder::AppendTriangle(Mesh,Points);
    bool Changed=false;FString Hash,Error;
    if (!TestTrue(TEXT("Exact native union retains valid emitted float positions"),
        ProjectWorldWaterHorizontalComposition::Compose(Mesh,Changed,Hash,Error))) {AddError(Error);return false;}
    TestTrue(TEXT("Actual duplicate replacement is exercised"),Changed);
    TestEqual(TEXT("Two valid coincident faces become one"),Mesh.Triangles().Num(),1);
    const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
    double Area=0;
    for (const auto Id : Mesh.Triangles().GetElementIDs())
    {
        FVector3d P[3];const auto Instances=Mesh.GetTriangleVertexInstances(Id);
        for (int32 I=0;I<3;++I) P[I]=FVector3d(Positions[Mesh.GetVertexInstanceVertex(Instances[I])]);
        Area+=FMath::Abs(FVector3d::CrossProduct(P[1]-P[0],P[2]-P[0]).Z)*.5;
    }
    TestEqual(TEXT("Independent double measurement retains the exact 0.5 cm2 coverage"),Area,.5);
    return true;
}
#endif
