// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
//
// ProjectInventoryComponent world-container TRANSFER glue.
//
// Authority dispatch lives in UProjectWorldContainerAuthoritySubsystem
// (server-only UWorldSubsystem). The component provides:
//   - inventory-side snapshot/restore used by Resolved helpers for rollback
//   - Resolved helpers (inventory-mutation primitives called by the authority
//     subsystem with the container source already resolved)
//   - bridge interface implementations that route to the authority subsystem
//     on authority, or dispatch a Server RPC on the client
//   - Server RPC implementations that re-enter the authority path
//
// No authority state or authority handshake work happens in this TU. The
// component holds no active-session member. Take/Store/Move/TakeAll all
// delegate to UProjectWorldContainerAuthoritySubsystem, which applies
// authorization (IsSessionInstigatedBy) before mutating.

#include "Components/ProjectInventoryComponent.h"
#include "Components/ProjectInventoryComponentInternals.h"

#include "Helpers/InventoryWorldContainerTransferHelper.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "Loot/LootTypes.h"
#include "ProjectInventory.h"
#include "Subsystems/ProjectWorldContainerAuthoritySubsystem.h"

#include "Engine/World.h"

using namespace ProjectInventoryInternal;

namespace
{
	/** Resolve the server-gated authority subsystem from the component's world. */
	UProjectWorldContainerAuthoritySubsystem* ResolveAuthoritySubsystem(const UProjectInventoryComponent* Inventory)
	{
		if (!Inventory)
		{
			return nullptr;
		}
		const UWorld* World = Inventory->GetWorld();
		return World ? World->GetSubsystem<UProjectWorldContainerAuthoritySubsystem>() : nullptr;
	}
}

// -------------------------------------------------------------------------
// Inventory state snapshot/restore (rollback support for Resolved helpers)
// -------------------------------------------------------------------------

void UProjectInventoryComponent::CaptureInventoryStateSnapshot(FInventoryStateSnapshot& OutSnapshot) const
{
	OutSnapshot.Entries = Inventory.Entries;
	OutSnapshot.NextInstanceId = Inventory.NextInstanceId;
}

void UProjectInventoryComponent::RestoreInventoryStateSnapshot(const FInventoryStateSnapshot& Snapshot)
{
	Inventory.Entries = Snapshot.Entries;
	Inventory.NextInstanceId = Snapshot.NextInstanceId;
	Inventory.MarkArrayDirty();
	for (FInventoryEntry& Entry : Inventory.Entries)
	{
		Inventory.MarkEntryDirty(Entry);
	}
}

// -------------------------------------------------------------------------
// Session-aware resolved helpers (inventory-side atomic ops with rollback).
// These are the authority-side mutation primitives; the authority subsystem
// calls them with the container SourceObject already resolved.
// -------------------------------------------------------------------------

