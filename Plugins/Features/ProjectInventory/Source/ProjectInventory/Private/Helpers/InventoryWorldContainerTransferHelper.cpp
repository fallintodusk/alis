// Copyright ALIS. All Rights Reserved.

#include "Helpers/InventoryWorldContainerTransferHelper.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "Loot/LootTypes.h"

bool FInventoryWorldContainerTransferHelper::BuildLootEntries(
	UObject* SourceObject,
	TArray<FContainerEntryTransfer>& OutEntryViews,
	TArray<FLootEntry>& OutEntries,
	FText& OutError)
{
	OutError = FText::GetEmpty();
	OutEntryViews.Reset();
	OutEntries.Reset();

	if (!SourceObject)
	{
		OutError = NSLOCTEXT("InventoryWorldContainerTransfer", "MissingSource", "Transfer source is required.");
		return false;
	}

	if (!SourceObject->Implements<UWorldContainerSessionSource>())
	{
		OutError = NSLOCTEXT("InventoryWorldContainerTransfer", "UnsupportedSource", "Transfer source does not expose a world-container session.");
		return false;
	}

	const TArray<FInventoryEntryView> EntryViews = IWorldContainerSessionSource::Execute_GetContainerEntryViews(SourceObject);
	OutEntryViews.Reserve(EntryViews.Num());
	OutEntries.Reserve(EntryViews.Num());
	int32 InvalidCount = 0;

	for (const FInventoryEntryView& EntryView : EntryViews)
	{
		if (!EntryView.ItemId.IsValid() || EntryView.Quantity <= 0 || EntryView.InstanceId <= 0)
		{
			++InvalidCount;
			continue;
		}

		FContainerEntryTransfer TransferEntry;
		TransferEntry.EntryInstanceId = EntryView.InstanceId;
		TransferEntry.ObjectId = EntryView.ItemId;
		TransferEntry.Quantity = EntryView.Quantity;
		TransferEntry.GridPos = EntryView.GridPos;
		TransferEntry.bRotated = EntryView.bRotated;
		OutEntryViews.Add(TransferEntry);

		OutEntries.Add(FLootEntry(EntryView.ItemId, EntryView.Quantity));
	}

	if (OutEntries.Num() == 0)
	{
		OutError = InvalidCount > 0
			? NSLOCTEXT("InventoryWorldContainerTransfer", "OnlyInvalidEntries", "Container entries are invalid.")
			: NSLOCTEXT("InventoryWorldContainerTransfer", "ContainerEmpty", "Container is empty.");
		return false;
	}

	return true;
}
