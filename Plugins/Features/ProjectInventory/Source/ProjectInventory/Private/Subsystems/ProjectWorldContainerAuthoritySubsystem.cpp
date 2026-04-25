// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectWorldContainerAuthoritySubsystem.h"

#include "Components/ActorComponent.h"
#include "Components/ProjectInventoryComponent.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "Operations/WorldContainerMoveOp.h"

#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldContainerAuthority, Log, All);

namespace
{
	/** Resolve the IWorldContainerSessionSource impl on a target actor (actor or component). */
	UObject* ResolveSessionSourceOnTarget(AActor* TargetActor)
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
}

bool UProjectWorldContainerAuthoritySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// Server-authority scope only. Pure clients never hold authoritative
	// session state; they see only a mirror populated by Client RPCs via
	// the local-player session subsystem.
	const UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}

	// Accept PIE/editor previews, standalone, listen server, dedicated
	// server. Reject only pure clients connected to a remote server.
	return World->GetNetMode() != NM_Client;
}

void UProjectWorldContainerAuthoritySubsystem::Deinitialize()
{
	CloseAllSessions();
	Super::Deinitialize();
}

void UProjectWorldContainerAuthoritySubsystem::CloseAllSessions()
{
	// Snapshot handles first -- CloseSession mutates the map.
	TArray<FContainerSessionHandle> Handles;
	Handles.Reserve(ActiveSessions.Num());
	for (const TPair<FGuid, FActiveContainerSession>& Pair : ActiveSessions)
	{
		Handles.Add(Pair.Value.Handle);
	}

	for (const FContainerSessionHandle& Handle : Handles)
	{
		FText CloseError;
		if (!CloseSession(Handle, CloseError))
		{
			UE_LOG(LogProjectWorldContainerAuthority, Warning,
				TEXT("CloseAllSessions: Close for %s failed: %s"),
				*Handle.SessionId.ToString(EGuidFormats::DigitsWithHyphensLower),
				*CloseError.ToString());
		}
	}

	ActiveSessions.Reset();
}

bool UProjectWorldContainerAuthoritySubsystem::OpenSession(
	AActor* TargetActor,
	EContainerSessionMode Mode,
	AActor* Instigator,
	FContainerSessionHandle& OutHandle,
	FText& OutError)
{
	OutHandle.Reset();
	OutError = FText::GetEmpty();

	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "OpenMissingTarget",
			"World-container target is required.");
		return false;
	}

	UObject* SourceObject = ResolveSessionSourceOnTarget(TargetActor);
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "OpenMissingSource",
			"Target does not expose a world-container session source.");
		return false;
	}

	if (!IWorldContainerSessionSource::Execute_SupportsContainerSession(SourceObject, Mode))
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "OpenUnsupportedMode",
			"Target does not support the requested world-container mode.");
		return false;
	}

	FContainerSessionHandle Handle;
	Handle.SessionId = FGuid::NewGuid();
	Handle.ContainerKey = IWorldContainerSessionSource::Execute_GetWorldContainerKey(SourceObject);
	Handle.Mode = Mode;
	if (!Handle.IsValid())
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "OpenInvalidKey",
			"Target world-container key is invalid.");
		return false;
	}

	if (!IWorldContainerSessionSource::Execute_TryBeginContainerSession(
			SourceObject, Instigator, Handle.SessionId, Mode, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "OpenRejected",
				"World-container session open was rejected.");
		}
		return false;
	}

	FActiveContainerSession& Session = ActiveSessions.Add(Handle.SessionId);
	Session.Handle = Handle;
	Session.TargetActor = TargetActor;
	Session.SourceObject = SourceObject;
	Session.InstigatorKey = FObjectKey(Instigator);
	Session.Instigator = Instigator;

	OutHandle = Handle;
	UE_LOG(LogProjectWorldContainerAuthority, Log,
		TEXT("Opened authority session %s (%s) for instigator %s on target %s"),
		*Handle.SessionId.ToString(EGuidFormats::DigitsWithHyphensLower),
		Mode == EContainerSessionMode::FullOpen ? TEXT("FullOpen") : TEXT("QuickLoot"),
		*GetNameSafe(Instigator),
		*GetNameSafe(TargetActor));
	return true;
}

