#pragma once

#include "ProjectWorldTerrainProducerRegistry.h"

class UMaterialInterface;
namespace UE::Geometry
{
	class FDynamicMesh3;
}

namespace ProjectWorldMeshTerrainProducer
{
	extern const TCHAR* SharedDefinitionObjectPath;
	extern const FName GroundChannel;
	extern const FName HydroTransitionChannel;

	FProjectWorldTerrainProducerContract Contract();
	bool MatchesBaseIdentity(
		const TArray<FName>& Tags,
		const FString& ExpectedInput,
		const FString& ExpectedEngine,
		const FString& ExpectedMaterial,
		const FString& ExpectedAdapterCompiler);
	bool EnsureSharedDefinition(UMaterialInterface* TerrainMaterial, FString& OutError);
	bool ValidateSharedDefinition(const FString& TerrainMaterialObjectPath, FString& OutError);
	void AppendRendererFacingGridTriangles(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 SamplesX,
		int32 SamplesY);
}
