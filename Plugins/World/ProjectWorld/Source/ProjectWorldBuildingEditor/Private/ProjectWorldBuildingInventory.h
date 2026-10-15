// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldLayerInventory;
struct FProjectWorldRealizationLayer;
struct FProjectWorldRealizationResult;
class UWorld;
class UMaterialInterface;

namespace ProjectWorldBuildingInventory
{
	bool Capture(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError,
		UMaterialInterface* ExpectedMaterial=nullptr);
}
