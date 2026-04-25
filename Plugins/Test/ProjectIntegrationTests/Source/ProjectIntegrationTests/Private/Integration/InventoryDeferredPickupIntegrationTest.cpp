// Copyright ALIS. All Rights Reserved.

/**
 * Integration coverage for the ObjectDefinition load-race fix. Pattern +
 * rationale: docs/agents/canonical.md section 9 "Deferred-pickup +
 * loud-error pattern"; plugin-side pitfall entries in
 * Plugins/Features/ProjectInventory/docs/pitfalls.md (object definition
 * cache).
 *
 * Race reproduction (deterministic)
 * ---------------------------------
 * The tests use UDeterministicObjectDefinitionCache, a UObjectDefinitionCache
 * subclass that captures RequestLoad callbacks instead of hitting
 * AssetManager, and fires them on demand via ResolvePendingLoads. This gives
 * every test exact control over:
 *   - when the cache reports Missing vs. Loading vs. Loaded,
 *   - how many RequestLoad calls the handler actually made,
 *   - what payload the cache delivers (valid object or nullptr),
 *   - whether the pickup source or inventory survive until the callback.
 *
 * Pre-fix failure mode:
 *   Internal_AddItem returned 0 on Missing/Loading.
 *   HandlePickupSource saw Added == 0 and never called Consume.
 *   Intent bookkeeping did not exist, so repeated submissions issued
 *   redundant work; silent pickup failure was the default.
 *
 * Post-fix contract (asserted below):
 *   - Missing/Loading -> FInventoryAddOutcome { bDeferred = true, Fail=None }.
 *   - InvalidRequest / InvalidProvider / InvalidData / NoCapacity / ...
 *     -> Fail != None, BroadcastError fires (Slice 3).
 *   - Handler queues one intent per (source, id); spam merges, callback
 *     drains, Consume fires exactly once on authoritative add.
 *   - nullptr callback payload -> terminal fail, toast, no Consume.
 *   - weak-ptr null on callback (pickup or inventory gone) -> silent drop.
 *
 * Every test below is deterministic: ResolvePendingLoads is called
 * explicitly, not polled via FlushAsyncLoading. Regression of any branch
 * above fails one of these tests with a specific assertion, not a timeout.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DefaultPawn.h"
#include "Misc/ScopeExit.h"

#include "Components/ProjectInventoryComponent.h"
#include "Interaction/InventoryInteractionHandler.h"
#include "Interfaces/IPickupSource.h"
#include "Services/ObjectDefinitionCache.h"
#include "Subsystems/ProjectObjectDefinitionCacheSubsystem.h"

#include "Support/DeferredPickupTestDouble.h"
#include "Support/DeterministicObjectDefinitionCacheFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DeferredPickupTest
{
UWorld* ResolveWorld(FAutomationTestBase* Test)
{
	if (UWorld* W = AutomationCommon::GetAnyGameWorld())
	{
		return W;
	}
	if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
	{
		if (Test)
		{
			Test->AddError(TEXT("Failed to open MainMenu_Persistent for deferred pickup test"));
		}
		return nullptr;
	}
	return AutomationCommon::GetAnyGameWorld();
}

ADeferredPickupTestActor* SpawnPickup(UWorld* World, const FPrimaryAssetId& ObjectId, int32 Quantity)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADeferredPickupTestActor* Actor = World->SpawnActor<ADeferredPickupTestActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Actor)
	{
		Actor->ConfigurePickup(ObjectId, Quantity);
	}
	return Actor;
}

APawn* SpawnPawn(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ADefaultPawn>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

/**
 * Spawn a pawn + attach an inventory component bound to a FRESH
 * deterministic cache (NOT the shared game-instance cache). This isolates
 * the test from any cache residency leftover from earlier tests.
 *
 * Returns the cache so tests can stage objects and resolve loads.
 */
/**
 * Guard that saves the original shared-cache pointer on construction and
 * restores it on destruction. Required because the component's
 * ResolveItemDataView path calls BindObjectDefinitionCache on every query and
 * re-fetches from the subsystem, so the test cache must live on the subsystem
 * for the duration of the test.
 */
struct FScopedCacheOverride
{
	UProjectObjectDefinitionCacheSubsystem* Subsystem = nullptr;
	UObjectDefinitionCache* OriginalCache = nullptr;

	FScopedCacheOverride(UProjectObjectDefinitionCacheSubsystem* InSubsystem, UObjectDefinitionCache* Replacement)
		: Subsystem(InSubsystem)
	{
		if (Subsystem)
		{
			OriginalCache = Subsystem->GetCache();
			Subsystem->OverrideCacheForTests(Replacement);
		}
	}

