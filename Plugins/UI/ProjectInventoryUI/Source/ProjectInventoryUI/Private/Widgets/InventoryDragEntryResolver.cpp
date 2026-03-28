// Copyright ALIS. All Rights Reserved.

#include "Widgets/InventoryDragEntryResolver.h"

#include "MVVM/InventoryViewModel.h"

bool FInventoryDragEntryResolver::Resolve(
	const UInventoryViewModel* InventoryVM,
	int32 PendingDragInstanceId,
	int32 PendingDragCellIndex,
	bool bPendingDragSecondary,
	FInventoryEntryView& OutEntry)
{
	if (!InventoryVM)
	{
		return false;
	}

	if (PendingDragCellIndex != INDEX_NONE)
	{
		return bPendingDragSecondary
			? InventoryVM->TryGetSecondaryEntryByCellIndex(PendingDragCellIndex, OutEntry)
			: InventoryVM->TryGetEntryByCellIndex(PendingDragCellIndex, OutEntry);
	}

	if (PendingDragInstanceId != INDEX_NONE)
	{
		return InventoryVM->TryGetEntryByInstanceId(PendingDragInstanceId, OutEntry);
	}

	return false;
}
