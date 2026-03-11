// Copyright ALIS. All Rights Reserved.

#include "Inventory/InventoryTypes.h"
#include "Components/ProjectInventoryComponent.h"

// -------------------------------------------------------------------------
// FInventoryEntry callbacks
// -------------------------------------------------------------------------

void FInventoryEntry::PreReplicatedRemove(const FInventoryList& InArraySerializer)
{
	if (InArraySerializer.OwnerComponent)
	{
		InArraySerializer.OwnerComponent->OnEntryRemoved(*this);
	}
}

void FInventoryEntry::PostReplicatedAdd(const FInventoryList& InArraySerializer)
{
	if (InArraySerializer.OwnerComponent)
	{
		InArraySerializer.OwnerComponent->OnEntryAdded(*this);
	}
}

void FInventoryEntry::PostReplicatedChange(const FInventoryList& InArraySerializer)
{
	if (InArraySerializer.OwnerComponent)
	{
		InArraySerializer.OwnerComponent->OnEntryChanged(*this);
	}
}

// -------------------------------------------------------------------------
// FInventoryList implementation
// -------------------------------------------------------------------------

uint32 FInventoryList::AddEntry(FPrimaryAssetId ItemId, int32 Quantity, FGameplayTag ContainerId, FIntPoint GridPos, bool bRotated, int32 SlotIndex)
{
	const uint32 NewInstanceId = NextInstanceId++;
	FInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.InstanceId = NewInstanceId;
	NewEntry.ItemId = ItemId;
	NewEntry.Quantity = Quantity;
	NewEntry.SlotIndex = SlotIndex;
	NewEntry.ContainerId = ContainerId;
	NewEntry.GridPos = GridPos;
	NewEntry.bRotated = bRotated;
	MarkItemDirty(NewEntry);
	return NewInstanceId;
}

uint32 FInventoryList::AddEntryWithInstanceId(uint32 InstanceId, FPrimaryAssetId ItemId, int32 Quantity, FGameplayTag ContainerId, FIntPoint GridPos, bool bRotated, int32 SlotIndex)
{
	FInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.InstanceId = InstanceId;
	NewEntry.ItemId = ItemId;
	NewEntry.Quantity = Quantity;
	NewEntry.SlotIndex = SlotIndex;
	NewEntry.ContainerId = ContainerId;
	NewEntry.GridPos = GridPos;
	NewEntry.bRotated = bRotated;
	MarkItemDirty(NewEntry);

	if (InstanceId >= NextInstanceId)
	{
		NextInstanceId = InstanceId + 1;
	}

	return InstanceId;
}

bool FInventoryList::RemoveEntry(uint32 InstanceId)
{
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (Entries[i].InstanceId == InstanceId)
		{
			Entries.RemoveAt(i);
			MarkArrayDirty();
			return true;
		}
	}
	return false;
}

FInventoryEntry* FInventoryList::FindEntry(uint32 InstanceId)
{
	for (FInventoryEntry& Entry : Entries)
	{
		if (Entry.InstanceId == InstanceId)
		{
			return &Entry;
		}
	}
	return nullptr;
}

const FInventoryEntry* FInventoryList::FindEntry(uint32 InstanceId) const
{
	for (const FInventoryEntry& Entry : Entries)
	{
		if (Entry.InstanceId == InstanceId)
		{
			return &Entry;
		}
	}
	return nullptr;
}

FInventoryEntry* FInventoryList::FindEntryByItemId(FPrimaryAssetId ItemId)
{
	for (FInventoryEntry& Entry : Entries)
	{
		if (Entry.ItemId == ItemId)
		{
			return &Entry;
		}
	}
	return nullptr;
}

FInventoryEntry* FInventoryList::FindEntryAtSlot(int32 SlotIndex)
{
	for (FInventoryEntry& Entry : Entries)
	{
		if (Entry.SlotIndex == SlotIndex)
		{
			return &Entry;
		}
	}
	return nullptr;
}

void FInventoryList::MarkEntryDirty(FInventoryEntry& Entry)
{
	MarkItemDirty(Entry);
}

int32 FInventoryList::GetNextFreeSlot(int32 MaxSlots) const
{
	TSet<int32> OccupiedSlots;
	for (const FInventoryEntry& Entry : Entries)
	{
		OccupiedSlots.Add(Entry.SlotIndex);
	}

	for (int32 i = 0; i < MaxSlots; ++i)
	{
		if (!OccupiedSlots.Contains(i))
		{
			return i;
		}
	}
	return -1;
}
