// Copyright ALIS. All Rights Reserved.

#include "LootContainer/LootContainerCapabilityComponent.h"
#include "CapabilityValidationRegistry.h"
#include "Engine/AssetManager.h"
#include "Interfaces/IDefinitionIdProvider.h"
#include "Interfaces/IItemDataProvider.h"
#include "Net/UnrealNetwork.h"
#include "ProjectGameplayTags.h"
#include "ProjectObjectCapabilitiesModule.h"

namespace
{
FIntPoint SanitizeGridSize(FIntPoint InSize)
{
	return FIntPoint(FMath::Max(InSize.X, 1), FMath::Max(InSize.Y, 1));
}

FIntPoint GetPlacedItemSize(const FItemDataView& ItemData, bool bRotated)
{
	const FIntPoint GridSize = SanitizeGridSize(ItemData.GridSize);
	return bRotated ? FIntPoint(GridSize.Y, GridSize.X) : GridSize;
}

FIntPoint GetRuntimeGridSize(const ULootContainerCapabilityComponent& Component, int32 EntryCount)
{
	if (Component.RuntimeGridSize.X > 0 && Component.RuntimeGridSize.Y > 0)
	{
		return Component.RuntimeGridSize;
	}

	return FIntPoint(FMath::Max(EntryCount, 1), 1);
}

bool HasRuntimeStorageSpec(const ULootContainerCapabilityComponent& Component)
{
	return Component.RuntimeGridSize.X > 0
		|| Component.RuntimeGridSize.Y > 0
		|| Component.RuntimeMaxCells > 0
		|| Component.RuntimeMaxWeight > 0.f
		|| Component.RuntimeMaxVolume > 0.f
		|| Component.RuntimeAllowedTags.Num() > 0;
}

bool ResolveItemDataView(const FPrimaryAssetId& ObjectId, FItemDataView& OutItemData)
{
	OutItemData = FItemDataView();

	if (!ObjectId.IsValid())
	{
		return false;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return false;
	}

	UObject* AssetObject = AssetManager->GetPrimaryAssetObject(ObjectId);
	if (!AssetObject)
	{
		AssetObject = AssetManager->GetPrimaryAssetPath(ObjectId).TryLoad();
	}

	if (!AssetObject || !AssetObject->GetClass()->ImplementsInterface(UItemDataProvider::StaticClass()))
	{
		return false;
	}

	OutItemData = IItemDataProvider::Execute_GetItemDataView(AssetObject);
	return OutItemData.IsValid();
}

bool ContainerAllowsItem(const ULootContainerCapabilityComponent& Component, const FItemDataView& ItemData)
{
	if (Component.RuntimeAllowedTags.Num() == 0)
	{
		return true;
	}

	return ItemData.Tags.HasAny(Component.RuntimeAllowedTags);
}

struct FResolvedRuntimeEntry
{
	int32 EntryIndex = INDEX_NONE;
	const FWorldContainerRuntimeEntry* Entry = nullptr;
	FItemDataView ItemData;
};

bool TryPlaceSize(
	const FIntPoint& GridSize,
	TArray<bool>& OccupiedCells,
	const FIntPoint& CandidateSize,
	FIntPoint& OutGridPos,
	bool bMarkOccupied = true)
{
	if (CandidateSize.X > GridSize.X || CandidateSize.Y > GridSize.Y)
	{
		return false;
	}

	const int32 MaxX = GridSize.X - CandidateSize.X;
	const int32 MaxY = GridSize.Y - CandidateSize.Y;
	for (int32 Y = 0; Y <= MaxY; ++Y)
	{
		for (int32 X = 0; X <= MaxX; ++X)
		{
			bool bBlocked = false;
			for (int32 OffsetY = 0; OffsetY < CandidateSize.Y && !bBlocked; ++OffsetY)
			{
				for (int32 OffsetX = 0; OffsetX < CandidateSize.X; ++OffsetX)
				{
					const int32 CellIndex = (Y + OffsetY) * GridSize.X + (X + OffsetX);
					if (!OccupiedCells.IsValidIndex(CellIndex) || OccupiedCells[CellIndex])
					{
						bBlocked = true;
						break;
					}
				}
			}

			if (!bBlocked)
			{
				OutGridPos = FIntPoint(X, Y);
				if (bMarkOccupied)
				{
					for (int32 OffsetY = 0; OffsetY < CandidateSize.Y; ++OffsetY)
					{
						for (int32 OffsetX = 0; OffsetX < CandidateSize.X; ++OffsetX)
						{
							const int32 CellIndex = (Y + OffsetY) * GridSize.X + (X + OffsetX);
							OccupiedCells[CellIndex] = true;
						}
					}
				}
				return true;
			}
		}
	}

	return false;
}

bool TryPlaceEntry(
	const ULootContainerCapabilityComponent& Component,
	const FItemDataView& ItemData,
	const FIntPoint& GridSize,
	TArray<bool>& OccupiedCells,
	FIntPoint& OutGridPos,
	bool& OutRotated)
{
	const FIntPoint ItemSize = SanitizeGridSize(ItemData.GridSize);
	const bool bCanRotate = Component.bRuntimeAllowRotation && ItemSize.X != ItemSize.Y;
	OutRotated = false;
	if (TryPlaceSize(GridSize, OccupiedCells, ItemSize, OutGridPos))
	{
		return true;
	}

	if (!bCanRotate)
	{
		return false;
	}

	OutRotated = true;
	return TryPlaceSize(GridSize, OccupiedCells, FIntPoint(ItemSize.Y, ItemSize.X), OutGridPos);
}

bool MarkPlacedEntry(
	const ULootContainerCapabilityComponent& Component,
	const FWorldContainerRuntimeEntry& Entry,
	const FItemDataView& ItemData,
	const FIntPoint& GridSize,
	TArray<bool>& OccupiedCells,
	float& OutCurrentWeight,
	float& OutCurrentVolume,
	FText* OutError)
{
	if (!Entry.IsValid())
	{
		if (OutError)
		{
			*OutError = NSLOCTEXT("LootContainerCapability", "InvalidRuntimeEntry", "World container contains an invalid runtime entry.");
		}
		return false;
	}

	if (!ContainerAllowsItem(Component, ItemData))
	{
		if (OutError)
		{
			*OutError = NSLOCTEXT("LootContainerCapability", "ItemRejectedByTags", "Container does not accept one or more items.");
		}
		return false;
	}

	const int32 MaxStack = FMath::Max(ItemData.MaxStack, 1);
	if (Entry.Quantity > MaxStack)
	{
		if (OutError)
		{
			*OutError = NSLOCTEXT("LootContainerCapability", "StackLimitExceeded", "Container entry exceeds item stack limits.");
		}
		return false;
	}

	OutCurrentWeight += ItemData.Weight * Entry.Quantity;
	OutCurrentVolume += ItemData.Volume * Entry.Quantity;

	if (Component.RuntimeMaxWeight > 0.f && OutCurrentWeight > Component.RuntimeMaxWeight)
	{
		if (OutError)
		{
			*OutError = NSLOCTEXT("LootContainerCapability", "WeightLimitExceeded", "Container weight limit would be exceeded.");
		}
		return false;
	}

	if (Component.RuntimeMaxVolume > 0.f && OutCurrentVolume > Component.RuntimeMaxVolume)
	{
		if (OutError)
		{
			*OutError = NSLOCTEXT("LootContainerCapability", "VolumeLimitExceeded", "Container volume limit would be exceeded.");
		}
		return false;
	}

	const FIntPoint ItemSize = GetPlacedItemSize(ItemData, Entry.bRotated);
	if (Entry.GridPos.X < 0
		|| Entry.GridPos.Y < 0
		|| Entry.GridPos.X + ItemSize.X > GridSize.X
		|| Entry.GridPos.Y + ItemSize.Y > GridSize.Y)
	{
		if (OutError)
		{
			*OutError = NSLOCTEXT("LootContainerCapability", "InvalidGridPlacement", "Container entry placement is outside the storage grid.");
		}
		return false;
	}

	for (int32 OffsetY = 0; OffsetY < ItemSize.Y; ++OffsetY)
	{
		for (int32 OffsetX = 0; OffsetX < ItemSize.X; ++OffsetX)
		{
			const int32 CellIndex = (Entry.GridPos.Y + OffsetY) * GridSize.X + (Entry.GridPos.X + OffsetX);
			if (!OccupiedCells.IsValidIndex(CellIndex) || OccupiedCells[CellIndex])
			{
				if (OutError)
				{
					*OutError = NSLOCTEXT("LootContainerCapability", "OverlappingPlacement", "Container entries overlap or target disabled cells.");
				}
				return false;
			}

			OccupiedCells[CellIndex] = true;
		}
	}

	return true;
}

bool BuildResolvedRuntimeEntries(
	const ULootContainerCapabilityComponent& Component,
	const TArray<FWorldContainerRuntimeEntry>& Entries,
	TArray<FResolvedRuntimeEntry>& OutResolvedEntries,
	TArray<bool>& OutOccupiedCells,
	FIntPoint& OutGridSize,
	float& OutCurrentWeight,
	float& OutCurrentVolume,
	FText* OutError)
{
	OutResolvedEntries.Reset();
	OutCurrentWeight = 0.f;
	OutCurrentVolume = 0.f;
	OutGridSize = GetRuntimeGridSize(Component, Entries.Num());

	const int32 TotalCells = OutGridSize.X * OutGridSize.Y;
	OutOccupiedCells.Init(false, FMath::Max(TotalCells, 0));
	if (Component.RuntimeMaxCells > 0 && Component.RuntimeMaxCells < OutOccupiedCells.Num())
	{
		for (int32 CellIndex = Component.RuntimeMaxCells; CellIndex < OutOccupiedCells.Num(); ++CellIndex)
		{
			OutOccupiedCells[CellIndex] = true;
		}
	}

	OutResolvedEntries.Reserve(Entries.Num());
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FWorldContainerRuntimeEntry& Entry = Entries[Index];
		FItemDataView ItemData;
		if (!ResolveItemDataView(Entry.ObjectId, ItemData))
		{
			if (OutError)
			{
				*OutError = NSLOCTEXT("LootContainerCapability", "MissingItemData", "Container item data is missing.");
			}
			return false;
		}

		if (!MarkPlacedEntry(Component, Entry, ItemData, OutGridSize, OutOccupiedCells, OutCurrentWeight, OutCurrentVolume, OutError))
		{
			return false;
		}

		FResolvedRuntimeEntry ResolvedEntry;
		ResolvedEntry.EntryIndex = Index;
		ResolvedEntry.Entry = &Entry;
		ResolvedEntry.ItemData = ItemData;
		OutResolvedEntries.Add(MoveTemp(ResolvedEntry));
	}

