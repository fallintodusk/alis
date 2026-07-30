// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

/**
 * Unit Tests for ProjectInventoryComponent
 *
 * Tests verify component creation, query API, and basic state.
 * Note: Full add/remove tests require ObjectDefinition assets (integration tests).
 */

#include "Misc/AutomationTest.h"
#include "Components/ProjectInventoryComponent.h"
#include "Helpers/InventoryMoveHelper.h"
#include "Inventory/InventoryTypes.h"
#include "Services/ObjectDefinitionCache.h"
#include "Subsystems/ProjectObjectDefinitionCacheSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// Component Creation Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryComponent_DefaultConstruction,
	"ProjectInventory.Component.DefaultConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryComponent_DefaultConstruction::RunTest(const FString& Parameters)
{
	UProjectInventoryComponent* Component = NewObject<UProjectInventoryComponent>();

	TestNotNull(TEXT("Component created"), Component);
	TestEqual(TEXT("Item count is 0"), Component->GetItemCount(), 0);
	TestTrue(TEXT("Has space initially"), Component->HasSpace());

	return true;
}

// ============================================================================
// Query API Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryComponent_GetEntries_Empty,
	"ProjectInventory.Component.GetEntries.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryComponent_GetEntries_Empty::RunTest(const FString& Parameters)
{
	UProjectInventoryComponent* Component = NewObject<UProjectInventoryComponent>();

	const TArray<FInventoryEntry>& Entries = Component->GetEntries();

	TestEqual(TEXT("GetEntries returns empty array"), Entries.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryComponent_FindEntry_NotExists,
	"ProjectInventory.Component.FindEntry.NotExists",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryComponent_FindEntry_NotExists::RunTest(const FString& Parameters)
{
	UProjectInventoryComponent* Component = NewObject<UProjectInventoryComponent>();

	FInventoryEntry OutEntry;
	const bool bFound = Component->FindEntry(999, OutEntry);

	TestFalse(TEXT("FindEntry returns false for non-existent InstanceId"), bFound);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryComponent_FindEntryByItemId_NotExists,
	"ProjectInventory.Component.FindEntryByItemId.NotExists",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryComponent_FindEntryByItemId_NotExists::RunTest(const FString& Parameters)
{
	UProjectInventoryComponent* Component = NewObject<UProjectInventoryComponent>();

	FInventoryEntry OutEntry;
	FPrimaryAssetId TestId(FPrimaryAssetType("ObjectDefinition"), FName("NonExistent"));
	const bool bFound = Component->FindEntryByItemId(TestId, OutEntry);

	TestFalse(TEXT("FindEntryByItemId returns false for non-existent ItemId"), bFound);

	return true;
}

// ============================================================================
// GetItemCount Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryComponent_GetItemCount_Empty,
	"ProjectInventory.Component.GetItemCount.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryComponent_GetItemCount_Empty::RunTest(const FString& Parameters)
{
	UProjectInventoryComponent* Component = NewObject<UProjectInventoryComponent>();

	TestEqual(TEXT("Item count is 0 initially"), Component->GetItemCount(), 0);

	return true;
}

// ============================================================================
// HasSpace Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryComponent_HasSpace_Empty,
	"ProjectInventory.Component.HasSpace.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryComponent_HasSpace_Empty::RunTest(const FString& Parameters)
{
	UProjectInventoryComponent* Component = NewObject<UProjectInventoryComponent>();

	TestTrue(TEXT("HasSpace returns true for empty inventory"), Component->HasSpace());

	return true;
}

// ============================================================================
// IsItemEquipped Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryComponent_IsItemEquipped_NotEquipped,
	"ProjectInventory.Component.IsItemEquipped.NotEquipped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryComponent_IsItemEquipped_NotEquipped::RunTest(const FString& Parameters)
{
	UProjectInventoryComponent* Component = NewObject<UProjectInventoryComponent>();

	TestFalse(TEXT("IsItemEquipped returns false for non-existent item"), Component->IsItemEquipped(999));

	return true;
}

// ============================================================================
// Cache Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryComponent_Cache_DefaultNull,
	"ProjectInventory.Component.Cache.DefaultNull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryComponent_Cache_DefaultNull::RunTest(const FString& Parameters)
{
	UProjectInventoryComponent* Component = NewObject<UProjectInventoryComponent>();

	TestNull(TEXT("ObjectDefinitionCache is null by default"), Component->GetObjectDefinitionCache());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryCacheSubsystem_SingleGameInstanceOwner,
	"ProjectInventory.CacheSubsystem.SingleGameInstanceOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryCacheSubsystem_SingleGameInstanceOwner::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance should be created"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}

	GameInstance->Init();

	UProjectObjectDefinitionCacheSubsystem* Subsystem =
		GameInstance->GetSubsystem<UProjectObjectDefinitionCacheSubsystem>();
	TestNotNull(TEXT("Definition cache subsystem should exist"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	UObjectDefinitionCache* CacheA = Subsystem->GetCache();
	UObjectDefinitionCache* CacheB = Subsystem->GetCache();
	TestNotNull(TEXT("Subsystem should own a cache"), CacheA);
	TestEqual(TEXT("Subsystem should return a stable cache instance"), CacheA, CacheB);
	TestTrue(TEXT("Cache outer should be the subsystem"), CacheA && CacheA->GetOuter() == Subsystem);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryComponent_ResolveMissingDoesNotStartLoad,
	"ProjectInventory.Component.Cache.ResolveMissingDoesNotStartLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryComponent_ResolveMissingDoesNotStartLoad::RunTest(const FString& Parameters)
{
	UObjectDefinitionCache* Cache = NewObject<UObjectDefinitionCache>(GetTransientPackage());
	UProjectInventoryComponent* Component = NewObject<UProjectInventoryComponent>();
	TestNotNull(TEXT("Cache should be created"), Cache);
	TestNotNull(TEXT("Component should be created"), Component);
	if (!Cache || !Component)
	{
		return false;
	}

	Component->SetObjectDefinitionCache(Cache);

	const FPrimaryAssetId MissingId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:ArchitectureMissingDefinition"));
	TestTrue(TEXT("Missing test id should still be a valid primary asset id"), MissingId.IsValid());

	FItemDataView ItemData;
	const EInventoryItemDataResolveState ResolveState = Component->ResolveItemDataView(MissingId, ItemData);
	TestTrue(TEXT("Read resolver should report Missing for unknown definitions"),
		ResolveState == EInventoryItemDataResolveState::Missing);
	TestTrue(TEXT("Read resolver should not start an async load for missing definitions"),
		Cache->GetLoadState(MissingId) == EObjectDefinitionLoadState::Missing);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryCache_CapturesLoadedDefinition,
	"ProjectInventory.Cache.CapturesLoadedDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryCache_CapturesLoadedDefinition::RunTest(const FString& Parameters)
{
	UObjectDefinitionCache* Cache = NewObject<UObjectDefinitionCache>(GetTransientPackage());
	TestNotNull(TEXT("ObjectDefinitionCache should be created"), Cache);
	if (!Cache)
	{
		return false;
	}

	const FPrimaryAssetId CigaretteId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Cigarette"));
	TestTrue(TEXT("Cigarette asset id should be valid"), CigaretteId.IsValid());
	if (!CigaretteId.IsValid())
	{
		return false;
	}

	TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::Get().LoadPrimaryAsset(CigaretteId, TArray<FName>());
	TestTrue(TEXT("AssetManager should return a load handle for Cigarette"), LoadHandle.IsValid());
	if (!LoadHandle.IsValid())
	{
		return false;
	}

	LoadHandle->WaitUntilComplete();

	UObject* LoadedObject = Cache->GetLoaded(CigaretteId);
	TestNotNull(TEXT("Cache should resolve the loaded Cigarette definition"), LoadedObject);
	TestTrue(TEXT("Cache should report Cigarette as loaded"), Cache->IsLoaded(CigaretteId));

	TArray<FObjectDefinitionCacheEntryDiagnostic> Diagnostics;
	Cache->GetDiagnostics(Diagnostics);

	const FObjectDefinitionCacheEntryDiagnostic* Diagnostic = Diagnostics.FindByPredicate(
		[&CigaretteId](const FObjectDefinitionCacheEntryDiagnostic& Entry)
		{
			return Entry.ObjectId == CigaretteId;
		});

	TestNotNull(TEXT("Diagnostics should include Cigarette"), Diagnostic);
	if (Diagnostic)
	{
		TestTrue(TEXT("Diagnostic state should be Loaded"), Diagnostic->State == EObjectDefinitionLoadState::Loaded);
		TestTrue(TEXT("Diagnostic should record a resolved object"), Diagnostic->bHasResolvedObject);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryCache_LoadStateTransitions,
	"ProjectInventory.Cache.LoadStateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryCache_LoadStateTransitions::RunTest(const FString& Parameters)
{
	UObjectDefinitionCache* Cache = NewObject<UObjectDefinitionCache>(GetTransientPackage());
	TestNotNull(TEXT("ObjectDefinitionCache should be created"), Cache);
	if (!Cache)
	{
		return false;
	}

	const FPrimaryAssetId CigaretteId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Cigarette"));
	TestTrue(TEXT("Cigarette asset id should be valid"), CigaretteId.IsValid());
	if (!CigaretteId.IsValid())
	{
		return false;
	}

	TestTrue(
		TEXT("Initial load state should be Missing or Loaded if already resident"),
		Cache->GetLoadState(CigaretteId) == EObjectDefinitionLoadState::Missing
			|| Cache->GetLoadState(CigaretteId) == EObjectDefinitionLoadState::Loaded);

	bool bCallbackInvoked = false;
	Cache->RequestLoad(
		CigaretteId,
		FOnObjectDefinitionLoaded::CreateLambda([&bCallbackInvoked](UObject* LoadedObject)
		{
			bCallbackInvoked = (LoadedObject != nullptr);
		}));

	const EObjectDefinitionLoadState AfterRequestState = Cache->GetLoadState(CigaretteId);
	TestTrue(
		TEXT("After RequestLoad the state should be Loading or Loaded"),
		AfterRequestState == EObjectDefinitionLoadState::Loading
			|| AfterRequestState == EObjectDefinitionLoadState::Loaded);

	const double DeadlineSeconds = FPlatformTime::Seconds() + 5.0;
	while (Cache->GetLoadState(CigaretteId) != EObjectDefinitionLoadState::Loaded
		&& FPlatformTime::Seconds() < DeadlineSeconds)
	{
		FlushAsyncLoading();
		FPlatformProcess::Sleep(0.01f);
	}

	UObject* LoadedObject = Cache->GetLoaded(CigaretteId);
	TestNotNull(TEXT("Cache should resolve Cigarette after load completion"), LoadedObject);
	TestTrue(TEXT("Cache should report Cigarette as Loaded"), Cache->GetLoadState(CigaretteId) == EObjectDefinitionLoadState::Loaded);
	TestTrue(TEXT("Cache callback should be invoked with the resolved object"), bCallbackInvoked);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInventoryMoveHelper_SelfOverlapDetection,
	"ProjectInventory.MoveHelper.SelfOverlapDetection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectInventoryMoveHelper_SelfOverlapDetection::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Identical source/target rects should overlap"),
		FInventoryMoveHelper::CheckSelfOverlap(FIntPoint(0, 0), FIntPoint(1, 1), FIntPoint(0, 0), FIntPoint(1, 1)));

	TestTrue(
		TEXT("Partially overlapping rects should overlap"),
		FInventoryMoveHelper::CheckSelfOverlap(FIntPoint(0, 0), FIntPoint(2, 1), FIntPoint(1, 0), FIntPoint(2, 1)));

	TestFalse(
		TEXT("Adjacent non-overlapping rects should not overlap"),
		FInventoryMoveHelper::CheckSelfOverlap(FIntPoint(0, 0), FIntPoint(1, 1), FIntPoint(1, 0), FIntPoint(1, 1)));

	return true;
}

// ============================================================================
// InventoryEntry Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryEntry_DefaultConstruction,
	"ProjectInventory.InventoryEntry.DefaultConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInventoryEntry_DefaultConstruction::RunTest(const FString& Parameters)
{
	FInventoryEntry Entry;

	TestEqual(TEXT("InstanceId defaults to 0"), Entry.InstanceId, 0);
	TestFalse(TEXT("ItemId is invalid by default"), Entry.ItemId.IsValid());
	TestEqual(TEXT("Quantity defaults to 1"), Entry.Quantity, 1);
	TestEqual(TEXT("SlotIndex defaults to -1"), Entry.SlotIndex, -1);
	TestFalse(TEXT("ContainerId is invalid by default"), Entry.ContainerId.IsValid());
	TestEqual(TEXT("GridPos defaults to (-1,-1)"), Entry.GridPos, FIntPoint(-1, -1));
	TestFalse(TEXT("bRotated defaults to false"), Entry.bRotated);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryEntry_ParameterizedConstruction,
	"ProjectInventory.InventoryEntry.ParameterizedConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInventoryEntry_ParameterizedConstruction::RunTest(const FString& Parameters)
{
	FPrimaryAssetId TestId(FPrimaryAssetType("ObjectDefinition"), FName("TestItem"));
	FInventoryEntry Entry(42, TestId, 5, FGameplayTag::RequestGameplayTag(FName("Item.Container")), FIntPoint(2, 1), true, 3);

	TestEqual(TEXT("InstanceId set correctly"), Entry.InstanceId, 42);
	TestTrue(TEXT("ItemId set correctly"), Entry.ItemId == TestId);
	TestEqual(TEXT("Quantity set correctly"), Entry.Quantity, 5);
	TestEqual(TEXT("SlotIndex set correctly"), Entry.SlotIndex, 3);
	TestTrue(TEXT("ContainerId set correctly"), Entry.ContainerId == FGameplayTag::RequestGameplayTag(FName("Item.Container")));
	TestEqual(TEXT("GridPos set correctly"), Entry.GridPos, FIntPoint(2, 1));
	TestTrue(TEXT("bRotated set correctly"), Entry.bRotated);

	return true;
}

// ============================================================================
// Large Inventory List Test (basic stress)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryList_LargeAddRemove,
	"ProjectInventory.InventoryList.LargeAddRemove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInventoryList_LargeAddRemove::RunTest(const FString& Parameters)
{
	FInventoryList List;
	const int32 Count = 500;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FPrimaryAssetId ItemId(FPrimaryAssetType("ObjectDefinition"), FName(*FString::Printf(TEXT("TestItem_%d"), Index)));
		List.AddEntry(ItemId, 1, FGameplayTag(), FIntPoint(0, 0), false, Index);
	}

	TestEqual(TEXT("Entries added"), List.Entries.Num(), Count);

	const int32 RemoveTarget = 250;
	const bool bRemoved = List.RemoveEntry(RemoveTarget + 1); // InstanceIds start at 1
	TestTrue(TEXT("Entry removed"), bRemoved);
	TestEqual(TEXT("Entries after remove"), List.Entries.Num(), Count - 1);

	return true;
}

// ============================================================================
// InventoryInstanceData Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryInstanceData_DefaultConstruction,
	"ProjectInventory.InventoryInstanceData.DefaultConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInventoryInstanceData_DefaultConstruction::RunTest(const FString& Parameters)
{
	FInventoryInstanceData Data;

	TestEqual(TEXT("Durability defaults to 1000"), Data.Durability, 1000);
	TestEqual(TEXT("Ammo defaults to 0"), Data.Ammo, 0);
	TestEqual(TEXT("Modifiers is empty"), Data.Modifiers.Num(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
