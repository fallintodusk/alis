// Copyright ALIS. All Rights Reserved.

#include "Widgets/InventoryPanelState.h"
#include "MVVM/InventoryViewModel.h"

bool FInventoryPanelState::TryGetSelectedEntry(UInventoryViewModel* ViewModel, FInventoryEntryView& OutEntry) const
{
	if (!ViewModel)
	{
		return false;
	}

	if (SelectedInstanceId != INDEX_NONE)
	{
		return ViewModel->TryGetEntryByInstanceId(SelectedInstanceId, OutEntry);
	}

	if (bSelectedSecondary)
	{
		if (SelectedCellIndexSecondary == INDEX_NONE)
		{
			return false;
		}
		return ViewModel->TryGetSecondaryEntryByCellIndex(SelectedCellIndexSecondary, OutEntry);
	}

	if (SelectedCellIndex == INDEX_NONE)
	{
		return false;
	}

	return ViewModel->TryGetEntryByCellIndex(SelectedCellIndex, OutEntry);
}

bool FInventoryPanelState::TryGetHoveredEntry(UInventoryViewModel* ViewModel, FInventoryEntryView& OutEntry) const
{
	if (!ViewModel) { return false; }

	if (bHoveredSecondary)
	{
		if (HoveredCellIndexSecondary == INDEX_NONE) { return false; }
		return ViewModel->TryGetSecondaryEntryByCellIndex(HoveredCellIndexSecondary, OutEntry);
	}

	if (HoveredCellIndex == INDEX_NONE) { return false; }
	return ViewModel->TryGetEntryByCellIndex(HoveredCellIndex, OutEntry);
}
