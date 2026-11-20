#include "ProjectTimeSubsystem.h"
#include "ProjectTimeWorldContextComponent.h"
#include "ProjectTimeZone.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectTime, Log, All);

bool UProjectTimeSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UProjectTimeSubsystem::BindContext(UProjectTimeWorldContextComponent& InContext, FString& OutError)
{
	if (!GetWorld() || !DoesSupportWorldType(GetWorld()->WorldType) || GetWorld()->GetNetMode() != NM_Standalone ||
		InContext.GetWorld() != GetWorld() || !InContext.IsRegistered() || !InContext.IsValidContext() ||
		(Context.IsValid() && Context.Get() != &InContext))
	{
		OutError = TEXT("time-context-invalid-or-foreign");
		return false;
	}
	Context = &InContext;
	BoundIntegrity = InContext.IntegrityRecord();
	Clock.SetControlsAllowed(false);
	UE_LOG(LogProjectTime, Display, TEXT("Bound world=%s zone=%s profile=%s"),
		*GetWorld()->GetName(), *InContext.ZoneId, *InContext.ProfileId);
	Changed.Broadcast();
	return true;
}

bool UProjectTimeSubsystem::CanControlTime() const
{
	if (!Context.IsValid() || !GetWorld() || !DoesSupportWorldType(GetWorld()->WorldType) || GetWorld()->GetNetMode() != NM_Standalone ||
		Context->IntegrityRecord() != BoundIntegrity) return false;
	const auto* Policy = Cast<IProjectTimeControlPolicy>(GetWorld()->GetAuthGameMode());
	return Policy && Policy->AllowsEnvironmentalTimeControl();
}

void UProjectTimeSubsystem::RefreshPermission()
{
	const bool bAllowed = CanControlTime();
	if (Clock.ControlsAllowed() != bAllowed)
	{
		Clock.SetControlsAllowed(bAllowed);
		ObserveDemoPermission();
		UE_LOG(LogProjectTime, Display, TEXT("Demo controls=%s world=%s"),
			bAllowed ? TEXT("allowed") : TEXT("denied"), *GetWorld()->GetName());
		Changed.Broadcast();
	}
}

bool UProjectTimeSubsystem::TryReadTime(FProjectTimeSnapshot& Out)
{
	if (!Context.IsValid() || Context->IntegrityRecord() != BoundIntegrity ||
		!GetWorld() || !DoesSupportWorldType(GetWorld()->WorldType) || GetWorld()->GetNetMode() != NM_Standalone) return false;
	RefreshPermission();
	return Clock.Read(ProjectTimeZone::ReadPcUtc(), FPlatformTime::Seconds(), Out);
}

bool UProjectTimeSubsystem::SeekUtc(FDateTime Utc)
{
	RefreshPermission();
	const bool bApplied = Clock.Seek(Utc, FPlatformTime::Seconds());
	if (bApplied) { ObserveDemoPermission(); Changed.Broadcast(); }
	return bApplied;
}

bool UProjectTimeSubsystem::SetRate(double Rate)
{
	RefreshPermission();
	const bool bApplied = Clock.SetRate(Rate, ProjectTimeZone::ReadPcUtc(), FPlatformTime::Seconds());
	if (bApplied) { ObserveDemoPermission(); Changed.Broadcast(); }
	return bApplied;
}

bool UProjectTimeSubsystem::ResetToPcTime()
{
	RefreshPermission();
	const bool bApplied = Clock.Reset();
	if (bApplied) { ObserveDemoPermission(); Changed.Broadcast(); }
	return bApplied;
}

FText UProjectTimeSubsystem::GetLocalTimeText()
{
	FProjectTimeSnapshot Value;
	return TryReadTime(Value) ? ProjectTimeZone::Format(Value.Utc, Context->ZoneId) : FText::GetEmpty();
}

void UProjectTimeSubsystem::Deinitialize()
{
	Changed.Clear();
	UnbindContext();
	Super::Deinitialize();
}

void UProjectTimeSubsystem::OnWorldEndPlay(UWorld& World)
{
	UnbindContext();
	Super::OnWorldEndPlay(World);
}

void UProjectTimeSubsystem::UnbindContext()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PermissionTimer);
	Context.Reset();
	BoundIntegrity.Reset();
	Clock.SetControlsAllowed(false);
	Changed.Broadcast();
}

void UProjectTimeSubsystem::ObserveDemoPermission()
{
	if (!GetWorld()) return;
	auto& Timers = GetWorld()->GetTimerManager();
	if (!Clock.IsDemo()) Timers.ClearTimer(PermissionTimer);
	else if (!Timers.IsTimerActive(PermissionTimer))
		Timers.SetTimer(PermissionTimer, this, &UProjectTimeSubsystem::RefreshPermission, 1.0f, true);
}
