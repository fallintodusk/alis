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
 * Audio playback capability for interactable objects (gramophone, radio, etc.).
 *
 * Receives actions via IProjectActionReceiver from other capabilities (e.g. Dialogue).
 * Actions filtered by "audio." namespace:
 * - "audio.play:<track_id>" - play a track from the preset (one-shot, spatial)
 * - "audio.stop" - stop current playback
 * - "$end" - ignored (song keeps playing until it finishes naturally)
 *
 * Spatial audio: distance-based attenuation configured via InnerRadius/FalloffDistance
 * in the AUDIO_*.json preset. Simulates a physical sound source (gramophone horn, radio speaker).
 *
 * Track definitions loaded from UAudioPresetDefinition (generated from AUDIO_*.json).
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

	// TrackId -> SoftPtr<USoundBase>, built from preset in BeginPlay
	TMap<FString, TSoftObjectPtr<USoundBase>> TrackMap;

	// Cached attenuation from preset (cm)
	float InnerRadius = 200.f;
	float FalloffDistance = 1500.f;
};
