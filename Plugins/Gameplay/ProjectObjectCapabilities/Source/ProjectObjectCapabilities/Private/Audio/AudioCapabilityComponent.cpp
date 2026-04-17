// Copyright ALIS. All Rights Reserved.

#include "Audio/AudioCapabilityComponent.h"
#include "Audio/AudioPresetDefinition.h"
#include "Components/AudioComponent.h"

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

	CachedPreset = AudioPresetAsset.LoadSynchronous();
	if (!CachedPreset)
	{
		UE_LOG(LogAudioCapability, Warning,
			TEXT("[%s::BeginPlay] Failed to load '%s'"),
			*GetNameSafe(GetOwner()), *AudioPresetAsset.ToString());
		return;
	}

	// Build track lookup map
	for (const FAudioTrack& Track : CachedPreset->Tracks)
	{
		TrackMap.Add(Track.Id, Track.Sound);
	}

	UE_LOG(LogAudioCapability, Log,
		TEXT("[%s::BeginPlay] Loaded %d tracks from '%s'"),
		*GetNameSafe(GetOwner()), TrackMap.Num(), *CachedPreset->AudioId.ToString());
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

	StopTrack();

	USoundBase* Sound = FoundSound->LoadSynchronous();
	if (!Sound)
	{
		UE_LOG(LogAudioCapability, Warning,
			TEXT("[%s::PlayTrack] Failed to load sound for track '%s'"),
			*GetNameSafe(GetOwner()), *TrackId);
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		return;
	}

	ActiveAudioComp = NewObject<UAudioComponent>(Owner);
	ActiveAudioComp->SetupAttachment(Owner->GetRootComponent());
	ActiveAudioComp->SetSound(Sound);
	ActiveAudioComp->bAutoDestroy = false;
	ActiveAudioComp->bIsUISound = false;

	// Apply source transform from preset (relative to actor)
	if (CachedPreset && CachedPreset->HasSourceTransform())
	{
		ActiveAudioComp->SetRelativeLocation(CachedPreset->SourceOffset);
		ActiveAudioComp->SetRelativeRotation(CachedPreset->SourceRotation);
	}

	// Apply attenuation from preset
	if (CachedPreset && CachedPreset->Attenuation.IsValid())
	{
		const FAudioAttenuationConfig& Att = CachedPreset->Attenuation;

		ActiveAudioComp->bOverrideAttenuation = true;
		ActiveAudioComp->AttenuationOverrides.bAttenuate = true;

		if (Att.InnerRadius > 0.0f)
		{
			ActiveAudioComp->AttenuationOverrides.AttenuationShapeExtents = FVector(Att.InnerRadius);
		}
		if (Att.FalloffDistance > 0.0f)
		{
			ActiveAudioComp->AttenuationOverrides.FalloffDistance = Att.FalloffDistance;
		}

		// Shape
		if (!Att.Shape.IsEmpty())
		{
			if (Att.Shape == TEXT("Sphere"))
				ActiveAudioComp->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
			else if (Att.Shape == TEXT("Capsule"))
				ActiveAudioComp->AttenuationOverrides.AttenuationShape = EAttenuationShape::Capsule;
			else if (Att.Shape == TEXT("Box"))
				ActiveAudioComp->AttenuationOverrides.AttenuationShape = EAttenuationShape::Box;
			else if (Att.Shape == TEXT("Cone"))
				ActiveAudioComp->AttenuationOverrides.AttenuationShape = EAttenuationShape::Cone;
		}

		// Cone angles
		if (Att.ConeAngle > 0.0f)
		{
			ActiveAudioComp->AttenuationOverrides.ConeOffset = Att.ConeAngle;
		}
		if (Att.ConeFalloffAngle > 0.0f)
		{
			ActiveAudioComp->AttenuationOverrides.FalloffDistance = Att.ConeFalloffAngle;
		}

		// Spatialization
		if (!Att.Spatialization.IsEmpty())
		{
			ActiveAudioComp->AttenuationOverrides.bSpatialize = true;
			if (Att.Spatialization == TEXT("Binaural"))
			{
				ActiveAudioComp->AttenuationOverrides.SpatializationAlgorithm =
					ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF;
				if (Att.BinauralRadius > 0.0f)
				{
					ActiveAudioComp->AttenuationOverrides.BinauralRadius = Att.BinauralRadius;
				}
			}
		}
	}
	else
	{
		// No preset attenuation: use sound asset's own settings
		ActiveAudioComp->bOverrideAttenuation = false;
		ActiveAudioComp->bAllowSpatialization = true;
	}

	ActiveAudioComp->RegisterComponent();
	ActiveAudioComp->OnAudioFinished.AddDynamic(this, &UAudioCapabilityComponent::HandleAudioFinished);
	ActiveAudioComp->Play();

	UE_LOG(LogAudioCapability, Log,
		TEXT("[%s::PlayTrack] Playing '%s' (hasAttenuationOverride=%s, hasSourceTransform=%s)"),
		*GetNameSafe(GetOwner()), *TrackId,
		CachedPreset && CachedPreset->Attenuation.IsValid() ? TEXT("true") : TEXT("false"),
		CachedPreset && CachedPreset->HasSourceTransform() ? TEXT("true") : TEXT("false"));
}

void UAudioCapabilityComponent::HandleAudioFinished()
{
	UE_LOG(LogAudioCapability, Log,
		TEXT("[%s::HandleAudioFinished] Sound finished"),
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
