// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldWaterSourceApproximation.h"
#include "ProjectWorldWaterMeshBuilder.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "ConstrainedDelaunay2.h"
#include "StaticMeshAttributes.h"
#include "Utilities/ProjectSha256.h"
#include "Algo/Reverse.h"
#include <limits>

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldWaterSourceApproximation,Log,All);

namespace ProjectWorldWaterSourceApproximation
{
    namespace
    {
        struct FFace
        {
            FVector P[3];
            const FProjectWorldCanonicalFeature* Source=nullptr;
            FBox2D Bounds() const
            {FBox2D B(ForceInit);for (const auto& V : P) B+=FVector2D(V);return B;}
        };

        struct FEdge
        {
            const FProjectWorldCanonicalFeature* Source;
            FVector2D A,B;
            FEdge(const FFace& Face,int32 I) : Source(Face.Source),A(Face.P[I]),B(Face.P[(I+1)%3])
            {if (A.X>B.X || (A.X==B.X && A.Y>B.Y)) Swap(A,B);}
            bool operator==(const FEdge& Other) const
            {return Source==Other.Source && A==Other.A && B==Other.B;}
            friend uint32 GetTypeHash(const FEdge& Edge)
            {return HashCombine(PointerHash(Edge.Source),HashCombine(GetTypeHash(Edge.A),GetTypeHash(Edge.B)));}
        };

        double Cross(const FVector& A,const FVector& B,const FVector& C)
        {return FVector3d::CrossProduct(B-A,C-A).Z;}

