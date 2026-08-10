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
	FString PresentationProfilePath;
	FString RuntimeProfilePath;
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
	FString PresentationProfileId;
	FString PresentationProfileHash;
	FString RuntimeProfileId;
	FString RuntimeProfileHash;
	FString RuntimeRouteId;
	FString RuntimeRouteFeatureId;
	FString NanitePolicy;
	FString InstancingPolicy;
	FString HlodPolicy;
	FString WorldDataPluginName;
	FString GridId;
	FString CanonicalCrs;
	FString CoordinateTransform;
	FString MapPackagePath;
	FString LandscapeReason;
	FString SemanticFingerprint;
	double CoordinateRoundTripErrorMeters = 0.0;
	double GeoReferencingPlacementErrorMeters = 0.0;
	double VerticalOriginMeters = 0.0;
	FVector2D LatticeOriginMeters = FVector2D::ZeroVector;
	FVector2D EngineGeoreferenceOriginMeters = FVector2D::ZeroVector;
	double DurationSeconds = 0.0;
	int64 GeneratedSourceBytes = 0;
	int64 ProceduralMeshBufferBytes = 0;
	int32 VerifiedOutputCount = 0;
	int32 CreatedActorCount = 0;
	int32 UpdatedActorCount = 0;
	int32 RemovedActorCount = 0;
	int32 PreservedActorCount = 0;
	int32 TerrainSectionCount = 0;
	int32 LandscapeComponentCount = 0;
	int32 UpdatedLandscapeComponentCount = 0;
	int32 RoadSectionCount = 0;
	int32 BuildingSectionCount = 0;
	int32 PresentationActorCount = 0;
	int32 CaptureViewpointCount = 0;
	int32 GeoReferencingProbePointCount = 0;
	int32 CrossCellRoadExpectedFragmentCount = 0;
	int32 CrossCellRoadRealizedFragmentCount = 0;
	int32 CrossCellRoadSharedBoundaryPointCount = 0;
	int32 GeneratedActorCount = 0;
	int32 SpatiallyLoadedActorCount = 0;
	int32 RuntimeRouteSpatialActorCount = 0;
	int32 RuntimeAlwaysLoadedActorCount = 0;
	int32 ProceduralMeshSectionDrawCallUpperBound = 0;
	int32 HlodProxyActorCount = 0;
	int32 RuntimeCollisionProbeCount = 0;
	int32 RuntimeCollisionOrientationProbeCount = 0;
	double RuntimeRouteVolumeYawDegrees = 0.0;
	double RuntimeNavigationPathMeters = 0.0;
	double RuntimeP95FrameTimeBudgetMilliseconds = 0.0;
	FString CrossCellRoadFeatureId;
	bool bWorldPartition = false;
	bool bLandscapeCompatible = false;
	bool bGeoReferencingProbed = false;
	bool bAuthoredCorrectionLayerPreserved = false;
	bool bRuntimeRouteCollisionProbed = false;
	bool bRuntimeRouteCollisionOrientationProbed = false;
	bool bRuntimeNavigationProbed = false;
	bool bRuntimeStreamingPolicyProbed = false;
	bool bRuntimeNanitePolicyProbed = false;
	bool bRuntimeInstancingPolicyProbed = false;
	bool bRuntimeHlodPolicyProbed = false;
	bool bRuntimeStructuralBudgetsPassed = false;
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
