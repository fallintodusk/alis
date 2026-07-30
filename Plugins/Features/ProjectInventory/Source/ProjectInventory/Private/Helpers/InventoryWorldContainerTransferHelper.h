// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Loot/LootTypes.h"
#include "Types/LootEntryTypes.h"

class UProjectInventoryComponent;

struct FInventoryWorldContainerTransferHelper
{
	static bool BuildLootEntries(
		UObject* SourceObject,
		TArray<FContainerEntryTransfer>& OutEntryViews,
		TArray<FLootEntry>& OutEntries,
		FText& OutError);
};
