// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IInventoryReadOnly.h"

class UInventoryViewModel;

/**
 * Manages selection and hover state for the inventory panel.
 * Single responsibility: track and query selection/hover indices.
 */
class PROJECTINVENTORYUI_API FInventoryPanelState
{
public:
	FInventoryPanelState() = default;

	// Selection state
	int32 SelectedCellIndex = INDEX_NONE;
	int32 SelectedCellIndexSecondary = INDEX_NONE;
	int32 SelectedInstanceId = INDEX_NONE;
	bool bSelectedSecondary = false;

	// Hover state
	int32 HoveredCellIndex = INDEX_NONE;
	int32 HoveredCellIndexSecondary = INDEX_NONE;
	bool bHoveredSecondary = false;

	// Quantity selection for drop/split
	int32 SelectedQuantity = 1;
	int32 SelectedMaxQuantity = 0;

	// Pending drag state
	int32 PendingDragCellIndex = INDEX_NONE;
	bool bPendingDragSecondary = false;
	bool bRotateNextDrop = false;

	/** Set selection on primary grid */
	void SetSelectedPrimary(int32 CellIndex)
	{
		bSelectedSecondary = false;
		SelectedCellIndex = CellIndex;
		SelectedInstanceId = INDEX_NONE;
	}

	/** Set selection on secondary grid */
	void SetSelectedSecondary(int32 CellIndex)
	{
		bSelectedSecondary = true;
		SelectedCellIndexSecondary = CellIndex;
		SelectedInstanceId = INDEX_NONE;
	}

	/** Set selection by item instance id (used by non-grid cells, e.g. hands). */
	void SetSelectedByInstanceId(int32 InstanceId)
	{
		bSelectedSecondary = false;
		SelectedCellIndex = INDEX_NONE;
		SelectedCellIndexSecondary = INDEX_NONE;
		SelectedInstanceId = InstanceId;
	}

	/** Set hover on primary grid */
	void SetHoveredPrimary(int32 CellIndex)
	{
		bHoveredSecondary = false;
		HoveredCellIndex = CellIndex;
	}

	/** Set hover on secondary grid */
	void SetHoveredSecondary(int32 CellIndex)
	{
		bHoveredSecondary = true;
		HoveredCellIndexSecondary = CellIndex;
	}

	/** Clear hover state */
	void ClearHover()
	{
		HoveredCellIndex = INDEX_NONE;
		HoveredCellIndexSecondary = INDEX_NONE;
	}

	/** Get effective selected index based on which grid is active */
	int32 GetEffectiveSelectedIndex() const
	{
		return bSelectedSecondary ? SelectedCellIndexSecondary : SelectedCellIndex;
	}

	/** Get effective hovered index based on which grid is active */
	int32 GetEffectiveHoveredIndex() const
	{
		return bHoveredSecondary ? HoveredCellIndexSecondary : HoveredCellIndex;
	}

	/** Try to get the selected entry from the view model */
	bool TryGetSelectedEntry(UInventoryViewModel* ViewModel, FInventoryEntryView& OutEntry) const;

	/** Try to get the hovered entry from the view model */
	bool TryGetHoveredEntry(UInventoryViewModel* ViewModel, FInventoryEntryView& OutEntry) const;

	/** Reset quantity to default */
	void ResetQuantity()
	{
		SelectedQuantity = 1;
		SelectedMaxQuantity = 0;
	}

	/** Clamp quantity within valid range */
	void ClampQuantity()
	{
		if (SelectedMaxQuantity > 0)
		{
			SelectedQuantity = FMath::Clamp(SelectedQuantity, 1, SelectedMaxQuantity);
		}
		else
		{
			SelectedQuantity = 1;
		}
	}

	/** Reset all selection state */
	void Reset()
	{
		SelectedCellIndex = INDEX_NONE;
		SelectedCellIndexSecondary = INDEX_NONE;
		SelectedInstanceId = INDEX_NONE;
		bSelectedSecondary = false;
		HoveredCellIndex = INDEX_NONE;
		HoveredCellIndexSecondary = INDEX_NONE;
		bHoveredSecondary = false;
		SelectedQuantity = 1;
		SelectedMaxQuantity = 0;
		PendingDragCellIndex = INDEX_NONE;
		bPendingDragSecondary = false;
		bRotateNextDrop = false;
	}
};
