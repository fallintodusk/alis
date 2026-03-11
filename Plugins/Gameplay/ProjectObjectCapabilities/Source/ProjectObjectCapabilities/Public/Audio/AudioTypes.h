// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioTypes.generated.h"

class USoundBase;

/**
 * A single audio track within a preset.
 * Parsed from AUDIO_*.json via FJsonObjectConverter.
 */
USTRUCT(BlueprintType)
struct FAudioTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> Sound;
};
