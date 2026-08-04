// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;

enum class EProjectWorldRealizationMode : uint8
{
	Validate,
	Apply,
	Delete
};

struct FProjectWorldRealizationRequest
{
	FString CompileResultPath;
	FString MapPackagePath;
	FString ResultPath;
	EProjectWorldRealizationMode Mode = EProjectWorldRealizationMode::Validate;
	bool bRequireLandscapeCompatible = false;
	int32 MaxRoadFeatures = 1;
	int32 MaxBuildingFeatures = 4;
};

struct FProjectWorldRealizationResult
{
	FString Status = TEXT("rejected");
	FString OperationId;
	FString ErrorCode;
	FString Message;
	FString Detail;
	FString InputHash;
	FString CompileResultHash;
	FString GridId;
	FString CanonicalCrs;
	FString CoordinateTransform;
	FString MapPackagePath;
	FString LandscapeReason;
	FString SemanticFingerprint;
	double CoordinateRoundTripErrorMeters = 0.0;
	double GeoReferencingPlacementErrorMeters = 0.0;
	double VerticalOriginMeters = 0.0;
	double DurationSeconds = 0.0;
	int64 GeneratedSourceBytes = 0;
	int32 VerifiedOutputCount = 0;
	int32 CreatedActorCount = 0;
	int32 RemovedActorCount = 0;
	int32 PreservedActorCount = 0;
	int32 TerrainSectionCount = 0;
	int32 LandscapeComponentCount = 0;
	int32 UpdatedLandscapeComponentCount = 0;
	int32 RoadSectionCount = 0;
	int32 BuildingSectionCount = 0;
	int32 GeoReferencingProbePointCount = 0;
	int32 CrossCellRoadExpectedFragmentCount = 0;
	int32 CrossCellRoadRealizedFragmentCount = 0;
	int32 CrossCellRoadSharedBoundaryPointCount = 0;
	FString CrossCellRoadFeatureId;
	bool bWorldPartition = false;
	bool bLandscapeCompatible = false;
	bool bGeoReferencingProbed = false;
	bool bAuthoredCorrectionLayerPreserved = false;
	FString AuthoredCorrectionLayerGuid;
	FString AuthoredCorrectionLayerHash;

	int32 ExitCode() const;
};

class PROJECTWORLDEDITOR_API FProjectWorldRealizationService final
{
public:
	static int32 Run(
		const FProjectWorldRealizationRequest& Request,
		FProjectWorldRealizationResult& OutResult);

	static bool WriteResult(
		const FProjectWorldRealizationRequest& Request,
		const FProjectWorldRealizationResult& Result);
};
