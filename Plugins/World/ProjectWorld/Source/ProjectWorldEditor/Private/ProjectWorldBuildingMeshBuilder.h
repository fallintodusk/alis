// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "MeshDescription.h"

struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;

struct FProjectWorldBuildingSettings
{
	int32 GeneratorVersion = 1;
	double MaximumHeightMeters = 300.0;
};

struct FProjectWorldBuildingAdmissionStats
{
	int32 CandidateFragmentCount = 0;
	int32 AcceptedFragmentCount = 0;
	int32 DuplicateFragmentCount = 0;
	int32 ContainedFragmentCount = 0;
	int32 ConflictFragmentCount = 0;
	int32 MalformedFragmentCount = 0;
	int32 AuthoredMaskExcludedFragmentCount = 0;
};

struct FProjectWorldBuildingRejection
{
	FString Reason;
	TArray<FString> FeatureIds;
};

struct FProjectWorldBuildingMeshBuildResult
{
	FMeshDescription MeshDescription;
	FVector ActorOrigin = FVector::ZeroVector;
	FString SemanticDigest;
	int32 TriangleCount = 0;
	FProjectWorldBuildingAdmissionStats Stats;
	TArray<FProjectWorldBuildingRejection> Rejections;
};

namespace ProjectWorldBuildingMeshBuilder
{
	TArray<FString> CellBuildingFeatureIds(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell);

	bool BuildCell(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		const FProjectWorldBuildingSettings& Settings,
		FProjectWorldBuildingMeshBuildResult& OutResult,
		FString& OutError);
}
