// Copyright ALIS. All Rights Reserved.
//
// ProjectInventoryComponent world-container SESSION lifecycle glue.
//
// The session registry and the authoritative TryBegin/End handshake live
// in UProjectWorldContainerAuthoritySubsystem (server-only UWorldSubsystem).
// This TU defines the thin glue that lets the inventory component be the
// IInventoryWorldContainerTransferBridge entry point:
//
//   Resolve helpers (shared by all world-container TUs, defined here):
//     - ResolveWorldContainerSessionSource
//     - ResolveWorldContainerActor
//   Local UI cache sync (called on authority + after Client RPC delivery):
//     - HandleWorldContainerSessionOpenedLocal
//     - HandleWorldContainerSessionClosedLocal
//   Client RPC impls (session open/close delivered from server to client):
//     - Client_WorldContainerSessionOpened_Implementation
//     - Client_WorldContainerSessionClosed_Implementation
//   Bridge interface impls (session lifecycle):
//     - RequestOpenWorldContainerSession_Implementation
//     - RequestCloseWorldContainerSession_Implementation
//     - GetActiveWorldContainerSession_Implementation
//   Server RPC impls (session open/close dispatched from the client):
//     - Server_RequestOpenWorldContainerSession_Implementation
//     - Server_RequestCloseWorldContainerSession_Implementation
//
// All authoritative work (TryBegin, End, session registry, transfer
// dispatch) routes through UProjectWorldContainerAuthoritySubsystem.

#include "Components/ProjectInventoryComponent.h"
#include "Components/ProjectInventoryComponentInternals.h"

#include "Interfaces/IWorldContainerSessionSource.h"
#include "ProjectInventory.h"
#include "Subsystems/ProjectContainerSessionSubsystem.h"
#include "Subsystems/ProjectWorldContainerAuthoritySubsystem.h"

#include "Components/ActorComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

using namespace ProjectInventoryInternal;

// -------------------------------------------------------------------------
// Resolve helpers (shared across world-container TUs)
// -------------------------------------------------------------------------

UObject* UProjectInventoryComponent::ResolveWorldContainerSessionSource(AActor* TargetActor) const
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

AActor* UProjectInventoryComponent::ResolveWorldContainerActor(UObject* WorldContainerSource) const
{
	return ResolveWorldContainerActorFromSource(WorldContainerSource);
}

// -------------------------------------------------------------------------
// Local UI cache sync
// -------------------------------------------------------------------------

void UProjectInventoryComponent::HandleWorldContainerSessionOpenedLocal(
	AActor* TargetActor,
	const FContainerSessionHandle& SessionHandle)
{
	if (!TargetActor || !SessionHandle.IsValid())
	{
		return;
	}

	UObject* SourceObject = ResolveWorldContainerSessionSource(TargetActor);
	if (!SourceObject)
	{
		return;
	}

	// Client-side UI cache population. On dedicated server this returns
	// nullptr (no local player, no UI, no-op). On listen server + owning
	// client, this keeps the nearby-container panel in sync.
	if (UProjectContainerSessionSubsystem* SessionCache = ResolveLocalPlayerSessionSubsystem(this))
	{
		FText RegisterError;
		if (!SessionCache->RegisterOpenedSession(TargetActor, SourceObject, GetOwner(), SessionHandle, RegisterError))
		{
			if (!RegisterError.IsEmpty())
			{
				BroadcastErrorLocal(RegisterError);
			}
			return;
		}
	}

	WorldContainerSessionOpenedNative.Broadcast(SourceObject, SessionHandle);
}

void UProjectInventoryComponent::HandleWorldContainerSessionClosedLocal(const FContainerSessionHandle& SessionHandle)
{
	if (!SessionHandle.IsValid())
	{
		return;
	}

	if (UProjectContainerSessionSubsystem* SessionCache = ResolveLocalPlayerSessionSubsystem(this))
	{
		SessionCache->CloseSessionLocal(SessionHandle);
	}

	WorldContainerSessionClosedNative.Broadcast(SessionHandle);
}

// -------------------------------------------------------------------------
// Client RPC impls
// -------------------------------------------------------------------------

void UProjectInventoryComponent::Client_WorldContainerSessionOpened_Implementation(
	AActor* TargetActor,
	FContainerSessionHandle SessionHandle)
{
	HandleWorldContainerSessionOpenedLocal(TargetActor, SessionHandle);
}

void UProjectInventoryComponent::Client_WorldContainerSessionClosed_Implementation(
	FContainerSessionHandle SessionHandle)
{
	HandleWorldContainerSessionClosedLocal(SessionHandle);
}

// -------------------------------------------------------------------------
// IInventoryWorldContainerTransferBridge impls (session lifecycle)
// -------------------------------------------------------------------------

