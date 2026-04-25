// Copyright ALIS. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Helpers/InventoryViewHelper.h"
#include "Inventory/InventoryTypes.h"
#include "Interfaces/IItemDataProvider.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "ProjectGameplayTags.h"
#include "Types/EquippedItemData.h"
#include "Types/InventoryContainerConfig.h"
#include "Types/InventoryStackRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 AuthoredMaxStack = 60;
constexpr int32 AuthoredUnitsPerDepth = 30;

FItemDataView MakeDepthStackableItemData()
{
	FItemDataView ItemData;
	ItemData.bIsValid = true;
	ItemData.GridSize = FIntPoint(1, 1);
	ItemData.MaxStack = AuthoredMaxStack;
	ItemData.UnitsPerDepthUnit = AuthoredUnitsPerDepth;
	ItemData.Weight = 0.1f;
	ItemData.Volume = 0.05f;
	return ItemData;
}

FItemDataView MakeMultiCellItemData()
{
	FItemDataView ItemData;
	ItemData.bIsValid = true;
	ItemData.GridSize = FIntPoint(2, 2);
	ItemData.MaxStack = 10;
	ItemData.UnitsPerDepthUnit = 5;
	ItemData.Weight = 1.0f;
	ItemData.Volume = 0.5f;
	return ItemData;
}

FInventoryContainerConfig MakeContainer(FGameplayTag ContainerId, int32 CellDepthUnits)
{
	FInventoryContainerConfig Container;
	Container.ContainerId = ContainerId;
	Container.GridSize = FIntPoint(4, 4);
	Container.CellDepthUnits = CellDepthUnits;
	return Container;
}

