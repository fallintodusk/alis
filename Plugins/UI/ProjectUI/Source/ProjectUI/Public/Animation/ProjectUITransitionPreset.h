#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectUIAnimations.h"
#include "ProjectUITransitionPreset.generated.h"

class UWidgetAnimation;

/**
 * Data asset encapsulating a reusable transition timing preset.
 */
UCLASS(BlueprintType)
class PROJECTUI_API UProjectUITransitionPreset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UProjectUITransitionPreset();

	/** Designer friendly name for UI tools. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	FText DisplayName;

	/** Optional description for documentation / tooling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (MultiLine = true))
	FText Description;

	/** Procedural transition type executed alongside optional animations. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	EProjectUITransitionType TransitionType = EProjectUITransitionType::Fade;

	/** Relevant when TransitionType uses slide behaviour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	EProjectUISlideDirection SlideDirection = EProjectUISlideDirection::Bottom;

	/** Total duration in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float Duration = 0.25f;

	/** Delay before the transition begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float Delay = 0.0f;

	/** Easing function to use for interpolation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	TEnumAsByte<EEasingFunc::Type> EasingFunction = EEasingFunc::EaseInOut;

	/** Extra easing parameter (elastic amplitude, overshoot, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	float EasingAmplitude = 0.0f;

	/** Optional intro animation asset played on enter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TSoftObjectPtr<UWidgetAnimation> IntroAnimation;

	/** Optional outro animation asset played on exit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TSoftObjectPtr<UWidgetAnimation> OutroAnimation;

	// UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
