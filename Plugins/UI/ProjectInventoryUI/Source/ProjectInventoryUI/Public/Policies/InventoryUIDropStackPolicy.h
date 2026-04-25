// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FInventoryEntryView;

/**
 * UI-side preview policy for stack drops.
 *
 * ProjectInventory remains authoritative. This helper only decides whether an
 * occupied visual cell may be treated as a legal drop preview before the server
 * validates and applies the move.
 */
struct PROJECTINVENTORYUI_API FInventoryUIDropStackPolicy
{
	static bool CanPreviewStackOnto(
		const FInventoryEntryView& SourceEntry,
		const FInventoryEntryView& TargetEntry,
		int32 Quantity);
};