bool UProjectInventoryComponent::TakeEntryFromWorldContainerResolved(
	UObject* SourceObject,
	const FContainerSessionHandle& SessionHandle,
	int32 EntryInstanceId,
	int32 Quantity,
	FGameplayTag TargetContainerId,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryResolvedAuthorityRequired",
			"World-container take requires authority.");
		return false;
	}

	if (!SourceObject || !SourceObject->Implements<UWorldContainerSessionSource>())
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryResolvedMissingSource",
			"World-container source is no longer valid.");
		return false;
	}

	const TArray<FInventoryEntryView> EntryViews =
		IWorldContainerSessionSource::Execute_GetContainerEntryViews(SourceObject);
	const FInventoryEntryView* EntryView = EntryViews.FindByPredicate(
		[EntryInstanceId](const FInventoryEntryView& Entry)
		{
			return Entry.InstanceId == EntryInstanceId;
		});
	if (!EntryView)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryResolvedMissingEntry",
			"World-container entry is no longer available.");
		return false;
	}

	const int32 TakeQuantity = FMath::Clamp(Quantity, 1, EntryView->Quantity);
	const bool bHasExplicitPlacement =
		TargetContainerId.IsValid() && TargetGridPos.X >= 0 && TargetGridPos.Y >= 0;

	TArray<FLootEntry> LootItems;
	if (!bHasExplicitPlacement)
	{
		FLootEntry LootItem;
		LootItem.ObjectId = EntryView->ItemId;
		LootItem.Quantity = TakeQuantity;
		LootItems.Add(LootItem);
		if (!CanFitItems(LootItems))
		{
			OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryNoSpace",
				"Inventory does not have enough free space.");
			return false;
		}
	}

	FInventoryStateSnapshot Snapshot;
	CaptureInventoryStateSnapshot(Snapshot);
	const int32 QuantityBefore = GetTotalItemQuantity(Snapshot.Entries, EntryView->ItemId);

	if (bHasExplicitPlacement)
	{
		if (!TryAddItemAtPosition(
				EntryView->ItemId,
				TakeQuantity,
				TargetContainerId,
				TargetGridPos,
				bTargetRotated,
				OutError))
		{
			return false;
		}
	}
	else
	{
		AddItemsBatch(LootItems);
	}

	const int32 QuantityAfter = GetTotalItemQuantity(Inventory.Entries, EntryView->ItemId);
	if (QuantityAfter - QuantityBefore != TakeQuantity)
	{
		RestoreInventoryStateSnapshot(Snapshot);
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryInventoryMutationMismatch",
			"Inventory add did not commit the expected quantity.");
		return false;
	}

	FContainerEntryTransfer ConsumeEntry;
	ConsumeEntry.EntryInstanceId = EntryInstanceId;
	ConsumeEntry.ObjectId = EntryView->ItemId;
	ConsumeEntry.Quantity = TakeQuantity;

	TArray<FContainerEntryTransfer> ConsumeEntries;
	ConsumeEntries.Add(ConsumeEntry);
	if (!IWorldContainerSessionSource::Execute_ConsumeContainerEntries(
			SourceObject, SessionHandle.SessionId, ConsumeEntries, OutError))
	{
		RestoreInventoryStateSnapshot(Snapshot);
		if (OutError.IsEmpty())
		{
			OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryConsumeRejected",
				"World-container failed to consume the transferred entry.");
		}
		return false;
	}

	return true;
}

