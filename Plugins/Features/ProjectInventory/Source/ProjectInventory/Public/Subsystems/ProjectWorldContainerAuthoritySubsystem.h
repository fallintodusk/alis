// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/ContainerSessionTypes.h"
#include "ProjectWorldContainerAuthoritySubsystem.generated.h"

class UProjectInventoryComponent;

/**
 * Server-authority owner of world-container session state.
 *
 * Callspace contract:
 *   Client intent (UI / ViewModel / drag)
 *   -> owner-bound RPC edge (Server_* on UProjectInventoryComponent for now)
 *   -> authoritative session validation + dispatch  << THIS SUBSYSTEM >>
 *   -> pure-logic domain operation (FWorldContainerMoveOp and friends)
 *   -> storage authority (IWorldContainerSessionSource impl, e.g. loot container)
 *
 * Why a UWorldSubsystem and not a ULocalPlayerSubsystem:
 *  - session state is authoritative, its lifetime follows the world, not a
 *    local player; on a dedicated server there is no local player, so a
 *    ULocalPlayerSubsystem is simply the wrong home.
 *  - ShouldCreateSubsystem gates creation to server-authority worlds (listen
 *    server, dedicated server, standalone), per Epic's documented pattern
 *    for world-scoped server-only subsystems (UWorldSubsystem).
 *
 * This subsystem is the authoritative session registry. The sibling
 * UProjectContainerSessionSubsystem (ULocalPlayerSubsystem) continues to
 * own the client-side view cache for UI (nearby-container panel, session-
 * aware hints). The two never overlap ownership: authority writes here,
 * client reads its local mirror populated by Client RPCs.
 */
UCLASS()
class PROJECTINVENTORY_API UProjectWorldContainerAuthoritySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Server-only. Skip on pure clients so no authority state exists there. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/**
	 * Open a world-container session authoritatively.
	 *
	 * Performs the TryBeginContainerSession handshake with the target's
	 * IWorldContainerSessionSource impl, allocates a handle, and registers
	 * the session in this subsystem. Server-side only -- callers on
	 * non-authority worlds should route a Server RPC to reach this method.
	 *
	 * Returns true on success with OutHandle filled; false with OutError
	 * set on handshake rejection, invalid target, or mode mismatch.
	 */
	bool OpenSession(
		AActor* TargetActor,
		EContainerSessionMode Mode,
		AActor* Instigator,
		FContainerSessionHandle& OutHandle,
		FText& OutError);

	/**
	 * Close a world-container session authoritatively.
	 *
	 * Runs EndContainerSession on the target (for FullOpen mode) and
	 * removes the session from this subsystem's registry. Returns true iff
	 * the handle was known and the handshake succeeded.
	 */
	bool CloseSession(const FContainerSessionHandle& Handle, FText& OutError);

	/**
	 * Close every registered session. Used by test teardown to reset world
	 * state between runs. Each session gets the same EndContainerSession
	 * handshake as a normal Close. Errors are logged and skipped.
	 */
	void CloseAllSessions();

	/** Authoritative check that a session handle is known to this subsystem. */
	UFUNCTION(BlueprintPure, Category = "Inventory|WorldContainerAuthority")
	bool IsSessionActive(const FContainerSessionHandle& Handle) const;

	/**
	 * Find the session currently opened by the given instigator (pawn or
	 * controller). Returns false if the instigator has no open session.
	 * Used by bridge Implementations to answer GetActiveWorldContainerSession
	 * without keeping member state on the inventory component.
	 */
	bool FindSessionByInstigator(
		const AActor* Instigator,
		AActor*& OutTargetActor,
		FContainerSessionHandle& OutHandle) const;

	/** True iff the instigator has any open session of any mode. */
	UFUNCTION(BlueprintPure, Category = "Inventory|WorldContainerAuthority")
	bool HasAnyActiveSessionForInstigator(const AActor* Instigator) const;

	/** True iff the instigator has an open FullOpen session. */
	UFUNCTION(BlueprintPure, Category = "Inventory|WorldContainerAuthority")
	bool HasActiveFullOpenSessionForInstigator(const AActor* Instigator) const;

	/** True iff any session (any instigator, any mode) is active in this world. */
	UFUNCTION(BlueprintPure, Category = "Inventory|WorldContainerAuthority")
	bool HasAnyActiveSession() const;

	/** True iff any FullOpen session is active in this world. */
	UFUNCTION(BlueprintPure, Category = "Inventory|WorldContainerAuthority")
	bool HasActiveFullOpenSession() const;

	/**
	 * Read the container view for a session handle. For UI consumers that
	 * need label + grid + entries in one call. Returns false if the handle
	 * is unknown.
	 */
	bool GetSessionContainerView(
		const FContainerSessionHandle& Handle,
		FText& OutLabel,
		FInventoryContainerView& OutContainerView,
		TArray<FInventoryEntryView>& OutEntries) const;

	/**
	 * Authorization predicate: is this session one the given instigator
	 * opened? Called by Take/Store/Move/TakeAll dispatch to prevent a
	 * client from acting on someone else's session.
	 */
	bool IsSessionInstigatedBy(const FContainerSessionHandle& Handle, const AActor* Instigator) const;

	/**
	 * Take one entry from the world container into the player inventory.
	 * Authoritative: validates the session, resolves the source, and
	 * delegates the inventory-side snapshot+consume+rollback dance to the
	 * inventory component's Resolved helper.
	 */
	bool TakeEntryFromWorldContainerSession(
		const FContainerSessionHandle& Handle,
		UProjectInventoryComponent* Inventory,
		int32 EntryInstanceId,
		int32 Quantity,
		FGameplayTag TargetContainerId,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);

	/**
	 * Store one inventory entry into the world container.
	 */
	bool StoreInventoryEntryInWorldContainerSession(
		const FContainerSessionHandle& Handle,
		UProjectInventoryComponent* Inventory,
		int32 InventoryInstanceId,
		int32 Quantity,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);

	/**
	 * Take every entry from the world container into the player inventory.
	 */
	bool TakeAllFromWorldContainerSession(
		const FContainerSessionHandle& Handle,
		UProjectInventoryComponent* Inventory,
		FText& OutError);

	/**
	 * Rearrange an entry within the open world container. Dispatches to
	 * FWorldContainerMoveOp (pure, stateless snapshot+consume+store+rollback).
	 */
	bool MoveWithinWorldContainerSession(
		const FContainerSessionHandle& Handle,
		int32 EntryInstanceId,
		int32 Quantity,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);

private:
	struct FActiveContainerSession
	{
		FContainerSessionHandle Handle;
		TWeakObjectPtr<AActor> TargetActor;
		TWeakObjectPtr<UObject> SourceObject;
		// Instigator is stored as an FObjectKey (stable identity pair:
		// ObjectIndex + SerialNumber) rather than TWeakObjectPtr so that
		// identity comparison works even after the instigator is marked
		// as garbage (tests destroy the controlling PC between runs, which
		// is a valid state that should not invalidate an active session's
		// identity match).
		FObjectKey InstigatorKey;
		TWeakObjectPtr<AActor> Instigator;
	};

	/** Authoritative session registry. */
	TMap<FGuid, FActiveContainerSession> ActiveSessions;
};
