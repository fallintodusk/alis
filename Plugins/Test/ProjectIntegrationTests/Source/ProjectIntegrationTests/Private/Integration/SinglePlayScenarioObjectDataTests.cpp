// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Data/ObjectDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UObjectDefinition* LoadScenarioDefinition(const TCHAR* ObjectName)
	{
		const FPrimaryAssetId Id(
			FPrimaryAssetType(TEXT("ObjectDefinition")),
			FName(ObjectName));
		UAssetManager& Manager = UAssetManager::Get();
		if (UObjectDefinition* Existing = Manager.GetPrimaryAssetObject<UObjectDefinition>(Id))
		{
			return Existing;
		}
		TSharedPtr<FStreamableHandle> Handle = Manager.LoadPrimaryAsset(Id, {});
		if (Handle.IsValid())
		{
			Handle->WaitUntilComplete();
		}
		return Manager.GetPrimaryAssetObject<UObjectDefinition>(Id);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSinglePlayScenarioObjectDataTest,
	"ProjectIntegrationTests.ProjectObject.Data.ScenarioCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSinglePlayScenarioObjectDataTest::RunTest(const FString& Parameters)
{
	UObjectDefinition* Pouch = LoadScenarioDefinition(TEXT("EmergencyPouch"));
	TestNotNull(TEXT("Emergency pouch definition is generated."), Pouch);
	if (Pouch == nullptr)
	{
		return false;
	}
	const FItemSection* PouchItem = Pouch->GetItemSection();
	TestNotNull(TEXT("Emergency pouch exposes item data."), PouchItem);
	if (PouchItem == nullptr || PouchItem->ContainerGrants.Num() != 1)
	{
		AddError(TEXT("Emergency pouch must expose exactly one container grant."));
		return false;
	}
	const FInventoryContainerGrantView& Grant = PouchItem->ContainerGrants[0];
	TestEqual(TEXT("Pouch grants the universal backpack container."),
		Grant.ContainerId.ToString(), FString(TEXT("Item.Container.Backpack")));
	TestEqual(TEXT("Pouch grid is 2x4."), Grant.GridSize, FIntPoint(2, 4));
	TestEqual(TEXT("Pouch weight cap is one kilogram."), Grant.MaxWeight, 1.0f);
	TestEqual(TEXT("Pouch volume cap is three liters."), Grant.MaxVolume, 3.0f);
	TestEqual(TEXT("Pouch has eight cells."), Grant.MaxCells, 8);

	struct FExpectedItem
	{
		const TCHAR* Id;
		float Weight;
		float Volume;
	};
	const FExpectedItem ExpectedItems[] = {
		{TEXT("EmergencyWater"), 0.8f, 0.75f},
		{TEXT("EmergencyRation"), 0.6f, 1.0f},
		{TEXT("EmergencyMedkit"), 0.6f, 1.5f},
		{TEXT("CompactPryBar"), 0.8f, 0.8f}};
	for (const FExpectedItem& Expected : ExpectedItems)
	{
		UObjectDefinition* Definition = LoadScenarioDefinition(Expected.Id);
		TestNotNull(FString::Printf(TEXT("%s definition is generated."), Expected.Id), Definition);
		if (Definition == nullptr || Definition->GetItemSection() == nullptr)
		{
			continue;
		}
		const FItemSection* Item = Definition->GetItemSection();
		TestEqual(FString::Printf(TEXT("%s uses a 2x2 hand footprint."), Expected.Id),
			Item->GridSize, FIntPoint(2, 2));
		TestEqual(FString::Printf(TEXT("%s weight is frozen."), Expected.Id),
			Item->Weight, Expected.Weight);
		TestEqual(FString::Printf(TEXT("%s volume is frozen."), Expected.Id),
			Item->Volume, Expected.Volume);
	}

	UObjectDefinition* Cache = LoadScenarioDefinition(TEXT("EmergencySupplyCache"));
	TestNotNull(TEXT("Emergency supply cache definition is generated."), Cache);
	const FStorageSection* Storage = Cache == nullptr ? nullptr : Cache->GetStorageSection();
	TestNotNull(TEXT("Emergency supply cache exposes deterministic storage."), Storage);
	if (Storage != nullptr)
	{
		TestEqual(TEXT("Cache contains exactly five deterministic records."),
			Storage->SeedEntries.Num(), 5);
		TestFalse(TEXT("Cache has no random loot profile."), Storage->LootProfileId.IsValid());
	}
	return true;
}

#endif
