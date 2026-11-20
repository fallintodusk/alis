#include "ProjectLightComponent.h"
#include "Interfaces/ProjectTimeView.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/MiscTrace.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectLight, Log, All);

UProjectLightComponent::UProjectLightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UProjectLightComponent::Start(UObject& TimeView, double Latitude, double Longitude,
	const FVector& East, const FVector& North, const FVector& Up, FString& OutError)
{
	auto* View = Cast<IProjectTimeView>(&TimeView);
	FProjectTimeSnapshot Snapshot;
	FVector Direction;
	double Elevation;
	if (!GetWorld() || (GetWorld()->WorldType != EWorldType::Game && GetWorld()->WorldType != EWorldType::PIE) ||
		GetWorld()->GetNetMode() != NM_Standalone || !View || View->GetTimeWorld() != GetWorld() ||
		!Settings.IsValid() || !Bindings.IsValidForWorld(*GetWorld()) || !View->TryReadTime(Snapshot) ||
		(Settings.NightFillLux > 0 && !Bindings.NightFill) ||
		!FMath::IsFinite(Snapshot.Rate) || Snapshot.Rate < 0 ||
		!ProjectLight::SunDirection(Snapshot.Utc, Latitude, Longitude, East, North, Up, Direction, Elevation))
	{
		OutError = TEXT("light-binding-or-time-context-invalid");
		return false;
	}
	Stop();
	Provider = &TimeView;
	LatitudeValue = Latitude;
	LongitudeValue = Longitude;
	EastVector = East;
	NorthVector = North;
	UpVector = Up;
	ChangedHandle = View->TimeChanged().AddUObject(this, &UProjectLightComponent::OnTimeChanged);
	return ApplyCurrent();
}

void UProjectLightComponent::RequestApply()
{
	SetComponentTickEnabled(true);
}

void UProjectLightComponent::OnTimeChanged()
{
	bClockChangePending = true;
	RequestApply();
}

void UProjectLightComponent::BeginLightingRecovery()
{
	if (!RecoveryVolume.IsValid())
	{
		RecoveryVolume = Bindings.PostProcess;
		const auto& Post = RecoveryVolume->Settings;
		bPreviousSceneUpdateOverride = Post.bOverride_LumenSceneLightingUpdateSpeed;
		PreviousSceneUpdateSpeed = Post.LumenSceneLightingUpdateSpeed;
	}
	auto& Post = RecoveryVolume->Settings;
	Post.bOverride_LumenSceneLightingUpdateSpeed = true;
	Post.LumenSceneLightingUpdateSpeed = bPreviousSceneUpdateOverride ? FMath::Max(4.0f, PreviousSceneUpdateSpeed) : 4.0f;
	// A discrete calendar seek needs brief Scene recovery; steady clocks retain their budget.
	GetWorld()->GetTimerManager().SetTimer(LightingRecoveryTimer, this, &UProjectLightComponent::EndLightingRecovery, 15.0f, false);
	TRACE_BOOKMARK(TEXT("ProjectLight.SeekRecovery begin frame=%llu"), GFrameCounter);
}

void UProjectLightComponent::EndLightingRecovery()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LightingRecoveryTimer);
	if (RecoveryVolume.IsValid())
	{
		auto& Post = RecoveryVolume->Settings;
		Post.bOverride_LumenSceneLightingUpdateSpeed = bPreviousSceneUpdateOverride;
		Post.LumenSceneLightingUpdateSpeed = PreviousSceneUpdateSpeed;
		TRACE_BOOKMARK(TEXT("ProjectLight.SeekRecovery end frame=%llu"), GFrameCounter);
	}
	RecoveryVolume.Reset();
}

