// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IItemDataProvider.h"

/**
 * Shared stack and depth rules for inventory-style containers.
 *
 * Lives in ProjectCore so player inventory, world storage, and UI projections
 * can apply the same authored rules without cross-plugin dependencies.
 */
struct PROJECTCORE_API FInventoryStackRules
{
	/** True when the item uses the MVP 1x1 depth-stacking model. */
	static bool UsesDepthStacking(const FItemDataView& ItemData);

	/**
	 * Resolve authored units-per-depth for a 1x1 stackable item.
	 * Compatibility rule: if unset, fall back to MaxStack so existing content
	 * keeps its old one-cell stack behavior until depth is authored explicitly.
	 */
	static int32 ResolveUnitsPerDepthUnit(const FItemDataView& ItemData);

	/** Depth units consumed by Quantity inside a single stack cell. */
	static int32 CalculateDepthUnitsForQuantity(const FItemDataView& ItemData, int32 Quantity);

	/** Effective max quantity for one stack in a container cell. */
	static int32 CalculateMaxStackForContainer(const FItemDataView& ItemData, int32 CellDepthUnits);
};
