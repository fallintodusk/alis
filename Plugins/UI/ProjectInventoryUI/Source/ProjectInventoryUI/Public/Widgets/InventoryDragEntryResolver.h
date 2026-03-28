// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IInventoryReadOnly.h"

class UInventoryViewModel;

struct PROJECTINVENTORYUI_API FInventoryDragEntryResolver
{
	static bool Resolve(
		const UInventoryViewModel* InventoryVM,
		int32 PendingDragInstanceId,
		int32 PendingDragCellIndex,
		bool bPendingDragSecondary,
		FInventoryEntryView& OutEntry);
};
