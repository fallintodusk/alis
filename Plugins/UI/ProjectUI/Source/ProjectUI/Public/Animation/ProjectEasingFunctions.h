// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProjectEasingFunctions.generated.h"

/**
 * Easing curve types for animations
 *
 * Provides industry-standard easing curves for smooth, professional animations.
 * Based on Robert Penner's easing functions and CSS animation curves.
 */
UENUM(BlueprintType)
enum class EProjectEasingType : uint8
{
	/** Linear interpolation (no easing) */
	Linear UMETA(DisplayName = "Linear"),

	/** Ease In (Quadratic) - Slow start, fast end */
	EaseIn UMETA(DisplayName = "Ease In"),

	/** Ease Out (Quadratic) - Fast start, slow end */
	EaseOut UMETA(DisplayName = "Ease Out"),

	/** Ease In-Out (Quadratic) - Slow start and end, fast middle */
	EaseInOut UMETA(DisplayName = "Ease In-Out"),

	/** Cubic Ease In - Very slow start */
	CubicIn UMETA(DisplayName = "Cubic In"),

	/** Cubic Ease Out - Very slow end */
	CubicOut UMETA(DisplayName = "Cubic Out"),

	/** Cubic Ease In-Out - Very slow start and end */
	CubicInOut UMETA(DisplayName = "Cubic In-Out"),

	/** Quart Ease In - Extremely slow start */
	QuartIn UMETA(DisplayName = "Quart In"),

	/** Quart Ease Out - Extremely slow end */
	QuartOut UMETA(DisplayName = "Quart Out"),

	/** Quart Ease In-Out - Extremely slow start and end */
	QuartInOut UMETA(DisplayName = "Quart In-Out"),

	/** Sine Ease In - Smooth circular start */
	SineIn UMETA(DisplayName = "Sine In"),

	/** Sine Ease Out - Smooth circular end */
	SineOut UMETA(DisplayName = "Sine Out"),

	/** Sine Ease In-Out - Smooth circular start and end */
	SineInOut UMETA(DisplayName = "Sine In-Out"),

	/** Exponential Ease In - Explosive acceleration */
	ExpoIn UMETA(DisplayName = "Expo In"),

	/** Exponential Ease Out - Explosive deceleration */
	ExpoOut UMETA(DisplayName = "Expo Out"),

	/** Exponential Ease In-Out */
	ExpoInOut UMETA(DisplayName = "Expo In-Out"),

	/** Circular Ease In */
	CircIn UMETA(DisplayName = "Circ In"),

	/** Circular Ease Out */
	CircOut UMETA(DisplayName = "Circ Out"),

	/** Circular Ease In-Out */
	CircInOut UMETA(DisplayName = "Circ In-Out"),

	/** Elastic Ease In - Bounces back before moving forward */
	ElasticIn UMETA(DisplayName = "Elastic In"),

	/** Elastic Ease Out - Overshoots then bounces back */
	ElasticOut UMETA(DisplayName = "Elastic Out"),

	/** Elastic Ease In-Out - Combination of elastic in and out */
	ElasticInOut UMETA(DisplayName = "Elastic In-Out"),

	/** Bounce Ease In - Bounces at start */
	BounceIn UMETA(DisplayName = "Bounce In"),

	/** Bounce Ease Out - Bounces at end */
	BounceOut UMETA(DisplayName = "Bounce Out"),

	/** Bounce Ease In-Out - Bounces at start and end */
	BounceInOut UMETA(DisplayName = "Bounce In-Out"),

	/** Back Ease In - Backs up slightly before moving forward */
	BackIn UMETA(DisplayName = "Back In"),

	/** Back Ease Out - Overshoots then returns */
	BackOut UMETA(DisplayName = "Back Out"),

	/** Back Ease In-Out - Combination of back in and out */
	BackInOut UMETA(DisplayName = "Back In-Out")
};

/**
 * Easing Function Library
 *
 * Provides mathematical easing functions for smooth animations.
 * All functions take a normalized time value (0.0-1.0) and return
 * a normalized result (0.0-1.0, though some may overshoot).
 *
 * Usage:
 * @code
 * float Alpha = 0.5f; // Halfway through animation
 * float EasedAlpha = UProjectEasingFunctions::Ease(Alpha, EProjectEasingType::EaseInOut);
 * FVector CurrentPos = FMath::Lerp(StartPos, EndPos, EasedAlpha);
 * @endcode
 *
 * References:
 * - Robert Penner's Easing Functions (http://robertpenner.com/easing/)
 * - CSS Easing Functions (https://easings.net/)
 */
UCLASS()
class PROJECTUI_API UProjectEasingFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Apply easing curve to normalized time value.
	 *
	 * @param Time - Normalized time (0.0 = start, 1.0 = end)
	 * @param EasingType - Type of easing curve to apply
	 * @return Eased value (typically 0.0-1.0, but may overshoot for elastic/back curves)
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float Ease(float Time, EProjectEasingType EasingType);

	// Individual easing functions (for direct use)

	/** Linear: No easing, constant speed */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float Linear(float Time) { return Time; }

	/** Quadratic Ease In */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float EaseIn(float Time) { return Time * Time; }

	/** Quadratic Ease Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float EaseOut(float Time) { return Time * (2.0f - Time); }

	/** Quadratic Ease In-Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float EaseInOut(float Time);

	/** Cubic Ease In */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float CubicIn(float Time) { return Time * Time * Time; }

	/** Cubic Ease Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float CubicOut(float Time);

	/** Cubic Ease In-Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float CubicInOut(float Time);

	/** Quartic Ease In */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float QuartIn(float Time) { return Time * Time * Time * Time; }

	/** Quartic Ease Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float QuartOut(float Time);

	/** Quartic Ease In-Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float QuartInOut(float Time);

	/** Sine Ease In */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float SineIn(float Time);

	/** Sine Ease Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float SineOut(float Time);

	/** Sine Ease In-Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float SineInOut(float Time);

	/** Exponential Ease In */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float ExpoIn(float Time);

	/** Exponential Ease Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float ExpoOut(float Time);

	/** Exponential Ease In-Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float ExpoInOut(float Time);

	/** Circular Ease In */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float CircIn(float Time);

	/** Circular Ease Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float CircOut(float Time);

	/** Circular Ease In-Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float CircInOut(float Time);

	/** Elastic Ease In */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float ElasticIn(float Time);

	/** Elastic Ease Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float ElasticOut(float Time);

	/** Elastic Ease In-Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float ElasticInOut(float Time);

	/** Bounce Ease In */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float BounceIn(float Time);

	/** Bounce Ease Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float BounceOut(float Time);

	/** Bounce Ease In-Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float BounceInOut(float Time);

	/** Back Ease In */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float BackIn(float Time);

	/** Back Ease Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float BackOut(float Time);

	/** Back Ease In-Out */
	UFUNCTION(BlueprintPure, Category = "Project UI|Animation|Easing")
	static float BackInOut(float Time);
};
