// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectContainerSessionSubsystem.h"
#include "Components/ProjectInventoryComponent.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "Components/ActorComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectContainerSessionSubsystem, Log, All);

bool UProjectContainerSessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	return !IsRunningDedicatedServer();
}

void UProjectContainerSessionSubsystem::Deinitialize()
{
	CloseAllSessions();
	Super::Deinitialize();
}

bool UProjectContainerSessionSubsystem::OpenWorldContainerSession(
	AActor* TargetActor,
	EContainerSessionMode Mode,
	FContainerSessionHandle& OutHandle,
	FText& OutError)
{
	OutHandle.Reset();
	OutError = FText::GetEmpty();

	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "MissingTarget", "Target actor is required.");
		return false;
	}

	if (ActiveSessions.Num() > 0)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "SessionAlreadyActive", "A container session is already active for this player.");
		return false;
	}

	AActor* Instigator = ResolveSessionInstigator();
	if (!Instigator)
	{
		Instigator = TargetActor;
		UE_LOG(
			LogProjectContainerSessionSubsystem,
			Verbose,
			TEXT("OpenWorldContainerSession: no local player controller or pawn; using target actor %s as fallback instigator"),
			*GetNameSafe(TargetActor));
	}

	UObject* SourceObject = ResolveSessionSource(TargetActor);
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "MissingSessionSource", "Target does not expose a world container session source.");
		return false;
	}

	if (!IWorldContainerSessionSource::Execute_SupportsContainerSession(SourceObject, Mode))
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "UnsupportedMode", "Target does not support the requested container interaction mode.");
		return false;
	}

	FContainerSessionHandle Handle;
	Handle.SessionId = FGuid::NewGuid();
	Handle.ContainerKey = IWorldContainerSessionSource::Execute_GetWorldContainerKey(SourceObject);
	Handle.Mode = Mode;

	if (!Handle.IsValid())
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "InvalidContainerKey", "Target does not expose a valid world container key.");
		return false;
	}

	if (!IWorldContainerSessionSource::Execute_TryBeginContainerSession(
		SourceObject, Instigator, Handle.SessionId, Mode, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "OpenRejected", "World container rejected the session open request.");
		}
		return false;
	}

	FActiveContainerSession& Session = ActiveSessions.Add(Handle.SessionId);
	Session.Handle = Handle;
	Session.TargetActor = TargetActor;
	Session.SourceObject = SourceObject;
	Session.Instigator = Instigator;

	OutHandle = Handle;
	UE_LOG(LogProjectContainerSessionSubsystem, Log,
		TEXT("Opened %s container session %s for %s"),
		Mode == EContainerSessionMode::FullOpen ? TEXT("FullOpen") : TEXT("QuickLoot"),
		*Handle.SessionId.ToString(EGuidFormats::DigitsWithHyphensLower),
		*GetNameSafe(TargetActor));
	return true;
}

bool UProjectContainerSessionSubsystem::RegisterOpenedSession(
	AActor* TargetActor,
	UObject* SourceObject,
	AActor* Instigator,
	const FContainerSessionHandle& Handle,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!TargetActor || !SourceObject || !Handle.IsValid())
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "RegisterInvalidSession", "Container session registration is invalid.");
		return false;
	}

	if (ActiveSessions.Num() > 0 && !ActiveSessions.Contains(Handle.SessionId))
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "RegisterSessionAlreadyActive", "A container session is already active for this player.");
		return false;
	}

	FActiveContainerSession& Session = ActiveSessions.FindOrAdd(Handle.SessionId);
	Session.Handle = Handle;
	Session.TargetActor = TargetActor;
	Session.SourceObject = SourceObject;
	Session.Instigator = Instigator;
	return true;
}

bool UProjectContainerSessionSubsystem::CloseSession(const FContainerSessionHandle& Handle)
{
	FActiveContainerSession* Session = ActiveSessions.Find(Handle.SessionId);
	if (!Session)
	{
		return false;
	}

	const bool bNeedsWorldRelease = Session->Handle.Mode == EContainerSessionMode::FullOpen;
	UObject* SourceObject = Session->SourceObject.Get();
	if (bNeedsWorldRelease && SourceObject)
	{
		if (!IWorldContainerSessionSource::Execute_EndContainerSession(SourceObject, Session->Handle.SessionId))
		{
			return false;
		}
	}

	UE_LOG(LogProjectContainerSessionSubsystem, Log,
		TEXT("Closed container session %s for %s"),
		*Session->Handle.SessionId.ToString(EGuidFormats::DigitsWithHyphensLower),
		*GetNameSafe(Session->TargetActor.Get()));

	ActiveSessions.Remove(Handle.SessionId);
	return true;
}

bool UProjectContainerSessionSubsystem::CloseSessionLocal(const FContainerSessionHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return false;
	}

	return ActiveSessions.Remove(Handle.SessionId) > 0;
}