bool UProjectLightComponent::ApplyCurrent()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProjectLightApply);
	auto* View = Cast<IProjectTimeView>(Provider.Get());
	FProjectTimeSnapshot Snapshot;
	FVector Direction;
	double Elevation;
	if (!GetWorld() || (GetWorld()->WorldType != EWorldType::Game && GetWorld()->WorldType != EWorldType::PIE) ||
		GetWorld()->GetNetMode() != NM_Standalone || !View || View->GetTimeWorld() != GetWorld() || !View->TryReadTime(Snapshot) ||
		!FMath::IsFinite(Snapshot.Rate) || Snapshot.Rate < 0 ||
		!Settings.IsValid() || !Bindings.IsValidForWorld(*GetWorld()) ||
		(Settings.NightFillLux > 0 && !Bindings.NightFill) ||
		!ProjectLight::SunDirection(Snapshot.Utc, LatitudeValue, LongitudeValue,
			EastVector, NorthVector, UpVector, Direction, Elevation))
	{
		UE_LOG(LogProjectLight, Error, TEXT("Daylight stopped: bound context is unavailable or invalid."));
		Stop();
		return false;
	}
	const bool bRecoverLighting = bClockChangePending && LastUtcTicks >= 0 &&
		FMath::Abs(Snapshot.Utc.GetTicks() - LastUtcTicks) > FTimespan::FromMinutes(5).GetTicks();
	bClockChangePending = false;
	if (Snapshot.Utc.GetTicks() != LastUtcTicks)
	{
		if (bRecoverLighting) BeginLightingRecovery();
		TRACE_BOOKMARK(TEXT("ProjectLight.Apply frame=%llu rate=%.3f utc=%s"), GFrameCounter,
			Snapshot.Rate, *Snapshot.Utc.ToIso8601());
		UE_LOG(LogProjectLight, Verbose, TEXT("Apply frame=%llu rate=%.3f utc=%s"), GFrameCounter,
			Snapshot.Rate, *Snapshot.Utc.ToIso8601());
		Bindings.Sun->SetWorldRotation((-Direction).Rotation());
		const float Day = FMath::Clamp((Elevation + 6.0) / 6.0, 0.0, 1.0);
		Bindings.Sun->SetIntensity(Settings.SunIntensityLux * Day);
		if (Bindings.NightFill)
		{
			// A non-shadowing night fill opposite the Sun; not an astronomical Moon.
			Bindings.NightFill->SetWorldRotation(Direction.Rotation());
			Bindings.NightFill->SetIntensity(Settings.NightFillLux * (1.0f - Day));
		}
		// Forward Water/fog use one directional source; never leave their selection tied.
		auto Brightness = [](const UDirectionalLightComponent& Light)
		{
			const FLinearColor Color = Light.GetColoredLightBrightness();
			return FVector(Color.R, Color.G, Color.B).SizeSquared();
		};
		const bool bNightDominant = Bindings.NightFill && Brightness(*Bindings.NightFill) > Brightness(*Bindings.Sun);
		const int32 SunPriority = bNightDominant ? 0 : 1;
		if (Bindings.Sun->ForwardShadingPriority != SunPriority) Bindings.Sun->SetForwardShadingPriority(SunPriority);
		if (Bindings.NightFill && Bindings.NightFill->ForwardShadingPriority != 1 - SunPriority)
			Bindings.NightFill->SetForwardShadingPriority(1 - SunPriority);
		if (Settings.bHistogram)
		{
			const float MinEv = FMath::Lerp(Settings.NightMinEv100, Settings.MinEv100, Day);
			const auto* Extended = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
			auto& Exposure = Bindings.PostProcess->Settings;
			Exposure.AutoExposureMinBrightness = Extended && Extended->GetValueOnGameThread() != 0 ? MinEv : FMath::Exp2(MinEv);
			Exposure.HistogramLogMin = FMath::Min(Settings.HistogramMinEv100, MinEv - 2.0f);
		}
		LastUtcTicks = Snapshot.Utc.GetTicks();
		LastApplyFrame = GFrameCounter;
		++ApplyCount;
	}
	const float Interval = Snapshot.Rate == 0 ? 0 : Snapshot.Rate > 1 ? Settings.DemoUpdateSeconds : Settings.LiveUpdateSeconds;
	if (CurrentInterval != Interval)
	{
		GetWorld()->GetTimerManager().ClearTimer(Timer);
		CurrentInterval = Interval;
		if (Interval > 0)
			GetWorld()->GetTimerManager().SetTimer(Timer, this, &UProjectLightComponent::RequestApply, Interval, true);
	}
	return true;
}

void UProjectLightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	SetComponentTickEnabled(false);
	ApplyCurrent();
}

void UProjectLightComponent::Stop()
{
	EndLightingRecovery();
	bClockChangePending = false;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Timer);
	if (auto* View = Cast<IProjectTimeView>(Provider.Get())) View->TimeChanged().Remove(ChangedHandle);
	Provider.Reset();
	ChangedHandle.Reset();
	CurrentInterval = -1;
	LastUtcTicks = -1;
	SetComponentTickEnabled(false);
}

void UProjectLightComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Stop();
	Super::EndPlay(Reason);
}
