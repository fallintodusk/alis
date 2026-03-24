// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Data/LootProfileDefinition.h"
#include "Data/ObjectDefinition.h"
#include "DefinitionGeneratorEditorSubsystem.h"
#include "DefinitionGeneratorSubsystem.h"
#include "DefinitionJsonParser.h"
#include "DefinitionTypeInfo.h"
#include "Components/ProjectInventoryComponent.h"
#include "Interfaces/IInventoryWorldContainerTransferBridge.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "MVVM/InventoryViewModel.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryDragVisualBuilder.h"
#include "Spawning/ObjectSpawnUtility.h"
#include "LootContainer/LootContainerCapabilityComponent.h"
#include "Integration/Fixtures/WorldContainerSessionTestDouble.h"
#include "ProjectGameplayTags.h"
#include "Subsystems/ProjectContainerSessionSubsystem.h"
#include "Types/ContainerSessionTypes.h"
#include "Types/WorldContainerKey.h"

#include "Dom/JsonObject.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "ProjectWidgetHelpers.h"

namespace
{
UWorld* ResolveInventoryLootPlacesTestWorld()
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (World)
	{
		return World;
	}

	if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
	{
		return nullptr;
	}

	return AutomationCommon::GetAnyGameWorld();
}

ULocalPlayer* ResolveInventoryLootPlacesTestLocalPlayer(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			return LocalPlayer;
		}
	}

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		const TArray<ULocalPlayer*>& LocalPlayers = GameInstance->GetLocalPlayers();
		for (ULocalPlayer* LocalPlayer : LocalPlayers)
		{
			if (LocalPlayer)
			{
				return LocalPlayer;
			}
		}
	}

	return nullptr;
}

bool EnsureInventoryLootPlacesTestAssetLoaded(const FPrimaryAssetId& ObjectId)
{
	if (!ObjectId.IsValid())
	{
		return false;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	if (AssetManager.GetPrimaryAssetObject<UObject>(ObjectId))
	{
		return true;
	}

	TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAsset(ObjectId, TArray<FName>());
	if (Handle.IsValid())
	{
		Handle->WaitUntilComplete();
	}

	return AssetManager.GetPrimaryAssetObject<UObject>(ObjectId) != nullptr;
}

struct FScopedInventoryLootPlacesItemOverride
{
	FScopedInventoryLootPlacesItemOverride(const FPrimaryAssetId& InObjectId)
		: ObjectId(InObjectId)
	{
		if (!EnsureInventoryLootPlacesTestAssetLoaded(ObjectId))
		{
			return;
		}

		Definition = Cast<UObjectDefinition>(UAssetManager::Get().GetPrimaryAssetObject(ObjectId));
		if (!Definition)
		{
			return;
		}

		FItemSection* ItemSection = Definition->GetMutableItemSection();
		if (!ItemSection)
		{
			Definition = nullptr;
			return;
		}

		OriginalGridSize = ItemSection->GridSize;
		OriginalMaxStack = ItemSection->MaxStack;
		OriginalUnitsPerDepthUnit = ItemSection->UnitsPerDepthUnit;
		bValid = true;
	}

	~FScopedInventoryLootPlacesItemOverride()
	{
		if (!bValid || !Definition)
		{
			return;
		}

		if (FItemSection* ItemSection = Definition->GetMutableItemSection())
		{
			ItemSection->GridSize = OriginalGridSize;
			ItemSection->MaxStack = OriginalMaxStack;
			ItemSection->UnitsPerDepthUnit = OriginalUnitsPerDepthUnit;
		}
	}

	bool IsValid() const
	{
		return bValid && Definition != nullptr;
	}

	void ApplyDepthStacking(FIntPoint InGridSize, int32 InMaxStack, int32 InUnitsPerDepthUnit)
	{
		if (!IsValid())
		{
			return;
		}

		if (FItemSection* ItemSection = Definition->GetMutableItemSection())
		{
			ItemSection->GridSize = InGridSize;
			ItemSection->MaxStack = InMaxStack;
			ItemSection->UnitsPerDepthUnit = InUnitsPerDepthUnit;
		}
	}

private:
	FPrimaryAssetId ObjectId;
	TObjectPtr<UObjectDefinition> Definition;
	FIntPoint OriginalGridSize = FIntPoint(1, 1);
	int32 OriginalMaxStack = 1;
	int32 OriginalUnitsPerDepthUnit = 0;
	bool bValid = false;
};

bool LoadInventoryLootPlacesDefinitionFromFile(
	const FString& RelativePath,
	TStrongObjectPtr<UObjectDefinition>& OutDefinition,
	FString& OutError)
{
	OutDefinition.Reset();
	OutError.Reset();

	const FString AbsolutePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativePath);
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *AbsolutePath))
	{
		OutError = FString::Printf(TEXT("Failed to read %s"), *AbsolutePath);
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Definition(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	if (!Definition.IsValid())
	{
		OutError = TEXT("Failed to allocate transient ObjectDefinition");
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutError = FString::Printf(TEXT("Failed to deserialize JSON from %s"), *AbsolutePath);
		return false;
	}

	if (!FDefinitionJsonParser::ParseJsonToAsset(TypeInfo, JsonObject, Definition.Get(), OutError))
	{
		return false;
	}

	OutDefinition = MoveTemp(Definition);
	return true;
}

bool LoadInventoryLootPlacesLootProfileFromFile(
	const FString& RelativePath,
	TStrongObjectPtr<ULootProfileDefinition>& OutProfile,
	FString& OutError)
{
	OutProfile.Reset();
	OutError.Reset();

	const FString AbsolutePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativePath);
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *AbsolutePath))
	{
		OutError = FString::Printf(TEXT("Failed to read %s"), *AbsolutePath);
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = ULootProfileDefinition::StaticClass();

	TStrongObjectPtr<ULootProfileDefinition> Profile(
		NewObject<ULootProfileDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	if (!Profile.IsValid())
	{
		OutError = TEXT("Failed to allocate transient LootProfileDefinition");
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutError = FString::Printf(TEXT("Failed to deserialize JSON from %s"), *AbsolutePath);
		return false;
	}

	if (!FDefinitionJsonParser::ParseJsonToAsset(TypeInfo, JsonObject, Profile.Get(), OutError))
	{
		return false;
	}

	OutProfile = MoveTemp(Profile);
	return true;
}

UProjectInventoryComponent* CreateInventoryLootPlacesTestInventory(UWorld* World, AActor*& OutOwner)
{
	OutOwner = nullptr;
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	OutOwner = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!OutOwner)
	{
		return nullptr;
	}

	UProjectInventoryComponent* Inventory = NewObject<UProjectInventoryComponent>(OutOwner);
	OutOwner->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	return Inventory;
}

UProjectInventoryComponent* CreateInventoryLootPlacesPlayerInventory(UWorld* World, APlayerController*& OutOwner)
{
	OutOwner = World ? World->GetFirstPlayerController() : nullptr;
	if (!OutOwner)
	{
		return nullptr;
	}

	UProjectInventoryComponent* Inventory = NewObject<UProjectInventoryComponent>(OutOwner);
	OutOwner->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	return Inventory;
}

UObject* ResolveInventoryLootPlacesWorldContainerSource(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return nullptr;
	}

	if (TargetActor->Implements<UWorldContainerSessionSource>())
	{
		return TargetActor;
	}

	TInlineComponentArray<UActorComponent*> Components;
	TargetActor->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (Component && Component->Implements<UWorldContainerSessionSource>())
		{
			return Component;
		}
	}

	return nullptr;
}

AActor* CreateWorldContainerSessionTestActor(
	UWorld* World,
	UWorldContainerSessionTestDouble*& OutSessionSource)
{
	OutSessionSource = nullptr;
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	AActor* Owner = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!Owner)
	{
		return nullptr;
	}

	OutSessionSource = NewObject<UWorldContainerSessionTestDouble>(Owner);
	Owner->AddInstanceComponent(OutSessionSource);
	OutSessionSource->RegisterComponent();

	OutSessionSource->DisplayLabel = FText::FromString(TEXT("TestContainer"));
	OutSessionSource->ContainerView.ContainerId = ProjectTags::Item_Container_WorldStorage;
	OutSessionSource->ContainerView.GridSize = FIntPoint(4, 5);
	OutSessionSource->ContainerView.MaxCells = 20;
	OutSessionSource->ContainerKey.InstanceId = FGuid::NewGuid();
	OutSessionSource->ContainerKey.WorldScopeId = FName(TEXT("Automation"));
	OutSessionSource->ContainerKey.ContainerSlotId = FName(TEXT("Primary"));
	return Owner;
}

FInventoryEntryView MakeWorldContainerEntryView(
	const FPrimaryAssetId& ObjectId,
	int32 InstanceId,
	int32 Quantity,
	FIntPoint GridPos = FIntPoint(0, 0),
	FIntPoint GridSize = FIntPoint(1, 1))
{
	FInventoryEntryView EntryView;
	EntryView.ItemId = ObjectId;
	EntryView.InstanceId = InstanceId;
	EntryView.Quantity = Quantity;
	EntryView.ContainerId = ProjectTags::Item_Container_WorldStorage;
	EntryView.GridPos = GridPos;
	EntryView.GridSize = GridSize;
	return EntryView;
}

TSharedPtr<FJsonObject> MakeLootContainerJson(
	const FString& ObjectId,
	bool bIncludeStorage,
	const FString& StorageEntryId = TEXT("ObjectDefinition:StorageSeed"),
	int32 StorageQuantity = 2)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("id"), ObjectId);

	TSharedPtr<FJsonObject> Mesh = MakeShared<FJsonObject>();
	Mesh->SetStringField(TEXT("id"), TEXT("body"));
	Mesh->SetStringField(TEXT("asset"), TEXT("/Engine/BasicShapes/Cube"));

	TArray<TSharedPtr<FJsonValue>> Meshes;
	Meshes.Add(MakeShared<FJsonValueObject>(Mesh));
	Root->SetArrayField(TEXT("meshes"), Meshes);

	TSharedPtr<FJsonObject> Capability = MakeShared<FJsonObject>();
	Capability->SetStringField(TEXT("type"), TEXT("LootContainer"));

	TArray<TSharedPtr<FJsonValue>> ScopeValues;
	ScopeValues.Add(MakeShared<FJsonValueString>(TEXT("actor")));
	Capability->SetArrayField(TEXT("scope"), ScopeValues);

	TArray<TSharedPtr<FJsonValue>> Capabilities;
	Capabilities.Add(MakeShared<FJsonValueObject>(Capability));
	Root->SetArrayField(TEXT("capabilities"), Capabilities);

	TSharedPtr<FJsonObject> Sections = MakeShared<FJsonObject>();

	if (bIncludeStorage)
	{
		TSharedPtr<FJsonObject> Storage = MakeShared<FJsonObject>();
		Storage->SetStringField(TEXT("gridSize"), TEXT("4,5"));
		Storage->SetNumberField(TEXT("maxWeight"), 15.0);
		Storage->SetNumberField(TEXT("maxVolume"), 30.0);
		Storage->SetNumberField(TEXT("maxCells"), 20);
		Storage->SetBoolField(TEXT("persistent"), true);
		Storage->SetStringField(TEXT("containerSlotId"), TEXT("Primary"));

		TSharedPtr<FJsonObject> SeedEntry = MakeShared<FJsonObject>();
		SeedEntry->SetStringField(TEXT("objectId"), StorageEntryId);
		SeedEntry->SetNumberField(TEXT("quantity"), StorageQuantity);

		TArray<TSharedPtr<FJsonValue>> SeedEntries;
		SeedEntries.Add(MakeShared<FJsonValueObject>(SeedEntry));
		Storage->SetArrayField(TEXT("seedEntries"), SeedEntries);

		Sections->SetObjectField(TEXT("storage"), Storage);
	}

	Root->SetObjectField(TEXT("sections"), Sections);
	return Root;
}

TSharedPtr<FJsonObject> MakeLegacyLootContainerJson(
	const FString& ObjectId,
	const FString& LegacyEntryId = TEXT("ObjectDefinition:LegacySeed"),
	int32 LegacyQuantity = 5)
{
	TSharedPtr<FJsonObject> Root = MakeLootContainerJson(ObjectId, false);
	TSharedPtr<FJsonObject> Sections = Root->GetObjectField(TEXT("sections"));

	TSharedPtr<FJsonObject> Loot = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> LootEntry = MakeShared<FJsonObject>();
	LootEntry->SetStringField(TEXT("objectId"), LegacyEntryId);
	LootEntry->SetNumberField(TEXT("quantity"), LegacyQuantity);

	TArray<TSharedPtr<FJsonValue>> LootEntries;
	LootEntries.Add(MakeShared<FJsonValueObject>(LootEntry));
	Loot->SetArrayField(TEXT("entries"), LootEntries);
	Sections->SetObjectField(TEXT("loot"), Loot);
	return Root;
}

TSharedPtr<FJsonObject> MakeLootProfileJson(
	const FString& ProfileId,
	int32 PickCountMin,
	int32 PickCountMax,
	const TArray<FString>& ObjectIds)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("profileId"), ProfileId);
	Root->SetNumberField(TEXT("pickCountMin"), PickCountMin);
	Root->SetNumberField(TEXT("pickCountMax"), PickCountMax);

	TArray<TSharedPtr<FJsonValue>> EntryValues;
	for (const FString& ObjectId : ObjectIds)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("objectId"), ObjectId);
		Entry->SetNumberField(TEXT("quantity"), 1);
		EntryValues.Add(MakeShared<FJsonValueObject>(Entry));
	}

	Root->SetArrayField(TEXT("entries"), EntryValues);
	return Root;
}

