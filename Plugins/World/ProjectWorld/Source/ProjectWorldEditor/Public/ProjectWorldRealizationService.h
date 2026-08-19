// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWorldTerrainVerification.h"

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
	FString AuthoredOverlayProfilePath;
	FString RealizationProfilePath;
	FString LayerDirtyInputPath;
	FString MapPackagePath;
	FString ResultPath;
	EProjectWorldRealizationMode Mode = EProjectWorldRealizationMode::Validate;
	bool bRequireLandscapeCompatible = false;
	bool bFirstLayerApply = false;
	int32 MaxRoadFeatures = 1;
	int32 MaxBuildingFeatures = 4;
};

struct FProjectWorldLayerArtifactInventory
{
	FString Path;
	FString Kind;
	FString Digest;
	FString SemanticHash;
};

struct FProjectWorldLayerInputInventory
{
	FString UnitId;
	FString Hash;
};

struct FProjectWorldLayerInventory
{
	FString LayerId;
	FString ScopeId;
	FString NormalizedLayerContractHash;
	FString GeneratorId;
	int32 GeneratorVersion = 0;
	FString ArtifactRoot;
	TArray<FProjectWorldLayerInputInventory> CanonicalInputs;
	TArray<FProjectWorldLayerInputInventory> DependencyInputs;
	TArray<FString> FinalDirtyUnits;
	TArray<FProjectWorldLayerArtifactInventory> Artifacts;
};

struct FProjectWorldAuthoredAnchorEvidence
{
	FString OverlayId;
	FString AuthoredPackage;
	FVector WorldLocation = FVector::ZeroVector;
	FRotator WorldRotation = FRotator::ZeroRotator;
	double DriftMeters = 0.0;
	double HorizontalTotalErrorMeters = 0.0;
	double VerticalTotalErrorMeters = 0.0;
	bool bSurfaceSnapped = false;
	bool bPlaces = false;
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
	FString AuthoredOverlaySetId;
	FString AuthoredOverlaySetHash;
	FString RealizationProfileId;
	FString RealizationProfileHash;
	FString LayerDirtyInputHash;
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
	FVector2D SampleSpacingMeters = FVector2D::ZeroVector;
	double DurationSeconds = 0.0;
	int64 GeneratedSourceBytes = 0;
	int64 ProceduralMeshBufferBytes = 0;
	int32 VerifiedOutputCount = 0;
	int32 CanonicalCellCount = 0;
	int32 CreatedActorCount = 0;
	int32 UpdatedActorCount = 0;
	int32 RemovedActorCount = 0;
	int32 PreservedActorCount = 0;
	int32 TerrainSectionCount = 0;
	int32 LandscapeComponentCount = 0;
	int32 UpdatedLandscapeComponentCount = 0;
	int32 LandscapeProxyCount = 0;
	int32 WaterCellActorCount = 0;
	int32 WaterMeshAssetCount = 0;
	int32 WaterTriangleCount = 0;
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
	int32 HlodLayerReferenceCount = 0;
	int32 HlodEligibleGeneratedActorCount = 0;
	int32 RuntimeCollisionProbeCount = 0;
	int32 RuntimeCollisionOrientationProbeCount = 0;
	int32 AuthoredAnchorResolvedCount = 0;
	int32 AuthoredAnchorRefusedCount = 0;
	int32 AuthoredAnchorPlacedCount = 0;
	int32 AuthoredMaskCount = 0;
	double RuntimeRouteVolumeYawDegrees = 0.0;
	double RuntimeNavigationPathMeters = 0.0;
	double RuntimeP95FrameTimeBudgetMilliseconds = 0.0;
	double AuthoredAnchorMaximumDriftMeters = 0.0;
	FString CrossCellRoadFeatureId;
	// SOURCE surface: the Generated Base edit layer, one input to UE's edit-layer blend.
	// Diagnostic only - a correct source layer does not imply correct rendered terrain.
	FProjectWorldTerrainHeightComparison TerrainHeight;
	// FINAL surface: the blended final/base heightmap that renders, collides, and drives
	// cached bounds. This is the acceptance surface.
	FProjectWorldTerrainHeightComparison TerrainFinalHeight;
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
	TArray<FProjectWorldAuthoredAnchorEvidence> AuthoredAnchors;
	TArray<FProjectWorldLayerInventory> LayerInventories;

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