	return true;
}

bool BuildRuntimeEntryViews(
	const ULootContainerCapabilityComponent& Component,
	const TArray<FWorldContainerRuntimeEntry>& Entries,
	TArray<FInventoryEntryView>& OutEntryViews,
	float& OutCurrentWeight,
	float& OutCurrentVolume,
	FText* OutError)
{
	TArray<FResolvedRuntimeEntry> ResolvedEntries;
	TArray<bool> OccupiedCells;
	FIntPoint GridSize = FIntPoint::ZeroValue;
	if (!BuildResolvedRuntimeEntries(
			Component,
			Entries,
			ResolvedEntries,
			OccupiedCells,
			GridSize,
			OutCurrentWeight,
			OutCurrentVolume,
			OutError))
	{
		OutEntryViews.Reset();
		return false;
	}

	OutEntryViews.Reset();
	OutEntryViews.Reserve(ResolvedEntries.Num());
	for (const FResolvedRuntimeEntry& ResolvedEntry : ResolvedEntries)
	{
		const FWorldContainerRuntimeEntry& Entry = *ResolvedEntry.Entry;

		FInventoryEntryView View;
		View.ItemId = Entry.ObjectId;
		View.InstanceId = Entry.InstanceId;
		View.DisplayName = ResolvedEntry.ItemData.DisplayName;
		View.Description = ResolvedEntry.ItemData.Description;
		View.IconCode = ResolvedEntry.ItemData.IconCode;
		View.Quantity = Entry.Quantity;
		View.Weight = ResolvedEntry.ItemData.Weight;
		View.Volume = ResolvedEntry.ItemData.Volume;
		View.GridSize = SanitizeGridSize(ResolvedEntry.ItemData.GridSize);
		View.MaxStack = ResolvedEntry.ItemData.MaxStack;
		View.ContainerId = ProjectTags::Item_Container_WorldStorage;
		View.GridPos = Entry.GridPos;
		View.bRotated = Entry.bRotated;
		View.bCanBeDropped = false;
		View.bCanUse = false;
		View.bCanEquip = false;
		View.bActionCapsPopulated = true;
		OutEntryViews.Add(MoveTemp(View));
	}

	return true;
}