void SetStorageLootProfileId(const TSharedPtr<FJsonObject>& Storage, const FString& LootProfileId)
{
	if (Storage.IsValid())
	{
		Storage->SetStringField(TEXT("lootProfileId"), LootProfileId);
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_WorldContainerKeyContractTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Contract.WorldContainerKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_WorldContainerKeyContractTest::RunTest(const FString& Parameters)
{
	FWorldContainerKey InvalidKey;
	TestFalse(TEXT("Default world container key should be invalid"), InvalidKey.IsValid());

	FWorldContainerKey ValidKey;
	ValidKey.InstanceId = FGuid::NewGuid();
	ValidKey.WorldScopeId = FName(TEXT("MainMenu_Persistent"));
	ValidKey.ContainerSlotId = FName(TEXT("Primary"));

	TestTrue(TEXT("Key with instance id and slot should be valid"), ValidKey.IsValid());
	TestTrue(TEXT("Key string should include slot id"), ValidKey.ToString().Contains(TEXT("Primary")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_StorageSectionParserTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Parser.StorageSection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_StorageSectionParserTest::RunTest(const FString& Parameters)
{
	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	TSharedPtr<FJsonObject> Root = MakeLootContainerJson(TEXT("StorageParserOnly"), true);
	TSharedPtr<FJsonObject> Sections = Root->GetObjectField(TEXT("sections"));
	TSharedPtr<FJsonObject> Storage = Sections->GetObjectField(TEXT("storage"));
	Storage->SetNumberField(TEXT("cellDepthUnits"), 4);

	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		Root,
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Storage section should parse"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	const FStorageSection* StorageSection = Def->GetStorageSection();
	TestNotNull(TEXT("Definition should expose storage section"), StorageSection);
	if (!StorageSection)
	{
		return false;
	}

	TestEqual(TEXT("GridSize should parse from storage section"), StorageSection->GridSize, FIntPoint(4, 5));
	TestEqual(TEXT("MaxWeight should parse"), StorageSection->MaxWeight, 15.0f);
	TestEqual(TEXT("MaxVolume should parse"), StorageSection->MaxVolume, 30.0f);
	TestEqual(TEXT("MaxCells should parse"), StorageSection->MaxCells, 20);
	TestEqual(TEXT("CellDepthUnits should parse"), StorageSection->CellDepthUnits, 4);
	TestTrue(TEXT("Persistent flag should parse"), StorageSection->Persistent);
	TestEqual(TEXT("ContainerSlotId should parse"), StorageSection->ContainerSlotId, FName(TEXT("Primary")));
	TestEqual(TEXT("SeedEntries should parse"), StorageSection->SeedEntries.Num(), 1);
	if (StorageSection->SeedEntries.Num() == 1)
	{
		TestEqual(TEXT("Seed entry id should parse"),
			StorageSection->SeedEntries[0].ObjectId.ToString(),
			FString(TEXT("ObjectDefinition:StorageSeed")));
		TestEqual(TEXT("Seed entry quantity should parse"), StorageSection->SeedEntries[0].Quantity, 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_StorageLootProfileIdParserTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Parser.StorageLootProfileId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_StorageLootProfileIdParserTest::RunTest(const FString& Parameters)
{
	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	TSharedPtr<FJsonObject> Root = MakeLootContainerJson(TEXT("StorageLootProfileParser"), true);
	TSharedPtr<FJsonObject> Sections = Root->GetObjectField(TEXT("sections"));
	TSharedPtr<FJsonObject> Storage = Sections->GetObjectField(TEXT("storage"));
	SetStorageLootProfileId(Storage, TEXT("LootProfileDefinition:Scavenge_SmallConsumables"));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		Root,
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Storage loot profile id should parse"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	const FStorageSection* StorageSection = Def->GetStorageSection();
	TestNotNull(TEXT("Definition should expose storage section with loot profile"), StorageSection);
	if (!StorageSection)
	{
		return false;
	}

	TestTrue(TEXT("Storage section should report loot profile"), StorageSection->HasLootProfile());
	TestEqual(TEXT("LootProfileId should parse"),
		StorageSection->LootProfileId.ToString(),
		FString(TEXT("LootProfileDefinition:Scavenge_SmallConsumables")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_LootProfileDefinitionParserTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Parser.LootProfileDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_LootProfileDefinitionParserTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = ULootProfileDefinition::StaticClass();

	TStrongObjectPtr<ULootProfileDefinition> Profile(
		NewObject<ULootProfileDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootProfileJson(TEXT("ParserProfile"), 2, 3, {
			TEXT("ObjectDefinition:WaterBottle"),
			TEXT("ObjectDefinition:BreadSlice"),
			TEXT("ObjectDefinition:BraisedBeans")
		}),
		Profile.Get(),
		ParseError);

	TestTrue(TEXT("Loot profile definition should parse"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	TestEqual(TEXT("Profile id should parse"), Profile->ProfileId, FName(TEXT("ParserProfile")));
	TestEqual(TEXT("PickCountMin should parse"), Profile->PickCountMin, 2);
	TestEqual(TEXT("PickCountMax should parse"), Profile->PickCountMax, 3);
	TestEqual(TEXT("Entries should parse"), Profile->Entries.Num(), 3);
	if (Profile->Entries.Num() == 3)
	{
		TestEqual(TEXT("First profile entry id should parse"),
			Profile->Entries[0].ObjectId.ToString(),
			FString(TEXT("ObjectDefinition:WaterBottle")));
		TestEqual(TEXT("Profile entry quantity should default to 1"), Profile->Entries[0].Quantity, 1);
	}

	TArray<FLootEntryView> EntryViews;
	FRandomStream RandomStream(12345);
	Profile->BuildLootEntries(EntryViews, &RandomStream);

	TestTrue(TEXT("Profile should realize into 2-3 loot entry views"),
		EntryViews.Num() >= 2 && EntryViews.Num() <= 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_InlineRandomPoolRejectedTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Parser.InlineRandomPoolRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_InlineRandomPoolRejectedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	TSharedPtr<FJsonObject> Root = MakeLootContainerJson(TEXT("InlineRandomRejected"), true);
	TSharedPtr<FJsonObject> Sections = Root->GetObjectField(TEXT("sections"));
	TSharedPtr<FJsonObject> Storage = Sections->GetObjectField(TEXT("storage"));
	Storage->SetNumberField(TEXT("randomPickCountMin"), 2);
	Storage->SetNumberField(TEXT("randomPickCountMax"), 3);
	Storage->SetArrayField(TEXT("randomPool"), TArray<TSharedPtr<FJsonValue>>());

	AddExpectedError(TEXT("sections.storage.randomPool has been removed. Use sections.storage.lootProfileId."), EAutomationExpectedErrorFlags::Contains, 1);

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		Root,
		Def.Get(),
		ParseError);

	TestFalse(TEXT("Inline storage randomPool should be rejected"), bParsed);
	TestTrue(TEXT("Parse error should mention sections.storage.lootProfileId"), ParseError.Contains(TEXT("sections.storage.lootProfileId")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_LegacyLootRejectedTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Parser.LegacyLootRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_LegacyLootRejectedTest::RunTest(const FString& Parameters)
{
	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	AddExpectedError(TEXT("sections.loot has been removed. Use sections.storage.seedEntries or sections.storage.lootProfileId."), EAutomationExpectedErrorFlags::Contains, 1);

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLegacyLootContainerJson(TEXT("LegacyLootRejected")),
		Def.Get(),
		ParseError);

	TestFalse(TEXT("Legacy loot section should be rejected"), bParsed);
	TestTrue(TEXT("Parse error should mention sections.storage.seedEntries"), ParseError.Contains(TEXT("sections.storage.seedEntries")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_LootProfileValidationUsesConfiguredIdFieldTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Editor.ValidationUsesConfiguredLootProfileIdField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_LootProfileValidationUsesConfiguredIdFieldTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestNotNull(TEXT("GEditor should exist for definition validation test"), GEditor);
	if (!GEditor)
	{
		return false;
	}

	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	TestNotNull(TEXT("Definition generator subsystem should exist"), Generator);
	if (!Generator)
	{
		return false;
	}

	Generator->DiscoverAndRegisterManifests();

	const FPrimaryAssetId LootProfileId = FPrimaryAssetId::FromString(TEXT("LootProfileDefinition:Scavenge_SmallConsumables"));
	TestTrue(TEXT("Shared loot profile asset should load for validation test"), EnsureInventoryLootPlacesTestAssetLoaded(LootProfileId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(LootProfileId))
	{
		return false;
	}

	UDefinitionGeneratorEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UDefinitionGeneratorEditorSubsystem>();
	TestNotNull(TEXT("Definition generator editor subsystem should exist"), EditorSubsystem);
	if (!EditorSubsystem)
	{
		return false;
	}

	const TArray<FDefinitionValidationResult> Results = EditorSubsystem->ValidateType(TEXT("LootProfileDefinition"));
	const FDefinitionValidationResult* TargetResult = Results.FindByPredicate([](const FDefinitionValidationResult& Result)
	{
		return Result.AssetId == TEXT("Scavenge_SmallConsumables");
	});

	TestNotNull(TEXT("Validation should resolve Scavenge_SmallConsumables by configured loot-profile id field"), TargetResult);
	if (!TargetResult)
	{
		return false;
	}

	TestFalse(TEXT("Up-to-date shared loot profile should not be flagged for regeneration"), TargetResult->bNeedsRegeneration);
	if (TargetResult->bNeedsRegeneration)
	{
		AddError(FString::Printf(TEXT("Unexpected validation reason: %s"), *TargetResult->Reason));
	}

	return !TargetResult->bNeedsRegeneration;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_SpawnUsesStorageSeedEntriesTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Spawn.UsesStorageSeedEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_SpawnUsesStorageSeedEntriesTest::RunTest(const FString& Parameters)
{
	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootContainerJson(TEXT("StorageSeedOnly"), true, TEXT("ObjectDefinition:StorageWins"), 2),
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Definition with storage seed entries should parse"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should be created"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = Spawned->FindComponentByClass<ULootContainerCapabilityComponent>();
	TestNotNull(TEXT("Loot container capability should be added"), LootComponent);
	if (!LootComponent)
	{
		Spawned->Destroy();
		return false;
	}

	TestEqual(TEXT("InstanceLoot should come from storage seed entries"), LootComponent->InstanceLoot.Num(), 1);
	if (LootComponent->InstanceLoot.Num() == 1)
	{
		TestEqual(TEXT("Storage object id should seed the component"),
			LootComponent->InstanceLoot[0].ObjectId.ToString(),
			FString(TEXT("ObjectDefinition:StorageWins")));
		TestEqual(TEXT("Storage quantity should seed the component"), LootComponent->InstanceLoot[0].Quantity, 2);
	}

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_SpawnUsesStorageLootProfileTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Spawn.UsesStorageLootProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_SpawnUsesStorageLootProfileTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId LootProfileId = FPrimaryAssetId::FromString(TEXT("LootProfileDefinition:Scavenge_SmallConsumables"));

	TestTrue(TEXT("Loot profile asset should load for loot-profile spawn test"),
		EnsureInventoryLootPlacesTestAssetLoaded(LootProfileId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(LootProfileId))
	{
		return false;
	}

	ULootProfileDefinition* LootProfile = UAssetManager::Get().GetPrimaryAssetObject<ULootProfileDefinition>(LootProfileId);
	TestNotNull(TEXT("Loot profile object should resolve for loot-profile spawn test"), LootProfile);
	if (!LootProfile)
	{
		return false;
	}

	TSet<FPrimaryAssetId> CandidateIds;
	for (const FLootProfileEntry& ProfileEntry : LootProfile->Entries)
	{
		CandidateIds.Add(ProfileEntry.ObjectId);
		TestTrue(FString::Printf(TEXT("%s should load for loot-profile spawn test"), *ProfileEntry.ObjectId.ToString()),
			EnsureInventoryLootPlacesTestAssetLoaded(ProfileEntry.ObjectId));
		if (!EnsureInventoryLootPlacesTestAssetLoaded(ProfileEntry.ObjectId))
		{
			return false;
		}
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	TSharedPtr<FJsonObject> Root = MakeLootContainerJson(TEXT("StorageLootProfileSpawn"), true, TEXT("ObjectDefinition:StorageWins"), 1);
	TSharedPtr<FJsonObject> Sections = Root->GetObjectField(TEXT("sections"));
	TSharedPtr<FJsonObject> Storage = Sections->GetObjectField(TEXT("storage"));
	Storage->SetArrayField(TEXT("seedEntries"), TArray<TSharedPtr<FJsonValue>>());
	SetStorageLootProfileId(Storage, LootProfileId.ToString());

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		Root,
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Definition with storage loot profile should parse"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should be created"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = Spawned->FindComponentByClass<ULootContainerCapabilityComponent>();
	TestNotNull(TEXT("Loot container capability should be added"), LootComponent);
	if (!LootComponent)
	{
		Spawned->Destroy();
		return false;
	}

	TestTrue(TEXT("Loot profile should realize into 2-3 instance loot entries"),
		LootComponent->InstanceLoot.Num() >= 2 && LootComponent->InstanceLoot.Num() <= 3);

	TSet<FPrimaryAssetId> SeenIds;
	for (const FLootEntryView& Entry : LootComponent->InstanceLoot)
	{
		TestEqual(TEXT("Loot profile entry quantity should stay 1"), Entry.Quantity, 1);
		TestTrue(TEXT("Loot profile entry should come from authored candidate set"), CandidateIds.Contains(Entry.ObjectId));
		TestTrue(TEXT("Loot profile entries should be unique per container spawn"), !SeenIds.Contains(Entry.ObjectId));
		SeenIds.Add(Entry.ObjectId);
	}

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_FullOpenSessionSubsystemTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Session.FullOpenOpenClose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_FullOpenSessionSubsystemTest::RunTest(const FString& Parameters)
{
	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for session open/close test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem =
		LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootContainerJson(TEXT("SessionOpenClose"), true, TEXT("ObjectDefinition:WaterBottle"), 1),
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Definition should parse for session test"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should be created"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = Spawned->FindComponentByClass<ULootContainerCapabilityComponent>();
	TestNotNull(TEXT("Loot container capability should be added"), LootComponent);
	if (!LootComponent)
	{
		Spawned->Destroy();
		return false;
	}

	FText OpenError;
	FContainerSessionHandle SessionHandle;
	const bool bOpened = SessionSubsystem->OpenWorldContainerSession(
		Spawned,
		EContainerSessionMode::FullOpen,
		SessionHandle,
		OpenError);

	TestTrue(TEXT("FullOpen session should open"), bOpened);
	TestTrue(TEXT("Session handle should be valid"), SessionHandle.IsValid());
	TestTrue(TEXT("Subsystem should report active session"), SessionSubsystem->HasAnyActiveSession());
	TestTrue(TEXT("Subsystem should report active full-open session"), SessionSubsystem->HasActiveFullOpenSession());
	TestTrue(TEXT("Loot container should report active full-open session"),
		IWorldContainerSessionSource::Execute_HasActiveFullOpenSession(LootComponent));

	FWorldContainerKey ContainerKey =
		IWorldContainerSessionSource::Execute_GetWorldContainerKey(LootComponent);
	TestTrue(TEXT("Loot container key should be valid after spawn"), ContainerKey.IsValid());
	TestEqual(TEXT("Session handle should carry the container key"), SessionHandle.ContainerKey.ContainerSlotId, ContainerKey.ContainerSlotId);

	const bool bClosed = SessionSubsystem->CloseSession(SessionHandle);
	TestTrue(TEXT("FullOpen session should close"), bClosed);
	TestFalse(TEXT("Subsystem should no longer report active session"), SessionSubsystem->HasAnyActiveSession());
	TestFalse(TEXT("Loot container should no longer report active full-open session"),
		IWorldContainerSessionSource::Execute_HasActiveFullOpenSession(LootComponent));

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_FullOpenBusyRuleTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Session.FullOpenBusy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_FullOpenBusyRuleTest::RunTest(const FString& Parameters)
{
	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for busy rule test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem =
		LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	TestNotNull(TEXT("Player controller should exist"), PlayerController);
	if (!PlayerController)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootContainerJson(TEXT("SessionBusy"), true, TEXT("ObjectDefinition:WaterBottle"), 1),
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Definition should parse for busy rule test"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should be created"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = Spawned->FindComponentByClass<ULootContainerCapabilityComponent>();
	TestNotNull(TEXT("Loot container capability should be added"), LootComponent);
	if (!LootComponent)
	{
		Spawned->Destroy();
		return false;
	}

	FText OpenError;
	FContainerSessionHandle SessionHandle;
	const bool bOpened = SessionSubsystem->OpenWorldContainerSession(
		Spawned,
		EContainerSessionMode::FullOpen,
		SessionHandle,
		OpenError);

	TestTrue(TEXT("Initial FullOpen session should open"), bOpened);
	if (!bOpened)
	{
		Spawned->Destroy();
		return false;
	}

	FText BusyError;
	const bool bSecondOpenAllowed = IWorldContainerSessionSource::Execute_TryBeginContainerSession(
		LootComponent,
		PlayerController,
		FGuid::NewGuid(),
		EContainerSessionMode::FullOpen,
		BusyError);

	TestFalse(TEXT("Second FullOpen session should be rejected while first is active"), bSecondOpenAllowed);
	TestFalse(TEXT("Busy rejection should provide an error"), BusyError.IsEmpty());

	FText QuickLootError;
	const bool bQuickLootAllowed = IWorldContainerSessionSource::Execute_TryBeginContainerSession(
		LootComponent,
		PlayerController,
		FGuid::NewGuid(),
		EContainerSessionMode::QuickLoot,
		QuickLootError);

	TestTrue(TEXT("QuickLoot should remain lock-free while FullOpen is active"), bQuickLootAllowed);
	TestTrue(TEXT("QuickLoot should not report an error"), QuickLootError.IsEmpty());
	TestTrue(TEXT("QuickLoot should not clear or replace the active FullOpen lock"),
		IWorldContainerSessionSource::Execute_HasActiveFullOpenSession(LootComponent));

	const bool bClosed = SessionSubsystem->CloseSession(SessionHandle);
	TestTrue(TEXT("Initial FullOpen session should close"), bClosed);

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_QuickLootTakeAllTransferTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Session.QuickLootTakeAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_QuickLootTakeAllTransferTest::RunTest(const FString& Parameters)
{
	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for inventory transfer"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem =
		LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootContainerJson(TEXT("QuickLootTakeAll"), true, TEXT("ObjectDefinition:WaterBottle"), 2),
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Definition should parse for quick-loot transfer test"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should be created"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = Spawned->FindComponentByClass<ULootContainerCapabilityComponent>();
	TestNotNull(TEXT("Loot container capability should be added"), LootComponent);
	if (!LootComponent)
	{
		Spawned->Destroy();
		return false;
	}

	LootComponent->bOneTimeUse = false;

	AActor* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesTestInventory(World, InventoryOwner);
	TestNotNull(TEXT("Inventory component should be created"), Inventory);
	if (!Inventory)
	{
		Spawned->Destroy();
		return false;
	}

	FText OpenError;
	FContainerSessionHandle SessionHandle;
	const bool bOpened = SessionSubsystem->OpenWorldContainerSession(
		Spawned,
		EContainerSessionMode::QuickLoot,
		SessionHandle,
		OpenError);

	TestTrue(TEXT("QuickLoot session should open"), bOpened);
	if (!bOpened)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		if (InventoryOwner)
		{
			InventoryOwner->Destroy();
		}
		Spawned->Destroy();
		return false;
	}

	FText TakeAllError;
	const bool bTakeAll = SessionSubsystem->TakeAllFromWorldContainerSession(
		SessionHandle,
		Inventory,
		TakeAllError);

	TestTrue(TEXT("QuickLoot Take All should succeed"), bTakeAll);
	if (!bTakeAll)
	{
		AddError(FString::Printf(TEXT("TakeAll error: %s"), *TakeAllError.ToString()));
		if (InventoryOwner)
		{
			InventoryOwner->Destroy();
		}
		Spawned->Destroy();
		return false;
	}

	TestTrue(TEXT("Inventory should contain transferred WaterBottle quantity"), Inventory->ContainsItem(WaterBottleId, 2));
	TestFalse(TEXT("QuickLoot session should auto-close after transfer"), SessionSubsystem->HasAnyActiveSession());
	TestTrue(TEXT("Loot container should be marked looted after transfer"), LootComponent->bLooted);
	TestEqual(TEXT("Loot container entries should be empty after transfer"),
		IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent).Num(),
		0);

	if (InventoryOwner)
	{
		InventoryOwner->Destroy();
	}
	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_TakeEntryConsumeFailureRollbackTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Session.TakeEntryConsumeFailureRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_TakeEntryConsumeFailureRollbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for take rollback test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem = LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	AActor* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesTestInventory(World, InventoryOwner);
	TestNotNull(TEXT("Inventory component should be created"), Inventory);
	if (!Inventory)
	{
		return false;
	}

	UWorldContainerSessionTestDouble* SessionSource = nullptr;
	AActor* SourceActor = CreateWorldContainerSessionTestActor(World, SessionSource);
	TestNotNull(TEXT("Forced-failure source actor should be created"), SourceActor);
	TestNotNull(TEXT("Forced-failure session source should be created"), SessionSource);
	if (!SourceActor || !SessionSource)
	{
		if (InventoryOwner)
		{
			InventoryOwner->Destroy();
		}
		return false;
	}

	SessionSource->bFailConsume = true;
	SessionSource->EntryViews.Add(MakeWorldContainerEntryView(WaterBottleId, 1, 2, FIntPoint(0, 0), FIntPoint(1, 2)));

	FText OpenError;
	FContainerSessionHandle SessionHandle;
	const bool bOpened = SessionSubsystem->OpenWorldContainerSession(
		SourceActor,
		EContainerSessionMode::QuickLoot,
		SessionHandle,
		OpenError);

	TestTrue(TEXT("QuickLoot session should open for take rollback test"), bOpened);
	if (!bOpened)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		InventoryOwner->Destroy();
		SourceActor->Destroy();
		return false;
	}

	FText TakeError;
	const bool bTakeSucceeded = SessionSubsystem->TakeEntryFromWorldContainerSession(
		SessionHandle,
		Inventory,
		1,
		1,
		ProjectTags::Item_Container_Hands,
		FIntPoint(0, 0),
		false,
		TakeError);

	TestFalse(TEXT("Take should fail when world consume rejects"), bTakeSucceeded);
	TestTrue(TEXT("Failure should surface a consume error"), !TakeError.IsEmpty());
	TestFalse(TEXT("Inventory should remain unchanged after failed take"), Inventory->ContainsItem(WaterBottleId, 1));
	TestEqual(TEXT("Inventory should still be empty after failed take"), Inventory->GetEntries().Num(), 0);
	TestEqual(TEXT("World entry quantity should remain unchanged after failed take"), SessionSource->EntryViews[0].Quantity, 2);

	SessionSubsystem->CloseSession(SessionHandle);
	InventoryOwner->Destroy();
	SourceActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_TakeAllConsumeFailureRollbackTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Session.TakeAllConsumeFailureRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_TakeAllConsumeFailureRollbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for take-all rollback test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem = LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	AActor* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesTestInventory(World, InventoryOwner);
	TestNotNull(TEXT("Inventory component should be created"), Inventory);
	if (!Inventory)
	{
		return false;
	}

	UWorldContainerSessionTestDouble* SessionSource = nullptr;
	AActor* SourceActor = CreateWorldContainerSessionTestActor(World, SessionSource);
	TestNotNull(TEXT("Forced-failure source actor should be created"), SourceActor);
	TestNotNull(TEXT("Forced-failure session source should be created"), SessionSource);
	if (!SourceActor || !SessionSource)
	{
		if (InventoryOwner)
		{
			InventoryOwner->Destroy();
		}
		return false;
	}

	SessionSource->bFailConsume = true;
	SessionSource->EntryViews.Add(MakeWorldContainerEntryView(WaterBottleId, 1, 2, FIntPoint(0, 0), FIntPoint(1, 2)));
	SessionSource->EntryViews.Add(MakeWorldContainerEntryView(WaterBottleId, 2, 1, FIntPoint(1, 0), FIntPoint(1, 2)));

	FText OpenError;
	FContainerSessionHandle SessionHandle;
	const bool bOpened = SessionSubsystem->OpenWorldContainerSession(
		SourceActor,
		EContainerSessionMode::QuickLoot,
		SessionHandle,
		OpenError);

	TestTrue(TEXT("QuickLoot session should open for take-all rollback test"), bOpened);
	if (!bOpened)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		InventoryOwner->Destroy();
		SourceActor->Destroy();
		return false;
	}

	FText TakeAllError;
	const bool bTakeAllSucceeded = SessionSubsystem->TakeAllFromWorldContainerSession(
		SessionHandle,
		Inventory,
		TakeAllError);

	TestFalse(TEXT("Take All should fail when world consume rejects"), bTakeAllSucceeded);
	TestTrue(TEXT("Failure should surface a consume error"), !TakeAllError.IsEmpty());
	TestFalse(TEXT("Inventory should remain unchanged after failed take-all"), Inventory->ContainsItem(WaterBottleId, 1));
	TestEqual(TEXT("Inventory should still be empty after failed take-all"), Inventory->GetEntries().Num(), 0);
	TestEqual(TEXT("World should still expose both entries after failed take-all"), SessionSource->EntryViews.Num(), 2);
	TestEqual(TEXT("First world entry quantity should remain unchanged"), SessionSource->EntryViews[0].Quantity, 2);
	TestEqual(TEXT("Second world entry quantity should remain unchanged"), SessionSource->EntryViews[1].Quantity, 1);

	SessionSubsystem->CloseSession(SessionHandle);
	InventoryOwner->Destroy();
	SourceActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_StoreFailureExactRestoreTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Session.StoreFailureExactRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_StoreFailureExactRestoreTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for store rollback test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem = LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	AActor* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesTestInventory(World, InventoryOwner);
	TestNotNull(TEXT("Inventory component should be created"), Inventory);
	if (!Inventory)
	{
		return false;
	}

	TArray<FMagnitudeOverride> Overrides;
	Overrides.Add(FMagnitudeOverride(ProjectTags::State_Hydration, 42.0f));
	const uint32 InstanceId = Inventory->TryAddItemWithOverrides(WaterBottleId, 1, Overrides);
	TestTrue(TEXT("Inventory entry with overrides should be created"), InstanceId > 0);
	if (InstanceId == 0)
	{
		InventoryOwner->Destroy();
		return false;
	}

	TArray<FInventoryEntry>& MutableEntries = const_cast<TArray<FInventoryEntry>&>(Inventory->GetEntries());
	FInventoryEntry* MutableEntry = MutableEntries.FindByPredicate([InstanceId](const FInventoryEntry& Entry)
	{
		return Entry.InstanceId == static_cast<int32>(InstanceId);
	});
	TestNotNull(TEXT("Mutable inventory entry should resolve"), MutableEntry);
	if (!MutableEntry)
	{
		InventoryOwner->Destroy();
		return false;
	}

	MutableEntry->InstanceData.Durability = 777;
	MutableEntry->InstanceData.Ammo = 9;

	FInventoryEntry OriginalEntry;
	TestTrue(TEXT("Original inventory entry should be readable"), Inventory->FindEntry(static_cast<int32>(InstanceId), OriginalEntry));

	UWorldContainerSessionTestDouble* SessionSource = nullptr;
	AActor* SourceActor = CreateWorldContainerSessionTestActor(World, SessionSource);
	TestNotNull(TEXT("Forced-store-failure source actor should be created"), SourceActor);
	TestNotNull(TEXT("Forced-store-failure session source should be created"), SessionSource);
	if (!SourceActor || !SessionSource)
	{
		InventoryOwner->Destroy();
		return false;
	}

	SessionSource->bFailStore = true;
	SessionSource->bSupportsQuickLoot = false;
	SessionSource->bSupportsFullOpen = true;

	FText OpenError;
	FContainerSessionHandle SessionHandle;
	const bool bOpened = SessionSubsystem->OpenWorldContainerSession(
		SourceActor,
		EContainerSessionMode::FullOpen,
		SessionHandle,
		OpenError);

	TestTrue(TEXT("FullOpen session should open for store rollback test"), bOpened);
	if (!bOpened)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		InventoryOwner->Destroy();
		SourceActor->Destroy();
		return false;
	}

	FText StoreError;
	const bool bStoreSucceeded = SessionSubsystem->StoreInventoryEntryInWorldContainerSession(
		SessionHandle,
		Inventory,
		static_cast<int32>(InstanceId),
		1,
		FIntPoint(2, 3),
		false,
		StoreError);

	TestFalse(TEXT("Store should fail when world store rejects"), bStoreSucceeded);
	TestTrue(TEXT("Failure should surface a store error"), !StoreError.IsEmpty());

	FInventoryEntry RestoredEntry;
	const bool bRestored = Inventory->FindEntry(static_cast<int32>(InstanceId), RestoredEntry);
	TestTrue(TEXT("Original inventory entry should be restored after failed store"), bRestored);
	if (bRestored)
	{
		TestEqual(TEXT("Restored entry should keep instance id"), RestoredEntry.InstanceId, OriginalEntry.InstanceId);
		TestEqual(TEXT("Restored entry should keep container id"), RestoredEntry.ContainerId, OriginalEntry.ContainerId);
		TestEqual(TEXT("Restored entry should keep grid position"), RestoredEntry.GridPos, OriginalEntry.GridPos);
		TestEqual(TEXT("Restored entry should keep rotation"), RestoredEntry.bRotated, OriginalEntry.bRotated);
		TestEqual(TEXT("Restored entry should keep durability"), RestoredEntry.InstanceData.Durability, OriginalEntry.InstanceData.Durability);
		TestEqual(TEXT("Restored entry should keep ammo"), RestoredEntry.InstanceData.Ammo, OriginalEntry.InstanceData.Ammo);
		TestEqual(TEXT("Restored entry should keep override count"), RestoredEntry.OverrideMagnitudes.Num(), OriginalEntry.OverrideMagnitudes.Num());
		if (RestoredEntry.OverrideMagnitudes.Num() == OriginalEntry.OverrideMagnitudes.Num() && RestoredEntry.OverrideMagnitudes.Num() > 0)
		{
			TestEqual(TEXT("Restored override tag should match"), RestoredEntry.OverrideMagnitudes[0].Tag, OriginalEntry.OverrideMagnitudes[0].Tag);
			TestEqual(TEXT("Restored override value should match"), RestoredEntry.OverrideMagnitudes[0].Value, OriginalEntry.OverrideMagnitudes[0].Value);
		}
	}

	TestEqual(TEXT("World container should remain unchanged after failed store"), SessionSource->EntryViews.Num(), 0);

	SessionSubsystem->CloseSession(SessionHandle);
	InventoryOwner->Destroy();
	SourceActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_WorldContainerSnapshotTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Contract.WorldContainerSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_WorldContainerSnapshotTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for snapshot test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootContainerJson(TEXT("SnapshotContract"), true, TEXT("ObjectDefinition:WaterBottle"), 2),
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Definition should parse for snapshot test"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should exist"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = Spawned->FindComponentByClass<ULootContainerCapabilityComponent>();
	TestNotNull(TEXT("Loot container capability should exist"), LootComponent);
	if (!LootComponent)
	{
		Spawned->Destroy();
		return false;
	}

	const FInventoryContainerView ContainerView = IWorldContainerSessionSource::Execute_GetContainerView(LootComponent);
	TestEqual(TEXT("Snapshot grid width should come from storage section"), ContainerView.GridSize.X, 4);
	TestEqual(TEXT("Snapshot grid height should come from storage section"), ContainerView.GridSize.Y, 5);
	TestEqual(TEXT("Snapshot max weight should come from storage section"), ContainerView.MaxWeight, 15.0f);
	TestEqual(TEXT("Snapshot max volume should come from storage section"), ContainerView.MaxVolume, 30.0f);
	TestEqual(TEXT("Snapshot max cells should come from storage section"), ContainerView.MaxCells, 20);

	const TArray<FInventoryEntryView> EntryViews = IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent);
	TestEqual(TEXT("Snapshot should expose one entry view"), EntryViews.Num(), 1);
	if (EntryViews.Num() > 0)
	{
		TestEqual(TEXT("Snapshot item id should match seeded item"), EntryViews[0].ItemId, WaterBottleId);
		TestEqual(TEXT("Snapshot quantity should match seeded quantity"), EntryViews[0].Quantity, 2);
		TestTrue(TEXT("Snapshot instance id should be stable and positive"), EntryViews[0].InstanceId > 0);
		TestTrue(TEXT("Snapshot should use world storage container tag"), EntryViews[0].ContainerId == ProjectTags::Item_Container_WorldStorage);
	}

	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_ViewModelNearbyContainerSessionTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.ViewModelNearbyContainerSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_ViewModelNearbyContainerSessionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for nearby session UI test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem =
		LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootContainerJson(TEXT("ViewModelNearbySession"), true, TEXT("ObjectDefinition:WaterBottle"), 2),
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Definition should parse for nearby session UI test"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should exist"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = Spawned->FindComponentByClass<ULootContainerCapabilityComponent>();
	TestNotNull(TEXT("Loot container capability should exist"), LootComponent);
	if (!LootComponent)
	{
		Spawned->Destroy();
		return false;
	}

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory component should be created"), Inventory);
	if (!Inventory)
	{
		Spawned->Destroy();
		return false;
	}

	FText OpenError;
	FContainerSessionHandle SessionHandle;
	const bool bOpened = SessionSubsystem->OpenWorldContainerSession(
		Spawned,
		EContainerSessionMode::FullOpen,
		SessionHandle,
		OpenError);

	TestTrue(TEXT("FullOpen session should open for nearby UI test"), bOpened);
	if (!bOpened)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		Inventory->DestroyComponent();
		Spawned->Destroy();
		return false;
	}

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Inventory view model should be created"), ViewModel);
	if (!ViewModel)
	{
		SessionSubsystem->CloseSession(SessionHandle);
		Inventory->DestroyComponent();
		Spawned->Destroy();
		return false;
	}
	ViewModel->Initialize(nullptr);

	ViewModel->SetInventorySource(Inventory);
	ViewModel->SetNearbyContainerSource(LootComponent, SessionHandle);
	ViewModel->RefreshFromInventory();

	TestTrue(TEXT("ViewModel should expose nearby container state"), ViewModel->GetbHasNearbyContainer());
	TestEqual(TEXT("Nearby container grid width should match storage section"), ViewModel->GetSecondaryGridWidth(), 4);
	TestEqual(TEXT("Nearby container grid height should match storage section"), ViewModel->GetSecondaryGridHeight(), 5);
	TestEqual(TEXT("Nearby container label should use source display label"), ViewModel->GetNearbyContainerLabel().ToString(), Spawned->GetActorNameOrLabel());

	FInventoryEntryView NearbyEntry;
	const bool bFoundNearbyEntry = ViewModel->TryGetSecondaryEntryByCellIndex(0, NearbyEntry);
	TestTrue(TEXT("Nearby entry should resolve from secondary grid"), bFoundNearbyEntry);
	if (bFoundNearbyEntry)
	{
		TestEqual(TEXT("Nearby entry item id should match seeded item"), NearbyEntry.ItemId, WaterBottleId);
		TestEqual(TEXT("Nearby entry quantity should start at 2"), NearbyEntry.Quantity, 2);
	}

	if (bFoundNearbyEntry)
	{
		FGameplayTag RightHandContainerId;
		FIntPoint RightHandGridPos = FIntPoint(-1, -1);
		TestTrue(TEXT("ViewModel should resolve a concrete right-hand drop target"), ViewModel->ResolveHandDropTarget(false, RightHandContainerId, RightHandGridPos));

		ViewModel->RequestTakeNearbyItemToContainer(
			NearbyEntry.InstanceId,
			RightHandContainerId,
			RightHandGridPos,
			false,
			1);
		TestTrue(TEXT("Inventory should contain one transferred item after take"), Inventory->ContainsItem(WaterBottleId, 1));

		TArray<FInventoryEntryView> InventoryEntries;
		Inventory->GetEntriesView(InventoryEntries);
		TestEqual(TEXT("Inventory should expose one entry after take"), InventoryEntries.Num(), 1);
		if (InventoryEntries.Num() > 0)
		{
			TestTrue(TEXT("Taken entry should be placed in the resolved hand container"), InventoryEntries[0].ContainerId == RightHandContainerId);
			TestEqual(TEXT("Taken entry should use the resolved hand slot"), InventoryEntries[0].GridPos, RightHandGridPos);
		}

		const TArray<FInventoryEntryView> AfterTakeEntries = IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent);
		TestEqual(TEXT("World container should retain one item after single take"), AfterTakeEntries.Num(), 1);
		if (AfterTakeEntries.Num() > 0)
		{
			TestEqual(TEXT("World container quantity should decrement after single take"), AfterTakeEntries[0].Quantity, 1);
		}

		if (InventoryEntries.Num() > 0)
		{
			const FIntPoint StoreGridPos(
				FMath::Max(0, 4 - NearbyEntry.GridSize.X),
				FMath::Max(0, 5 - NearbyEntry.GridSize.Y));
			ViewModel->RequestStoreItemInNearbyContainerAt(
				InventoryEntries[0].InstanceId,
				StoreGridPos,
				false,
				1);

			const TArray<FInventoryEntryView> AfterStoreEntries = IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent);
			TestEqual(TEXT("World container should expose two exact-placement entries after store"), AfterStoreEntries.Num(), 2);

			const FInventoryEntryView* StoredEntry = AfterStoreEntries.FindByPredicate([&StoreGridPos](const FInventoryEntryView& Entry)
			{
				return Entry.GridPos == StoreGridPos;
			});
			TestNotNull(TEXT("World container should include entry at requested store grid position"), StoredEntry);
			if (StoredEntry)
			{
				TestEqual(TEXT("Stored entry should keep quantity 1 at explicit placement"), StoredEntry->Quantity, 1);
			}
		}
	}

	ViewModel->ClearNearbyContainerSource();
	TestFalse(TEXT("ViewModel should clear nearby container state"), ViewModel->GetbHasNearbyContainer());

	SessionSubsystem->CloseSession(SessionHandle);
	Inventory->DestroyComponent();
	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_ViewModelResolveHandDropTargetTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.ViewModelResolveHandDropTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_ViewModelResolveHandDropTargetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve for hand target test"), World);
	if (!World)
	{
		return false;
	}

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory component should be created for hand target test"), Inventory);
	if (!Inventory)
	{
		return false;
	}

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Inventory view model should be created for hand target test"), ViewModel);
	if (!ViewModel)
	{
		Inventory->DestroyComponent();
		return false;
	}
	ViewModel->Initialize(nullptr);

	ViewModel->SetInventorySource(Inventory);
	ViewModel->RefreshFromInventory();

	FGameplayTag LeftHandContainerId;
	FIntPoint LeftHandGridPos = FIntPoint(-1, -1);
	TestTrue(TEXT("ViewModel should resolve a left-hand drop target"), ViewModel->ResolveHandDropTarget(true, LeftHandContainerId, LeftHandGridPos));

	FGameplayTag RightHandContainerId;
	FIntPoint RightHandGridPos = FIntPoint(-1, -1);
	TestTrue(TEXT("ViewModel should resolve a right-hand drop target"), ViewModel->ResolveHandDropTarget(false, RightHandContainerId, RightHandGridPos));
	TestTrue(
		TEXT("Left and right hand targets should resolve to distinct placements"),
		LeftHandContainerId != RightHandContainerId || LeftHandGridPos != RightHandGridPos);

	if (LeftHandContainerId == ProjectTags::Item_Container_Hands)
	{
		TestEqual(TEXT("Left-hand fallback to canonical hands should map to X=0"), LeftHandGridPos, FIntPoint(0, 0));
	}

	if (RightHandContainerId == ProjectTags::Item_Container_Hands)
	{
		TestEqual(TEXT("Right-hand fallback to canonical hands should map to X=1"), RightHandGridPos, FIntPoint(1, 0));
	}

	Inventory->DestroyComponent();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_RequestMoveItemRejectsDepthOverflowOverlapTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Inventory.RequestMoveItemRejectsDepthOverflowOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_RequestMoveItemRejectsDepthOverflowOverlapTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId CigaretteId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Cigarette"));
	FScopedInventoryLootPlacesItemOverride ItemOverride(CigaretteId);
	TestTrue(TEXT("Cigarette asset should load for depth-overlap inventory test"), ItemOverride.IsValid());
	if (!ItemOverride.IsValid())
	{
		return false;
	}
	ItemOverride.ApplyDepthStacking(FIntPoint(1, 1), 10, 1);

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve for depth-overlap inventory test"), World);
	if (!World)
	{
		return false;
	}

	AActor* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesTestInventory(World, InventoryOwner);
	TestNotNull(TEXT("Inventory component should be created for depth-overlap inventory test"), Inventory);
	if (!Inventory)
	{
		return false;
	}

	TestEqual(TEXT("Initial add should split across two hand stacks under the depth cap"), Inventory->TryAddItem(CigaretteId, 5), 5);

	TArray<FInventoryEntry> CigaretteEntries;
	for (const FInventoryEntry& Entry : Inventory->GetEntries())
	{
		if (Entry.ItemId == CigaretteId)
		{
			CigaretteEntries.Add(Entry);
		}
	}

	TestEqual(TEXT("Depth-limited add should create two cigarette stacks"), CigaretteEntries.Num(), 2);
	if (CigaretteEntries.Num() != 2)
	{
		Inventory->DestroyComponent();
		if (InventoryOwner)
		{
			InventoryOwner->Destroy();
		}
		return false;
	}

	const FInventoryEntry* FullStack = CigaretteEntries.FindByPredicate([](const FInventoryEntry& Entry) { return Entry.Quantity == 4; });
	const FInventoryEntry* RemainderStack = CigaretteEntries.FindByPredicate([](const FInventoryEntry& Entry) { return Entry.Quantity == 1; });
	TestNotNull(TEXT("Depth-limited add should create one full stack"), FullStack);
	TestNotNull(TEXT("Depth-limited add should create one remainder stack"), RemainderStack);
	if (!FullStack || !RemainderStack)
	{
		Inventory->DestroyComponent();
		if (InventoryOwner)
		{
			InventoryOwner->Destroy();
		}
		return false;
	}

	Inventory->RequestMoveItem(
		RemainderStack->InstanceId,
		RemainderStack->ContainerId,
		RemainderStack->GridPos,
		FullStack->ContainerId,
		FullStack->GridPos,
		1,
		RemainderStack->bRotated);

	TArray<FInventoryEntry> AfterEntries;
	for (const FInventoryEntry& Entry : Inventory->GetEntries())
	{
		if (Entry.ItemId == CigaretteId)
		{
			AfterEntries.Add(Entry);
		}
	}

	TestEqual(TEXT("Rejected move should keep exactly two stacks"), AfterEntries.Num(), 2);

	const FInventoryEntry* AfterFullStack = AfterEntries.FindByPredicate([](const FInventoryEntry& Entry) { return Entry.Quantity == 4; });
	const FInventoryEntry* AfterRemainderStack = AfterEntries.FindByPredicate([](const FInventoryEntry& Entry) { return Entry.Quantity == 1; });
	TestNotNull(TEXT("Rejected move should preserve the full stack"), AfterFullStack);
	TestNotNull(TEXT("Rejected move should preserve the remainder stack"), AfterRemainderStack);

	if (AfterFullStack && FullStack)
	{
		TestEqual(TEXT("Full stack container unchanged"), AfterFullStack->ContainerId, FullStack->ContainerId);
		TestEqual(TEXT("Full stack grid unchanged"), AfterFullStack->GridPos, FullStack->GridPos);
		TestEqual(TEXT("Full stack instance unchanged"), AfterFullStack->InstanceId, FullStack->InstanceId);
	}
	if (AfterRemainderStack && RemainderStack)
	{
		TestEqual(TEXT("Remainder stack container unchanged"), AfterRemainderStack->ContainerId, RemainderStack->ContainerId);
		TestEqual(TEXT("Remainder stack grid unchanged"), AfterRemainderStack->GridPos, RemainderStack->GridPos);
		TestEqual(TEXT("Remainder stack instance unchanged"), AfterRemainderStack->InstanceId, RemainderStack->InstanceId);
	}

	Inventory->DestroyComponent();
	if (InventoryOwner)
	{
		InventoryOwner->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_WorldContainerStoreRejectsDepthOverflowPlacementTest,
	"ProjectIntegrationTests.InventoryLootPlaces.World.StoreRejectsDepthOverflowPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_WorldContainerStoreRejectsDepthOverflowPlacementTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId CigaretteId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Cigarette"));
	FScopedInventoryLootPlacesItemOverride ItemOverride(CigaretteId);
	TestTrue(TEXT("Cigarette asset should load for world depth-overflow store test"), ItemOverride.IsValid());
	if (!ItemOverride.IsValid())
	{
		return false;
	}
	ItemOverride.ApplyDepthStacking(FIntPoint(1, 1), 10, 1);

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve for world depth-overflow store test"), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	AActor* Owner = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("World container owner should spawn"), Owner);
	if (!Owner)
	{
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = NewObject<ULootContainerCapabilityComponent>(Owner);
	Owner->AddInstanceComponent(LootComponent);
	LootComponent->RegisterComponent();
	LootComponent->RuntimeGridSize = FIntPoint(2, 2);
	LootComponent->RuntimeMaxCells = 4;
	LootComponent->RuntimeCellDepthUnits = 1;
	LootComponent->ContainerKey.InstanceId = FGuid::NewGuid();
	LootComponent->ContainerKey.WorldScopeId = FName(TEXT("Automation"));
	LootComponent->ContainerKey.ContainerSlotId = FName(TEXT("Primary"));

	FContainerEntryTransfer Transfer;
	Transfer.ObjectId = CigaretteId;
	Transfer.Quantity = 2;
	Transfer.GridPos = FIntPoint(0, 0);
	Transfer.bRotated = false;

	TArray<FContainerEntryTransfer> Transfers;
	Transfers.Add(Transfer);

	FText StoreError;
	const FGuid SessionId = FGuid::NewGuid();
	const bool bCanStore = IWorldContainerSessionSource::Execute_CanStoreContainerEntries(
		LootComponent,
		SessionId,
		Transfers,
		StoreError);

	TestFalse(TEXT("World container should reject explicit placement that exceeds shallow cell depth"), bCanStore);
	TestTrue(TEXT("Depth-overflow store rejection should return an error"), !StoreError.IsEmpty());

	FText CommitError;
	const bool bStored = IWorldContainerSessionSource::Execute_StoreContainerEntries(
		LootComponent,
		SessionId,
		Transfers,
		CommitError);

	TestFalse(TEXT("StoreContainerEntries should fail when depth-overflow placement is invalid"), bStored);
	TestEqual(TEXT("Rejected world-container store should leave runtime entries unchanged"), LootComponent->RuntimeEntries.Num(), 0);

	LootComponent->DestroyComponent();
	Owner->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_DragVisualUsesTooltipWidgetTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.DragVisualUsesIconCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_DragOperationAppliesRotationFromEntryTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.DragOperationAppliesRotationFromEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_DragOperationAppliesRotationFromEntryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>(GetTransientPackage());
	TestNotNull(TEXT("Drag operation should allocate"), DragOp);
	if (!DragOp)
	{
		return false;
	}

	FInventoryEntryView Entry;
	Entry.GridSize = FIntPoint(1, 3);

	DragOp->ApplyRotationFromEntry(Entry, false);
	TestFalse(TEXT("Drag operation should report unrotated state"), DragOp->bRotated);
	TestEqual(TEXT("Unrotated drag footprint should match entry size"), DragOp->ItemSize, FIntPoint(1, 3));

	DragOp->ApplyRotationFromEntry(Entry, true);
	TestTrue(TEXT("Drag operation should report rotated state"), DragOp->bRotated);
	TestEqual(TEXT("Rotated drag footprint should swap entry dimensions"), DragOp->ItemSize, FIntPoint(3, 1));
	return true;
}

bool FInventoryLootPlaces_DragVisualUsesTooltipWidgetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve for drag visual test"), World);
	if (!World)
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	TestNotNull(TEXT("GameInstance should exist for drag visual test"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}

	FInventoryEntryView Entry;
	Entry.DisplayName = FText::FromString(TEXT("Cigarette"));
	Entry.Description = FText::FromString(TEXT("Filtered cigarette."));
	Entry.IconCode = TEXT("C");
	Entry.Weight = 0.1f;
	Entry.Volume = 0.05f;

	UWidget* DragVisual = FInventoryDragVisualBuilder::Build(GameInstance, Entry, 2, nullptr);
	TestNotNull(TEXT("Drag visual should build an icon-led drag card"), DragVisual);
	if (!DragVisual)
	{
		return false;
	}

	UTextBlock* IconText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(DragVisual, TEXT("DragIconText"));
	TestNotNull(TEXT("Drag visual should expose DragIconText"), IconText);
	if (!IconText)
	{
		return false;
	}

	UTextBlock* LabelText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(DragVisual, TEXT("DragLabelText"));
	TestNotNull(TEXT("Drag visual should expose DragLabelText"), LabelText);
	if (!LabelText)
	{
		return false;
	}

	TestEqual(TEXT("Drag visual should carry the item icon"), IconText->GetText().ToString(), FString(TEXT("C")));
	TestEqual(TEXT("Drag visual should show stack quantity in label"), LabelText->GetText().ToString(), FString(TEXT("Cigarette x2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_TakeNearbyFallsBackToAlternateHandTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.TakeNearbyFallsBackToAlternateHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_HandsProjectHeldItemsAcrossGripZoneTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.HandsAllowMultipleSmallItemsInSameHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_HandsProjectHeldItemsAcrossGripZoneTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId CigaretteId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Cigarette"));
	const FPrimaryAssetId BreadSliceId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:BreadSlice"));
	TestTrue(TEXT("Cigarette asset should load for same-hand capacity test"), EnsureInventoryLootPlacesTestAssetLoaded(CigaretteId));
	TestTrue(TEXT("BreadSlice asset should load for same-hand capacity test"), EnsureInventoryLootPlacesTestAssetLoaded(BreadSliceId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(CigaretteId) || !EnsureInventoryLootPlacesTestAssetLoaded(BreadSliceId))
	{
		return false;
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve for same-hand capacity test"), World);
	if (!World)
	{
		return false;
	}

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory component should be created for same-hand capacity test"), Inventory);
	if (!Inventory)
	{
		return false;
	}

	const int32 AddedCigarettes = Inventory->TryAddItem(CigaretteId, 1);
	const int32 AddedBreadSlices = Inventory->TryAddItem(BreadSliceId, 1);
	TestEqual(TEXT("First hand item should be added"), AddedCigarettes, 1);
	TestEqual(TEXT("Second hand item should be added"), AddedBreadSlices, 1);
	if (AddedCigarettes != 1 || AddedBreadSlices != 1)
	{
		Inventory->DestroyComponent();
		return false;
	}

	FInventoryEntry CigaretteEntry;
	FInventoryEntry BreadSliceEntry;
	TestTrue(TEXT("Cigarette entry should exist after add"), Inventory->FindEntryByItemId(CigaretteId, CigaretteEntry));
	TestTrue(TEXT("Bread slice entry should exist after add"), Inventory->FindEntryByItemId(BreadSliceId, BreadSliceEntry));
	TestEqual(TEXT("First small item should stay in left hand container"), CigaretteEntry.ContainerId, FGameplayTag(ProjectTags::Item_Container_LeftHand));
	TestEqual(TEXT("Second small item should also stay in left hand container"), BreadSliceEntry.ContainerId, FGameplayTag(ProjectTags::Item_Container_LeftHand));
	TestTrue(TEXT("Second small item should occupy a different hand cell"), CigaretteEntry.GridPos != BreadSliceEntry.GridPos);

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Inventory view model should be created for same-hand capacity test"), ViewModel);
	if (!ViewModel)
	{
		Inventory->DestroyComponent();
		return false;
	}
	ViewModel->Initialize(nullptr);
	ViewModel->SetInventorySource(Inventory);
	ViewModel->RefreshFromInventory();

	const int32 CigaretteCellIndex = CigaretteEntry.GridPos.Y * UInventoryViewModel::HandGridSize + CigaretteEntry.GridPos.X;
	const int32 BreadSliceCellIndex = BreadSliceEntry.GridPos.Y * UInventoryViewModel::HandGridSize + BreadSliceEntry.GridPos.X;
	TestEqual(TEXT("Left hand should expose cigarette in its own cell"),
		ViewModel->GetLeftHandInstanceId(CigaretteCellIndex),
		static_cast<int32>(CigaretteEntry.InstanceId));
	TestEqual(TEXT("Left hand should expose bread slice in its own cell"),
		ViewModel->GetLeftHandInstanceId(BreadSliceCellIndex),
		static_cast<int32>(BreadSliceEntry.InstanceId));
	for (int32 CellIndex = 0; CellIndex < UInventoryViewModel::HandCellCount; ++CellIndex)
	{
		TestEqual(
			FString::Printf(TEXT("Right hand cell %d should remain empty when both small items fit in the left hand"), CellIndex),
			ViewModel->GetRightHandInstanceId(CellIndex),
			UInventoryViewModel::EmptyCellInstanceId);
	}

	Inventory->DestroyComponent();
	return true;
}

bool FInventoryLootPlaces_TakeNearbyFallsBackToAlternateHandTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId CigaretteId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Cigarette"));
	const FPrimaryAssetId BreadSliceId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:BreadSlice"));
	TestTrue(TEXT("Cigarette asset should load"), EnsureInventoryLootPlacesTestAssetLoaded(CigaretteId));
	TestTrue(TEXT("BreadSlice asset should load"), EnsureInventoryLootPlacesTestAssetLoaded(BreadSliceId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(CigaretteId) || !EnsureInventoryLootPlacesTestAssetLoaded(BreadSliceId))
	{
		return false;
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem = LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory component should be created"), Inventory);
	if (!Inventory)
	{
		return false;
	}

	UWorldContainerSessionTestDouble* SessionSource = nullptr;
	AActor* SourceActor = CreateWorldContainerSessionTestActor(World, SessionSource);
	TestNotNull(TEXT("World container test actor should be created"), SourceActor);
	TestNotNull(TEXT("World container session source should be created"), SessionSource);
	if (!SourceActor || !SessionSource)
	{
		if (Inventory)
		{
			Inventory->DestroyComponent();
		}
		return false;
	}

	SessionSource->DisplayLabel = FText::FromString(TEXT("Nearby Box"));
	SessionSource->ContainerKey.InstanceId = FGuid::NewGuid();
	SessionSource->ContainerKey.WorldScopeId = FName(TEXT("Automation"));
	SessionSource->ContainerKey.ContainerSlotId = FName(TEXT("NearbyLoot"));
	SessionSource->ContainerView.ContainerId = ProjectTags::Item_Container_WorldStorage;
	SessionSource->ContainerView.GridSize = FIntPoint(4, 5);
	SessionSource->ContainerView.MaxWeight = 12.f;
	SessionSource->ContainerView.MaxVolume = 18.f;
	SessionSource->ContainerView.MaxCells = 20;
	SessionSource->EntryViews.Add(MakeWorldContainerEntryView(CigaretteId, 1, 1, FIntPoint(0, 0), FIntPoint(1, 1)));
	SessionSource->EntryViews.Add(MakeWorldContainerEntryView(BreadSliceId, 2, 1, FIntPoint(1, 0), FIntPoint(1, 1)));

	FText OpenError;
	FContainerSessionHandle SessionHandle;
	const bool bOpened = SessionSubsystem->OpenWorldContainerSession(
		SourceActor,
		EContainerSessionMode::FullOpen,
		SessionHandle,
		OpenError);
	TestTrue(TEXT("Nearby hand fallback test should open a FullOpen session"), bOpened);
	if (!bOpened)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		Inventory->DestroyComponent();
		SourceActor->Destroy();
		return false;
	}

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Inventory view model should be created"), ViewModel);
	if (!ViewModel)
	{
		SessionSubsystem->CloseSession(SessionHandle);
		Inventory->DestroyComponent();
		SourceActor->Destroy();
		return false;
	}
	ViewModel->Initialize(nullptr);

	ViewModel->SetInventorySource(Inventory);
	ViewModel->SetNearbyContainerSource(SessionSource, SessionHandle);
	ViewModel->RefreshFromInventory();

	FGameplayTag LeftHandContainerId;
	FIntPoint LeftHandGridPos = FIntPoint(-1, -1);
	TestTrue(TEXT("Left hand target should resolve"), ViewModel->ResolveHandDropTarget(true, LeftHandContainerId, LeftHandGridPos));

	FGameplayTag RightHandContainerId;
	FIntPoint RightHandGridPos = FIntPoint(-1, -1);
	TestTrue(TEXT("Right hand target should resolve"), ViewModel->ResolveHandDropTarget(false, RightHandContainerId, RightHandGridPos));

	ViewModel->RequestTakeNearbyItemToContainer(1, RightHandContainerId, RightHandGridPos, false, 1);
	ViewModel->RefreshFromInventory();
	TestTrue(TEXT("Inventory should contain the cigarette in hand after first take"), Inventory->ContainsItem(CigaretteId, 1));

	ViewModel->RequestTakeNearbyItemToContainer(2, RightHandContainerId, RightHandGridPos, false, 1);
	ViewModel->RefreshFromInventory();

	TestTrue(TEXT("Inventory should contain the bread slice after same-hand retry"), Inventory->ContainsItem(BreadSliceId, 1));

	TArray<FInventoryEntryView> InventoryEntries;
	Inventory->GetEntriesView(InventoryEntries);
	const FInventoryEntryView* BreadEntry = InventoryEntries.FindByPredicate([&BreadSliceId](const FInventoryEntryView& Entry)
	{
		return Entry.ItemId == BreadSliceId;
	});

	TestNotNull(TEXT("Bread entry should exist after same-hand retry transfer"), BreadEntry);
	if (BreadEntry)
	{
		TestEqual(TEXT("Bread entry should stay in the requested right hand container"), BreadEntry->ContainerId, FGameplayTag(RightHandContainerId));
		TestTrue(TEXT("Bread entry should use a different free cell in the requested hand"), BreadEntry->GridPos != RightHandGridPos);
	}

	const TArray<FInventoryEntryView> RemainingNearbyEntries = IWorldContainerSessionSource::Execute_GetContainerEntryViews(SessionSource);
	TestEqual(TEXT("World container should be empty after two successful takes"), RemainingNearbyEntries.Num(), 0);

	SessionSubsystem->CloseSession(SessionHandle);
	Inventory->DestroyComponent();
	SourceActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_WorldContainerBridgeLifecycleTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.WorldContainerBridgeLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_WorldContainerBridgeLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for bridge lifecycle UI test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem =
		LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootContainerJson(TEXT("BridgeLifecycle"), true, TEXT("ObjectDefinition:WaterBottle"), 2),
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Definition should parse for bridge lifecycle test"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should exist"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory component should be created"), Inventory);
	if (!Inventory)
	{
		Spawned->Destroy();
		return false;
	}

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Inventory view model should be created"), ViewModel);
	if (!ViewModel)
	{
		Inventory->DestroyComponent();
		Spawned->Destroy();
		return false;
	}
	ViewModel->Initialize(nullptr);

	ViewModel->SetInventorySource(Inventory);

	FText OpenError;
	const bool bOpenRequested =
		IInventoryWorldContainerTransferBridge::Execute_RequestOpenWorldContainerSession(
			Inventory,
			Spawned,
			EContainerSessionMode::FullOpen,
			OpenError);

	TestTrue(TEXT("Bridge should request world-container open"), bOpenRequested);
	if (!bOpenRequested)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		Inventory->DestroyComponent();
		Spawned->Destroy();
		return false;
	}

	TestTrue(TEXT("Session subsystem should track active session after bridge open"), SessionSubsystem->HasAnyActiveSession());
	TestTrue(TEXT("ViewModel should expose nearby container after bridge open"), ViewModel->GetbHasNearbyContainer());
	TestTrue(TEXT("ViewModel should show panel after bridge open"), ViewModel->GetbPanelVisible());
	TestEqual(TEXT("Nearby container grid width should match storage section after bridge open"), ViewModel->GetSecondaryGridWidth(), 4);
	TestEqual(TEXT("Nearby container grid height should match storage section after bridge open"), ViewModel->GetSecondaryGridHeight(), 5);

	FInventoryEntryView NearbyEntry;
	const bool bHasNearbyEntry = ViewModel->TryGetSecondaryEntryByCellIndex(0, NearbyEntry);
	TestTrue(TEXT("Bridge-opened nearby container should expose entry views"), bHasNearbyEntry);
	if (bHasNearbyEntry)
	{
		TestEqual(TEXT("Bridge-opened nearby entry should match seeded item"), NearbyEntry.ItemId, WaterBottleId);
	}

	ViewModel->HidePanel();

	TestFalse(TEXT("Panel should hide on explicit close"), ViewModel->GetbPanelVisible());
	TestFalse(TEXT("Nearby container state should clear after panel close"), ViewModel->GetbHasNearbyContainer());
	TestFalse(TEXT("Session subsystem should clear active session after panel close"), SessionSubsystem->HasAnyActiveSession());

	Inventory->DestroyComponent();
	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_LiveLootContainerDefinitionsCanonicalStorageTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Content.LiveLootContainerDefinitionsCanonicalStorage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_ToggleCloseAllowsReopenTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.ToggleCloseAllowsReopen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_FullOpenEmptyContainerStaysOpenAndAcceptsStoreTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.FullOpenEmptyContainerStaysOpenAndAcceptsStore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_ViewModelStoreFailureRefreshesInventoryStateTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.ViewModelStoreFailureRefreshesInventoryState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_TakeOneItemThenStoreBackViaViewModelTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.TakeOneItemThenStoreBackViaViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_LiveCardboardBoxStoreBackBreadAfterEmptyTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Content.LiveCardboardBoxStoreBackBreadAfterEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_ToggleCloseAllowsReopenTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for toggle-close reopen test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve"), World);
	if (!World)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem =
		LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootContainerJson(TEXT("ToggleCloseAllowsReopen"), true, TEXT("ObjectDefinition:WaterBottle"), 2),
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Definition should parse for toggle-close reopen test"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should exist"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory component should be created"), Inventory);
	if (!Inventory)
	{
		Spawned->Destroy();
		return false;
	}

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Inventory view model should be created"), ViewModel);
	if (!ViewModel)
	{
		Inventory->DestroyComponent();
		Spawned->Destroy();
		return false;
	}
	ViewModel->Initialize(nullptr);

	ViewModel->SetInventorySource(Inventory);

	FText OpenError;
	const bool bFirstOpenRequested =
		IInventoryWorldContainerTransferBridge::Execute_RequestOpenWorldContainerSession(
			Inventory,
			Spawned,
			EContainerSessionMode::FullOpen,
			OpenError);

	TestTrue(TEXT("Bridge should request first world-container open"), bFirstOpenRequested);
	if (!bFirstOpenRequested)
	{
		AddError(FString::Printf(TEXT("First open error: %s"), *OpenError.ToString()));
		Inventory->DestroyComponent();
		Spawned->Destroy();
		return false;
	}

	TestTrue(TEXT("Session subsystem should track active session after first open"), SessionSubsystem->HasAnyActiveSession());
	TestTrue(TEXT("ViewModel should expose nearby container after first open"), ViewModel->GetbHasNearbyContainer());
	TestTrue(TEXT("ViewModel should show panel after first open"), ViewModel->GetbPanelVisible());

	ViewModel->TogglePanel();

	TestFalse(TEXT("TogglePanel should hide the panel when nearby loot is open"), ViewModel->GetbPanelVisible());
	TestFalse(TEXT("TogglePanel should clear nearby container state when nearby loot is open"), ViewModel->GetbHasNearbyContainer());
	TestFalse(TEXT("TogglePanel should close the active session so reopen is possible"), SessionSubsystem->HasAnyActiveSession());

	FText ReopenError;
	const bool bReopenRequested =
		IInventoryWorldContainerTransferBridge::Execute_RequestOpenWorldContainerSession(
			Inventory,
			Spawned,
			EContainerSessionMode::FullOpen,
			ReopenError);

	TestTrue(TEXT("Bridge should allow reopen after toggle-close"), bReopenRequested);
	if (!bReopenRequested)
	{
		AddError(FString::Printf(TEXT("Reopen error: %s"), *ReopenError.ToString()));
	}
	else
	{
		TestTrue(TEXT("Session subsystem should track active session after reopen"), SessionSubsystem->HasAnyActiveSession());
		TestTrue(TEXT("ViewModel should expose nearby container after reopen"), ViewModel->GetbHasNearbyContainer());
		TestTrue(TEXT("ViewModel should show panel after reopen"), ViewModel->GetbPanelVisible());
		TestEqual(TEXT("Reopened nearby container grid width should match storage section"), ViewModel->GetSecondaryGridWidth(), 4);
		TestEqual(TEXT("Reopened nearby container grid height should match storage section"), ViewModel->GetSecondaryGridHeight(), 5);
	}

	ViewModel->HidePanel();

	Inventory->DestroyComponent();
	Spawned->Destroy();
	return true;
}

bool FInventoryLootPlaces_FullOpenEmptyContainerStaysOpenAndAcceptsStoreTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	const FPrimaryAssetId CigaretteId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Cigarette"));
	TestTrue(TEXT("WaterBottle asset should load for empty-full-open session test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	TestTrue(TEXT("Cigarette asset should load for empty-full-open session test"), EnsureInventoryLootPlacesTestAssetLoaded(CigaretteId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId) || !EnsureInventoryLootPlacesTestAssetLoaded(CigaretteId))
	{
		return false;
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Automation world should resolve for empty-full-open session test"), World);
	if (!World)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve for empty-full-open session test"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem =
		LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Container session subsystem should exist for empty-full-open session test"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Def(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootContainerJson(TEXT("FullOpenEmptyContainer"), true, TEXT("ObjectDefinition:WaterBottle"), 1),
		Def.Get(),
		ParseError);

	TestTrue(TEXT("Definition should parse for empty-full-open session test"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Def.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should exist for empty-full-open session test"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory component should be created for empty-full-open session test"), Inventory);
	if (!Inventory)
	{
		Spawned->Destroy();
		return false;
	}

	TestEqual(TEXT("Player inventory should receive one cigarette to store later"), Inventory->TryAddItem(CigaretteId, 1), 1);

	FInventoryEntry CigaretteEntry;
	TestTrue(TEXT("Stored cigarette entry should be readable"), Inventory->FindEntryByItemId(CigaretteId, CigaretteEntry));

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Inventory view model should be created for empty-full-open session test"), ViewModel);
	if (!ViewModel)
	{
		Inventory->DestroyComponent();
		Spawned->Destroy();
		return false;
	}
	ViewModel->Initialize(nullptr);
	ViewModel->SetInventorySource(Inventory);

	FText OpenError;
	const bool bOpenRequested =
		IInventoryWorldContainerTransferBridge::Execute_RequestOpenWorldContainerSession(
			Inventory,
			Spawned,
			EContainerSessionMode::FullOpen,
			OpenError);

	TestTrue(TEXT("Bridge should request world-container open for empty-full-open session test"), bOpenRequested);
	if (!bOpenRequested)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		Inventory->DestroyComponent();
		Spawned->Destroy();
		return false;
	}

	TestTrue(TEXT("Session subsystem should track active session after full-open"), SessionSubsystem->HasAnyActiveSession());
	TestTrue(TEXT("ViewModel should expose nearby container after full-open"), ViewModel->GetbHasNearbyContainer());
	TestTrue(TEXT("ViewModel should report nearby entries before take-all"), ViewModel->HasNearbyEntries());

	ViewModel->RequestTakeAllNearbyContainer();

	TestTrue(TEXT("Full-open session should remain active after container becomes empty"), SessionSubsystem->HasAnyActiveSession());
	TestTrue(TEXT("Nearby container section should remain visible after take-all"), ViewModel->GetbHasNearbyContainer());
	TestTrue(TEXT("Inventory panel should remain visible after take-all"), ViewModel->GetbPanelVisible());
	TestFalse(TEXT("Nearby container should now be empty without closing"), ViewModel->HasNearbyEntries());
	TestTrue(TEXT("Container actor should remain valid after take-all"), IsValid(Spawned));

	ViewModel->RequestStoreItemInNearbyContainerAt(static_cast<int32>(CigaretteEntry.InstanceId), FIntPoint(0, 0), false, 1);

	TestTrue(TEXT("Nearby container should accept a stored item after becoming empty"), ViewModel->HasNearbyEntries());
	TestFalse(TEXT("Inventory should no longer contain the stored cigarette"), Inventory->ContainsItem(CigaretteId, 1));

	ViewModel->HidePanel();

	Inventory->DestroyComponent();
	if (IsValid(Spawned))
	{
		Spawned->Destroy();
	}
	return true;
}

bool FInventoryLootPlaces_ViewModelStoreFailureRefreshesInventoryStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId BreadSliceId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:BreadSlice"));
	TestTrue(TEXT("BreadSlice asset should load for store-failure refresh test"), EnsureInventoryLootPlacesTestAssetLoaded(BreadSliceId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(BreadSliceId))
	{
		return false;
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem = LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory component should be created"), Inventory);
	if (!Inventory)
	{
		return false;
	}

	TestEqual(TEXT("BreadSlice should be added to inventory"), Inventory->TryAddItem(BreadSliceId, 1), 1);

	FInventoryEntry BreadEntryForFailureTest;
	TestTrue(TEXT("BreadSlice entry should be findable for store-failure test"), Inventory->FindEntryByItemId(BreadSliceId, BreadEntryForFailureTest));
	if (!Inventory->FindEntryByItemId(BreadSliceId, BreadEntryForFailureTest))
	{
		InventoryOwner->Destroy();
		return false;
	}

	UWorldContainerSessionTestDouble* SessionSource = nullptr;
	AActor* SourceActor = CreateWorldContainerSessionTestActor(World, SessionSource);
	TestNotNull(TEXT("Store-failure source actor should be created"), SourceActor);
	TestNotNull(TEXT("Store-failure session source should be created"), SessionSource);
	if (!SourceActor || !SessionSource)
	{
		InventoryOwner->Destroy();
		return false;
	}

	SessionSource->bFailStore = true;
	SessionSource->bSupportsQuickLoot = false;
	SessionSource->bSupportsFullOpen = true;

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Inventory view model should be created"), ViewModel);
	if (!ViewModel)
	{
		InventoryOwner->Destroy();
		SourceActor->Destroy();
		return false;
	}
	ViewModel->Initialize(nullptr);
	ViewModel->SetInventorySource(Inventory);

	FText OpenError;
	const bool bOpenRequested =
		IInventoryWorldContainerTransferBridge::Execute_RequestOpenWorldContainerSession(
			Inventory,
			SourceActor,
			EContainerSessionMode::FullOpen,
			OpenError);

	TestTrue(TEXT("Bridge should request world-container open for store-failure refresh test"), bOpenRequested);
	if (!bOpenRequested)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		InventoryOwner->Destroy();
		SourceActor->Destroy();
		return false;
	}

	TestTrue(TEXT("View model should expose nearby container before failed store"), ViewModel->GetbHasNearbyContainer());

	ViewModel->RequestStoreItemInNearbyContainerAt(BreadEntryForFailureTest.InstanceId, FIntPoint(0, 0), false, 1);

	TestTrue(TEXT("Inventory should still contain BreadSlice after failed store"), Inventory->ContainsItem(BreadSliceId, 1));
	TestFalse(TEXT("Nearby container should remain empty after failed store"), ViewModel->HasNearbyEntries());

	FInventoryEntry RestoredEntry;
	TestTrue(TEXT("BreadSlice entry should still be readable after failed store"), Inventory->FindEntryByItemId(BreadSliceId, RestoredEntry));

	InventoryOwner->Destroy();
	SourceActor->Destroy();
	return true;
}

bool FInventoryLootPlaces_TakeOneItemThenStoreBackViaViewModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Test world should resolve"), World);
	if (!World) { return false; }

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer) { return false; }

	TStrongObjectPtr<UObjectDefinition> Definition;
	FString ParseError;
	const bool bLoaded = LoadInventoryLootPlacesDefinitionFromFile(
		TEXT("Plugins/Resources/ProjectObject/Content/HumanMade/Trash/Packaging/Cardboard/Set_1/Loot_CardboardBox.json"),
		Definition, ParseError);
	TestTrue(TEXT("Cardboard-box definition should parse"), bLoaded);
	if (!bLoaded)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World, Definition.Get(), FTransform::Identity, SpawnParams, &SpawnError);
	TestNotNull(TEXT("Cardboard-box actor should spawn"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = Spawned->FindComponentByClass<ULootContainerCapabilityComponent>();
	TestNotNull(TEXT("Loot component should exist"), LootComponent);
	if (!LootComponent) { Spawned->Destroy(); return false; }

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory should be created"), Inventory);
	if (!Inventory) { Spawned->Destroy(); return false; }

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("ViewModel should be created"), ViewModel);
	if (!ViewModel) { InventoryOwner->Destroy(); Spawned->Destroy(); return false; }
	ViewModel->Initialize(nullptr);
	ViewModel->SetInventorySource(Inventory);

	FText OpenError;
	const bool bOpened = IInventoryWorldContainerTransferBridge::Execute_RequestOpenWorldContainerSession(
		Inventory, Spawned, EContainerSessionMode::FullOpen, OpenError);
	TestTrue(TEXT("Session should open"), bOpened);
	if (!bOpened)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		InventoryOwner->Destroy();
		Spawned->Destroy();
		return false;
	}

	TestTrue(TEXT("ViewModel should expose nearby container"), ViewModel->GetbHasNearbyContainer());
	TestTrue(TEXT("Nearby should have entries at start"), ViewModel->HasNearbyEntries());

	const int32 InitialNearbyCount = IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent).Num();
	AddInfo(FString::Printf(TEXT("Initial nearby entry count: %d"), InitialNearbyCount));
	TestTrue(TEXT("Box should have at least one loot entry"), InitialNearbyCount > 0);
	if (InitialNearbyCount == 0) { InventoryOwner->Destroy(); Spawned->Destroy(); return false; }

	const TArray<FInventoryEntryView> NearbyBefore = IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent);
	const FInventoryEntryView FirstNearbyEntry = NearbyBefore[0];
	const FPrimaryAssetId TakenItemId = FirstNearbyEntry.ItemId;
	const FIntPoint TakenGridPos = FirstNearbyEntry.GridPos;
	AddInfo(FString::Printf(TEXT("Taking first nearby entry: %s InstanceId=%d at (%d,%d)"),
		*TakenItemId.ToString(), FirstNearbyEntry.InstanceId, TakenGridPos.X, TakenGridPos.Y));

	ViewModel->RequestTakeNearbyItem(FirstNearbyEntry.InstanceId, FirstNearbyEntry.Quantity);

	const int32 AfterTakeNearbyCount = IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent).Num();
	TestEqual(TEXT("Nearby should have one fewer entry after take"), AfterTakeNearbyCount, InitialNearbyCount - 1);

	FInventoryEntry TakenEntry;
	TestTrue(TEXT("Taken item should now be in player inventory"), Inventory->FindEntryByItemId(TakenItemId, TakenEntry));
	if (!Inventory->FindEntryByItemId(TakenItemId, TakenEntry))
	{
		AddError(TEXT("Taken item not found in inventory - cannot test store-back"));
		InventoryOwner->Destroy();
		Spawned->Destroy();
		return false;
	}

	AddInfo(FString::Printf(TEXT("Taken item in inventory: InstanceId=%d at %s (%d,%d). Storing back at (%d,%d)"),
		TakenEntry.InstanceId, *TakenEntry.ContainerId.ToString(),
		TakenEntry.GridPos.X, TakenEntry.GridPos.Y,
		TakenGridPos.X, TakenGridPos.Y));

	const int32 InventoryCountBefore = Inventory->GetEntries().Num();

	ViewModel->RequestStoreItemInNearbyContainerAt(
		TakenEntry.InstanceId,
		TakenGridPos,
		false,
		TakenEntry.Quantity);

	const int32 InventoryCountAfter = Inventory->GetEntries().Num();
	TestEqual(TEXT("Item should be removed from inventory after store-back"),
		InventoryCountAfter, InventoryCountBefore - 1);

	TestFalse(TEXT("Inventory should no longer contain the stored-back item"),
		Inventory->ContainsItem(TakenItemId, 1));

	const int32 FinalNearbyCount = IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent).Num();
	TestEqual(TEXT("Container should have original entry count after store-back"), FinalNearbyCount, InitialNearbyCount);

	TestTrue(TEXT("ViewModel HasNearbyEntries should be true after store-back"), ViewModel->HasNearbyEntries());

	const TArray<FInventoryEntryView> FinalNearbyViews =
		IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent);
	const FInventoryEntryView* StoredBackView = FinalNearbyViews.FindByPredicate(
		[TakenItemId](const FInventoryEntryView& V) { return V.ItemId == TakenItemId; });
	TestNotNull(TEXT("Stored-back item should be visible in container entry views"), StoredBackView);
	if (StoredBackView)
	{
		TestEqual(TEXT("Stored-back item should be at original grid pos"), StoredBackView->GridPos, TakenGridPos);
	}

	AddInfo(FString::Printf(TEXT("Final state: Inventory=%d entries, Nearby=%d entries, VM.HasNearby=%d"),
		InventoryCountAfter, FinalNearbyCount, ViewModel->HasNearbyEntries() ? 1 : 0));

	InventoryOwner->Destroy();
	Spawned->Destroy();
	return true;
}

bool FInventoryLootPlaces_LiveCardboardBoxStoreBackBreadAfterEmptyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId BreadSliceId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:BreadSlice"));
	TestTrue(TEXT("BreadSlice asset should load for live cardboard store-back test"), EnsureInventoryLootPlacesTestAssetLoaded(BreadSliceId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(BreadSliceId))
	{
		return false;
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	TStrongObjectPtr<UObjectDefinition> Definition;
	FString ParseError;
	const FString RelativePath =
		TEXT("Plugins/Resources/ProjectObject/Content/HumanMade/Trash/Packaging/Cardboard/Set_1/Loot_CardboardBox.json");
	const bool bLoaded = LoadInventoryLootPlacesDefinitionFromFile(RelativePath, Definition, ParseError);
	TestTrue(TEXT("Live cardboard-box definition should parse"), bLoaded);
	if (!bLoaded)
	{
		AddError(FString::Printf(TEXT("%s parse error: %s"), *RelativePath, *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Definition.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Live cardboard-box actor should spawn"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = Spawned->FindComponentByClass<ULootContainerCapabilityComponent>();
	TestNotNull(TEXT("Live cardboard-box loot component should exist"), LootComponent);
	if (!LootComponent)
	{
		Spawned->Destroy();
		return false;
	}

	TestFalse(TEXT("Live cardboard-box should no longer be one-time-use"), LootComponent->bOneTimeUse);

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory component should be created"), Inventory);
	if (!Inventory)
	{
		Spawned->Destroy();
		return false;
	}

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Inventory view model should be created"), ViewModel);
	if (!ViewModel)
	{
		InventoryOwner->Destroy();
		Spawned->Destroy();
		return false;
	}
	ViewModel->Initialize(nullptr);
	ViewModel->SetInventorySource(Inventory);

	FText OpenError;
	const bool bOpenRequested =
		IInventoryWorldContainerTransferBridge::Execute_RequestOpenWorldContainerSession(
			Inventory,
			Spawned,
			EContainerSessionMode::FullOpen,
			OpenError);

	TestTrue(TEXT("Bridge should request world-container open for live cardboard-box test"), bOpenRequested);
	if (!bOpenRequested)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		InventoryOwner->Destroy();
		Spawned->Destroy();
		return false;
	}

	TestTrue(TEXT("View model should expose nearby cardboard box"), ViewModel->GetbHasNearbyContainer());
	ViewModel->RequestTakeAllNearbyContainer();
	TestFalse(TEXT("Nearby cardboard box should be empty after take-all"), ViewModel->HasNearbyEntries());

	TestEqual(TEXT("BreadSlice should be added for store-back regression"), Inventory->TryAddItem(BreadSliceId, 1), 1);

	FInventoryEntry BreadEntryForStore;
	TestTrue(TEXT("BreadSlice entry should be findable before store-back"), Inventory->FindEntryByItemId(BreadSliceId, BreadEntryForStore));
	if (!Inventory->FindEntryByItemId(BreadSliceId, BreadEntryForStore))
	{
		InventoryOwner->Destroy();
		Spawned->Destroy();
		return false;
	}

	ViewModel->RequestStoreItemInNearbyContainerAt(BreadEntryForStore.InstanceId, FIntPoint(0, 0), false, 1);

	const TArray<FInventoryEntryView> NearbyEntries = IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent);
	TestTrue(TEXT("Live cardboard box should expose entries after bread is stored back"), NearbyEntries.Num() > 0);

	const FInventoryEntryView* StoredBreadEntry = NearbyEntries.FindByPredicate([BreadSliceId](const FInventoryEntryView& Entry)
	{
		return Entry.ItemId == BreadSliceId;
	});
	TestNotNull(TEXT("Live cardboard box should expose stored BreadSlice entry"), StoredBreadEntry);
	if (StoredBreadEntry)
	{
		TestEqual(TEXT("Stored BreadSlice should occupy requested nearby cell"), StoredBreadEntry->GridPos, FIntPoint(0, 0));
	}

	TestTrue(TEXT("View model should show nearby entries after bread is stored back"), ViewModel->HasNearbyEntries());

	InventoryOwner->Destroy();
	Spawned->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_LiveCardboardBoxTakeOneThenStoreBackTest,
	"ProjectIntegrationTests.InventoryLootPlaces.Content.LiveCardboardBoxTakeOneThenStoreBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_LiveCardboardBoxTakeOneThenStoreBackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	TStrongObjectPtr<UObjectDefinition> Definition;
	FString ParseError;
	const FString RelativePath =
		TEXT("Plugins/Resources/ProjectObject/Content/HumanMade/Trash/Packaging/Cardboard/Set_1/Loot_CardboardBox.json");
	const bool bLoaded = LoadInventoryLootPlacesDefinitionFromFile(RelativePath, Definition, ParseError);
	TestTrue(TEXT("Cardboard-box definition should parse"), bLoaded);
	if (!bLoaded)
	{
		AddError(FString::Printf(TEXT("%s parse error: %s"), *RelativePath, *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Definition.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Cardboard-box actor should spawn"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	ULootContainerCapabilityComponent* LootComponent = Spawned->FindComponentByClass<ULootContainerCapabilityComponent>();
	TestNotNull(TEXT("Cardboard-box loot component should exist"), LootComponent);
	if (!LootComponent)
	{
		Spawned->Destroy();
		return false;
	}

	AActor* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesTestInventory(World, InventoryOwner);
	TestNotNull(TEXT("Inventory component should be created"), Inventory);
	if (!Inventory)
	{
		Spawned->Destroy();
		return false;
	}

	const TArray<FInventoryEntryView> InitialEntries =
		IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent);
	const int32 InitialEntryCount = InitialEntries.Num();
	TestTrue(TEXT("Cardboard box should have loot entries at spawn"), InitialEntryCount > 0);
	if (InitialEntryCount == 0)
	{
		AddError(TEXT("No loot entries generated - cannot test take-one-then-store-back"));
		Inventory->DestroyComponent();
		if (InventoryOwner) { InventoryOwner->Destroy(); }
		Spawned->Destroy();
		return false;
	}

	const FInventoryEntryView FirstEntry = InitialEntries[0];
	AddInfo(FString::Printf(TEXT("Taking first entry: %s x%d (InstanceId=%d) at (%d,%d)"),
		*FirstEntry.ItemId.ToString(), FirstEntry.Quantity, FirstEntry.InstanceId,
		FirstEntry.GridPos.X, FirstEntry.GridPos.Y));

	const FIntPoint TakenGridPos = FirstEntry.GridPos;
	const FPrimaryAssetId TakenItemId = FirstEntry.ItemId;

	FContainerEntryTransfer ConsumeTransfer;
	ConsumeTransfer.EntryInstanceId = FirstEntry.InstanceId;
	ConsumeTransfer.ObjectId = FirstEntry.ItemId;
	ConsumeTransfer.Quantity = FirstEntry.Quantity;
	ConsumeTransfer.GridPos = FirstEntry.GridPos;
	ConsumeTransfer.bRotated = FirstEntry.bRotated;

	TArray<FLootEntry> LootItems;
	LootItems.Add(FLootEntry(FirstEntry.ItemId, FirstEntry.Quantity));
	Inventory->AddItemsBatch(LootItems);

	FInventoryEntry TakenInventoryEntry;
	TestTrue(TEXT("Taken item should be in inventory"), Inventory->FindEntryByItemId(TakenItemId, TakenInventoryEntry));
	if (!Inventory->FindEntryByItemId(TakenItemId, TakenInventoryEntry))
	{
		Inventory->DestroyComponent();
		if (InventoryOwner) { InventoryOwner->Destroy(); }
		Spawned->Destroy();
		return false;
	}

	TArray<FContainerEntryTransfer> ConsumeEntries;
	ConsumeEntries.Add(ConsumeTransfer);
	FText ConsumeError;
	const bool bConsumed = IWorldContainerSessionSource::Execute_ConsumeContainerEntries(
		LootComponent,
		FGuid::NewGuid(),
		ConsumeEntries,
		ConsumeError);
	TestTrue(TEXT("Container should accept consume of first entry"), bConsumed);

	const TArray<FInventoryEntryView> AfterTakeEntries =
		IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent);
	const int32 AfterTakeCount = AfterTakeEntries.Num();
	TestEqual(TEXT("Container should have one fewer entry after take"), AfterTakeCount, InitialEntryCount - 1);

	AddInfo(FString::Printf(TEXT("Container after take: %d entries (was %d). Storing %s back at (%d,%d)"),
		AfterTakeCount, InitialEntryCount,
		*TakenItemId.ToString(), TakenGridPos.X, TakenGridPos.Y));

	FContainerEntryTransfer StoreTransfer;
	StoreTransfer.ObjectId = TakenItemId;
	StoreTransfer.Quantity = TakenInventoryEntry.Quantity;
	StoreTransfer.GridPos = TakenGridPos;
	StoreTransfer.bRotated = false;

	TArray<FContainerEntryTransfer> StoreEntries;
	StoreEntries.Add(StoreTransfer);

	FText CanStoreError;
	const bool bCanStore = IWorldContainerSessionSource::Execute_CanStoreContainerEntries(
		LootComponent,
		FGuid::NewGuid(),
		StoreEntries,
		CanStoreError);

	AddInfo(FString::Printf(TEXT("CanStore result: %s (Error: %s)"),
		bCanStore ? TEXT("true") : TEXT("false"),
		*CanStoreError.ToString()));
	TestTrue(TEXT("Container should accept store-back at original position"), bCanStore);

	FText StoreError;
	const bool bStored = IWorldContainerSessionSource::Execute_StoreContainerEntries(
		LootComponent,
		FGuid::NewGuid(),
		StoreEntries,
		StoreError);

	AddInfo(FString::Printf(TEXT("Store result: %s (Error: %s)"),
		bStored ? TEXT("true") : TEXT("false"),
		*StoreError.ToString()));
	TestTrue(TEXT("Container should commit store-back"), bStored);

	const TArray<FInventoryEntryView> FinalEntries =
		IWorldContainerSessionSource::Execute_GetContainerEntryViews(LootComponent);
	TestEqual(TEXT("Container should have original entry count after store-back"), FinalEntries.Num(), InitialEntryCount);

	const FInventoryEntryView* RestoredEntry = FinalEntries.FindByPredicate([TakenItemId](const FInventoryEntryView& Entry)
	{
		return Entry.ItemId == TakenItemId;
	});
	TestNotNull(TEXT("Stored item should be visible in container entries"), RestoredEntry);
	if (RestoredEntry)
	{
		TestEqual(TEXT("Stored item should be at original grid position"), RestoredEntry->GridPos, TakenGridPos);
	}

	Inventory->DestroyComponent();
	if (InventoryOwner) { InventoryOwner->Destroy(); }
	Spawned->Destroy();
	return true;
}

bool FInventoryLootPlaces_LiveLootContainerDefinitionsCanonicalStorageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	const FString LootProfileRelativePath =
		TEXT("Plugins/Resources/ProjectObject/Data/LootProfiles/Scavenge/Scavenge_SmallConsumables.json");
	TStrongObjectPtr<ULootProfileDefinition> LootProfileDefinition;
	FString LootProfileParseError;
	const bool bLootProfileLoaded = LoadInventoryLootPlacesLootProfileFromFile(
		LootProfileRelativePath,
		LootProfileDefinition,
		LootProfileParseError);
	TestTrue(TEXT("Shared loot profile JSON should parse"), bLootProfileLoaded);
	if (!bLootProfileLoaded)
	{
		AddError(FString::Printf(TEXT("%s parse error: %s"), *LootProfileRelativePath, *LootProfileParseError));
		return false;
	}

	TestEqual(TEXT("Shared loot profile id should match expected canonical id"),
		LootProfileDefinition->ProfileId,
		FName(TEXT("Scavenge_SmallConsumables")));
	TestEqual(TEXT("Shared loot profile pick min should be 2"), LootProfileDefinition->PickCountMin, 2);
	TestEqual(TEXT("Shared loot profile pick max should be 3"), LootProfileDefinition->PickCountMax, 3);
	TestEqual(TEXT("Shared loot profile should keep 6 authored entries"), LootProfileDefinition->Entries.Num(), 6);

	const TArray<FString> RelativePaths = {
		TEXT("Plugins/Resources/ProjectObject/Content/Human/DeadJunky/Loot_DeadJunky.json"),
		TEXT("Plugins/Resources/ProjectObject/Content/HumanMade/Container/Bags/Bag/Loot_DuffleBag.json"),
		TEXT("Plugins/Resources/ProjectObject/Content/HumanMade/Container/Store/Mailcase/Loot_Postbox.json"),
		TEXT("Plugins/Resources/ProjectObject/Content/HumanMade/Trash/Packaging/Cardboard/Set_1/Loot_CardboardBox.json")
	};

	for (const FString& RelativePath : RelativePaths)
	{
		TStrongObjectPtr<UObjectDefinition> Definition;
		FString ParseError;
		const bool bLoaded = LoadInventoryLootPlacesDefinitionFromFile(RelativePath, Definition, ParseError);
		TestTrue(FString::Printf(TEXT("%s should parse"), *RelativePath), bLoaded);
		if (!bLoaded)
		{
			AddError(FString::Printf(TEXT("%s parse error: %s"), *RelativePath, *ParseError));
			continue;
		}

		const FStorageSection* StorageSection = Definition->GetStorageSection();
		TestNotNull(FString::Printf(TEXT("%s should expose storage section"), *RelativePath), StorageSection);
		if (!StorageSection)
		{
			continue;
		}

		TestTrue(FString::Printf(TEXT("%s storage grid width should be positive"), *RelativePath), StorageSection->GridSize.X > 0);
		TestTrue(FString::Printf(TEXT("%s storage grid height should be positive"), *RelativePath), StorageSection->GridSize.Y > 0);
		TestTrue(FString::Printf(TEXT("%s should reference a shared loot profile"), *RelativePath), StorageSection->HasLootProfile());
		TestEqual(FString::Printf(TEXT("%s loot profile id should match shared canonical profile"), *RelativePath),
			StorageSection->LootProfileId.ToString(),
			FString(TEXT("LootProfileDefinition:Scavenge_SmallConsumables")));

		FText SpawnError;
		FActorSpawnParameters SpawnParams;
		AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
			World,
			Definition.Get(),
			FTransform::Identity,
			SpawnParams,
			&SpawnError);

		TestNotNull(FString::Printf(TEXT("%s should spawn"), *RelativePath), Spawned);
		if (!Spawned)
		{
			AddError(FString::Printf(TEXT("%s spawn error: %s"), *RelativePath, *SpawnError.ToString()));
			continue;
		}

		UObject* SourceObject = ResolveInventoryLootPlacesWorldContainerSource(Spawned);
		TestNotNull(FString::Printf(TEXT("%s should expose world-container source"), *RelativePath), SourceObject);
		if (SourceObject)
		{
			TestTrue(
				FString::Printf(TEXT("%s should support FullOpen after canonical storage migration"), *RelativePath),
				IWorldContainerSessionSource::Execute_SupportsContainerSession(SourceObject, EContainerSessionMode::FullOpen));

			const TArray<FInventoryEntryView> EntryViews =
				IWorldContainerSessionSource::Execute_GetContainerEntryViews(SourceObject);
			TestTrue(
				FString::Printf(TEXT("%s should realize 2-3 profile-driven entries"), *RelativePath),
				EntryViews.Num() >= 2 && EntryViews.Num() <= 3);

			TSet<FPrimaryAssetId> SeenIds;
			for (const FInventoryEntryView& EntryView : EntryViews)
			{
				TestEqual(
					FString::Printf(TEXT("%s random loot entries should have quantity 1"), *RelativePath),
					EntryView.Quantity,
					1);
				TestTrue(
					FString::Printf(TEXT("%s random loot entries should be unique"), *RelativePath),
					!SeenIds.Contains(EntryView.ItemId));
				SeenIds.Add(EntryView.ItemId);
			}
		}

		Spawned->Destroy();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLootPlaces_LateBoundViewModelRehydratesActiveSessionTest,
	"ProjectIntegrationTests.InventoryLootPlaces.UI.LateBoundViewModelRehydratesActiveSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryLootPlaces_LateBoundViewModelRehydratesActiveSessionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FPrimaryAssetId WaterBottleId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:WaterBottle"));
	TestTrue(TEXT("WaterBottle asset should load for late-bound session test"), EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId));
	if (!EnsureInventoryLootPlacesTestAssetLoaded(WaterBottleId))
	{
		return false;
	}

	UWorld* World = ResolveInventoryLootPlacesTestWorld();
	TestNotNull(TEXT("Test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = ResolveInventoryLootPlacesTestLocalPlayer(World);
	TestNotNull(TEXT("Local player should resolve"), LocalPlayer);
	if (!LocalPlayer)
	{
		return false;
	}

	UProjectContainerSessionSubsystem* SessionSubsystem = LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>();
	TestNotNull(TEXT("Session subsystem should exist"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();

	TStrongObjectPtr<UObjectDefinition> Definition(
		NewObject<UObjectDefinition>(GetTransientPackage(), NAME_None, RF_Transient));

	FString ParseError;
	const bool bParsed = FDefinitionJsonParser::ParseJsonToAsset(
		TypeInfo,
		MakeLootContainerJson(TEXT("LateBoundViewModel"), true, TEXT("ObjectDefinition:WaterBottle"), 2),
		Definition.Get(),
		ParseError);

	TestTrue(TEXT("Definition should parse for late-bound session test"), bParsed);
	if (!bParsed)
	{
		AddError(FString::Printf(TEXT("Parse error: %s"), *ParseError));
		return false;
	}

	FText SpawnError;
	FActorSpawnParameters SpawnParams;
	AActor* Spawned = ProjectObjectSpawn::SpawnFromDefinition(
		World,
		Definition.Get(),
		FTransform::Identity,
		SpawnParams,
		&SpawnError);

	TestNotNull(TEXT("Spawned actor should exist"), Spawned);
	if (!Spawned)
	{
		AddError(FString::Printf(TEXT("Spawn error: %s"), *SpawnError.ToString()));
		return false;
	}

	APlayerController* InventoryOwner = nullptr;
	UProjectInventoryComponent* Inventory = CreateInventoryLootPlacesPlayerInventory(World, InventoryOwner);
	TestNotNull(TEXT("Player inventory component should be created"), Inventory);
	if (!Inventory)
	{
		Spawned->Destroy();
		return false;
	}

	FText OpenError;
	const bool bOpenRequested =
		IInventoryWorldContainerTransferBridge::Execute_RequestOpenWorldContainerSession(
			Inventory,
			Spawned,
			EContainerSessionMode::FullOpen,
			OpenError);

	TestTrue(TEXT("Bridge should request world-container open before view model bind"), bOpenRequested);
	if (!bOpenRequested)
	{
		AddError(FString::Printf(TEXT("Open error: %s"), *OpenError.ToString()));
		Inventory->DestroyComponent();
		Spawned->Destroy();
		return false;
	}

	TestTrue(TEXT("Session subsystem should track active session before view model bind"), SessionSubsystem->HasAnyActiveSession());

	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Inventory view model should be created"), ViewModel);
	if (!ViewModel)
	{
		Inventory->DestroyComponent();
		Spawned->Destroy();
		return false;
	}
	ViewModel->Initialize(nullptr);

	ViewModel->SetInventorySource(Inventory);

	TestTrue(TEXT("Late-bound view model should recover nearby container state"), ViewModel->GetbHasNearbyContainer());
	TestTrue(TEXT("Late-bound view model should show the panel for active session"), ViewModel->GetbPanelVisible());
	TestEqual(TEXT("Recovered nearby container grid width should match storage section"), ViewModel->GetSecondaryGridWidth(), 4);
	TestEqual(TEXT("Recovered nearby container grid height should match storage section"), ViewModel->GetSecondaryGridHeight(), 5);

	FInventoryEntryView NearbyEntry;
	const bool bHasNearbyEntry = ViewModel->TryGetSecondaryEntryByCellIndex(0, NearbyEntry);
	TestTrue(TEXT("Recovered nearby container should expose entry views"), bHasNearbyEntry);
	if (bHasNearbyEntry)
	{
		TestEqual(TEXT("Recovered nearby entry should match seeded item"), NearbyEntry.ItemId, WaterBottleId);
	}

	ViewModel->HidePanel();

	TestFalse(TEXT("Panel should hide after explicit close"), ViewModel->GetbPanelVisible());
	TestFalse(TEXT("Nearby container state should clear after explicit close"), ViewModel->GetbHasNearbyContainer());
	TestFalse(TEXT("Session subsystem should clear active session after explicit close"), SessionSubsystem->HasAnyActiveSession());

	Inventory->DestroyComponent();
	Spawned->Destroy();
	return true;
}