        bool FindRefinements(const TArray<FFace>& Faces,const FProjectWorldCanonicalBundle& Bundle,
            const FVector& Origin,double Offset,TSet<int32>& Out,FString& Error)
        {
            Out.Reset();
            for (int32 A=0;A<Faces.Num();++A) for (int32 B=A+1;B<Faces.Num();++B)
            {
                const auto& Left=Faces[A];const auto& Right=Faces[B];
                if (Left.Source==Right.Source || !Left.Bounds().Intersect(Right.Bounds())) continue;
                if (Left.P[0].Z==Left.P[1].Z && Left.P[0].Z==Left.P[2].Z &&
                    Right.P[0].Z==Right.P[1].Z && Right.P[0].Z==Right.P[2].Z && Left.P[0].Z==Right.P[0].Z) continue;
                const FProjectWorldSurfaceTriangle LT{{Left.P[0],Left.P[1],Left.P[2]}},
                    RT{{Right.P[0],Right.P[1],Right.P[2]}};
                TArray<FVector2D> Overlap;
                if (!ProjectWorldTerrainSurface::IntersectXY(LT,RT,Overlap,Error)) return false;
                if (Overlap.Num()<3 || ProjectWorldTerrainSurface::Area(Overlap)<=0) continue;
                TArray<FVector2D> Region;
                for (const auto& XY : Overlap)
                    Region.Add(FVector2D(FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,FVector(XY,0)+Origin)));
                double LH=0,RH=0;
                if (!ProjectWorldWaterMeshBuilder::ConstantSurfaceZ(*Left.Source,Region,Bundle.HeightQuantizationMeters,LH) ||
                    !ProjectWorldWaterMeshBuilder::ConstantSurfaceZ(*Right.Source,Region,Bundle.HeightQuantizationMeters,RH)) continue;
                LH+=ProjectWorldWaterMeshBuilder::PresentationOffset(*Left.Source,Offset);
                RH+=ProjectWorldWaterMeshBuilder::PresentationOffset(*Right.Source,Offset);
                if (LH!=RH) continue;
                const float Expected=FVector3f(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(0,0,LH))-Origin).Z;
                for (const int32 I : {A,B})
                    for (const auto& P : Faces[I].P) if (P.Z!=Expected) {Out.Add(I);break;}
            }
            return true;
        }

        bool TriangulateIncident(const FFace& Face,const TMap<FEdge,FVector>& Splits,
            TArray<FFace>& Out,FString& Error)
        {
            using namespace UE::Geometry;
            TArray<FVector> Ring;
            for (int32 I=0;I<3;++I)
            {
                Ring.Add(Face.P[I]);
                if (const auto* Point=Splits.Find(FEdge(Face,I))) Ring.Add(*Point);
            }
            if (Ring.Num()==3) {Out.Add(Face);return true;}
            TArray<FVector2d> XY;for (const auto& P : Ring) XY.Add(FVector2d(P.X,P.Y));
            if (FPolygon2d(XY).SignedArea()<0) {Algo::Reverse(XY);Algo::Reverse(Ring);}
            TConstrainedDelaunay2<double> CDT;
            CDT.FillRule=TConstrainedDelaunay2<double>::EFillRule::Positive;
            CDT.Add(FPolygon2d(XY));
            if (!CDT.Triangulate() || CDT.Vertices!=XY || CDT.Triangles.Num()!=Ring.Num()-2)
            {Error=TEXT("Native Water incident-face triangulation failed its boundary contract.");return false;}
            TMap<TPair<int32,int32>,int32> Edges;
            double Area=0;
            for (const auto& Triangle : CDT.Triangles)
            {
                const int32 IDs[3]={Triangle.A,Triangle.B,Triangle.C};
                FFace Child{{Ring[IDs[0]],Ring[IDs[1]],Ring[IDs[2]]},Face.Source};
                const double Signed=Cross(Child.P[0],Child.P[1],Child.P[2]);
                if (!FMath::IsFinite(Signed) || Signed==0)
                {Error=TEXT("Native Water incident-face triangulation emitted a collapsed face.");return false;}
                if (Signed>0) Swap(Child.P[1],Child.P[2]);
                Area+=FMath::Abs(Signed)*.5;Out.Add(Child);
                for (int32 I=0;I<3;++I)
                {const int32 A=IDs[I],B=IDs[(I+1)%3];++Edges.FindOrAdd({FMath::Min(A,B),FMath::Max(A,B)});}
            }
            for (int32 I=0;I<Ring.Num();++I)
            {
                const int32 J=(I+1)%Ring.Num();const TPair<int32,int32> Edge(FMath::Min(I,J),FMath::Max(I,J));
                if (Edges.FindRef(Edge)!=1)
                {Error=TEXT("Native Water triangulation dropped a required shared-edge split.");return false;}
                Edges.Remove(Edge);
            }
            for (const auto& Edge : Edges) if (Edge.Value!=2)
            {Error=TEXT("Native Water triangulation emitted an unpaired internal edge.");return false;}
            const double Expected=FMath::Abs(Cross(Face.P[0],Face.P[1],Face.P[2]))*.5;
            if (FMath::Abs(Area-Expected)>128*std::numeric_limits<double>::epsilon()*FMath::Max(1.0,Expected)*Ring.Num())
            {Error=TEXT("Native Water incident-face coverage differs from its original face.");return false;}
            return true;
        }
    }

    bool Refine(FMeshDescription& Mesh,TConstArrayView<const FProjectWorldCanonicalFeature*> Sources,
        const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldCanonicalCell& Cell,
        const FVector& Origin,double Offset,bool& OutChanged,FString& OutHash,FString& Error)
    {
        OutChanged=false;OutHash.Reset();Error.Reset();
        if (Sources.Num()!=Mesh.Triangles().Num() || !FMath::IsFinite(Offset) || Offset<0 ||
            !FMath::IsFinite(Bundle.HeightQuantizationMeters) || Bundle.HeightQuantizationMeters<=0 ||
            Cell.Bounds.ContainsNaN() || Cell.Bounds.X>=Cell.Bounds.Z || Cell.Bounds.Y>=Cell.Bounds.W || Origin.ContainsNaN())
        {Error=TEXT("Water source approximation has invalid source membership or offset.");return false;}
        TArray<FFace> Faces;
        const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
        int32 Index=0;
        for (const auto Id : Mesh.Triangles().GetElementIDs())
        {
            FFace Face;Face.Source=Sources[Index++];
            if (!Face.Source || Face.Source->FeatureClass!=TEXT("water") || !Face.Source->WaterSurface.bValid)
            {Error=TEXT("Water source approximation lacks an authenticated source function.");return false;}
            const auto& Surface=Face.Source->WaterSurface;
            if (!ProjectWorldWaterMeshBuilder::ValidateStoredLevel(Bundle,*Face.Source,Origin,Error)) return false;
            if (!FMath::IsFinite(ProjectWorldWaterMeshBuilder::PresentationOffset(*Face.Source,Offset)))
            {Error=TEXT("Water source approximation received an invalid effective height mode.");return false;}
            if ((Surface.Behavior!=TEXT("standing") && Surface.Behavior!=TEXT("flowing")) ||
                (Surface.Behavior==TEXT("standing") && !FMath::IsFinite(Surface.LevelMeters)) ||
                (Surface.Behavior==TEXT("flowing") && Surface.Knots.Num()<2))
            {Error=TEXT("Water source approximation received an invalid surface function.");return false;}
            for (const auto& Knot : Surface.Knots) if (Knot.ContainsNaN())
            {Error=TEXT("Water source approximation received a nonfinite source knot.");return false;}
            const auto Instances=Mesh.GetTriangleVertexInstances(Id);
            for (int32 I=0;I<3;++I)
            {
                Face.P[I]=FVector(Positions[Mesh.GetVertexInstanceVertex(Instances[I])]);
                if (Face.P[I].ContainsNaN()) {Error=TEXT("Water source approximation received a nonfinite vertex.");return false;}
            }
            if (Cross(Face.P[0],Face.P[1],Face.P[2])>=0)
            {Error=TEXT("Water source approximation received a degenerate or nonclockwise face.");return false;}
            Faces.Add(Face);
        }
        const FVector Min=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(Cell.Bounds.X,Cell.Bounds.W,0))-Origin;
        const FVector Max=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(Cell.Bounds.Z,Cell.Bounds.Y,0))-Origin;
        const FVector2f BorderMin(FVector3f(Min).X,FVector3f(Min).Y),BorderMax(FVector3f(Max).X,FVector3f(Max).Y);
        constexpr int32 MaxRounds=4,MaxFaces=16384;
        int32 Rounds=0;
        for (;;)
        {
            TSet<int32> Selected;
            if (!FindRefinements(Faces,Bundle,Origin,Offset,Selected,Error)) return false;
            if (Selected.IsEmpty()) break;
            if (Rounds++>=MaxRounds || Faces.Num()+3*Selected.Num()>MaxFaces)
            {Error=TEXT("Certified Water source approximation exceeded its bounded refinement budget.");return false;}
            TMap<FEdge,FVector> Splits;
            for (const int32 I : Selected) for (int32 E=0;E<3;++E)
            {
                const FEdge Edge(Faces[I],E);
                if (Splits.Contains(Edge)) continue;
                const FVector2d Exact=(Edge.A+Edge.B)*.5;
                const FVector2f Emitted(Exact);
                // Off-edge rounding creates a shoreline change or a T-junction.
                if (FVector2d(Emitted)!=Exact || FVector2d(Emitted)==Edge.A || FVector2d(Emitted)==Edge.B)
                {Error=TEXT("Water source midpoint cannot preserve the exact emitted edge at float precision.");return false;}
                if (Emitted.X==BorderMin.X || Emitted.X==BorderMax.X || Emitted.Y==BorderMin.Y || Emitted.Y==BorderMax.Y)
                {Error=TEXT("Water source refinement requires authenticated shared-cell seam context.");return false;}
                FVector Point(Emitted.X,Emitted.Y,0);
                const FVector Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,Point+Origin);
                const double Z=ProjectWorldWaterMeshBuilder::EvaluateSurfaceZ(*Edge.Source,FVector2D(Canonical),Bundle.HeightQuantizationMeters);
                const double EffectiveOffset=ProjectWorldWaterMeshBuilder::PresentationOffset(*Edge.Source,Offset);
                Point.Z=FVector3f(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(Canonical.X,Canonical.Y,Z+EffectiveOffset))-Origin).Z;
                if (Point.ContainsNaN()) {Error=TEXT("Water source midpoint height is nonfinite.");return false;}
                Splits.Add(Edge,Point);
            }
            TArray<FFace> Next;
            for (int32 I=0;I<Faces.Num();++I)
            {
                const auto& Face=Faces[I];
                if (!Selected.Contains(I))
                {if (!TriangulateIncident(Face,Splits,Next,Error)) return false;continue;}
                const FVector A=Face.P[0],B=Face.P[1],C=Face.P[2];
                const FVector AB=Splits.FindChecked(FEdge(Face,0)),BC=Splits.FindChecked(FEdge(Face,1)),CA=Splits.FindChecked(FEdge(Face,2));
                Next.Add(FFace{{A,AB,CA},Face.Source});Next.Add(FFace{{AB,B,BC},Face.Source});
                Next.Add(FFace{{CA,BC,C},Face.Source});Next.Add(FFace{{AB,BC,CA},Face.Source});
            }
            if (Next.Num()>MaxFaces) {Error=TEXT("Water incident-face refinement exceeded its face budget.");return false;}
            Faces=MoveTemp(Next);
        }
        if (Rounds==0) return true;
        FMeshDescription Candidate;FStaticMeshAttributes Attributes(Candidate);Attributes.Register();
        Attributes.GetVertexInstanceUVs().SetNumChannels(1);
        Attributes.GetPolygonGroupMaterialSlotNames()[Candidate.CreatePolygonGroup()]=TEXT("Water");
        FString Identity;
        for (const auto& Face : Faces)
        {
            ProjectWorldWaterMeshBuilder::AppendTriangle(Candidate,Face.P);
            for (const auto& P : Face.P) Identity+=FString::Printf(TEXT("%.9g,%.9g,%.9g|"),P.X,P.Y,P.Z);
        }
        FTCHARToUTF8 Utf8(*Identity);TArray<uint8> Bytes;Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length());
        if (!FProjectSha256::HashBuffer(Bytes,OutHash)) {Error=TEXT("Cannot authenticate refined Water geometry.");return false;}
        UE_LOG(LogProjectWorldWaterSourceApproximation,Display,TEXT("[Refine] Cell=%s rounds=%d initial_faces=%d final_faces=%d"),
            *Cell.CellId,Rounds,Sources.Num(),Faces.Num());
        Mesh=MoveTemp(Candidate);OutChanged=true;return true;
    }
}
