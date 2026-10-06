#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UWorld;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldLayerInventory;
struct FProjectWorldRealizationResult;
namespace UE::Geometry
{
	class FDynamicMesh3;
}

namespace ProjectWorldMeshTerrainProducer
{
	extern const TCHAR* SharedDefinitionObjectPath;
	extern const FName GroundChannel;
	extern const FName HydroTransitionChannel;

	bool ApplyLayer(UWorld* World, const FProjectWorldCanonicalBundle& Bundle,
		const FString& Settings, UMaterialInterface* Material, const FString& ProducerFingerprint,
		FProjectWorldRealizationResult& OutResult, FString& OutError);
	bool DeleteLayer(UWorld* World, const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldRealizationResult& OutResult, FString& OutError);
	bool CaptureLayer(UWorld* World, const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& OutResult, FString& OutError);
	bool MatchesBaseIdentity(
		const TArray<FName>& Tags,
		const FString& ExpectedCellId,
		const FString& ExpectedInput,
		const FString& ExpectedMaterial,
		const FString& ExpectedProducerFingerprint);
	TArray<FName> BuildBaseIdentityTags(const FString& CellId, const FString& Input,
		const FString& MaterialPath, const FString& ProducerFingerprint);
	bool EnsureSharedDefinition(UMaterialInterface* TerrainMaterial, FString& OutError);
	bool ValidateSharedDefinition(const FString& TerrainMaterialObjectPath, FString& OutError);
	void AppendRendererFacingGridTriangles(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 SamplesX,
		int32 SamplesY);
}