	~FScopedCacheOverride()
	{
		if (Subsystem)
		{
			Subsystem->OverrideCacheForTests(OriginalCache);
		}
	}
};

UProjectInventoryComponent* SetUpIsolatedInventory(
	UWorld* World, APawn*& OutPawn, UDeterministicObjectDefinitionCache*& OutCache,
	TUniquePtr<FScopedCacheOverride>& OutOverrideGuard)
{
	OutPawn = SpawnPawn(World);
	if (!OutPawn)
	{
		return nullptr;
	}

	// Give the pawn explicit authority so the TryAddItem* server-auth guards pass.
	OutPawn->SetRole(ROLE_Authority);

	UGameInstance* GI = World->GetGameInstance();
	UProjectObjectDefinitionCacheSubsystem* Subsystem = GI ? GI->GetSubsystem<UProjectObjectDefinitionCacheSubsystem>() : nullptr;

	// Construct the test cache with the subsystem as outer so it has the same
	// lifetime semantics as the production cache.
	OutCache = NewObject<UDeterministicObjectDefinitionCache>(Subsystem ? static_cast<UObject*>(Subsystem) : static_cast<UObject*>(OutPawn));

	// Install the test cache on the subsystem BEFORE the component registers,
	// so the initial OnRegister->BindObjectDefinitionCache picks it up and
	// every subsequent ResolveItemDataView rebind stays on the test cache.
	OutOverrideGuard = MakeUnique<FScopedCacheOverride>(Subsystem, OutCache);

	UProjectInventoryComponent* Inventory = NewObject<UProjectInventoryComponent>(OutPawn);
	OutPawn->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	return Inventory;
}

/**
 * Build a minimal-but-complete FItemDataView so Internal_AddItem accepts
 * the staged object and runs a real placement pass. The values mirror a
 * small consumable like Cigarette.
 */
FItemDataView BuildValidItemDataView()
{
	FItemDataView Data;
	Data.bIsValid = true;
	Data.DisplayName = NSLOCTEXT("DeferredPickupTest", "Name", "Test Item");
	Data.Weight = 0.1f;
	Data.Volume = 0.05f;
	Data.GridSize = FIntPoint(1, 1);
	Data.MaxStack = 100;
	return Data;
}

UTestItemDataProvider* StageValidItemData(
	UDeterministicObjectDefinitionCache* Cache,
	const FPrimaryAssetId& Id,
	UObject* Outer)
{
	UTestItemDataProvider* Provider = NewObject<UTestItemDataProvider>(Outer);
	Provider->SetItemData(BuildValidItemDataView());
	Cache->StageObjectAsResident(Id, Provider);
	return Provider;
}
} // namespace DeferredPickupTest