bool BuildSeedRuntimeEntries(
	const ULootContainerCapabilityComponent& Component,
	const TArray<FLootEntryView>& SeedEntries,
	int32& InOutNextRuntimeEntryInstanceId,
	TArray<FWorldContainerRuntimeEntry>& OutEntries,
	FText& OutError)
{
	struct FPendingSeedStack
	{
		FPrimaryAssetId ObjectId;
		int32 Quantity = 1;
		FItemDataView ItemData;
	};

	OutEntries.Reset();
	OutError = FText::GetEmpty();

	TArray<FPendingSeedStack> PendingStacks;
	for (const FLootEntryView& SeedEntry : SeedEntries)
	{
		if (!SeedEntry.IsValid())
		{
			continue;
		}

		FItemDataView ItemData;
		if (!ResolveItemDataView(SeedEntry.ObjectId, ItemData))
		{
			OutError = NSLOCTEXT("LootContainerCapability", "MissingSeedItemData", "Container seed item data is missing.");
			return false;
		}

		if (!ContainerAllowsItem(Component, ItemData))
		{
			OutError = NSLOCTEXT("LootContainerCapability", "SeedItemRejectedByTags", "Container seed item does not match storage filters.");
			return false;
		}

		int32 Remaining = SeedEntry.Quantity;
		const int32 MaxStack = FMath::Max(ItemData.MaxStack, 1);
		while (Remaining > 0)
		{
			FPendingSeedStack Pending;
			Pending.ObjectId = SeedEntry.ObjectId;
			Pending.Quantity = FMath::Min(Remaining, MaxStack);
			Pending.ItemData = ItemData;
			PendingStacks.Add(MoveTemp(Pending));
			Remaining -= PendingStacks.Last().Quantity;
		}
	}

	const FIntPoint GridSize = GetRuntimeGridSize(Component, PendingStacks.Num());
	TArray<bool> OccupiedCells;
	OccupiedCells.Init(false, FMath::Max(GridSize.X * GridSize.Y, 0));
	if (Component.RuntimeMaxCells > 0 && Component.RuntimeMaxCells < OccupiedCells.Num())
	{
		for (int32 CellIndex = Component.RuntimeMaxCells; CellIndex < OccupiedCells.Num(); ++CellIndex)
		{
			OccupiedCells[CellIndex] = true;
		}
	}

	float CurrentWeight = 0.f;
	float CurrentVolume = 0.f;
	OutEntries.Reserve(PendingStacks.Num());

	for (const FPendingSeedStack& Pending : PendingStacks)
	{
		CurrentWeight += Pending.ItemData.Weight * Pending.Quantity;
		CurrentVolume += Pending.ItemData.Volume * Pending.Quantity;
		if (Component.RuntimeMaxWeight > 0.f && CurrentWeight > Component.RuntimeMaxWeight)
		{
			OutError = NSLOCTEXT("LootContainerCapability", "SeedWeightExceeded", "Container seed exceeds storage weight limit.");
			return false;
		}
		if (Component.RuntimeMaxVolume > 0.f && CurrentVolume > Component.RuntimeMaxVolume)
		{
			OutError = NSLOCTEXT("LootContainerCapability", "SeedVolumeExceeded", "Container seed exceeds storage volume limit.");
			return false;
		}

		FIntPoint GridPos = FIntPoint::ZeroValue;
		bool bRotated = false;
		if (!TryPlaceEntry(Component, Pending.ItemData, GridSize, OccupiedCells, GridPos, bRotated))
		{
			OutError = NSLOCTEXT("LootContainerCapability", "SeedGridFull", "Container seed does not fit inside the storage grid.");
			return false;
		}

		FWorldContainerRuntimeEntry RuntimeEntry;
		RuntimeEntry.ObjectId = Pending.ObjectId;
		RuntimeEntry.Quantity = Pending.Quantity;
		RuntimeEntry.InstanceId = InOutNextRuntimeEntryInstanceId++;
		RuntimeEntry.GridPos = GridPos;
		RuntimeEntry.bRotated = bRotated;
		OutEntries.Add(MoveTemp(RuntimeEntry));
	}

	return true;
}