bool UProjectInventoryComponent::RequestOpenWorldContainerSession_Implementation(
	AActor* TargetActor,
	EContainerSessionMode Mode,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectInventory", "RequestOpenWorldSessionMissingTarget",
			"World-container target is required.");
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		UWorld* World = GetWorld();
		UProjectWorldContainerAuthoritySubsystem* Auth = World
			? World->GetSubsystem<UProjectWorldContainerAuthoritySubsystem>()
			: nullptr;
		if (!Auth)
		{
			OutError = NSLOCTEXT("ProjectInventory", "RequestOpenMissingAuthority",
				"World-container authority subsystem is unavailable.");
			return false;
		}

		FContainerSessionHandle Handle;
		if (!Auth->OpenSession(TargetActor, Mode, OwnerActor, Handle, OutError))
		{
			return false;
		}

		HandleWorldContainerSessionOpenedLocal(TargetActor, Handle);
		if (APawn* OwnerPawn = Cast<APawn>(OwnerActor))
		{
			if (APlayerController* OwningPC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				if (!OwningPC->IsLocalController())
				{
					Client_WorldContainerSessionOpened(TargetActor, Handle);
				}
			}
		}
		return true;
	}

	Server_RequestOpenWorldContainerSession(TargetActor, Mode);
	return true;
}

bool UProjectInventoryComponent::RequestCloseWorldContainerSession_Implementation(
	const FContainerSessionHandle& SessionHandle,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!SessionHandle.IsValid())
	{
		OutError = NSLOCTEXT("ProjectInventory", "RequestCloseWorldSessionInvalidHandle",
			"World-container session handle is invalid.");
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		UWorld* World = GetWorld();
		UProjectWorldContainerAuthoritySubsystem* Auth = World
			? World->GetSubsystem<UProjectWorldContainerAuthoritySubsystem>()
			: nullptr;
		if (!Auth)
		{
			OutError = NSLOCTEXT("ProjectInventory", "RequestCloseMissingAuthority",
				"World-container authority subsystem is unavailable.");
			return false;
		}

		if (!Auth->CloseSession(SessionHandle, OutError))
		{
			return false;
		}

		HandleWorldContainerSessionClosedLocal(SessionHandle);
		if (APawn* OwnerPawn = Cast<APawn>(OwnerActor))
		{
			if (APlayerController* OwningPC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				if (!OwningPC->IsLocalController())
				{
					Client_WorldContainerSessionClosed(SessionHandle);
				}
			}
		}
		return true;
	}

	Server_RequestCloseWorldContainerSession(nullptr, SessionHandle);
	return true;
}

bool UProjectInventoryComponent::GetActiveWorldContainerSession_Implementation(
	AActor*& OutTargetActor,
	FContainerSessionHandle& OutSessionHandle) const
{
	OutTargetActor = nullptr;
	OutSessionHandle.Reset();

	const AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		// Authority source of truth is the UWorldSubsystem registry.
		if (const UWorld* World = GetWorld())
		{
			if (const UProjectWorldContainerAuthoritySubsystem* Auth =
				World->GetSubsystem<UProjectWorldContainerAuthoritySubsystem>())
			{
				return Auth->FindSessionByInstigator(OwnerActor, OutTargetActor, OutSessionHandle);
			}
		}
		return false;
	}

	// Client: read the local UI cache populated by Client_WorldContainer*
	// RPC delivery. One session per local player at a time.
	if (const UProjectContainerSessionSubsystem* SessionCache = ResolveLocalPlayerSessionSubsystem(this))
	{
		return SessionCache->GetFirstActiveSession(OutSessionHandle, OutTargetActor);
	}

	return false;
}

// -------------------------------------------------------------------------
// Server RPC impls (dispatched from the client when HasAuthority is false)
// -------------------------------------------------------------------------

void UProjectInventoryComponent::Server_RequestOpenWorldContainerSession_Implementation(
	AActor* TargetActor,
	EContainerSessionMode Mode)
{
	FText OpenError;
	if (RequestOpenWorldContainerSession_Implementation(TargetActor, Mode, OpenError))
	{
		return;
	}
	if (!OpenError.IsEmpty())
	{
		BroadcastError(OpenError);
	}
}

void UProjectInventoryComponent::Server_RequestCloseWorldContainerSession_Implementation(
	AActor* /*TargetActor*/,
	FContainerSessionHandle SessionHandle)
{
	FText CloseError;
	if (RequestCloseWorldContainerSession_Implementation(SessionHandle, CloseError))
	{
		return;
	}
	if (!CloseError.IsEmpty())
	{
		BroadcastError(CloseError);
	}
}
