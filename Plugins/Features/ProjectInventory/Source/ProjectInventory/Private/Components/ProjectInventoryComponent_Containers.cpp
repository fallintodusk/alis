// Copyright ALIS. All Rights Reserved.
//
// ProjectInventoryComponent container + grid resolution helpers.
//
// Split out of ProjectInventoryComponent.cpp on 2026-04-23 under the
// FILE SIZE GUARDRAIL rule (AGENTS.md). The class declaration is unchanged;
// this TU defines the subset responsible for mapping between the
// component's configured storage (hands/pockets/backpack/grants) and the
// grid math that the mutation paths need:
//
//   Container resolution:
//     - GetEffectiveContainers
//     - GetContainerConfig / GetDefaultContainerId
//     - GetContainerIndex / GetContainerSlotOffset / GetContainerCellCount
//     - ComputeSlotIndex / TryGetGridPosFromSlot
//     - GetContainerCellDepthUnits
//     - GetEffectiveMaxStackForContainer
//     - BuildContainerConfigFromGrant / UpsertContainerConfig
//     - GetEffectiveEntryPlacement
//     - ContainerAllowsItem
//     - GetContainerOrder
//     - GetEquipSlotContainerGrants
//     - IsContainerEmpty
//   Grid math:
//     - SanitizeGridSize / GetItemGridSize
//     - IsRectWithinContainer
//     - DoesRectOverlap
//     - FindFreeGridPos
//   Weight/volume per container:
//     - GetContainerCurrentWeight / GetContainerCurrentVolume
//
// Helpers shared with other split TUs live in
// Private/Components/ProjectInventoryComponentInternals.h.

#include "Components/ProjectInventoryComponent.h"
#include "Components/ProjectInventoryComponentInternals.h"

#include "Helpers/InventoryContainerHelper.h"
#include "Helpers/InventoryGridPlacement.h"
#include "Helpers/InventoryWeightHelper.h"
#include "ProjectGameplayTags.h"
#include "ProjectInventory.h"
#include "Types/InventoryStackRules.h"

using namespace ProjectInventoryInternal;

// -------------------------------------------------------------------------
// Container resolution
// -------------------------------------------------------------------------

void UProjectInventoryComponent::GetEffectiveContainers(TArray<FInventoryContainerConfig>& OutContainers) const
{
	OutContainers.Reset();

	if (bIncludeConfiguredContainersAsBase && Containers.Num() > 0)
	{
		OutContainers = Containers;
	}
	else
	{
		// Vision SOT: naked baseline is hands only.
		// Represent hands as two real 2x2 containers so small items can share a hand grid.
		const FGameplayTag EffectiveDefaultContainerId = GetDefaultContainerId();
		if (EffectiveDefaultContainerId == ProjectTags::Item_Container_LeftHand
			|| EffectiveDefaultContainerId == ProjectTags::Item_Container_RightHand
			|| EffectiveDefaultContainerId == ProjectTags::Item_Container_Hands)
		{
			auto AddHandContainer = [&OutContainers](const FGameplayTag& ContainerId)
			{
				FInventoryContainerConfig HandContainer;
				HandContainer.ContainerId = ContainerId;
				HandContainer.GridSize = FIntPoint(2, 2);
				HandContainer.MaxCells = 4;
				HandContainer.CellDepthUnits = 4;
				HandContainer.bWidthOnlyValidation = true;
				OutContainers.Add(HandContainer);
			};

			AddHandContainer(ProjectTags::Item_Container_LeftHand);
			AddHandContainer(ProjectTags::Item_Container_RightHand);
		}
		else
		{
			FInventoryContainerConfig DefaultContainer;
			DefaultContainer.ContainerId = EffectiveDefaultContainerId;
			DefaultContainer.MaxWeight = MaxWeight;
			DefaultContainer.MaxVolume = MaxVolume;
			DefaultContainer.MaxCells = MaxSlots;

			int32 Width = FMath::Max(1, DefaultContainerGridWidth);
			if (MaxSlots > 0)
			{
				Width = FMath::Min(Width, MaxSlots);
			}
			const int32 Height = (Width > 0) ? FMath::DivideAndRoundUp(MaxSlots, Width) : 0;
			DefaultContainer.GridSize = FIntPoint(Width, Height);
			OutContainers.Add(DefaultContainer);
		}
	}

	for (const auto& Pair : EquippedItems)
	{
		const uint32 InstanceId = Pair.Value.InstanceId;
		if (const FInventoryEntry* Entry = Inventory.FindEntry(InstanceId))
		{
			FItemDataView ItemData;
			if (GetItemDataView(Entry->ItemId, ItemData) && ItemData.ContainerGrants.Num() > 0)
			{
				// Item-driven grants are the primary source of container expansion.
				// This keeps pocket/backpack sizes attached to the item definition.
				for (const FInventoryContainerGrantView& Grant : ItemData.ContainerGrants)
				{
					if (!Grant.ContainerId.IsValid())
					{
						continue;
					}
					UpsertContainerConfig(BuildContainerConfigFromGrant(Grant), OutContainers);
				}
				continue;
			}
		}

		TArray<FInventoryContainerConfig> SlotGrants;
		if (GetEquipSlotContainerGrants(Pair.Key, SlotGrants))
		{
			for (const FInventoryContainerConfig& GrantConfig : SlotGrants)
			{
				if (GrantConfig.ContainerId.IsValid())
				{
					UpsertContainerConfig(GrantConfig, OutContainers);
				}
			}
		}
	}
}

