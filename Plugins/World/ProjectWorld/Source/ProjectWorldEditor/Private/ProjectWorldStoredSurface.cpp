// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldStoredSurface.h"

namespace ProjectWorldStoredSurface
{
	bool Reframe(const FProjectWorldStoredSurface& Input,const FVector& Origin,
		TArray<FProjectWorldSurfaceTriangle>& Out,FString& Error)
	{
		Out.Reset(); Error.Reset();
		if (Input.Origin.ContainsNaN() || Origin.ContainsNaN() || Input.Triangles.Num()>4096)
		{ Error=TEXT("Native surface requires finite origins and a bounded stored triangle census."); return false; }
		TArray<FProjectWorldSurfaceTriangle> Candidate;
		for (auto T : Input.Triangles)
		{
			for (auto& P : T.Points)
			{
				if (P.ContainsNaN() || FVector(FVector3f(P))!=P)
				{ Error=TEXT("Native surface input is not actual finite stored float geometry."); return false; }
				const FVector World=P+Input.Origin,Local=World-Origin;
				if (World.ContainsNaN() || Local.ContainsNaN() || Local+Origin!=World)
				{ Error=TEXT("Native surface frame conversion cannot preserve actual world positions."); return false; }
				P=Local;
			}
			Candidate.Add(T);
		}
		Out=MoveTemp(Candidate); return true;
	}
}
