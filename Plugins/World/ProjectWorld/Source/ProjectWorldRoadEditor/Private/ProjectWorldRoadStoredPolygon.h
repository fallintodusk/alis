// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
struct FProjectWorldSurfaceTriangle;
namespace ProjectWorldRoadStoredPolygon
{
	struct FVertexBinding
	{
		FVector Source;
		FVector Stored;
		double Gradient = 0;
		double EvaluationRoundoff = 0;
	};
	using FVertexMap = TMap<FVector2f, FVertexBinding>;
	// Convex local Unreal-centimetre polygon; storage conversion never invents a vertical height face.
	bool Store(const TArray<FVector>& Input, const FProjectWorldSurfaceTriangle& Plane, FVertexMap& Vertices, TArray<FVector>& Positions,
		TArray<FIntVector>& Faces, FString& Error);
	// Admit exact collapsed internal splits only with a complete original-domain conversion certificate.
	bool StoreGroup(const TArray<FProjectWorldSurfaceTriangle>& Pieces, const TArray<FVector2D>& Domain,
		FVertexMap& Vertices, TArray<FProjectWorldSurfaceTriangle>& Faces, FString& Error, const FBox2D* ClipBounds = nullptr);
}
