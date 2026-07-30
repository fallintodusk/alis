// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Policies/InventoryUIDropStackPolicy.h"

#include "Interfaces/IInventoryReadOnly.h"

bool FInventoryUIDropStackPolicy::CanPreviewStackOnto(
	const FInventoryEntryView& SourceEntry,
	const FInventoryEntryView& TargetEntry,
	int32 Quantity)
{
	if (Quantity <= 0 || SourceEntry.ItemId != TargetEntry.ItemId)
	{
		return false;
	}

	const FIntPoint SourceSize(
		FMath::Max(1, SourceEntry.GridSize.X),
		FMath::Max(1, SourceEntry.GridSize.Y));
	const FIntPoint TargetSize(
		FMath::Max(1, TargetEntry.GridSize.X),
		FMath::Max(1, TargetEntry.GridSize.Y));

	return SourceSize == FIntPoint(1, 1)
		&& TargetSize == FIntPoint(1, 1)
		&& TargetEntry.MaxStack > 1
		&& TargetEntry.Quantity + Quantity <= TargetEntry.MaxStack;
}