bool UProjectInventoryComponent::StoreInventoryEntryInWorldContainerResolved(
	UObject* SourceObject,
	const FContainerSessionHandle& SessionHandle,
	int32 InventoryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryResolvedAuthorityRequired",
			"World-container store requires authority.");
		return false;
	}

	if (!SourceObject || !SourceObject->Implements<UWorldContainerSessionSource>())
	{
		OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryResolvedMissingSource",
			"World-container source is no longer valid.");
		return false;
	}

	FInventoryEntry InventoryEntry;
	if (!FindEntry(InventoryInstanceId, InventoryEntry))
	{
		OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryMissingInventoryEntry",
			"Inventory entry is no longer available.");
		return false;
	}

	FContainerEntryTransfer CandidateEntry;
	CandidateEntry.ObjectId = InventoryEntry.ItemId;
	CandidateEntry.Quantity = FMath::Clamp(Quantity, 1, InventoryEntry.Quantity);
	if (TargetGridPos.X >= 0 && TargetGridPos.Y >= 0)
	{
		CandidateEntry.GridPos = TargetGridPos;
		CandidateEntry.bRotated = bTargetRotated;
	}

	TArray<FContainerEntryTransfer> CandidateEntries;
	CandidateEntries.Add(CandidateEntry);
	if (!IWorldContainerSessionSource::Execute_CanStoreContainerEntries(
			SourceObject, SessionHandle.SessionId, CandidateEntries, OutError))
	{
		UE_LOG(LogProjectInventory, Log,
			TEXT("StoreInventoryEntryInWorldContainerResolved: CanStore rejected %s x%d for %s at (%d,%d) rot:%d - %s"),
			*CandidateEntry.ObjectId.ToString(),
			CandidateEntry.Quantity,
			*GetNameSafe(SourceObject),
			CandidateEntry.GridPos.X,
			CandidateEntry.GridPos.Y,
			CandidateEntry.bRotated ? 1 : 0,
			*OutError.ToString());
		return false;
	}

	FInventoryStateSnapshot Snapshot;
	CaptureInventoryStateSnapshot(Snapshot);

	FContainerEntryTransfer ExtractedEntry;
	if (!TryExtractContainerTransferEntry(InventoryInstanceId, CandidateEntry.Quantity, ExtractedEntry, OutError))
	{
		return false;
	}
	ExtractedEntry.GridPos = CandidateEntry.GridPos;
	ExtractedEntry.bRotated = CandidateEntry.bRotated;

	TArray<FContainerEntryTransfer> StoreEntries;
	StoreEntries.Add(ExtractedEntry);
	if (!IWorldContainerSessionSource::Execute_StoreContainerEntries(
			SourceObject, SessionHandle.SessionId, StoreEntries, OutError))
	{
		RestoreInventoryStateSnapshot(Snapshot);
		UE_LOG(LogProjectInventory, Log,
			TEXT("StoreInventoryEntryInWorldContainerResolved: Store rejected %s x%d for %s at (%d,%d) rot:%d - %s"),
			*ExtractedEntry.ObjectId.ToString(),
			ExtractedEntry.Quantity,
			*GetNameSafe(SourceObject),
			ExtractedEntry.GridPos.X,
			ExtractedEntry.GridPos.Y,
			ExtractedEntry.bRotated ? 1 : 0,
			*OutError.ToString());
		if (OutError.IsEmpty())
		{
			OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryRejected",
				"World-container failed to store the extracted inventory entry.");
		}
		return false;
	}

	UE_LOG(LogProjectInventory, Log,
		TEXT("StoreInventoryEntryInWorldContainerResolved: Stored %s x%d into %s at (%d,%d) rot:%d"),
		*ExtractedEntry.ObjectId.ToString(),
		ExtractedEntry.Quantity,
		*GetNameSafe(SourceObject),
		ExtractedEntry.GridPos.X,
		ExtractedEntry.GridPos.Y,
		ExtractedEntry.bRotated ? 1 : 0);

	return true;
}

