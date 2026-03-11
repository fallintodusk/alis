// Copyright ALIS. All Rights Reserved.

#include "Audio/AudioCapabilityComponent.h"
#include "Audio/AudioPresetDefinition.h"
#include "Components/AudioComponent.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAudioCapability, Log, All);

UAudioCapabilityComponent::UAudioCapabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FPrimaryAssetId UAudioCapabilityComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("Audio")));
}

void UAudioCapabilityComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AudioPresetAsset.IsNull())
	{
		UE_LOG(LogAudioCapability, Warning,
			TEXT("[%s::BeginPlay] No AudioPresetAsset set"), *GetNameSafe(GetOwner()));
		return;
	}

	UAudioPresetDefinition* Preset = AudioPresetAsset.LoadSynchronous();
	if (!Preset)
	{
		UE_LOG(LogAudioCapability, Warning,
			TEXT("[%s::BeginPlay] Failed to load '%s'"),
			*GetNameSafe(GetOwner()), *AudioPresetAsset.ToString());
		return;
	}

	// Build track lookup map
	for (const FAudioTrack& Track : Preset->Tracks)
	{
		TrackMap.Add(Track.Id, Track.Sound);
	}

	// Cache attenuation from preset
	InnerRadius = Preset->InnerRadius;
	FalloffDistance = Preset->FalloffDistance;

	UE_LOG(LogAudioCapability, Log,
		TEXT("[%s::BeginPlay] Loaded %d tracks from '%s' (radius=%.0f, falloff=%.0f)"),
		*GetNameSafe(GetOwner()), TrackMap.Num(), *Preset->AudioId.ToString(),
		InnerRadius, FalloffDistance);
}

void UAudioCapabilityComponent::EndPlay(EEndPlayReason::Type Reason)
{
	StopTrack();
	Super::EndPlay(Reason);
}

void UAudioCapabilityComponent::HandleAction(const FString& Context, const FString& Action)
{
	UE_LOG(LogAudioCapability, Log,
		TEXT("[%s::HandleAction] Context='%s', Action='%s'"),
		*GetNameSafe(GetOwner()), *Context, *Action);

	// $end = dialogue closed. Don't stop - let the gramophone keep playing
	if (Action == TEXT("$end"))
	{
		return;
	}

	if (!Action.StartsWith(TEXT("audio.")))
	{
		return;
	}

	FString Command = Action.Mid(6);

	if (Command == TEXT("stop"))
	{
		StopTrack();
	}
	else if (Command.StartsWith(TEXT("play:")))
	{
		FString TrackId = Command.Mid(5);
		PlayTrack(TrackId);
	}
	else
	{
		UE_LOG(LogAudioCapability, Warning,
			TEXT("[%s::HandleAction] Unknown audio command: '%s'"),
			*GetNameSafe(GetOwner()), *Action);
	}
}

void UAudioCapabilityComponent::PlayTrack(const FString& TrackId)
{
	const TSoftObjectPtr<USoundBase>* FoundSound = TrackMap.Find(TrackId);
	if (!FoundSound)
	{
		UE_LOG(LogAudioCapability, Warning,
			TEXT("[%s::PlayTrack] Track '%s' not found in preset"),
			*GetNameSafe(GetOwner()), *TrackId);
		return;
	}

	// Stop current playback before switching tracks
	StopTrack();

	USoundBase* Sound = FoundSound->LoadSynchronous();
	if (!Sound)
	{
		UE_LOG(LogAudioCapability, Warning,
			TEXT("[%s::PlayTrack] Failed to load sound for track '%s' ('%s')"),
			*GetNameSafe(GetOwner()), *TrackId, *FoundSound->ToString());
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		return;
	}

	// Create audio component manually for full attenuation control
	ActiveAudioComp = NewObject<UAudioComponent>(Owner);
	ActiveAudioComp->SetupAttachment(Owner->GetRootComponent());
	ActiveAudioComp->SetSound(Sound);
	ActiveAudioComp->bAutoDestroy = false;
	ActiveAudioComp->bIsUISound = false;

	// Spatial attenuation from preset
	ActiveAudioComp->bOverrideAttenuation = true;
	ActiveAudioComp->AttenuationOverrides.bAttenuate = true;
	ActiveAudioComp->AttenuationOverrides.AttenuationShapeExtents = FVector(InnerRadius);
	ActiveAudioComp->AttenuationOverrides.FalloffDistance = FalloffDistance;

	ActiveAudioComp->RegisterComponent();
	ActiveAudioComp->OnAudioFinished.AddDynamic(this, &UAudioCapabilityComponent::HandleAudioFinished);
	ActiveAudioComp->Play();

	UE_LOG(LogAudioCapability, Log,
		TEXT("[%s::PlayTrack] Playing '%s' (inner=%.0f, falloff=%.0f)"),
		*GetNameSafe(GetOwner()), *TrackId, InnerRadius, FalloffDistance);
}

void UAudioCapabilityComponent::HandleAudioFinished()
{
	UE_LOG(LogAudioCapability, Log,
		TEXT("[%s::HandleAudioFinished] Song finished naturally"),
		*GetNameSafe(GetOwner()));

	if (ActiveAudioComp)
	{
		ActiveAudioComp->DestroyComponent();
		ActiveAudioComp = nullptr;
	}
}

void UAudioCapabilityComponent::StopTrack()
{
	if (ActiveAudioComp)
	{
		ActiveAudioComp->OnAudioFinished.RemoveAll(this);
		ActiveAudioComp->Stop();
		ActiveAudioComp->DestroyComponent();
		ActiveAudioComp = nullptr;
	}
}
