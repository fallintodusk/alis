// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldWaterHorizontalComposition.h"
#include "ProjectWorldWaterMeshBuilder.h"
#include "ConstrainedDelaunay2.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "StaticMeshAttributes.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldWaterHorizontalComposition
{
    bool Compose(FMeshDescription& Mesh, bool& OutChanged, FString& OutGeometryHash, FString& OutError)
    {
        using namespace UE::Geometry;
        OutChanged=false; OutGeometryHash.Reset(); OutError.Reset();
        const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
        struct FPlane
        {
            TArray<FTriangleID> Triangles;
            TArray<FGeneralPolygon2d> Polygons;
            TArray<FVector> Replacement;
            double Area=0;
        };
        TMap<float,FPlane> Planes;
        for (const FTriangleID Id : Mesh.Triangles().GetElementIDs())
        {
            const auto Instances=Mesh.GetTriangleVertexInstances(Id);
            const FVector3f A=Positions[Mesh.GetVertexInstanceVertex(Instances[0])];
            const FVector3f B=Positions[Mesh.GetVertexInstanceVertex(Instances[1])];
            const FVector3f C=Positions[Mesh.GetVertexInstanceVertex(Instances[2])];
            if (A.ContainsNaN() || B.ContainsNaN() || C.ContainsNaN())
            {OutError=TEXT("Water composition received a nonfinite emitted vertex.");return false;}
            // Exact final float equality preserves unequal levels and true slopes.
            if (A.Z!=B.Z || A.Z!=C.Z) continue;
            FPolygon2d Polygon({FVector2d(A.X,A.Y),FVector2d(B.X,B.Y),FVector2d(C.X,C.Y)});
            const double Area=FMath::Abs(Polygon.SignedArea());
            if (Area<=0) {OutError=TEXT("Water composition received a degenerate horizontal face.");return false;}
            if (Polygon.SignedArea()<0) Polygon.Reverse();
            auto& Plane=Planes.FindOrAdd(A.Z);
            Plane.Triangles.Add(Id); Plane.Polygons.Add(FGeneralPolygon2d(MoveTemp(Polygon))); Plane.Area+=Area;
        }
        TArray<float> Heights; Planes.GetKeys(Heights); Heights.Sort();
        TSet<FTriangleID> Removed;
        for (const float Z : Heights)
        {
            auto& Plane=Planes.FindChecked(Z);
            if (Plane.Triangles.Num()<2) continue;
            TArray<FGeneralPolygon2d> United;
            if (!PolygonsUnion(Plane.Polygons,United,false))
            {OutError=TEXT("Native horizontal Water polygon union failed.");return false;}
            double Area=0;
            for (const auto& Polygon : United) Area+=FMath::Abs(Polygon.SignedArea());
            if (!FMath::IsFinite(Area) || Area<=0 || Area>Plane.Area+1e-5)
            {OutError=TEXT("Native horizontal Water union returned invalid coverage.");return false;}
            // Boolean roundoff without positive overlap must not churn geometry.
            if (Plane.Area-Area<=1e-5) continue;
            double EmittedArea=0,NativeArea=0,FloatAreaErrorBound=0;
            for (const auto& Polygon : United)
            {
                TConstrainedDelaunay2<double> Triangulation;
                Triangulation.FillRule=TConstrainedDelaunay2<double>::EFillRule::Positive;
                Triangulation.Add(Polygon);
                if (!Triangulation.Triangulate() || Triangulation.Triangles.IsEmpty())
                {OutError=TEXT("Native horizontal Water triangulation failed.");return false;}
                const auto& Vertices=Triangulation.Vertices;
                for (const auto& Triangle : Triangulation.Triangles)
                {
                    FVector Points[3]={FVector(Vertices[Triangle.A].X,Vertices[Triangle.A].Y,Z),
                        FVector(Vertices[Triangle.B].X,Vertices[Triangle.B].Y,Z),
                        FVector(Vertices[Triangle.C].X,Vertices[Triangle.C].Y,Z)};
                    const FVector3f A(Points[0]),B(Points[1]),C(Points[2]);
                    // Measure emitted float positions using double arithmetic.
                    // Float products can cancel on long, thin Water triangles.
                    const double Cross=FVector3d::CrossProduct(FVector3d(B)-FVector3d(A),FVector3d(C)-FVector3d(A)).Z;
                    const FVector3d U=Points[1]-Points[0],V=Points[2]-Points[0];
                    const double NativeCross=FVector3d::CrossProduct(U,V).Z;
                    if (!FMath::IsFinite(Cross) || !FMath::IsFinite(NativeCross) || Cross*NativeCross<=0)
                    {OutError=TEXT("Horizontal Water composition collapses at emitted float precision.");return false;}
                    const FVector3d ErrorA=FVector3d(A)-Points[0];
                    const FVector3d ErrorU=(FVector3d(B)-Points[1])-ErrorA;
                    const FVector3d ErrorV=(FVector3d(C)-Points[2])-ErrorA;
                    // Bound only area drift caused by rounding these actual vertices:
                    // cross(U+dU,V+dV)-cross(U,V), using the triangle inequality.
                    FloatAreaErrorBound+=(U.Size()*ErrorV.Size()+V.Size()*ErrorU.Size()+ErrorU.Size()*ErrorV.Size())*.5;
                    NativeArea+=FMath::Abs(NativeCross)*.5;
                    EmittedArea+=FMath::Abs(Cross)*.5;
                    if (Cross>0) Swap(Points[1],Points[2]);
                    Plane.Replacement.Append(Points,3);
                }
            }
            // Native tessellation must cover its union. Float emission gets only
            // its derived coordinate-rounding bound, never a terrain quality limit.
            const double NumericTolerance=FMath::Max(1e-5,Area*1e-6);
            if (FMath::Abs(NativeArea-Area)>NumericTolerance ||
                FMath::Abs(EmittedArea-NativeArea)>FloatAreaErrorBound+NumericTolerance)
            {OutError=FString::Printf(TEXT("Emitted horizontal Water coverage differs from its native union: Z=%.9g union_cm2=%.17g native_cm2=%.17g emitted_cm2=%.17g float_bound_cm2=%.17g."),
                Z,Area,NativeArea,EmittedArea,FloatAreaErrorBound);return false;}
            Removed.Append(Plane.Triangles);
        }
        if (Removed.IsEmpty()) return true;
        FMeshDescription Candidate;
        FStaticMeshAttributes Attributes(Candidate); Attributes.Register();
        Attributes.GetVertexInstanceUVs().SetNumChannels(1);
        const auto Group=Candidate.CreatePolygonGroup();
        Attributes.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Water");
        for (const auto Id : Mesh.Triangles().GetElementIDs())
        {
            if (Removed.Contains(Id)) continue;
            const auto Instances=Mesh.GetTriangleVertexInstances(Id);
            FVector Points[3];
            for (int32 I=0;I<3;++I) Points[I]=FVector(Positions[Mesh.GetVertexInstanceVertex(Instances[I])]);
            ProjectWorldWaterMeshBuilder::AppendTriangle(Candidate,Points);
        }
        for (const float Z : Heights)
        {
            const auto& Points=Planes.FindChecked(Z).Replacement;
            for (int32 I=0;I<Points.Num();I+=3)
                ProjectWorldWaterMeshBuilder::AppendTriangle(Candidate,&Points[I]);
        }
        FString Geometry;
        for (const auto Id : Candidate.Vertices().GetElementIDs())
        {
            const FVector3f Point=Attributes.GetVertexPositions()[Id];
            Geometry+=FString::Printf(TEXT("%.9g,%.9g,%.9g|"),Point.X,Point.Y,Point.Z);
        }
        FTCHARToUTF8 Utf8(*Geometry); TArray<uint8> Bytes;
        Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length());
        if (!FProjectSha256::HashBuffer(Bytes,OutGeometryHash))
        {OutError=TEXT("Cannot authenticate horizontal Water composition.");return false;}
        Mesh=MoveTemp(Candidate); OutChanged=true;
        return true;
    }
}
