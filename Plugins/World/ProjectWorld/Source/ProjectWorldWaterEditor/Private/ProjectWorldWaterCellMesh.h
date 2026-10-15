// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldWaterMeshBuilder.h"

namespace ProjectWorldWaterCellMesh
{
	bool Build(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalCell& Cell,
		const TMap<FString, FProjectWorldPreparedWaterSurface>& PreparedSurfaces,
		const TArray<FString>& FeatureIds, double SurfaceOffsetMeters,
		FMeshDescription& OutDescription, FVector& OutOrigin, FString& OutSemantic,
		int32& OutTriangles, FString& OutError);
}
