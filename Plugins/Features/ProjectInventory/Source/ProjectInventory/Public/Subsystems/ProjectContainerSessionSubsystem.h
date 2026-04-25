// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Types/ContainerSessionTypes.h"
#include "ProjectContainerSessionSubsystem.generated.h"

/**
 * Local-player-owned CLIENT-SIDE UI cache for world-container sessions.
 *
 * Pure view cache. Populated by Client RPC delivery (the authority side
 * runs in UProjectWorldContainerAuthoritySubsystem). Consumed by inventory
 * UI widgets (nearby-container panel, session-aware hints).
 *
 * This class has no authoritative behavior: no TryBegin/End handshake,
 * no take/store/move/take-all dispatch. Use
 * UProjectWorldContainerAuthoritySubsystem (server-only UWorldSubsystem)
 * for those. See Epic docs on UWorldSubsystem::ShouldCreateSubsystem for
 * the callspace split rationale.
 *
 * Creation is client-scoped: ShouldCreateSubsystem returns false on a
 * pure dedicated server. On listen server the local player gets this
 * subsystem too, and the authority layer populates the cache via the
 * usual Client_* RPC path.
 */
UCLASS()
class PROJECTINVENTORY_API UProjectContainerSessionSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/**
	 * Populate the local cache from a Client RPC delivery. Called after
	 * the server's Client_WorldContainerSessionOpened RPC fires on the
	 * owning client (or the listen-server's local player).
	 */
	bool RegisterOpenedSession(
		AActor* TargetActor,
		UObject* SourceObject,
		AActor* Instigator,
		const FContainerSessionHandle& Handle,
		FText& OutError);

	/**
	 * Remove the handle from the local cache. Called after the server's
	 * Client_WorldContainerSessionClosed RPC fires. Does NOT run the
	 * authoritative End handshake (that lives on the authority subsystem).
	 */
	bool CloseSessionLocal(const FContainerSessionHandle& Handle);

	/** UI helper: read the active container view for this cached session. */
	bool GetSessionContainerView(
		const FContainerSessionHandle& Handle,
		FText& OutLabel,
		FInventoryContainerView& OutContainerView,
		TArray<FInventoryEntryView>& OutEntries) const;

	/** UI hint: does the local player have any cached session? */
	UFUNCTION(BlueprintPure, Category = "Inventory|ContainerSession")
	bool HasAnyActiveSession() const;

	/** UI hint: does the local player have a cached FullOpen session? */
	UFUNCTION(BlueprintPure, Category = "Inventory|ContainerSession")
	bool HasActiveFullOpenSession() const;

	/** UI hint: is this handle cached locally? */
	UFUNCTION(BlueprintPure, Category = "Inventory|ContainerSession")
	bool IsSessionActive(const FContainerSessionHandle& Handle) const;

	/**
	 * Return the first cached session, if any. Used by UI restoration flows
	 * (e.g., a new ViewModel binding to an inventory that already has an
	 * open session). Returns false when the cache is empty.
	 */
	bool GetFirstActiveSession(FContainerSessionHandle& OutHandle, AActor*& OutTargetActor) const;

private:
	struct FActiveContainerSession
	{
		FContainerSessionHandle Handle;
		TWeakObjectPtr<AActor> TargetActor;
		TWeakObjectPtr<UObject> SourceObject;
		TWeakObjectPtr<AActor> Instigator;
	};

	/** Client-side cache. Authoritative state lives on the authority subsystem. */
	TMap<FGuid, FActiveContainerSession> ActiveSessions;
};
