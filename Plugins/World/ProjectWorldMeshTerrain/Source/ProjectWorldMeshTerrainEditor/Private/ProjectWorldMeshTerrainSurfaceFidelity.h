#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldTerrainSurface.h"
struct FProjectWorldCanonicalBundle;

struct FProjectWorldMeshTerrainSurfaceFidelity
{
	int32 ExpectedTriangles = 0;
	int32 MismatchedTriangles = 0;
	double MaximumErrorMeters = 0;
};

namespace ProjectWorldMeshTerrainProducer
{
	bool CompareRefinedSurface(const FProjectWorldCanonicalBundle& Bundle,
		const TArray<FProjectWorldSurfaceTriangle>& CompiledCanonicalTriangles, double ToleranceMeters,
		FProjectWorldMeshTerrainSurfaceFidelity& Result, FString& Error);
}