bool UProjectWorldContainerAuthoritySubsystem::CloseSession(const FContainerSessionHandle& Handle, FText& OutError)
{
	OutError = FText::GetEmpty();

	FActiveContainerSession* Session = ActiveSessions.Find(Handle.SessionId);
	if (!Session)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "CloseUnknown",
			"World-container session handle is not active.");
		return false;
	}

	const bool bNeedsHandshake = Session->Handle.Mode == EContainerSessionMode::FullOpen;
	UObject* SourceObject = Session->SourceObject.Get();
	if (bNeedsHandshake && SourceObject)
	{
		if (!IWorldContainerSessionSource::Execute_EndContainerSession(SourceObject, Handle.SessionId))
		{
			OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "CloseRejected",
				"World-container session close was rejected.");
			return false;
		}
	}

	UE_LOG(LogProjectWorldContainerAuthority, Log,
		TEXT("Closed authority session %s"),
		*Handle.SessionId.ToString(EGuidFormats::DigitsWithHyphensLower));

	ActiveSessions.Remove(Handle.SessionId);
	return true;
}

bool UProjectWorldContainerAuthoritySubsystem::IsSessionActive(const FContainerSessionHandle& Handle) const
{
	return Handle.IsValid() && ActiveSessions.Contains(Handle.SessionId);
}

bool UProjectWorldContainerAuthoritySubsystem::FindSessionByInstigator(
	const AActor* Instigator,
	AActor*& OutTargetActor,
	FContainerSessionHandle& OutHandle) const
{
	OutTargetActor = nullptr;
	OutHandle.Reset();

	// Identity via FObjectKey so a marked-as-garbage instigator still
	// resolves its session (tests can destroy the controlling PC between
	// runs without invalidating mid-test session lookups).
	const FObjectKey Key(Instigator);
	for (const TPair<FGuid, FActiveContainerSession>& Pair : ActiveSessions)
	{
		if (Pair.Value.InstigatorKey == Key)
		{
			OutTargetActor = Pair.Value.TargetActor.Get();
			OutHandle = Pair.Value.Handle;
			return OutHandle.IsValid();
		}
	}

	return false;
}

bool UProjectWorldContainerAuthoritySubsystem::HasAnyActiveSessionForInstigator(const AActor* Instigator) const
{
	const FObjectKey Key(Instigator);
	for (const TPair<FGuid, FActiveContainerSession>& Pair : ActiveSessions)
	{
		if (Pair.Value.InstigatorKey == Key)
		{
			return true;
		}
	}
	return false;
}

bool UProjectWorldContainerAuthoritySubsystem::HasActiveFullOpenSessionForInstigator(const AActor* Instigator) const
{
	const FObjectKey Key(Instigator);
	for (const TPair<FGuid, FActiveContainerSession>& Pair : ActiveSessions)
	{
		if (Pair.Value.InstigatorKey == Key
			&& Pair.Value.Handle.Mode == EContainerSessionMode::FullOpen)
		{
			return true;
		}
	}
	return false;
}

bool UProjectWorldContainerAuthoritySubsystem::HasAnyActiveSession() const
{
	return ActiveSessions.Num() > 0;
}

bool UProjectWorldContainerAuthoritySubsystem::HasActiveFullOpenSession() const
{
	for (const TPair<FGuid, FActiveContainerSession>& Pair : ActiveSessions)
	{
		if (Pair.Value.Handle.Mode == EContainerSessionMode::FullOpen)
		{
			return true;
		}
	}
	return false;
}

bool UProjectWorldContainerAuthoritySubsystem::GetSessionContainerView(
	const FContainerSessionHandle& Handle,
	FText& OutLabel,
	FInventoryContainerView& OutContainerView,
	TArray<FInventoryEntryView>& OutEntries) const
{
	OutLabel = FText::GetEmpty();
	OutContainerView = FInventoryContainerView();
	OutEntries.Reset();

	const FActiveContainerSession* Session = ActiveSessions.Find(Handle.SessionId);
	if (!Session)
	{
		return false;
	}

	UObject* SourceObject = Session->SourceObject.Get();
	if (!SourceObject)
	{
		return false;
	}

	OutLabel = IWorldContainerSessionSource::Execute_GetContainerDisplayLabel(SourceObject);
	OutContainerView = IWorldContainerSessionSource::Execute_GetContainerView(SourceObject);
	OutEntries = IWorldContainerSessionSource::Execute_GetContainerEntryViews(SourceObject);
	return true;
}

bool UProjectWorldContainerAuthoritySubsystem::IsSessionInstigatedBy(
	const FContainerSessionHandle& Handle,
	const AActor* Instigator) const
{
	const FActiveContainerSession* Session = ActiveSessions.Find(Handle.SessionId);
	return Session && Session->InstigatorKey == FObjectKey(Instigator);
}

