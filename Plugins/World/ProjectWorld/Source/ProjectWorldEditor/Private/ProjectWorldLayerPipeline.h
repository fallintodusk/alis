#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UWorld;
struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldRealizationProfile;
struct FProjectWorldRealizationResult;
struct FProjectWorldLayerInventory;

namespace ProjectWorldLayerPipeline
{
	bool ValidateExecutionSelection(const FProjectWorldRealizationProfile& Profile,
		const TArray<FString>& SelectedIds, const TArray<FProjectWorldLayerInventory>& Inventories,
		FString& OutError, bool bRequireCleanGeometricDependencies = true);
	bool ApplyLayers(UWorld& World, const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlays,
		const TMap<FName, UMaterialInterface*>& PresentationMaterials,
		FProjectWorldRealizationResult& OutResult, FString& OutError);
	bool DeleteLayers(UWorld& World, const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlays,
		FProjectWorldRealizationResult& OutResult, FString& OutError);
}
