// Copyright ALIS. All Rights Reserved.

#include "Types/InventoryStackRules.h"

bool FInventoryStackRules::UsesDepthStacking(const FItemDataView& ItemData)
{
	const FIntPoint GridSize(
		FMath::Max(1, ItemData.GridSize.X),
		FMath::Max(1, ItemData.GridSize.Y));
	return GridSize.X == 1 && GridSize.Y == 1 && FMath::Max(1, ItemData.MaxStack) > 1;
}

int32 FInventoryStackRules::ResolveUnitsPerDepthUnit(const FItemDataView& ItemData)
{
	if (!UsesDepthStacking(ItemData))
	{
		return FMath::Max(1, ItemData.MaxStack);
	}

	if (ItemData.UnitsPerDepthUnit > 0)
	{
		return ItemData.UnitsPerDepthUnit;
	}

	return FMath::Max(1, ItemData.MaxStack);
}

int32 FInventoryStackRules::CalculateDepthUnitsForQuantity(const FItemDataView& ItemData, int32 Quantity)
{
	const int32 SafeQuantity = FMath::Max(1, Quantity);
	if (!UsesDepthStacking(ItemData))
	{
		return 0;
	}

	return FMath::DivideAndRoundUp(SafeQuantity, ResolveUnitsPerDepthUnit(ItemData));
}

int32 FInventoryStackRules::CalculateMaxStackForContainer(const FItemDataView& ItemData, int32 CellDepthUnits)
{
	const int32 AuthoredMaxStack = FMath::Max(1, ItemData.MaxStack);
	if (!UsesDepthStacking(ItemData))
	{
		return AuthoredMaxStack;
	}

	const int32 EffectiveCellDepthUnits = FMath::Max(1, CellDepthUnits);
	const int32 DepthLimitedMax = ResolveUnitsPerDepthUnit(ItemData) * EffectiveCellDepthUnits;
	return FMath::Clamp(DepthLimitedMax, 1, AuthoredMaxStack);
}