bool UProjectInventoryComponent::GetContainerConfig(FGameplayTag ContainerId, FInventoryContainerConfig& OutConfig) const
{
	if (!ContainerId.IsValid())
	{
		return false;
	}

	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);

	for (const FInventoryContainerConfig& Container : EffectiveContainers)
	{
		if (Container.ContainerId == ContainerId)
		{
			OutConfig = Container;
			return true;
		}
	}

	return false;
}

FGameplayTag UProjectInventoryComponent::GetDefaultContainerId() const
{
	if (!DefaultContainerId.IsValid() || DefaultContainerId == ProjectTags::Item_Container_Hands)
	{
		return ProjectTags::Item_Container_LeftHand;
	}

	return DefaultContainerId;
}

// SOLID: Delegated to FInventoryContainerHelper
int32 UProjectInventoryComponent::GetContainerIndex(FGameplayTag ContainerId, const TArray<FInventoryContainerConfig>& ContainersList) const
{
	return FInventoryContainerHelper::GetContainerIndex(ContainerId, ContainersList);
}

int32 UProjectInventoryComponent::GetContainerSlotOffset(FGameplayTag ContainerId, const TArray<FInventoryContainerConfig>& ContainersList) const
{
	return FInventoryContainerHelper::GetContainerSlotOffset(ContainerId, ContainersList);
}

int32 UProjectInventoryComponent::GetContainerCellCount(const FInventoryContainerConfig& Container) const
{
	return FInventoryContainerHelper::GetContainerCellCount(Container);
}

int32 UProjectInventoryComponent::ComputeSlotIndex(FGameplayTag ContainerId, FIntPoint GridPos) const
{
	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);
	return FInventoryContainerHelper::ComputeSlotIndex(ContainerId, GridPos, EffectiveContainers);
}

bool UProjectInventoryComponent::TryGetGridPosFromSlot(FGameplayTag ContainerId, int32 SlotIndex, FIntPoint& OutGridPos) const
{
	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);
	return FInventoryContainerHelper::TryGetGridPosFromSlot(ContainerId, SlotIndex, EffectiveContainers, OutGridPos);
}

// -------------------------------------------------------------------------
// Grid math
// -------------------------------------------------------------------------

// SOLID: Delegated to FInventoryGridPlacement
FIntPoint UProjectInventoryComponent::SanitizeGridSize(FIntPoint InSize) const
{
	return FInventoryGridPlacement::SanitizeGridSize(InSize);
}

FIntPoint UProjectInventoryComponent::GetItemGridSize(const FItemDataView& ItemData, bool bRotated) const
{
	return FInventoryGridPlacement::GetItemGridSize(ItemData.GridSize, bRotated);
}

int32 UProjectInventoryComponent::GetContainerCellDepthUnits(const FInventoryContainerConfig& Container) const
{
	return FMath::Max(1, Container.CellDepthUnits);
}

int32 UProjectInventoryComponent::GetEffectiveMaxStackForContainer(
	const FInventoryContainerConfig& Container,
	const FItemDataView& ItemData) const
{
	return FInventoryStackRules::CalculateMaxStackForContainer(ItemData, GetContainerCellDepthUnits(Container));
}

FInventoryContainerConfig UProjectInventoryComponent::BuildContainerConfigFromGrant(const FInventoryContainerGrantView& Grant) const
{
	FInventoryContainerConfig Config;
	Config.ContainerId = Grant.ContainerId;
	Config.MaxWeight = Grant.MaxWeight;
	Config.MaxVolume = Grant.MaxVolume;
	Config.MaxCells = Grant.MaxCells;
	Config.CellDepthUnits = FMath::Max(1, Grant.CellDepthUnits);
	Config.AllowedTags = Grant.AllowedTags;
	Config.bAllowRotation = Grant.bAllowRotation;

	if (Grant.GridSize.X > 0 && Grant.GridSize.Y > 0)
	{
		Config.GridSize = SanitizeGridSize(Grant.GridSize);
	}
	else if (Grant.MaxCells > 0)
	{
		int32 Width = FMath::Max(1, DefaultContainerGridWidth);
		Width = FMath::Min(Width, Grant.MaxCells);
		const int32 Height = FMath::DivideAndRoundUp(Grant.MaxCells, Width);
		Config.GridSize = FIntPoint(Width, Height);
	}
	else
	{
		Config.GridSize = FIntPoint(1, 1);
	}

	return Config;
}