bool UProjectInventoryComponent::TakeAllFromWorldContainerResolved(
	UObject* SourceObject,
	const FContainerSessionHandle& SessionHandle,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeAllWorldResolvedAuthorityRequired",
			"World-container take-all requires authority.");
		return false;
	}

	TArray<FContainerEntryTransfer> ConsumeEntries;
	TArray<FLootEntry> LootItems;
	if (!FInventoryWorldContainerTransferHelper::BuildLootEntries(SourceObject, ConsumeEntries, LootItems, OutError))
	{
		return false;
	}

	if (!CanFitItems(LootItems))
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeAllWorldNoSpace",
			"Inventory does not have enough free space.");
		return false;
	}

	FInventoryStateSnapshot Snapshot;
	CaptureInventoryStateSnapshot(Snapshot);

	TMap<FPrimaryAssetId, int32> ExpectedQuantities;
	BuildExpectedLootQuantities(LootItems, ExpectedQuantities);

	AddItemsBatch(LootItems);

	for (const TPair<FPrimaryAssetId, int32>& Pair : ExpectedQuantities)
	{
		const int32 QuantityBefore = GetTotalItemQuantity(Snapshot.Entries, Pair.Key);
		const int32 QuantityAfter = GetTotalItemQuantity(Inventory.Entries, Pair.Key);
		if (QuantityAfter - QuantityBefore != Pair.Value)
		{
			RestoreInventoryStateSnapshot(Snapshot);
			OutError = NSLOCTEXT("ProjectInventory", "TakeAllInventoryMutationMismatch",
				"Inventory add did not commit the expected take-all quantity.");
			return false;
		}
	}

	if (!IWorldContainerSessionSource::Execute_ConsumeContainerEntries(
			SourceObject, SessionHandle.SessionId, ConsumeEntries, OutError))
	{
		RestoreInventoryStateSnapshot(Snapshot);
		if (OutError.IsEmpty())
		{
			OutError = NSLOCTEXT("ProjectInventory", "TakeAllConsumeRejected",
				"World-container failed to consume transferred entries.");
		}
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------
// IInventoryWorldContainerTransferBridge impls (transfer ops). On
// authority, dispatch through UProjectWorldContainerAuthoritySubsystem.
// On client, forward via Server RPC.
// -------------------------------------------------------------------------

bool UProjectInventoryComponent::TransferWorldContainerEntryToInventory_Implementation(
	UObject* WorldContainerSource,
	const FContainerSessionHandle& SessionHandle,
	int32 EntryInstanceId,
	int32 Quantity,
	FGameplayTag TargetContainerId,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	AActor* TargetActor = ResolveWorldContainerActor(WorldContainerSource);
	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectInventory", "MissingWorldContainerActorForTake",
			"World-container actor is unavailable.");
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		UProjectWorldContainerAuthoritySubsystem* Auth = ResolveAuthoritySubsystem(this);
		if (!Auth)
		{
			OutError = NSLOCTEXT("ProjectInventory", "TransferMissingAuthority",
				"World-container authority subsystem is unavailable.");
			return false;
		}
		return Auth->TakeEntryFromWorldContainerSession(
			SessionHandle,
			this,
			EntryInstanceId,
			Quantity,
			TargetContainerId,
			TargetGridPos,
			bTargetRotated,
			OutError);
	}

	Server_RequestTakeEntryFromWorldContainer(
		TargetActor,
		SessionHandle,
		EntryInstanceId,
		Quantity,
		TargetContainerId,
		TargetGridPos,
		bTargetRotated);
	OutError = FText::GetEmpty();
	return true;
}

bool UProjectInventoryComponent::StoreInventoryEntryInWorldContainer_Implementation(
	UObject* WorldContainerSource,
	const FContainerSessionHandle& SessionHandle,
	int32 InventoryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	AActor* TargetActor = ResolveWorldContainerActor(WorldContainerSource);
	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectInventory", "MissingWorldContainerActorForStore",
			"World-container actor is unavailable.");
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		UProjectWorldContainerAuthoritySubsystem* Auth = ResolveAuthoritySubsystem(this);
		if (!Auth)
		{
			OutError = NSLOCTEXT("ProjectInventory", "StoreMissingAuthority",
				"World-container authority subsystem is unavailable.");
			return false;
		}
		return Auth->StoreInventoryEntryInWorldContainerSession(
			SessionHandle,
			this,
			InventoryInstanceId,
			Quantity,
			TargetGridPos,
			bTargetRotated,
			OutError);
	}

	Server_RequestStoreInventoryEntryInWorldContainer(
		TargetActor,
		SessionHandle,
		InventoryInstanceId,
		Quantity,
		TargetGridPos,
		bTargetRotated);
	OutError = FText::GetEmpty();
	return true;
}

bool UProjectInventoryComponent::MoveWithinWorldContainer_Implementation(
	UObject* WorldContainerSource,
	const FContainerSessionHandle& SessionHandle,
	int32 EntryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!WorldContainerSource || !WorldContainerSource->Implements<UWorldContainerSessionSource>())
	{
		OutError = NSLOCTEXT("ProjectInventory", "MoveWithinWorldContainerMissingSource",
			"World-container source is no longer valid.");
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		AActor* TargetActor = Cast<AActor>(WorldContainerSource);
		if (!TargetActor)
		{
			OutError = NSLOCTEXT("ProjectInventory", "MoveWithinWorldContainerNonActorClient",
				"Client-side move requires an actor-backed world container.");
			return false;
		}
		Server_RequestMoveWithinWorldContainer(
			TargetActor, SessionHandle, EntryInstanceId, Quantity, TargetGridPos, bTargetRotated);
		return true;
	}

	UProjectWorldContainerAuthoritySubsystem* Auth = ResolveAuthoritySubsystem(this);
	if (!Auth)
	{
		OutError = NSLOCTEXT("ProjectInventory", "MoveWithinMissingAuthority",
			"World-container authority subsystem is unavailable.");
		return false;
	}

	return Auth->MoveWithinWorldContainerSession(
		SessionHandle, EntryInstanceId, Quantity, TargetGridPos, bTargetRotated, OutError);
}