bool TryFindAutoPlacement(
	const ULootContainerCapabilityComponent& Component,
	const TArray<FWorldContainerRuntimeEntry>& ExistingEntries,
	const FItemDataView& ItemData,
	FIntPoint& OutGridPos,
	bool& OutRotated,
	FText& OutError)
{
	TArray<FResolvedRuntimeEntry> ResolvedEntries;
	TArray<bool> OccupiedCells;
	FIntPoint GridSize = FIntPoint::ZeroValue;
	float CurrentWeight = 0.f;
	float CurrentVolume = 0.f;
	if (!BuildResolvedRuntimeEntries(
			Component,
			ExistingEntries,
			ResolvedEntries,
			OccupiedCells,
			GridSize,
			CurrentWeight,
			CurrentVolume,
			&OutError))
	{
		return false;
	}

	if (!TryPlaceEntry(Component, ItemData, GridSize, OccupiedCells, OutGridPos, OutRotated))
	{
		OutError = NSLOCTEXT("LootContainerCapability", "GridFull", "Container does not have enough grid space.");
		return false;
	}

	return true;
}

bool FindOverlapEntryIndex(
	const TArray<FWorldContainerRuntimeEntry>& Entries,
	const FIntPoint& TargetPos,
	const FIntPoint& TargetSize,
	int32& OutOverlapIndex,
	bool& bOutMultipleOverlaps)
{
	OutOverlapIndex = INDEX_NONE;
	bOutMultipleOverlaps = false;

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FWorldContainerRuntimeEntry& ExistingEntry = Entries[Index];
		FItemDataView ExistingItemData;
		if (!ResolveItemDataView(ExistingEntry.ObjectId, ExistingItemData))
		{
			continue;
		}

		const FIntPoint ExistingSize = GetPlacedItemSize(ExistingItemData, ExistingEntry.bRotated);
		const bool bOverlap =
			TargetPos.X < ExistingEntry.GridPos.X + ExistingSize.X
			&& TargetPos.X + TargetSize.X > ExistingEntry.GridPos.X
			&& TargetPos.Y < ExistingEntry.GridPos.Y + ExistingSize.Y
			&& TargetPos.Y + TargetSize.Y > ExistingEntry.GridPos.Y;

		if (!bOverlap)
		{
			continue;
		}

		if (OutOverlapIndex != INDEX_NONE)
		{
			bOutMultipleOverlaps = true;
			return true;
		}

		OutOverlapIndex = Index;
	}

	return OutOverlapIndex != INDEX_NONE;
}

