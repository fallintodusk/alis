// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedMesh.h"
#include "CompGeom/ExactPredicates.h"

namespace ProjectWorldConstructedMesh
{
	namespace
	{
		double Side(const FVector& A,const FVector& B,const FVector& P)
		{ return UE::Geometry::ExactPredicates::Orient2(FVector2D(A),FVector2D(B),FVector2D(P)); }
		bool Separates(const FProjectWorldSurfaceTriangle& A,const FProjectWorldSurfaceTriangle& B)
		{
			const double Direction=Side(A.Points[0],A.Points[1],A.Points[2]);
			for (int32 I=0; I<3; ++I)
			{
				bool Outside=true;
				for (const auto& P : B.Points) Outside &= Side(A.Points[I],A.Points[(I+1)%3],P)*Direction<=0;
				if (Outside) return true;
			}
			return false;
		}
		bool Inside(const FProjectWorldSurfaceTriangle& A,const FVector& P)
		{
			const double Direction=Side(A.Points[0],A.Points[1],A.Points[2]);
			for (int32 I=0; I<3; ++I) if (Side(A.Points[I],A.Points[(I+1)%3],P)*Direction<0) return false;
			return true;
		}
		bool Join(const FProjectWorldSurfaceTriangle& A,const FProjectWorldSurfaceTriangle& B,double Limit,FString& Error)
		{
			for (const auto& P : A.Points) if (Inside(B,P))
			{
				double Z;
				if (!ProjectWorldTerrainSurface::Height(B,FVector2D(P),Z,Error)) return false;
				if (FMath::Abs(P.Z-Z)>Limit)
				{ Error=TEXT("Constructed stored edge or T junction exceeds the explicit join-step limit."); return false; }
			}
			return true;
		}
	}
	bool ValidateStored(const TArray<FProjectWorldSurfaceTriangle>& Body,
		double MaximumJoinStepCentimeters,FString& Error)
	{
		Error.Reset();
		if (!FMath::IsFinite(MaximumJoinStepCentimeters) || MaximumJoinStepCentimeters<0)
		{ Error=TEXT("Stored body relationships require a finite explicit join limit."); return false; }
		TArray<FBox2D> Bounds;
		auto Check = [&](const TArray<FProjectWorldSurfaceTriangle>& Faces)
		{
			for (const auto& T : Faces)
			{
				double Z;
				if (!ProjectWorldTerrainSurface::Height(T,FVector2D(T.Points[0]),Z,Error)) return false;
				if (Side(T.Points[0],T.Points[1],T.Points[2])==0)
				{ Error=TEXT("Stored body relationship contains a collapsed face."); return false; }
				FBox2D Box(ForceInit); for (const auto& P : T.Points) Box+=FVector2D(P); Bounds.Add(Box);
			}
			return true;
		};
		if (!Check(Body)) return false;
		for (int32 I=0; I<Body.Num(); ++I) for (int32 J=I+1; J<Body.Num(); ++J)
		{
			if (!Bounds[I].Intersect(Bounds[J])) continue;
			const auto& A=Body[I]; const auto& B=Body[J];
			if (!Separates(A,B) && !Separates(B,A))
			{
				Error=FString::Printf(TEXT("Constructed stored positive overlap: body=%d other=%d."),I,J);
				for (const auto* T : {&A,&B}) for (const auto& P : T->Points)
					Error+=FString::Printf(TEXT(" [%.17g,%.17g,%.17g]"),P.X,P.Y,P.Z);
				return false;
			}
			if (!Join(A,B,MaximumJoinStepCentimeters,Error) || !Join(B,A,MaximumJoinStepCentimeters,Error)) return false;
		}
		return true;
	}
}