FInventoryViewHelper::FViewCallbacks MakeCallbacks(
	const TMap<FPrimaryAssetId, FItemDataView>& ItemDataMap,
	const TMap<FGameplayTag, FInventoryContainerConfig>& ContainerMap)
{
	FInventoryViewHelper::FViewCallbacks Callbacks;
	Callbacks.GetItemDataView = [ItemDataMap](FPrimaryAssetId ItemId, FItemDataView& OutData)
	{
		if (const FItemDataView* Found = ItemDataMap.Find(ItemId))
		{
			OutData = *Found;
			return true;
		}
		return false;
	};
	Callbacks.GetEffectivePlacement = [](const FInventoryEntry& Entry, FGameplayTag& OutContainerId, FIntPoint& OutGridPos, bool& OutRotated)
	{
		OutContainerId = Entry.ContainerId;
		OutGridPos = Entry.GridPos;
		OutRotated = Entry.bRotated;
		return Entry.ContainerId.IsValid();
	};
	Callbacks.ComputeSlotIndex = [](FGameplayTag, FIntPoint) { return 0; };
	Callbacks.GetContainerConfig = [ContainerMap](FGameplayTag ContainerId, FInventoryContainerConfig& OutConfig)
	{
		if (const FInventoryContainerConfig* Found = ContainerMap.Find(ContainerId))
		{
			OutConfig = *Found;
			return true;
		}
		return false;
	};
	Callbacks.GetEquipSlotGrants = [](FGameplayTag, TArray<FInventoryContainerConfig>&) { return false; };
	Callbacks.GetContainerWeight = [](FGameplayTag, TMap<FPrimaryAssetId, FItemDataView>&) { return 0.0f; };
	Callbacks.GetContainerVolume = [](FGameplayTag, TMap<FPrimaryAssetId, FItemDataView>&) { return 0.0f; };
	return Callbacks;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectInventoryView_MaxStackProjectsContainerDepth,
	"ProjectInventory.View.MaxStackProjectsContainerDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryView_MaxStackProjectsContainerDepth::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId ItemId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("DepthStackable")));
	const FItemDataView ItemData = MakeDepthStackableItemData();

	const FInventoryContainerConfig ShallowContainer = MakeContainer(ProjectTags::Item_Container_Pockets, 1);
	const FInventoryContainerConfig DeepContainer = MakeContainer(ProjectTags::Item_Container_Backpack, 2);

	TMap<FPrimaryAssetId, FItemDataView> ItemDataMap;
	ItemDataMap.Add(ItemId, ItemData);

	TMap<FGameplayTag, FInventoryContainerConfig> ContainerMap;
	ContainerMap.Add(ShallowContainer.ContainerId, ShallowContainer);
	ContainerMap.Add(DeepContainer.ContainerId, DeepContainer);

	const FInventoryViewHelper::FViewCallbacks Callbacks = MakeCallbacks(ItemDataMap, ContainerMap);
	const TMap<FGameplayTag, FEquippedItemData> Equipped;

	const FInventoryEntry ShallowEntry(1, ItemId, 1, ShallowContainer.ContainerId, FIntPoint(0, 0), false, 0);
	const FInventoryEntry DeepEntry(2, ItemId, 1, DeepContainer.ContainerId, FIntPoint(0, 0), false, 0);

	const FInventoryEntryView ShallowView = FInventoryViewHelper::BuildEntryView(ShallowEntry, Equipped, Callbacks);
	const FInventoryEntryView DeepView = FInventoryViewHelper::BuildEntryView(DeepEntry, Equipped, Callbacks);

	const int32 ExpectedShallow = FInventoryStackRules::CalculateMaxStackForContainer(ItemData, ShallowContainer.CellDepthUnits);
	const int32 ExpectedDeep = FInventoryStackRules::CalculateMaxStackForContainer(ItemData, DeepContainer.CellDepthUnits);

	TestEqual(TEXT("Shallow view MaxStack matches container-effective rule"), ShallowView.MaxStack, ExpectedShallow);
	TestEqual(TEXT("Deep view MaxStack matches container-effective rule"), DeepView.MaxStack, ExpectedDeep);
	TestNotEqual(TEXT("Shallow and deep projections must differ"), ShallowView.MaxStack, DeepView.MaxStack);
	TestTrue(TEXT("Deep projection exceeds shallow"), DeepView.MaxStack > ShallowView.MaxStack);
	TestEqual(TEXT("Shallow MaxDepthUnits reflects container"), ShallowView.MaxDepthUnits, ShallowContainer.CellDepthUnits);
	TestEqual(TEXT("Deep MaxDepthUnits reflects container"), DeepView.MaxDepthUnits, DeepContainer.CellDepthUnits);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectInventoryView_MaxStackFallsBackToRawWhenConfigMissing,
	"ProjectInventory.View.MaxStackFallsBackToRawWhenConfigMissing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryView_MaxStackFallsBackToRawWhenConfigMissing::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId ItemId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("DepthStackable")));
	const FItemDataView ItemData = MakeDepthStackableItemData();

	TMap<FPrimaryAssetId, FItemDataView> ItemDataMap;
	ItemDataMap.Add(ItemId, ItemData);

	const TMap<FGameplayTag, FInventoryContainerConfig> EmptyContainerMap;
	FInventoryViewHelper::FViewCallbacks Callbacks = MakeCallbacks(ItemDataMap, EmptyContainerMap);
	const TMap<FGameplayTag, FEquippedItemData> Equipped;

	// Placement resolves, but container config lookup fails -> projection must fall back to raw.
	const FInventoryEntry Entry(1, ItemId, 1, ProjectTags::Item_Container_Backpack, FIntPoint(0, 0), false, 0);
	const FInventoryEntryView UnresolvedConfigView = FInventoryViewHelper::BuildEntryView(Entry, Equipped, Callbacks);

	TestEqual(
		TEXT("MaxStack falls back to raw item max when container config unavailable"),
		UnresolvedConfigView.MaxStack, ItemData.MaxStack);
	TestEqual(
		TEXT("MaxDepthUnits stays zero when container config unavailable"),
		UnresolvedConfigView.MaxDepthUnits, 0);

	// GetContainerConfig callback entirely absent -> same fallback path.
	Callbacks.GetContainerConfig = nullptr;
	const FInventoryEntryView NoCallbackView = FInventoryViewHelper::BuildEntryView(Entry, Equipped, Callbacks);

	TestEqual(
		TEXT("MaxStack falls back to raw item max when GetContainerConfig callback missing"),
		NoCallbackView.MaxStack, ItemData.MaxStack);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectInventoryView_MaxStackIgnoresContainerDepthForNonDepthStackable,
	"ProjectInventory.View.MaxStackIgnoresContainerDepthForNonDepthStackable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryView_MaxStackIgnoresContainerDepthForNonDepthStackable::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId ItemId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("MultiCell")));
	const FItemDataView ItemData = MakeMultiCellItemData();

	const FInventoryContainerConfig ShallowContainer = MakeContainer(ProjectTags::Item_Container_Pockets, 1);
	const FInventoryContainerConfig DeepContainer = MakeContainer(ProjectTags::Item_Container_Backpack, 4);

	TMap<FPrimaryAssetId, FItemDataView> ItemDataMap;
	ItemDataMap.Add(ItemId, ItemData);

	TMap<FGameplayTag, FInventoryContainerConfig> ContainerMap;
	ContainerMap.Add(ShallowContainer.ContainerId, ShallowContainer);
	ContainerMap.Add(DeepContainer.ContainerId, DeepContainer);

	const FInventoryViewHelper::FViewCallbacks Callbacks = MakeCallbacks(ItemDataMap, ContainerMap);
	const TMap<FGameplayTag, FEquippedItemData> Equipped;

	const FInventoryEntry ShallowEntry(1, ItemId, 1, ShallowContainer.ContainerId, FIntPoint(0, 0), false, 0);
	const FInventoryEntry DeepEntry(2, ItemId, 1, DeepContainer.ContainerId, FIntPoint(0, 0), false, 0);

	const FInventoryEntryView ShallowView = FInventoryViewHelper::BuildEntryView(ShallowEntry, Equipped, Callbacks);
	const FInventoryEntryView DeepView = FInventoryViewHelper::BuildEntryView(DeepEntry, Equipped, Callbacks);

	TestEqual(
		TEXT("Non-1x1 item: MaxStack stays at raw authored value in shallow container"),
		ShallowView.MaxStack, ItemData.MaxStack);
	TestEqual(
		TEXT("Non-1x1 item: MaxStack stays at raw authored value in deep container"),
		DeepView.MaxStack, ItemData.MaxStack);
	TestFalse(
		TEXT("Non-1x1 item: bUsesDepthStacking remains false"),
		ShallowView.bUsesDepthStacking);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