// SOLID: Delegated to FInventoryContainerHelper
void UProjectInventoryComponent::UpsertContainerConfig(const FInventoryContainerConfig& GrantConfig, TArray<FInventoryContainerConfig>& OutContainers) const
{
	FInventoryContainerHelper::UpsertContainerConfig(GrantConfig, OutContainers);
}

// SOLID: Delegated to FInventoryGridPlacement
bool UProjectInventoryComponent::IsRectWithinContainer(const FInventoryContainerConfig& Container, FIntPoint GridPos, FIntPoint ItemSize) const
{
	return FInventoryGridPlacement::IsRectWithinContainer(Container, GridPos, ItemSize);
}

bool UProjectInventoryComponent::DoesRectOverlap(FGameplayTag ContainerId, FIntPoint GridPos, FIntPoint ItemSize, int32 IgnoreInstanceId) const
{
	// Check container config for special handling.
	FInventoryContainerConfig ContainerConfig;
	const bool bHasConfig = GetContainerConfig(ContainerId, ContainerConfig);
	const bool bSlotBased = bHasConfig && ContainerConfig.bSlotBased;

	// SOLID: Use helper for size clamping (width-only containers)
	const FIntPoint EffectiveItemSize = bHasConfig
		? FInventoryGridPlacement::ClampSizeForContainer(ContainerConfig, ItemSize)
		: ItemSize;

	for (const FInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.InstanceId == IgnoreInstanceId)
		{
			continue;
		}

		FGameplayTag EntryContainerId;
		FIntPoint EntryPos;
		bool bEntryRotated = false;
		if (!GetEffectiveEntryPlacement(Entry, EntryContainerId, EntryPos, bEntryRotated))
		{
			continue;
		}

		if (EntryContainerId != ContainerId)
		{
			continue;
		}

		// Slot-based: items overlap only if same slot index (GridPos.X).
		if (bSlotBased)
		{
			if (GridPos.X == EntryPos.X)
			{
				return true;
			}
			continue;
		}

		// Grid-based: AABB intersection test.
		FItemDataView EntryData;
		if (!GetItemDataView(Entry.ItemId, EntryData))
		{
			continue;
		}

		FIntPoint EntrySize = GetItemGridSize(EntryData, bEntryRotated);
		// SOLID: Clamp entry size for width-only containers
		if (bHasConfig)
		{
			EntrySize = FInventoryGridPlacement::ClampSizeForContainer(ContainerConfig, EntrySize);
		}

		// SOLID: Use helper for AABB test
		if (FInventoryGridPlacement::DoRectsOverlap(GridPos, EffectiveItemSize, EntryPos, EntrySize))
		{
			return true;
		}
	}

	return false;
}

// SOLID: Uses FInventoryGridPlacement with occupancy callback
bool UProjectInventoryComponent::FindFreeGridPos(const FInventoryContainerConfig& Container, FIntPoint ItemSize, int32 IgnoreInstanceId, FIntPoint& OutPos) const
{
	// Slot-based: log warning if no slot count defined
	if (Container.bSlotBased && Container.MaxCells <= 0 && Container.GridSize.X <= 0)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("FindFreeGridPos: Slot-based container '%s' has no MaxCells or GridSize.X"),
			*Container.ContainerId.ToString());
		return false;
	}

	// Use helper with occupancy callback that checks our inventory state
	return FInventoryGridPlacement::FindFreeGridPos(
		Container,
		ItemSize,
		[this, &Container, IgnoreInstanceId](FIntPoint TestPos, FIntPoint TestSize) {
			return DoesRectOverlap(Container.ContainerId, TestPos, TestSize, IgnoreInstanceId);
		},
		OutPos);
}

bool UProjectInventoryComponent::GetEffectiveEntryPlacement(const FInventoryEntry& Entry, FGameplayTag& OutContainerId, FIntPoint& OutGridPos, bool& bOutRotated) const
{
	// Equipped items have no grid placement (ContainerId cleared, GridPos = -1,-1)
	if (!Entry.ContainerId.IsValid() && Entry.GridPos == FIntPoint(-1, -1))
	{
		return false;
	}

	OutContainerId = Entry.ContainerId.IsValid() ? Entry.ContainerId : GetDefaultContainerId();
	OutGridPos = Entry.GridPos;
	bOutRotated = Entry.bRotated;

	if (OutGridPos.X >= 0 && OutGridPos.Y >= 0)
	{
		return true;
	}

	if (Entry.SlotIndex >= 0)
	{
		return TryGetGridPosFromSlot(OutContainerId, Entry.SlotIndex, OutGridPos);
	}

	return false;
}

