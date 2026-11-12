// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldWaterJointBoundary.h"
#include "CompGeom/ExactPredicates.h"

namespace ProjectWorldWaterJointBoundary
{
    bool ValidateIndependent(TConstArrayView<FVector2D> Triangles,FString& Error)
    {
        using UE::Geometry::ExactPredicates::Orient2;
        if (Triangles.Num()%3!=0 || Triangles.Num()>3*16384)
        {Error=TEXT("Independent joint Water needs a complete bounded triangle census.");return false;}
        struct FFace { const FVector2D* P; double Winding; FVector2D Min,Max; };
        TArray<FFace> Faces;
        for (int32 I=0;I<Triangles.Num();I+=3)
        {
            const FVector2D* P=Triangles.GetData()+I;
            if (P[0].ContainsNaN() || P[1].ContainsNaN() || P[2].ContainsNaN())
            {Error=TEXT("Independent joint Water has a nonfinite triangle.");return false;}
            const double W=Orient2(P[0],P[1],P[2]);
            if (!FMath::IsFinite(W) || W==0)
            {Error=TEXT("Independent joint Water has an unknown or degenerate orientation.");return false;}
            Faces.Add({P,W,{FMath::Min3(P[0].X,P[1].X,P[2].X),FMath::Min3(P[0].Y,P[1].Y,P[2].Y)},
                {FMath::Max3(P[0].X,P[1].X,P[2].X),FMath::Max3(P[0].Y,P[1].Y,P[2].Y)}});
        }
        int32 PairScans=0;
        for (int32 I=0;I<Faces.Num();++I) for (int32 J=I+1;J<Faces.Num();++J)
        {
            if (++PairScans>1000000)
            {Error=TEXT("Independent joint Water exceeds its exact pair-scan capacity.");return false;}
            const auto& A=Faces[I]; const auto& B=Faces[J];
            if (A.Max.X<=B.Min.X || B.Max.X<=A.Min.X || A.Max.Y<=B.Min.Y || B.Max.Y<=A.Min.Y) continue;
            const FFace* Pair[2]={&A,&B}; bool Separated=false;
            for (int32 Shape=0;Shape<2 && !Separated;++Shape) for (int32 Edge=0;Edge<3 && !Separated;++Edge)
            {
                bool Outside=true;
                for (int32 Corner=0;Corner<3;++Corner)
                {
                    const double Side=Orient2(Pair[Shape]->P[Edge],Pair[Shape]->P[(Edge+1)%3],Pair[1-Shape]->P[Corner]);
                    if (!FMath::IsFinite(Side))
                    {Error=TEXT("Independent joint Water separation predicate is unknown.");return false;}
                    if (Pair[Shape]->Winding>0 ? Side>0 : Side<0) {Outside=false;break;}
                }
                Separated=Outside;
            }
            if (!Separated)
            {Error=TEXT("Independent joint Water refuses positive-area overlap; an explicit interaction model is required.");return false;}
        }
        return true;
    }
}