bool UProjectWorldContainerAuthoritySubsystem::TakeEntryFromWorldContainerSession(
	const FContainerSessionHandle& Handle,
	UProjectInventoryComponent* Inventory,
	int32 EntryInstanceId,
	int32 Quantity,
	FGameplayTag TargetContainerId,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	FActiveContainerSession* Session = ActiveSessions.Find(Handle.SessionId);
	if (!Session)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "UnknownSessionForTake",
			"Authority session handle is not active.");
		return false;
	}

	UObject* SourceObject = Session->SourceObject.Get();
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "MissingSourceForTake",
			"Authority session source is no longer valid.");
		ActiveSessions.Remove(Handle.SessionId);
		return false;
	}

	if (!Inventory)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "MissingInventoryForTake",
			"Player inventory is required.");
		return false;
	}

	const bool bResolved = Inventory->TakeEntryFromWorldContainerResolved(
		SourceObject,
		Handle,
		EntryInstanceId,
		Quantity,
		TargetContainerId,
		TargetGridPos,
		bTargetRotated,
		OutError);

	// QuickLoot sessions auto-close after a successful take so the UI
	// doesn't linger on an empty hint panel. FullOpen stays open for
	// repeated transfers until the player closes explicitly.
	if (bResolved && Session->Handle.Mode == EContainerSessionMode::QuickLoot)
	{
		FText AutoCloseErr;
		CloseSession(Handle, AutoCloseErr);
	}

	return bResolved;
}

bool UProjectWorldContainerAuthoritySubsystem::StoreInventoryEntryInWorldContainerSession(
	const FContainerSessionHandle& Handle,
	UProjectInventoryComponent* Inventory,
	int32 InventoryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	FActiveContainerSession* Session = ActiveSessions.Find(Handle.SessionId);
	if (!Session)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "UnknownSessionForStore",
			"Authority session handle is not active.");
		return false;
	}

	UObject* SourceObject = Session->SourceObject.Get();
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "MissingSourceForStore",
			"Authority session source is no longer valid.");
		ActiveSessions.Remove(Handle.SessionId);
		return false;
	}

	if (!Inventory)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "MissingInventoryForStore",
			"Player inventory is required.");
		return false;
	}

	return Inventory->StoreInventoryEntryInWorldContainerResolved(
		SourceObject,
		Handle,
		InventoryInstanceId,
		Quantity,
		TargetGridPos,
		bTargetRotated,
		OutError);
}

bool UProjectWorldContainerAuthoritySubsystem::TakeAllFromWorldContainerSession(
	const FContainerSessionHandle& Handle,
	UProjectInventoryComponent* Inventory,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	FActiveContainerSession* Session = ActiveSessions.Find(Handle.SessionId);
	if (!Session)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "UnknownSessionForTakeAll",
			"Authority session handle is not active.");
		return false;
	}

	UObject* SourceObject = Session->SourceObject.Get();
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "MissingSourceForTakeAll",
			"Authority session source is no longer valid.");
		ActiveSessions.Remove(Handle.SessionId);
		return false;
	}

	if (!Inventory)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "MissingInventoryForTakeAll",
			"Player inventory is required.");
		return false;
	}

	const bool bResolved = Inventory->TakeAllFromWorldContainerResolved(SourceObject, Handle, OutError);

	// QuickLoot auto-close after bulk transfer.
	if (bResolved && Session->Handle.Mode == EContainerSessionMode::QuickLoot)
	{
		FText AutoCloseErr;
		CloseSession(Handle, AutoCloseErr);
	}

	return bResolved;
}

bool UProjectWorldContainerAuthoritySubsystem::MoveWithinWorldContainerSession(
	const FContainerSessionHandle& Handle,
	int32 EntryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	FActiveContainerSession* Session = ActiveSessions.Find(Handle.SessionId);
	if (!Session)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "UnknownSessionForMove",
			"Authority session handle is not active.");
		return false;
	}

	UObject* SourceObject = Session->SourceObject.Get();
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectWorldContainerAuthority", "MissingSourceForMove",
			"Authority session source is no longer valid.");
		ActiveSessions.Remove(Handle.SessionId);
		return false;
	}

	// Pure-logic op: snapshot + consume + store + rollback.
	return FWorldContainerMoveOp::Execute(
		SourceObject,
		Handle.SessionId,
		EntryInstanceId,
		Quantity,
		TargetGridPos,
		bTargetRotated,
		OutError);
}