bool UProjectInventoryComponent::TakeAllFromWorldContainer_Implementation(
	UObject* WorldContainerSource,
	const FContainerSessionHandle& SessionHandle,
	FText& OutError)
{
	AActor* TargetActor = ResolveWorldContainerActor(WorldContainerSource);
	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectInventory", "MissingWorldContainerActorForTakeAll",
			"World-container actor is unavailable.");
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		UProjectWorldContainerAuthoritySubsystem* Auth = ResolveAuthoritySubsystem(this);
		if (!Auth)
		{
			OutError = NSLOCTEXT("ProjectInventory", "TakeAllMissingAuthority",
				"World-container authority subsystem is unavailable.");
			return false;
		}
		return Auth->TakeAllFromWorldContainerSession(SessionHandle, this, OutError);
	}

	Server_RequestTakeAllFromWorldContainer(TargetActor, SessionHandle);
	OutError = FText::GetEmpty();
	return true;
}

// -------------------------------------------------------------------------
// Server RPC impls (transfer ops dispatched from the client). All delegate
// back into the bridge Implementation so the "authority branch" single
// source of truth drives the actual mutation.
// -------------------------------------------------------------------------

void UProjectInventoryComponent::Server_RequestTakeEntryFromWorldContainer_Implementation(
	AActor* TargetActor,
	FContainerSessionHandle SessionHandle,
	int32 EntryInstanceId,
	int32 Quantity,
	FGameplayTag TargetContainerId,
	FIntPoint TargetGridPos,
	bool bTargetRotated)
{
	FText TakeError;
	if (!TransferWorldContainerEntryToInventory_Implementation(
			TargetActor,
			SessionHandle,
			EntryInstanceId,
			Quantity,
			TargetContainerId,
			TargetGridPos,
			bTargetRotated,
			TakeError))
	{
		if (!TakeError.IsEmpty())
		{
			BroadcastError(TakeError);
		}
		return;
	}
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_RequestStoreInventoryEntryInWorldContainer_Implementation(
	AActor* TargetActor,
	FContainerSessionHandle SessionHandle,
	int32 InventoryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated)
{
	FText StoreError;
	if (!StoreInventoryEntryInWorldContainer_Implementation(
			TargetActor,
			SessionHandle,
			InventoryInstanceId,
			Quantity,
			TargetGridPos,
			bTargetRotated,
			StoreError))
	{
		if (!StoreError.IsEmpty())
		{
			BroadcastError(StoreError);
		}
		return;
	}
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_RequestMoveWithinWorldContainer_Implementation(
	AActor* TargetActor,
	FContainerSessionHandle SessionHandle,
	int32 EntryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated)
{
	FText MoveError;
	if (!MoveWithinWorldContainer_Implementation(
			TargetActor,
			SessionHandle,
			EntryInstanceId,
			Quantity,
			TargetGridPos,
			bTargetRotated,
			MoveError))
	{
		if (!MoveError.IsEmpty())
		{
			BroadcastError(MoveError);
		}
		return;
	}
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_RequestTakeAllFromWorldContainer_Implementation(
	AActor* TargetActor,
	FContainerSessionHandle SessionHandle)
{
	FText TakeAllError;
	if (!TakeAllFromWorldContainer_Implementation(TargetActor, SessionHandle, TakeAllError))
	{
		if (!TakeAllError.IsEmpty())
		{
			BroadcastError(TakeAllError);
		}
		return;
	}
	InventoryViewChanged.Broadcast();
}
