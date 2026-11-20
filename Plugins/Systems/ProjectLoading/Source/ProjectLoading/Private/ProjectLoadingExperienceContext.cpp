#include "ProjectLoadingSubsystem.h"
#include "ProjectLoadingLog.h"
#include "Engine/World.h"

bool UProjectLoadingSubsystem::TryGetLoadedExperienceContext(FName& OutName, const UWorld*& OutWorld) const
{
	OutName = NAME_None;
	OutWorld = nullptr;
	if (IsLoadInProgress() || ConfirmedExperienceName.IsNone() || !ConfirmedExperienceWorld.IsValid()) return false;
	OutName = ConfirmedExperienceName;
	OutWorld = ConfirmedExperienceWorld.Get();
	return true;
}

void UProjectLoadingSubsystem::BeginExperienceContext(const FLoadRequest& Request)
{
	PendingExperienceName = Request.ExperienceName;
	PendingExperienceMap = Request.MapSoftPath.GetLongPackageName();
	PendingExperienceWorld.Reset();
	bPendingExperienceCompleted = false;
}

void UProjectLoadingSubsystem::HandleExperienceMapLoaded(UWorld* World)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || PendingExperienceName.IsNone() ||
		PendingExperienceMap.IsEmpty() ||
		UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != PendingExperienceMap) return;
	PendingExperienceWorld = World;
	PublishExperienceContext();
}

void UProjectLoadingSubsystem::CompleteExperienceContext(bool bSucceeded)
{
	if (!bSucceeded)
	{
		PendingExperienceName = NAME_None;
		PendingExperienceWorld.Reset();
		bPendingExperienceCompleted = false;
		return;
	}
	bPendingExperienceCompleted = true;
	PublishExperienceContext();
}

void UProjectLoadingSubsystem::PublishExperienceContext()
{
	if (!bPendingExperienceCompleted || PendingExperienceName.IsNone() || !PendingExperienceWorld.IsValid()) return;
	ConfirmedExperienceName = PendingExperienceName;
	ConfirmedExperienceWorld = PendingExperienceWorld;
	PendingExperienceName = NAME_None;
	PendingExperienceWorld.Reset();
	bPendingExperienceCompleted = false;
	UE_LOG(LogProjectLoading, Display, TEXT("Confirmed experience context: experience=%s world=%s"),
		*ConfirmedExperienceName.ToString(), *GetNameSafe(ConfirmedExperienceWorld.Get()));
}