bool UProjectContainerSessionSubsystem::TakeAllFromWorldContainerSession(
	const FContainerSessionHandle& Handle,
	UProjectInventoryComponent* Inventory,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	FActiveContainerSession* Session = ActiveSessions.Find(Handle.SessionId);
	if (!Session)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "UnknownSession", "Container session handle is not active.");
		return false;
	}

	UObject* SourceObject = Session->SourceObject.Get();
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "MissingSourceObject", "Container session source is no longer valid.");
		ActiveSessions.Remove(Handle.SessionId);
		return false;
	}

	if (!Inventory)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "MissingInventoryForTakeAll", "Player inventory is required.");
		return false;
	}

	if (!Inventory->TakeAllFromWorldContainerResolved(SourceObject, Handle, OutError))
	{
		return false;
	}

	const bool bShouldAutoClose = Session->Handle.Mode == EContainerSessionMode::QuickLoot;

	if (bShouldAutoClose)
	{
		CloseSession(Handle);
	}

	return true;
}

bool UProjectContainerSessionSubsystem::TakeEntryFromWorldContainerSession(
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
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "UnknownSessionForTakeEntry", "Container session handle is not active.");
		return false;
	}

	UObject* SourceObject = Session->SourceObject.Get();
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "MissingSourceObjectForTakeEntry", "Container session source is no longer valid.");
		ActiveSessions.Remove(Handle.SessionId);
		return false;
	}

	TArray<FInventoryEntryView> EntryViews = IWorldContainerSessionSource::Execute_GetContainerEntryViews(SourceObject);
	const FInventoryEntryView* EntryView = EntryViews.FindByPredicate([EntryInstanceId](const FInventoryEntryView& Entry)
	{
		return Entry.InstanceId == EntryInstanceId;
	});

	if (!EntryView)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "MissingContainerEntry", "Container entry is no longer available.");
		return false;
	}

	if (!Inventory)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "MissingInventoryForTakeEntry", "Player inventory is required.");
		return false;
	}

	if (!Inventory->TakeEntryFromWorldContainerResolved(
			SourceObject,
			Handle,
			EntryInstanceId,
			Quantity,
			TargetContainerId,
			TargetGridPos,
			bTargetRotated,
			OutError))
	{
		return false;
	}

	const bool bShouldAutoClose = Session->Handle.Mode == EContainerSessionMode::QuickLoot;

	if (bShouldAutoClose)
	{
		CloseSession(Handle);
	}

	return true;
}

bool UProjectContainerSessionSubsystem::StoreInventoryEntryInWorldContainerSession(
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
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "UnknownSessionForStore", "Container session handle is not active.");
		return false;
	}

	UObject* SourceObject = Session->SourceObject.Get();
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "MissingSourceObjectForStore", "Container session source is no longer valid.");
		ActiveSessions.Remove(Handle.SessionId);
		return false;
	}

	if (!Inventory)
	{
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "MissingInventoryForStore", "Player inventory is required.");
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

bool UProjectContainerSessionSubsystem::GetSessionContainerView(
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

bool UProjectContainerSessionSubsystem::HasAnyActiveSession() const
{
	return ActiveSessions.Num() > 0;
}

bool UProjectContainerSessionSubsystem::HasActiveFullOpenSession() const
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

bool UProjectContainerSessionSubsystem::IsSessionActive(const FContainerSessionHandle& Handle) const
{
	return Handle.IsValid() && ActiveSessions.Contains(Handle.SessionId);
}

UObject* UProjectContainerSessionSubsystem::ResolveSessionSource(AActor* TargetActor) const
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

AActor* UProjectContainerSessionSubsystem::ResolveSessionInstigator() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	const UWorld* PreferredWorld = LocalPlayer->GetWorld();
	if (!PreferredWorld)
	{
		PreferredWorld = GetWorld();
	}

	APlayerController* PlayerController = LocalPlayer->GetPlayerController(PreferredWorld);
	if (!PlayerController)
	{
		if (PreferredWorld)
		{
			for (FConstPlayerControllerIterator It = PreferredWorld->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* Candidate = It->Get())
				{
					if (Candidate->GetLocalPlayer() == LocalPlayer)
					{
						PlayerController = Candidate;
						break;
					}
				}
			}
		}
	}
	if (PlayerController)
	{
		if (APawn* PlayerPawn = PlayerController->GetPawn())
		{
			return PlayerPawn;
		}

		return PlayerController;
	}

	return nullptr;
}

void UProjectContainerSessionSubsystem::CloseAllSessions()
{
	TArray<FContainerSessionHandle> Handles;
	Handles.Reserve(ActiveSessions.Num());

	for (const TPair<FGuid, FActiveContainerSession>& Pair : ActiveSessions)
	{
		Handles.Add(Pair.Value.Handle);
	}

	for (const FContainerSessionHandle& Handle : Handles)
	{
		CloseSession(Handle);
	}

	ActiveSessions.Empty();
}
