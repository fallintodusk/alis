// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IProjectActionReceiver.h"
#include "AudioCapabilityComponent.generated.h"

class UAudioPresetDefinition;
class UAudioComponent;
class USoundBase;

/**
 * Audio playback capability for interactable objects.
 *
 * Receives actions via IProjectActionReceiver from other capabilities.
 * Actions filtered by "audio." namespace:
 * - "audio.play:<track_id>" - play a track from the preset (spatial)
 * - "audio.stop" - stop current playback
 *
 * All audio settings (tracks, attenuation, spatialization, source transform)
 * come from UAudioPresetDefinition (AUDIO_*.json). The preset is the single
 * source of truth -- no audio properties exposed on this component.
 */
UCLASS(ClassGroup = (ProjectCapabilities), meta = (BlueprintSpawnableComponent))
class PROJECTOBJECTCAPABILITIES_API UAudioCapabilityComponent
	: public UActorComponent
	, public IProjectActionReceiver
{
	GENERATED_BODY()

public:
	UAudioCapabilityComponent();

	// --- Stable ID for Capability Registry ---
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// --- IProjectActionReceiver ---
	virtual void HandleAction(const FString& Context, const FString& Action) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

	// Audio preset asset (set via object definition properties)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<UAudioPresetDefinition> AudioPresetAsset;

private:
	void PlayTrack(const FString& TrackId);
	void StopTrack();

	UFUNCTION()
	void HandleAudioFinished();

	UPROPERTY()
	TObjectPtr<UAudioComponent> ActiveAudioComp;

	// Cached preset (loaded in BeginPlay)
	UPROPERTY()
	TObjectPtr<UAudioPresetDefinition> CachedPreset;

	// TrackId -> SoftPtr<USoundBase>, built from preset in BeginPlay
	TMap<FString, TSoftObjectPtr<USoundBase>> TrackMap;
};
