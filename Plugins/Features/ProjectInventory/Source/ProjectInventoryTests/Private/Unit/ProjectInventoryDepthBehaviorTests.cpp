// Copyright ALIS. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Helpers/InventoryAddHelper.h"
#include "Helpers/InventoryLootHelper.h"
#include "Inventory/InventoryTypes.h"
#include "Interfaces/IItemDataProvider.h"
#include "Loot/LootTypes.h"
#include "ProjectGameplayTags.h"
#include "Types/InventoryStackRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FItemDataView MakeDepthLimitedItemData()
{
	FItemDataView ItemData;
	ItemData.GridSize = FIntPoint(1, 1);
	ItemData.MaxStack = 10;
	ItemData.UnitsPerDepthUnit = 1;
	ItemData.Weight = 0.1f;
	ItemData.Volume = 0.05f;
	return ItemData;
}

int32 ResolveDepthLimitedMaxStack(const FInventoryContainerConfig& Container, const FItemDataView& ItemData)
{
	return FInventoryStackRules::CalculateMaxStackForContainer(ItemData, Container.CellDepthUnits);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectInventoryDepth_StackTargetsRespectContainerDepthCap,
	"ProjectInventory.Depth.StackTargetsRespectContainerDepthCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryDepth_StackTargetsRespectContainerDepthCap::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId ItemId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("DepthStackItem")));
	const FItemDataView ItemData = MakeDepthLimitedItemData();

	FInventoryContainerConfig Container;
	Container.ContainerId = ProjectTags::Item_Container_LeftHand;
	Container.GridSize = FIntPoint(2, 2);
	Container.CellDepthUnits = 2;

	TArray<FInventoryEntry> Entries;
	Entries.Add(FInventoryEntry(1, ItemId, 2, Container.ContainerId, FIntPoint(0, 0), false, 0));

	TArray<FInventoryAddHelper::FContainerState> ContainerStates;
	FInventoryAddHelper::FContainerState State;
	State.Config = Container;
	State.CurrentWeight = ItemData.Weight * 2.0f;
	State.CurrentVolume = ItemData.Volume * 2.0f;
	ContainerStates.Add(State);

	FInventoryAddHelper::FAddCallbacks Callbacks;
	Callbacks.GetEffectivePlacement = [](const FInventoryEntry& Entry, FGameplayTag& OutContainerId, FIntPoint& OutGridPos, bool& OutRotated)
	{
		OutContainerId = Entry.ContainerId;
		OutGridPos = Entry.GridPos;
		OutRotated = Entry.bRotated;
		return true;
	};
	Callbacks.FindFreeGridPos = [](const FInventoryContainerConfig&, FIntPoint, int32, FIntPoint& OutGridPos)
	{
		OutGridPos = FIntPoint(-1, -1);
		return false;
	};
	Callbacks.ContainerAllowsItem = [](const FInventoryContainerConfig&, const FItemDataView&) { return true; };
	Callbacks.GetEffectiveMaxStack = [](const FInventoryContainerConfig& Config, const FItemDataView& Data)
	{
		return ResolveDepthLimitedMaxStack(Config, Data);
	};

	TArray<FInventoryAddHelper::FStackTarget> Targets;
	const int32 Remaining = FInventoryAddHelper::CalculateStackTargets(
		ItemId,
		ItemData,
		1,
		Entries,
		ContainerStates,
		Callbacks,
		Targets);

	TestEqual(TEXT("Depth-capped full stack should reject additional overlap quantity"), Remaining, 1);
	TestEqual(TEXT("No stack targets should be produced when the target cell is depth-full"), Targets.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectInventoryDepth_CanFitItemsRejectsDepthOverflow,
	"ProjectInventory.Depth.CanFitItemsRejectsDepthOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryDepth_CanFitItemsRejectsDepthOverflow::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId ItemId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("DepthLootItem")));
	const FItemDataView ItemData = MakeDepthLimitedItemData();

	FInventoryLootHelper::FSimulationInput Input;
	FInventoryContainerConfig Container;
	Container.ContainerId = ProjectTags::Item_Container_LeftHand;
	Container.GridSize = FIntPoint(1, 1);
	Container.CellDepthUnits = 1;
	Input.ContainerOrder.Add(Container);
	Input.MaxWeight = 10.0f;
	Input.MaxVolume = 10.0f;

	TArray<FLootEntry> Items;
	Items.Add(FLootEntry(ItemId, 2));

	FInventoryLootHelper::FSimulationCallbacks Callbacks;
	Callbacks.ResolveItemData = [&ItemId, &ItemData](FPrimaryAssetId InItemId, FItemDataView& OutData)
	{
		if (InItemId != ItemId)
		{
			return false;
		}
		OutData = ItemData;
		return true;
	};
	Callbacks.GetEffectivePlacement = [](const FInventoryEntry&, FGameplayTag&, FIntPoint&, bool&) { return false; };
	Callbacks.GetItemGridSize = [](const FItemDataView& Data, bool bRotated)
	{
		return bRotated ? FIntPoint(Data.GridSize.Y, Data.GridSize.X) : Data.GridSize;
	};
	Callbacks.ContainerAllowsItem = [](const FInventoryContainerConfig&, const FItemDataView&) { return true; };
	Callbacks.GetEffectiveMaxStack = [](const FInventoryContainerConfig& Config, const FItemDataView& Data)
	{
		return ResolveDepthLimitedMaxStack(Config, Data);
	};

	const bool bCanFit = FInventoryLootHelper::CanFitItems(Items, Input, Callbacks);
	TestFalse(TEXT("CanFitItems should reject quantity that exceeds a single shallow cell depth cap"), bCanFit);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
