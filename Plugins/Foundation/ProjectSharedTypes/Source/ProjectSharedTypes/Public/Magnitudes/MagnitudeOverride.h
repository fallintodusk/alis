// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MagnitudeOverride.generated.h"

/**
 * Single magnitude override entry (replicate-friendly alternative to TMap).
 * Used by pickups and inventory entries to store per-instance overrides
 * that apply on top of definition default magnitudes.
 */
USTRUCT(BlueprintType)
struct PROJECTSHAREDTYPES_API FMagnitudeOverride
{
	GENERATED_BODY()

	FMagnitudeOverride() = default;
	FMagnitudeOverride(FGameplayTag InTag, float InValue) : Tag(InTag), Value(InValue) {}

	/** SetByCaller tag (e.g., SetByCaller.Health). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
	FGameplayTag Tag;

	/** Magnitude value (overrides default from definition). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
	float Value = 0.f;
};
