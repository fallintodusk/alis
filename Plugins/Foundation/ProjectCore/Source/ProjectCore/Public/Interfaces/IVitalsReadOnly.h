// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "IVitalsReadOnly.generated.h"

USTRUCT(BlueprintType)
struct PROJECTCORE_API FVitalsReadOnlySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float Condition = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float MaxCondition = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float Calories = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float MaxCalories = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float Hydration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float MaxHydration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float Fatigue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float MaxFatigue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	FGameplayTagContainer StateTags;

	float GetHydrationFraction() const
	{
		return MaxHydration > UE_SMALL_NUMBER
			? FMath::Clamp(Hydration / MaxHydration, 0.0f, 1.0f)
			: 0.0f;
	}
};

UINTERFACE(MinimalAPI, BlueprintType)
class UVitalsReadOnly : public UInterface
{
	GENERATED_BODY()
};

/** Read-only projection for consumers that must not depend on ProjectVitals. */
class PROJECTCORE_API IVitalsReadOnly
{
	GENERATED_BODY()

public:
	virtual bool GetVitalsSnapshot(FVitalsReadOnlySnapshot& OutSnapshot) const = 0;
};