// -------------------------------------------------------------------------
// Weight/volume per container
// -------------------------------------------------------------------------

// SOLID: Delegated to FInventoryWeightHelper
float UProjectInventoryComponent::GetContainerCurrentWeight(FGameplayTag ContainerId, TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache) const
{
	FInventoryWeightHelper::FWeightCallbacks Callbacks;
	Callbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	Callbacks.GetItemDataView = [this](FPrimaryAssetId Id, FItemDataView& D) {
		return GetItemDataView(Id, D);
	};
	return FInventoryWeightHelper::CalculateContainerWeight(Inventory.Entries, ContainerId, ItemDataCache, Callbacks);
}

// SOLID: Delegated to FInventoryWeightHelper
float UProjectInventoryComponent::GetContainerCurrentVolume(FGameplayTag ContainerId, TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache) const
{
	FInventoryWeightHelper::FWeightCallbacks Callbacks;
	Callbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	Callbacks.GetItemDataView = [this](FPrimaryAssetId Id, FItemDataView& D) {
		return GetItemDataView(Id, D);
	};
	return FInventoryWeightHelper::CalculateContainerVolume(Inventory.Entries, ContainerId, ItemDataCache, Callbacks);
}

// -------------------------------------------------------------------------
// Container policy
// -------------------------------------------------------------------------

bool UProjectInventoryComponent::ContainerAllowsItem(const FInventoryContainerConfig& Container, const FItemDataView& ItemData) const
{
	if (Container.AllowedTags.Num() == 0)
	{
		return true;
	}

	return ItemData.Tags.HasAny(Container.AllowedTags);
}

bool UProjectInventoryComponent::GetContainerOrder(TArray<FInventoryContainerConfig>& OutContainers) const
{
	GetEffectiveContainers(OutContainers);
	if (OutContainers.Num() == 0)
	{
		return false;
	}

	// Sort by placement priority: Backpack > Jacket pockets > Pants pockets > Hands
	// Lower value = higher priority (tried first for auto-placement)
	auto GetContainerPriority = [](const FGameplayTag& ContainerId) -> int32
	{
		if (ContainerId == ProjectTags::Item_Container_Backpack) return 0;
		if (ContainerId == ProjectTags::Item_Container_Pockets3) return 1;
		if (ContainerId == ProjectTags::Item_Container_Pockets4) return 2;
		if (ContainerId == ProjectTags::Item_Container_Pockets1) return 3;
		if (ContainerId == ProjectTags::Item_Container_Pockets2) return 4;
		if (ContainerId == ProjectTags::Item_Container_LeftHand) return 10;
		if (ContainerId == ProjectTags::Item_Container_RightHand) return 11;
		if (ContainerId == ProjectTags::Item_Container_Hands) return 12;
		return 5; // Unknown containers between pockets and hands
	};

	OutContainers.Sort([&GetContainerPriority](const FInventoryContainerConfig& A, const FInventoryContainerConfig& B)
	{
		return GetContainerPriority(A.ContainerId) < GetContainerPriority(B.ContainerId);
	});

	UE_LOG(LogProjectInventory, Verbose, TEXT("GetContainerOrder: %d containers sorted:"), OutContainers.Num());
	for (int32 i = 0; i < OutContainers.Num(); ++i)
	{
		UE_LOG(LogProjectInventory, Verbose, TEXT("  [%d] %s (%dx%d)"),
			i, *OutContainers[i].ContainerId.ToString(), OutContainers[i].GridSize.X, OutContainers[i].GridSize.Y);
	}

	return true;
}

bool UProjectInventoryComponent::GetEquipSlotContainerGrants(FGameplayTag EquipSlot, TArray<FInventoryContainerConfig>& OutConfigs) const
{
	OutConfigs.Reset();

	for (const FEquipSlotContainerGrant& Grant : EquipSlotContainerGrants)
	{
		if (Grant.EquipSlot == EquipSlot)
		{
			OutConfigs.Add(Grant.Container);
		}
	}

	return OutConfigs.Num() > 0;
}

bool UProjectInventoryComponent::IsContainerEmpty(FGameplayTag ContainerId) const
{
	if (!ContainerId.IsValid())
	{
		return true;
	}

	for (const FInventoryEntry& Entry : Inventory.Entries)
	{
		FGameplayTag EntryContainerId;
		FIntPoint EntryPos;
		bool bEntryRotated = false;
		if (!GetEffectiveEntryPlacement(Entry, EntryContainerId, EntryPos, bEntryRotated))
		{
			continue;
		}

		if (EntryContainerId == ContainerId)
		{
			return false;
		}
	}

	return true;
}