bool TryApplyStoreTransfer(
	const ULootContainerCapabilityComponent& Component,
	const FContainerEntryTransfer& Transfer,
	TArray<FWorldContainerRuntimeEntry>& InOutEntries,
	int32& InOutNextRuntimeEntryInstanceId,
	FText& OutError)
{
	OutError = FText::GetEmpty();
	if (!Transfer.IsValidForStore())
	{
		OutError = NSLOCTEXT("LootContainerCapability", "InvalidStoreTransfer", "Container store transfer is invalid.");
		return false;
	}

	FItemDataView ItemData;
	if (!ResolveItemDataView(Transfer.ObjectId, ItemData))
	{
		OutError = NSLOCTEXT("LootContainerCapability", "MissingStoreItemData", "Stored item data is missing.");
		return false;
	}

	const int32 MaxStack = FMath::Max(ItemData.MaxStack, 1);
	int32 Remaining = Transfer.Quantity;

	if (Transfer.HasPlacement())
	{
		if (Remaining > MaxStack)
		{
			OutError = NSLOCTEXT("LootContainerCapability", "StoreSplitRequired", "Split the stack before placing it into a specific world-container cell.");
			return false;
		}

		const FIntPoint TargetSize = GetPlacedItemSize(ItemData, Transfer.bRotated);
		int32 OverlapIndex = INDEX_NONE;
		bool bMultipleOverlaps = false;
		FindOverlapEntryIndex(InOutEntries, Transfer.GridPos, TargetSize, OverlapIndex, bMultipleOverlaps);
		if (bMultipleOverlaps)
		{
			OutError = NSLOCTEXT("LootContainerCapability", "StoreMultipleOverlaps", "Target placement overlaps multiple world-container entries.");
			return false;
		}

		if (OverlapIndex != INDEX_NONE)
		{
			FWorldContainerRuntimeEntry& OverlapEntry = InOutEntries[OverlapIndex];
			if (OverlapEntry.ObjectId != Transfer.ObjectId || OverlapEntry.Quantity + Remaining > MaxStack)
			{
				OutError = NSLOCTEXT("LootContainerCapability", "StoreOverlapRejected", "Target world-container entry cannot accept this stack.");
				return false;
			}

			OverlapEntry.Quantity += Remaining;
		}
		else
		{
			FWorldContainerRuntimeEntry NewEntry;
			NewEntry.ObjectId = Transfer.ObjectId;
			NewEntry.Quantity = Remaining;
			NewEntry.InstanceId = InOutNextRuntimeEntryInstanceId++;
			NewEntry.GridPos = Transfer.GridPos;
			NewEntry.bRotated = Transfer.bRotated;
			InOutEntries.Add(MoveTemp(NewEntry));
		}
	}
	else
	{
		for (FWorldContainerRuntimeEntry& ExistingEntry : InOutEntries)
		{
			if (Remaining <= 0)
			{
				break;
			}

			if (ExistingEntry.ObjectId != Transfer.ObjectId)
			{
				continue;
			}

			const int32 Available = MaxStack - ExistingEntry.Quantity;
			if (Available <= 0)
			{
				continue;
			}

			const int32 ToMerge = FMath::Min(Available, Remaining);
			ExistingEntry.Quantity += ToMerge;
			Remaining -= ToMerge;
		}

		while (Remaining > 0)
		{
			const int32 StackQuantity = FMath::Min(Remaining, MaxStack);
			FIntPoint GridPos = FIntPoint::ZeroValue;
			bool bRotated = false;
			if (!TryFindAutoPlacement(Component, InOutEntries, ItemData, GridPos, bRotated, OutError))
			{
				return false;
			}

			FWorldContainerRuntimeEntry NewEntry;
			NewEntry.ObjectId = Transfer.ObjectId;
			NewEntry.Quantity = StackQuantity;
			NewEntry.InstanceId = InOutNextRuntimeEntryInstanceId++;
			NewEntry.GridPos = GridPos;
			NewEntry.bRotated = bRotated;
			InOutEntries.Add(MoveTemp(NewEntry));
			Remaining -= StackQuantity;
		}
	}

	TArray<FInventoryEntryView> IgnoredViews;
	float IgnoredWeight = 0.f;
	float IgnoredVolume = 0.f;
	return BuildRuntimeEntryViews(Component, InOutEntries, IgnoredViews, IgnoredWeight, IgnoredVolume, &OutError);
}
}

// Self-registration: component owns its validation registration
#if WITH_EDITOR
static struct FLootContainerValidationAutoReg
{
	FLootContainerValidationAutoReg()
	{
		FCapabilityValidationRegistry::RegisterValidation(
			FName(TEXT("LootContainer")),
			&ULootContainerCapabilityComponent::ValidateCapabilityProperties);
	}
} GLootContainerValidationAutoReg;
#endif

ULootContainerCapabilityComponent::ULootContainerCapabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void ULootContainerCapabilityComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			InitializeContainerKey();
			InitializeRuntimeLoot();
		}
	}
}

FPrimaryAssetId ULootContainerCapabilityComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("LootContainer")));
}