// ----------------------------------------------------------------------------
// Slice 2: Internal_AddItem returns the detailed outcome shape.
// ----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryDeferredPickup_InternalAddItem_ReturnsDeferredOnMissingData,
	"ProjectIntegrationTests.Inventory.DeferredPickup.InternalAddItem.ReturnsDeferredOnMissingData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryDeferredPickup_InternalAddItem_ReturnsDeferredOnMissingData::RunTest(const FString& /*Parameters*/)
{
	using namespace DeferredPickupTest;
	UWorld* World = ResolveWorld(this);
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	APawn* Pawn = nullptr;
	UDeterministicObjectDefinitionCache* Cache = nullptr;
	TUniquePtr<FScopedCacheOverride> Guard;
	UProjectInventoryComponent* Inventory = SetUpIsolatedInventory(World, Pawn, Cache, Guard);
	ON_SCOPE_EXIT { if (Pawn) { Pawn->Destroy(); } };
	if (!TestNotNull(TEXT("inventory"), Inventory))
	{
		return false;
	}

	const FPrimaryAssetId MissingId(FPrimaryAssetType(TEXT("ObjectDefinition")),
		FName(TEXT("NeverResidentTestId")));

	const FInventoryAddOutcome Outcome = Inventory->TryAddItemDetailed(MissingId, 1);

	TestEqual(TEXT("AddedQuantity == 0 on deferred"), Outcome.AddedQuantity, 0);
	TestTrue(TEXT("bDeferred on Missing state"), Outcome.bDeferred);
	TestTrue(TEXT("Fail == None on deferred"),
		Outcome.Fail == EInventoryAddFailReason::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryDeferredPickup_InternalAddItem_InvalidRequestIsTerminal,
	"ProjectIntegrationTests.Inventory.DeferredPickup.InternalAddItem.InvalidRequestIsTerminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryDeferredPickup_InternalAddItem_InvalidRequestIsTerminal::RunTest(const FString& /*Parameters*/)
{
	using namespace DeferredPickupTest;
	UWorld* World = ResolveWorld(this);
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	APawn* Pawn = nullptr;
	UDeterministicObjectDefinitionCache* Cache = nullptr;
	TUniquePtr<FScopedCacheOverride> Guard;
	UProjectInventoryComponent* Inventory = SetUpIsolatedInventory(World, Pawn, Cache, Guard);
	ON_SCOPE_EXIT { if (Pawn) { Pawn->Destroy(); } };
	if (!TestNotNull(TEXT("inventory"), Inventory))
	{
		return false;
	}

	int32 ErrorCount = 0;
	FDelegateHandle ErrorBinding = Inventory->OnInventoryErrorNative().AddLambda(
		[&ErrorCount](const FText&) { ++ErrorCount; });
	ON_SCOPE_EXIT { Inventory->OnInventoryErrorNative().Remove(ErrorBinding); };

	FPrimaryAssetId AnyId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Anything")));
	const FInventoryAddOutcome Outcome = Inventory->TryAddItemDetailed(AnyId, 0);

	TestEqual(TEXT("AddedQuantity == 0"), Outcome.AddedQuantity, 0);
	TestFalse(TEXT("Not deferred on invalid request"), Outcome.bDeferred);
	TestTrue(TEXT("Fail == InvalidRequest"),
		Outcome.Fail == EInventoryAddFailReason::InvalidRequest);
	TestTrue(TEXT("Error broadcast on invalid request"), ErrorCount >= 1);
	return true;
}

// ----------------------------------------------------------------------------
// Slice 2: handler queues exactly one intent per (source, id), drains on
// callback, and Consume fires exactly once.
// ----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryDeferredPickup_Handler_DeferredLoadResolvesAndConsumesOnce,
	"ProjectIntegrationTests.Inventory.DeferredPickup.Handler.DeferredLoadResolvesAndConsumesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryDeferredPickup_Handler_DeferredLoadResolvesAndConsumesOnce::RunTest(const FString& /*Parameters*/)
{
	using namespace DeferredPickupTest;
	UWorld* World = ResolveWorld(this);
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	APawn* Pawn = nullptr;
	UDeterministicObjectDefinitionCache* Cache = nullptr;
	TUniquePtr<FScopedCacheOverride> Guard;
	UProjectInventoryComponent* Inventory = SetUpIsolatedInventory(World, Pawn, Cache, Guard);
	ON_SCOPE_EXIT { if (Pawn) { Pawn->Destroy(); } };
	if (!TestNotNull(TEXT("inventory"), Inventory) || !TestNotNull(TEXT("cache"), Cache))
	{
		return false;
	}

	const FPrimaryAssetId ObjectId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Widget")));

	// Stage the resident payload ahead of the callback firing so the
	// re-entry into TryAddItemDetailed succeeds.
	StageValidItemData(Cache, ObjectId, Pawn);

	ADeferredPickupTestActor* Pickup = SpawnPickup(World, ObjectId, 1);
	ON_SCOPE_EXIT { if (Pickup) { Pickup->Destroy(); } };

	TSharedPtr<FInventoryInteractionHandler> Handler = MakeShared<FInventoryInteractionHandler>();
	Handler->SubmitPickupIntent(Pickup, Pawn);

	// Initial submit must queue exactly one intent and fire exactly one
	// RequestLoad to the cache (anti-spam + dedup invariants).
	TestEqual(TEXT("One pending intent after first submit"),
		Handler->GetPendingPickupIntentCount(), 1);
	TestEqual(TEXT("One distinct RequestLoad dispatched"),
		Cache->GetDistinctRequestCount(), 1);
	TestEqual(TEXT("Pickup not yet consumed during deferred phase"),
		Pickup->GetConsumeCallCount(), 0);

	// Fire the cache callback. Handler re-enters the add path; this time
	// the cache reports Loaded and placement succeeds.
	Cache->ResolvePendingLoads();

	TestEqual(TEXT("Pickup consumed exactly once"), Pickup->GetConsumeCallCount(), 1);
	TestEqual(TEXT("Consumed quantity matches pickup availability"), Pickup->GetConsumedTotal(), 1);
	TestEqual(TEXT("Intent cleared after success"),
		Handler->GetPendingPickupIntentCount(), 0);
	TestEqual(TEXT("Inventory gained one entry"), Inventory->GetItemCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryDeferredPickup_Handler_SpamMergesIntoSingleIntent,
	"ProjectIntegrationTests.Inventory.DeferredPickup.Handler.SpamMergesIntoSingleIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryDeferredPickup_Handler_SpamMergesIntoSingleIntent::RunTest(const FString& /*Parameters*/)
{
	using namespace DeferredPickupTest;
	UWorld* World = ResolveWorld(this);
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	APawn* Pawn = nullptr;
	UDeterministicObjectDefinitionCache* Cache = nullptr;
	TUniquePtr<FScopedCacheOverride> Guard;
	UProjectInventoryComponent* Inventory = SetUpIsolatedInventory(World, Pawn, Cache, Guard);
	ON_SCOPE_EXIT { if (Pawn) { Pawn->Destroy(); } };

	const FPrimaryAssetId ObjectId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Widget")));
	StageValidItemData(Cache, ObjectId, Pawn);
	ADeferredPickupTestActor* Pickup = SpawnPickup(World, ObjectId, 1);
	ON_SCOPE_EXIT { if (Pickup) { Pickup->Destroy(); } };

	TSharedPtr<FInventoryInteractionHandler> Handler = MakeShared<FInventoryInteractionHandler>();

	// 10 back-to-back submits against the same (source, id).
	const int32 SpamCount = 10;
	for (int32 I = 0; I < SpamCount; ++I)
	{
		Handler->SubmitPickupIntent(Pickup, Pawn);
	}

	TestEqual(TEXT("Exactly one pending intent regardless of spam"),
		Handler->GetPendingPickupIntentCount(), 1);
	TestEqual(TEXT("Exactly one RequestLoad dispatched to cache"),
		Cache->GetDistinctRequestCount(), 1);
	TestEqual(TEXT("Exactly one callback captured for the id"),
		Cache->GetCapturedCallbackCount(ObjectId), 1);

	Cache->ResolvePendingLoads();

	TestEqual(TEXT("Consume fires exactly once after dedup"),
		Pickup->GetConsumeCallCount(), 1);
	return true;
}

// ----------------------------------------------------------------------------
// Slice 2: weak-ptr-null on callback is a silent drop.
// ----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryDeferredPickup_PickupDestroyedPreCallback_IsSilentDrop,
	"ProjectIntegrationTests.Inventory.DeferredPickup.PickupDestroyedPreCallback.IsSilentDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryDeferredPickup_PickupDestroyedPreCallback_IsSilentDrop::RunTest(const FString& /*Parameters*/)
{
	using namespace DeferredPickupTest;
	UWorld* World = ResolveWorld(this);
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	APawn* Pawn = nullptr;
	UDeterministicObjectDefinitionCache* Cache = nullptr;
	TUniquePtr<FScopedCacheOverride> Guard;
	UProjectInventoryComponent* Inventory = SetUpIsolatedInventory(World, Pawn, Cache, Guard);
	ON_SCOPE_EXIT { if (Pawn) { Pawn->Destroy(); } };

	const FPrimaryAssetId ObjectId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Widget")));
	StageValidItemData(Cache, ObjectId, Pawn);
	ADeferredPickupTestActor* Pickup = SpawnPickup(World, ObjectId, 1);

	int32 ErrorCount = 0;
	FDelegateHandle ErrorBinding = Inventory->OnInventoryErrorNative().AddLambda(
		[&ErrorCount](const FText&) { ++ErrorCount; });
	ON_SCOPE_EXIT { Inventory->OnInventoryErrorNative().Remove(ErrorBinding); };

	TSharedPtr<FInventoryInteractionHandler> Handler = MakeShared<FInventoryInteractionHandler>();
	Handler->SubmitPickupIntent(Pickup, Pawn);
	TestEqual(TEXT("Intent queued"), Handler->GetPendingPickupIntentCount(), 1);

	// Destroy the pickup actor BEFORE firing the callback. The weak ptr
	// captured by the intent must now be invalid on callback entry.
	Pickup->Destroy();
	Pickup = nullptr;

	Cache->ResolvePendingLoads();

	TestEqual(TEXT("Intent cleared silently after weak-ptr null"),
		Handler->GetPendingPickupIntentCount(), 0);
	TestEqual(TEXT("No error toast on silent drop"), ErrorCount, 0);
	return true;
}

// ----------------------------------------------------------------------------
// Slice 3: terminal nullptr callback broadcasts and does NOT consume.
// ----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryDeferredPickup_TerminalLoadFailure_BroadcastsAndDoesNotConsume,
	"ProjectIntegrationTests.Inventory.DeferredPickup.TerminalLoadFailure.BroadcastsAndDoesNotConsume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryDeferredPickup_TerminalLoadFailure_BroadcastsAndDoesNotConsume::RunTest(const FString& /*Parameters*/)
{
	using namespace DeferredPickupTest;
	UWorld* World = ResolveWorld(this);
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	APawn* Pawn = nullptr;
	UDeterministicObjectDefinitionCache* Cache = nullptr;
	TUniquePtr<FScopedCacheOverride> Guard;
	UProjectInventoryComponent* Inventory = SetUpIsolatedInventory(World, Pawn, Cache, Guard);
	ON_SCOPE_EXIT { if (Pawn) { Pawn->Destroy(); } };

	const FPrimaryAssetId GhostId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Ghost")));

	// Stage as "unloadable" — callback will fire with nullptr.
	Cache->StageObjectAsUnloadable(GhostId);

	ADeferredPickupTestActor* Pickup = SpawnPickup(World, GhostId, 1);
	ON_SCOPE_EXIT { if (Pickup) { Pickup->Destroy(); } };

	int32 ErrorCount = 0;
	FDelegateHandle ErrorBinding = Inventory->OnInventoryErrorNative().AddLambda(
		[&ErrorCount](const FText&) { ++ErrorCount; });
	ON_SCOPE_EXIT { Inventory->OnInventoryErrorNative().Remove(ErrorBinding); };

	TSharedPtr<FInventoryInteractionHandler> Handler = MakeShared<FInventoryInteractionHandler>();
	Handler->SubmitPickupIntent(Pickup, Pawn);

	TestEqual(TEXT("Intent queued"), Handler->GetPendingPickupIntentCount(), 1);

	Cache->ResolvePendingLoads();

	TestEqual(TEXT("Intent cleared after terminal fail"),
		Handler->GetPendingPickupIntentCount(), 0);
	TestEqual(TEXT("Pickup never consumed on terminal fail"),
		Pickup->GetConsumeCallCount(), 0);
	TestTrue(TEXT("At least one error toast broadcast"), ErrorCount > 0);
	return true;
}

// ----------------------------------------------------------------------------
// Slice 3: hard fail at add-path (invalid provider) broadcasts a toast.
// ----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryDeferredPickup_InvalidProvider_BroadcastsErrorAndDoesNotConsume,
	"ProjectIntegrationTests.Inventory.DeferredPickup.InvalidProvider.BroadcastsErrorAndDoesNotConsume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryDeferredPickup_InvalidProvider_BroadcastsErrorAndDoesNotConsume::RunTest(const FString& /*Parameters*/)
{
	using namespace DeferredPickupTest;
	UWorld* World = ResolveWorld(this);
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	APawn* Pawn = nullptr;
	UDeterministicObjectDefinitionCache* Cache = nullptr;
	TUniquePtr<FScopedCacheOverride> Guard;
	UProjectInventoryComponent* Inventory = SetUpIsolatedInventory(World, Pawn, Cache, Guard);
	ON_SCOPE_EXIT { if (Pawn) { Pawn->Destroy(); } };

	const FPrimaryAssetId BrokenId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(TEXT("Broken")));

	// Stage an object that does NOT implement IItemDataProvider.
	UTestPlainObject* NonProvider = NewObject<UTestPlainObject>(Pawn);
	Cache->StageObjectAsResident(BrokenId, NonProvider);

	ADeferredPickupTestActor* Pickup = SpawnPickup(World, BrokenId, 1);
	ON_SCOPE_EXIT { if (Pickup) { Pickup->Destroy(); } };

	int32 ErrorCount = 0;
	FDelegateHandle ErrorBinding = Inventory->OnInventoryErrorNative().AddLambda(
		[&ErrorCount](const FText&) { ++ErrorCount; });
	ON_SCOPE_EXIT { Inventory->OnInventoryErrorNative().Remove(ErrorBinding); };

	TSharedPtr<FInventoryInteractionHandler> Handler = MakeShared<FInventoryInteractionHandler>();
	Handler->SubmitPickupIntent(Pickup, Pawn);
	Cache->ResolvePendingLoads();

	TestEqual(TEXT("Pickup never consumed (invalid provider)"),
		Pickup->GetConsumeCallCount(), 0);
	TestTrue(TEXT("Error toast broadcast for invalid provider"), ErrorCount > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
