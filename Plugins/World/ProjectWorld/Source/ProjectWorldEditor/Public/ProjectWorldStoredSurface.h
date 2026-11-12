// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldTerrainSurface.h"

// Unreal centimetres. Triangles contain actual native float positions in Origin's frame.
struct FProjectWorldStoredSurface
{
	FVector Origin=FVector::ZeroVector;
	TArray<FProjectWorldSurfaceTriangle> Triangles;
};

namespace ProjectWorldStoredSurface
{
	// Comparison coordinates remain double precision; native world positions must round-trip exactly.
	PROJECTWORLDEDITOR_API bool Reframe(const FProjectWorldStoredSurface& Input,const FVector& Origin,
		TArray<FProjectWorldSurfaceTriangle>& Out,FString& Error);
}