void ULootContainerCapabilityComponent::InitializeRuntimeLoot()
{
	RuntimeEntries.Reset();

	if (InstanceLoot.Num() <= 0)
	{
		return;
	}

	FText InitError;
	if (!BuildSeedRuntimeEntries(*this, InstanceLoot, NextRuntimeEntryInstanceId, RuntimeEntries, InitError))
	{
		RuntimeEntries.Reset();
		UE_LOG(
			LogProjectObjectCapabilities,
			Warning,
			TEXT("LootContainer: Failed to initialize runtime entries on %s - %s"),
			*GetNameSafe(GetOwner()),
			*InitError.ToString());
		return;
	}

	UE_LOG(
		LogProjectObjectCapabilities,
		Log,
		TEXT("LootContainer: Initialized %d runtime entries on %s"),
		RuntimeEntries.Num(),
		*GetNameSafe(GetOwner()));
}

void ULootContainerCapabilityComponent::InitializeContainerKey()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!ContainerKey.InstanceId.IsValid())
	{
		ContainerKey.InstanceId = FGuid::NewGuid();
	}

	if (ContainerKey.WorldScopeId.IsNone())
	{
		if (const ULevel* Level = Owner->GetLevel())
		{
			ContainerKey.WorldScopeId = Level->GetOutermost()->GetFName();
		}
	}

	if (ContainerKey.ContainerSlotId.IsNone())
	{
		ContainerKey.ContainerSlotId = FName(TEXT("Primary"));
	}
}

int32 ULootContainerCapabilityComponent::AllocateRuntimeEntryInstanceId()
{
	return NextRuntimeEntryInstanceId++;
}

void ULootContainerCapabilityComponent::AddLootEntry(FPrimaryAssetId ObjectId, int32 Quantity)
{
	if (!ObjectId.IsValid() || Quantity < 1)
	{
		return;
	}

	FLootEntryView Entry;
	Entry.ObjectId = ObjectId;
	Entry.Quantity = Quantity;

	TArray<FLootEntryView> SeedEntries;
	SeedEntries.Add(Entry);

	FText AddError;
	TArray<FWorldContainerRuntimeEntry> AddedEntries;
	if (!BuildSeedRuntimeEntries(*this, SeedEntries, NextRuntimeEntryInstanceId, AddedEntries, AddError))
	{
		UE_LOG(
			LogProjectObjectCapabilities,
			Warning,
			TEXT("LootContainer: Failed to add runtime entry on %s - %s"),
			*GetNameSafe(GetOwner()),
			*AddError.ToString());
		return;
	}

	for (FWorldContainerRuntimeEntry& AddedEntry : AddedEntries)
	{
		FContainerEntryTransfer Transfer;
		Transfer.ObjectId = AddedEntry.ObjectId;
		Transfer.Quantity = AddedEntry.Quantity;
		Transfer.GridPos = AddedEntry.GridPos;
		Transfer.bRotated = AddedEntry.bRotated;

		FText StoreError;
		if (!TryApplyStoreTransfer(*this, Transfer, RuntimeEntries, NextRuntimeEntryInstanceId, StoreError))
		{
			UE_LOG(
				LogProjectObjectCapabilities,
				Warning,
				TEXT("LootContainer: Failed to store added runtime entry on %s - %s"),
				*GetNameSafe(GetOwner()),
				*StoreError.ToString());
			return;
		}
	}

	bLooted = RuntimeEntries.Num() == 0;
}

void ULootContainerCapabilityComponent::ClearLoot()
{
	RuntimeEntries.Empty();
	bLooted = false;
	ActiveFullOpenSessionId.Invalidate();
	ActiveFullOpenInstigator.Reset();
}

FText ULootContainerCapabilityComponent::GetContainerDisplayLabel_Implementation() const
{
	const AActor* Owner = GetOwner();
	return Owner ? FText::FromString(Owner->GetActorNameOrLabel()) : FText::GetEmpty();
}

FInventoryContainerView ULootContainerCapabilityComponent::GetContainerView_Implementation() const
{
	FInventoryContainerView View;
	View.ContainerId = ProjectTags::Item_Container_WorldStorage;
	View.GridSize = GetRuntimeGridSize(*this, RuntimeEntries.Num());
	View.MaxWeight = RuntimeMaxWeight;
	View.MaxVolume = RuntimeMaxVolume;
	View.MaxCells = RuntimeMaxCells;

	TArray<FInventoryEntryView> EntryViews;
	FText IgnoredError;
	BuildRuntimeEntryViews(*this, RuntimeEntries, EntryViews, View.CurrentWeight, View.CurrentVolume, &IgnoredError);
	return View;
}

TArray<FInventoryEntryView> ULootContainerCapabilityComponent::GetContainerEntryViews_Implementation() const
{
	TArray<FInventoryEntryView> EntryViews;
	float CurrentWeight = 0.f;
	float CurrentVolume = 0.f;
	FText IgnoredError;
	BuildRuntimeEntryViews(*this, RuntimeEntries, EntryViews, CurrentWeight, CurrentVolume, &IgnoredError);
	return EntryViews;
}

