// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Subsystems/ProjectContainerSessionSubsystem.h"

#include "Interfaces/IWorldContainerSessionSource.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectContainerSessionSubsystem, Log, All);

bool UProjectContainerSessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	// Client cache only. Dedicated servers have no UI; they do not need
	// this subsystem and the authoritative session state lives in
	// UProjectWorldContainerAuthoritySubsystem.
	return !IsRunningDedicatedServer();
}

void UProjectContainerSessionSubsystem::Deinitialize()
{
	ActiveSessions.Reset();
	Super::Deinitialize();
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
		OutError = NSLOCTEXT("ProjectContainerSessionSubsystem", "RegisterInvalidSession",
			"Container session registration is invalid.");
		return false;
	}

	FActiveContainerSession& Session = ActiveSessions.FindOrAdd(Handle.SessionId);
	Session.Handle = Handle;
	Session.TargetActor = TargetActor;
	Session.SourceObject = SourceObject;
	Session.Instigator = Instigator;

	UE_LOG(LogProjectContainerSessionSubsystem, Log,
		TEXT("Cached session %s for %s"),
		*Handle.SessionId.ToString(EGuidFormats::DigitsWithHyphensLower),
		*GetNameSafe(TargetActor));
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

bool UProjectContainerSessionSubsystem::GetFirstActiveSession(
	FContainerSessionHandle& OutHandle,
	AActor*& OutTargetActor) const
{
	OutHandle.Reset();
	OutTargetActor = nullptr;

	for (const TPair<FGuid, FActiveContainerSession>& Pair : ActiveSessions)
	{
		OutHandle = Pair.Value.Handle;
		OutTargetActor = Pair.Value.TargetActor.Get();
		return OutHandle.IsValid();
	}
	return false;
}
