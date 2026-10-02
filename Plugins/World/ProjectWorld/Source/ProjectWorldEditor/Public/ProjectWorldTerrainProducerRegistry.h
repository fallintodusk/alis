#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UWorld;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldLayerInventory;
struct FProjectWorldRealizationResult;

struct FProjectWorldTerrainProducerContract
{
	FString GeneratorId;
	int32 GeneratorVersion = 0;
	TArray<FString> CanonicalSelectors;
	FString SpatialOwnership;
	FString RuntimeMapping;
	int32 DependencyHaloCells = 0;
	TFunction<bool(const FString&, FString&)> ValidateSettings;
	TFunction<bool(
		UWorld*,
		const FProjectWorldCanonicalBundle&,
		const FString&,
		UMaterialInterface*,
		FProjectWorldRealizationResult&,
		FString&)> Apply;
	TFunction<bool(
		UWorld*,
		const FProjectWorldCanonicalBundle&,
		FProjectWorldRealizationResult&,
		FString&)> Delete;
	TFunction<bool(
		UWorld*,
		const FProjectWorldCanonicalBundle&,
		FProjectWorldLayerInventory&,
		FProjectWorldRealizationResult&,
		FString&)> CaptureArtifacts;
};

namespace ProjectWorldTerrainProducerRegistry
{
	PROJECTWORLDEDITOR_API bool Register(FProjectWorldTerrainProducerContract Contract, FString& OutError);
	PROJECTWORLDEDITOR_API void Unregister(const FString& GeneratorId, int32 GeneratorVersion);
	PROJECTWORLDEDITOR_API bool IsRegistered(const FString& GeneratorId, int32 GeneratorVersion);
	PROJECTWORLDEDITOR_API bool ValidateLayer(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		const TArray<FString>& CanonicalSelectors,
		const FString& SpatialOwnership,
		const FString& RuntimeMapping,
		int32 DependencyHaloCells,
		const FString& NormalizedSettings,
		FString& OutError);
	PROJECTWORLDEDITOR_API bool Apply(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FString& NormalizedSettings,
		UMaterialInterface* Material,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
	PROJECTWORLDEDITOR_API bool Delete(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
	PROJECTWORLDEDITOR_API bool CaptureArtifacts(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