FWorldContainerKey ULootContainerCapabilityComponent::GetWorldContainerKey_Implementation() const
{
	return ContainerKey;
}

bool ULootContainerCapabilityComponent::SupportsContainerSession_Implementation(EContainerSessionMode Mode) const
{
	if (Mode == EContainerSessionMode::QuickLoot)
	{
		return RuntimeEntries.Num() > 0;
	}

	return RuntimeEntries.Num() > 0 || HasRuntimeStorageSpec(*this);
}

bool ULootContainerCapabilityComponent::TryBeginContainerSession_Implementation(
	AActor* Instigator,
	FGuid SessionId,
	EContainerSessionMode Mode,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutError = NSLOCTEXT("LootContainerCapability", "MissingOwner", "Container owner is missing.");
		return false;
	}

	if (!Owner->HasAuthority())
	{
		OutError = NSLOCTEXT("LootContainerCapability", "AuthorityRequired", "Container session open requires authority.");
		return false;
	}

	if (!SessionId.IsValid())
	{
		OutError = NSLOCTEXT("LootContainerCapability", "InvalidSessionId", "Container session id is invalid.");
		return false;
	}

	if (!SupportsContainerSession_Implementation(Mode))
	{
		OutError = NSLOCTEXT("LootContainerCapability", "UnsupportedSessionMode", "Container does not support the requested session mode.");
		return false;
	}

	if (Mode == EContainerSessionMode::QuickLoot)
	{
		return true;
	}

	if (ActiveFullOpenSessionId.IsValid() && ActiveFullOpenSessionId != SessionId)
	{
		OutError = NSLOCTEXT("LootContainerCapability", "Busy", "Container is busy.");
		return false;
	}

	ActiveFullOpenSessionId = SessionId;
	ActiveFullOpenInstigator = Instigator;
	return true;
}

bool ULootContainerCapabilityComponent::ConsumeContainerEntries_Implementation(
	FGuid SessionId,
	const TArray<FContainerEntryTransfer>& Entries,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutError = NSLOCTEXT("LootContainerCapability", "MissingOwnerOnConsume", "Container owner is missing.");
		return false;
	}

	if (!Owner->HasAuthority())
	{
		OutError = NSLOCTEXT("LootContainerCapability", "AuthorityRequiredOnConsume", "Container consume requires authority.");
		return false;
	}

	if (Entries.Num() == 0)
	{
		OutError = NSLOCTEXT("LootContainerCapability", "NoEntriesToConsume", "No container entries were requested for consume.");
		return false;
	}

	if (ActiveFullOpenSessionId.IsValid() && ActiveFullOpenSessionId != SessionId)
	{
		OutError = NSLOCTEXT("LootContainerCapability", "ConsumeBusy", "Container is busy with another full-open session.");
		return false;
	}

	TMap<int32, int32> RequestedByInstanceId;
	for (const FContainerEntryTransfer& Entry : Entries)
	{
		if (!Entry.IsValidForConsume())
		{
			continue;
		}
		RequestedByInstanceId.FindOrAdd(Entry.EntryInstanceId) += Entry.Quantity;
	}

	if (RequestedByInstanceId.Num() == 0)
	{
		OutError = NSLOCTEXT("LootContainerCapability", "NoValidEntriesToConsume", "Requested container entries are invalid.");
		return false;
	}

	for (const TPair<int32, int32>& Pair : RequestedByInstanceId)
	{
		const FWorldContainerRuntimeEntry* RuntimeEntry = RuntimeEntries.FindByPredicate([&Pair](const FWorldContainerRuntimeEntry& Candidate)
		{
			return Candidate.InstanceId == Pair.Key;
		});
		if (!RuntimeEntry || RuntimeEntry->Quantity < Pair.Value)
		{
			OutError = NSLOCTEXT("LootContainerCapability", "EntriesNoLongerAvailable", "Requested container entries are no longer available.");
			return false;
		}
	}

	for (int32 Index = RuntimeEntries.Num() - 1; Index >= 0; --Index)
	{
		FWorldContainerRuntimeEntry& RuntimeEntry = RuntimeEntries[Index];
		int32* RequestedQuantity = RequestedByInstanceId.Find(RuntimeEntry.InstanceId);
		if (!RequestedQuantity || *RequestedQuantity <= 0)
		{
			continue;
		}

		const int32 QuantityToConsume = FMath::Min(RuntimeEntry.Quantity, *RequestedQuantity);
		RuntimeEntry.Quantity -= QuantityToConsume;
		*RequestedQuantity -= QuantityToConsume;

		if (RuntimeEntry.Quantity <= 0)
		{
			RuntimeEntries.RemoveAt(Index);
		}
	}

	if (RuntimeEntries.Num() == 0)
	{
		bLooted = true;
		OnContainerLooted.Broadcast();
	}

	return true;
}

