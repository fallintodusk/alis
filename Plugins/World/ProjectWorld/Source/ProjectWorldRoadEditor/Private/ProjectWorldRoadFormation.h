// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "ProjectWorldTerrainSurface.h"
struct FProjectWorldCanonicalFeature;

namespace ProjectWorldRoadFormation
{
	struct FSegment {FVector2D A,B,SideA,SideB;};
	TStaticArray<FProjectWorldSurfaceTriangle,2> Faces(const FSegment& Segment,double Height);
	bool ForCell(const FProjectWorldCanonicalFeature& Feature,const FString& CellId,double MaximumSegmentLength,
		double TangentTolerance,TArray<FSegment>& Out,FString& Consumed,FString& Error);
}
