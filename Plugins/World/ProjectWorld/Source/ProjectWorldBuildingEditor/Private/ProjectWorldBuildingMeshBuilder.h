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
    struct FEmission { FString FeatureId; int32 FirstTriangle = 0; int32 TriangleCount = 0; };
	FMeshDescription MeshDescription;
	FVector ActorOrigin = FVector::ZeroVector;
	FString SemanticDigest;
	int32 TriangleCount = 0;
	FProjectWorldBuildingAdmissionStats Stats;
	TArray<FProjectWorldBuildingRejection> Rejections;
    TArray<FEmission> Emissions;
    TArray<FString> AssemblyExcludedFeatures;
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
		FString& OutError,
        const TSet<FString>* AssemblySelection = nullptr);
}
