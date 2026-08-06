// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldRuntimeBudgets
{
	int64 GeneratedSourceBytes = 0;
	int64 ProceduralMeshBufferBytes = 0;
	int32 GeneratedActorCount = 0;
	int32 MeshSectionDrawCallUpperBound = 0;
	double RegenerationSeconds = 0.0;
	double P95FrameTimeMilliseconds = 0.0;
};

struct FProjectWorldRuntimeProfile
{
	FString ProfileId;
	FString ProfileHash;
	FString GridId;
	FString RouteId;
	FString RouteFeatureId;
	double EndpointInsetMeters = 0.0;
	double NavigationPaddingMeters = 0.0;
	double NavigationHeightMeters = 0.0;
	FString NanitePolicy;
	FString InstancingPolicy;
	FString HlodPolicy;
	FProjectWorldRuntimeBudgets Budgets;
};

namespace ProjectWorldRuntimeProfile
{
	bool Load(
		const FString& Path,
		FProjectWorldRuntimeProfile& OutProfile,
		FString& OutErrorCode,
		FString& OutError);
}