bool ULootContainerCapabilityComponent::CanStoreContainerEntries_Implementation(
	FGuid SessionId,
	const TArray<FContainerEntryTransfer>& Entries,
	FText& OutError) const
{
	OutError = FText::GetEmpty();

	if (Entries.Num() == 0)
	{
		OutError = NSLOCTEXT("LootContainerCapability", "NoEntriesToStore", "No container entries were provided for store.");
		return false;
	}

	if (ActiveFullOpenSessionId.IsValid() && ActiveFullOpenSessionId != SessionId)
	{
		OutError = NSLOCTEXT("LootContainerCapability", "StoreBusy", "Container is busy with another full-open session.");
		return false;
	}

	TArray<FWorldContainerRuntimeEntry> SimulatedEntries = RuntimeEntries;
	int32 SimulatedNextInstanceId = NextRuntimeEntryInstanceId;

	for (const FContainerEntryTransfer& Entry : Entries)
	{
		if (!TryApplyStoreTransfer(*this, Entry, SimulatedEntries, SimulatedNextInstanceId, OutError))
		{
			return false;
		}
	}

	TArray<FInventoryEntryView> IgnoredViews;
	float IgnoredWeight = 0.f;
	float IgnoredVolume = 0.f;
	return BuildRuntimeEntryViews(*this, SimulatedEntries, IgnoredViews, IgnoredWeight, IgnoredVolume, &OutError);
}

bool ULootContainerCapabilityComponent::StoreContainerEntries_Implementation(
	FGuid SessionId,
	const TArray<FContainerEntryTransfer>& Entries,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutError = NSLOCTEXT("LootContainerCapability", "MissingOwnerOnStore", "Container owner is missing.");
		return false;
	}

	if (!Owner->HasAuthority())
	{
		OutError = NSLOCTEXT("LootContainerCapability", "AuthorityRequiredOnStore", "Container store requires authority.");
		return false;
	}

	if (!CanStoreContainerEntries_Implementation(SessionId, Entries, OutError))
	{
		return false;
	}

	int32 SimulatedNextInstanceId = NextRuntimeEntryInstanceId;
	for (const FContainerEntryTransfer& Entry : Entries)
	{
		if (!TryApplyStoreTransfer(*this, Entry, RuntimeEntries, SimulatedNextInstanceId, OutError))
		{
			return false;
		}
	}

	NextRuntimeEntryInstanceId = SimulatedNextInstanceId;
	bLooted = RuntimeEntries.Num() == 0;
	return true;
}

bool ULootContainerCapabilityComponent::EndContainerSession_Implementation(FGuid SessionId)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	if (!SessionId.IsValid())
	{
		return false;
	}

	if (!ActiveFullOpenSessionId.IsValid() || ActiveFullOpenSessionId != SessionId)
	{
		return false;
	}

	ActiveFullOpenSessionId.Invalidate();
	ActiveFullOpenInstigator.Reset();

	if (bOneTimeUse && RuntimeEntries.Num() == 0)
	{
		if (Owner)
		{
			Owner->Destroy();
		}
	}
	return true;
}

bool ULootContainerCapabilityComponent::HasActiveFullOpenSession_Implementation() const
{
	return ActiveFullOpenSessionId.IsValid();
}

TArray<FCapabilityValidationResult> ULootContainerCapabilityComponent::ValidateCapabilityProperties(
	const TMap<FName, FString>& Properties)
{
	TArray<FCapabilityValidationResult> Results;
	return Results;
}

bool ULootContainerCapabilityComponent::OnComponentInteract_Implementation(AActor* Instigator)
{
	if (!Instigator)
	{
		return false;
	}

	if (HasActiveFullOpenSession_Implementation())
	{
		UE_LOG(LogProjectObjectCapabilities, Verbose,
			TEXT("LootContainer: %s is busy with an active full-open session"),
			*GetOwner()->GetName());
		return false;
	}

	OnContainerOpened.Broadcast(Instigator);

	UE_LOG(LogProjectObjectCapabilities, Verbose,
		TEXT("LootContainer interaction on %s (%d entries)"),
		*GetOwner()->GetName(),
		RuntimeEntries.Num());

	return true;
}

FText ULootContainerCapabilityComponent::GetInteractionLabel_Implementation() const
{
	return NSLOCTEXT("LootContainerCapability", "SearchLabel", "Search");
}

void ULootContainerCapabilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULootContainerCapabilityComponent, bLooted);
	DOREPLIFETIME(ULootContainerCapabilityComponent, RuntimeEntries);
	DOREPLIFETIME(ULootContainerCapabilityComponent, ContainerKey);
	DOREPLIFETIME(ULootContainerCapabilityComponent, RuntimeGridSize);
	DOREPLIFETIME(ULootContainerCapabilityComponent, RuntimeMaxWeight);
	DOREPLIFETIME(ULootContainerCapabilityComponent, RuntimeMaxVolume);
	DOREPLIFETIME(ULootContainerCapabilityComponent, RuntimeMaxCells);
	DOREPLIFETIME(ULootContainerCapabilityComponent, RuntimeAllowedTags);
	DOREPLIFETIME(ULootContainerCapabilityComponent, bRuntimeAllowRotation);
}
