// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/ProjectEasingFunctions.h"
#include "ProjectUIAnimations.generated.h"

/**
 * Animation transition types
 */
UENUM(BlueprintType)
enum class EProjectUITransitionType : uint8
{
	/** Fade in/out (opacity 0 to 1) */
	Fade UMETA(DisplayName = "Fade"),

	/** Slide from direction */
	Slide UMETA(DisplayName = "Slide"),

	/** Scale up/down */
	Scale UMETA(DisplayName = "Scale"),

	/** Combination: Fade + Scale */
	FadeScale UMETA(DisplayName = "Fade + Scale"),

	/** Combination: Fade + Slide */
	FadeSlide UMETA(DisplayName = "Fade + Slide")
};

/**
 * Slide direction for slide animations
 */
UENUM(BlueprintType)
enum class EProjectUISlideDirection : uint8
{
	Left UMETA(DisplayName = "From Left"),
	Right UMETA(DisplayName = "From Right"),
	Top UMETA(DisplayName = "From Top"),
	Bottom UMETA(DisplayName = "From Bottom")
};

/**
 * Delegate fired when animation completes
 */
DECLARE_DYNAMIC_DELEGATE(FOnAnimationComplete);

/**
 * Static animation helper functions
 *
 * Provides procedural animation utilities for common UI transitions.
 * These are C++ functions that can be called from any widget without
 * requiring UMG animation assets.
 *
 * Design Goals:
 * - Agent-friendly: Pure C++ animations, no UMG assets required
 * - Reusable: Common animation patterns as functions
 * - Theme-integrated: Uses theme animation settings for timing
 * - Flexible: Can be combined with UMG animations
 *
 * Usage Example:
 * @code
 * // Fade in a widget
 * UProjectUIAnimations::FadeIn(MyWidget, 0.3f, FOnAnimationComplete::CreateLambda([]() {
 *     UE_LOG(LogTemp, Log, TEXT("Fade complete"));
 * }));
 *
 * // Slide in from left
 * UProjectUIAnimations::SlideIn(MyWidget, EProjectUISlideDirection::Left, 0.25f);
 * @endcode
 */
UCLASS()
class PROJECTUI_API UProjectUIAnimations : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Fade in a widget from transparent to opaque
	 *
	 * @param Widget - Widget to animate
	 * @param Duration - Animation duration in seconds (0 = use theme default)
	 * @param Easing - Easing curve to apply (default: EaseOut for smooth deceleration)
	 * @param OnComplete - Optional callback when animation finishes
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Animation", meta = (AdvancedDisplay = "Easing,OnComplete", DefaultValues = "Duration=0.0,Easing=EaseOut"))
	static void FadeIn(UUserWidget* Widget, float Duration, EProjectEasingType Easing, FOnAnimationComplete OnComplete);

	/** Convenience overload without completion callback. */
	static void FadeIn(UUserWidget* Widget, float Duration = 0.0f);

	/**
	 * Fade out a widget from opaque to transparent
	 *
	 * @param Widget - Widget to animate
	 * @param Duration - Animation duration in seconds (0 = use theme default)
	 * @param Easing - Easing curve to apply (default: EaseIn for smooth acceleration)
	 * @param OnComplete - Optional callback when animation finishes
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Animation", meta = (AdvancedDisplay = "Easing,OnComplete", DefaultValues = "Duration=0.0,Easing=EaseIn"))
	static void FadeOut(UUserWidget* Widget, float Duration, EProjectEasingType Easing, FOnAnimationComplete OnComplete);

	/** Convenience overload without completion callback. */
	static void FadeOut(UUserWidget* Widget, float Duration = 0.0f);

	/**
	 * Slide in a widget from a direction
	 *
	 * @param Widget - Widget to animate
	 * @param Direction - Direction to slide from
	 * @param Duration - Animation duration in seconds (0 = use theme default)
	 * @param Distance - Distance to slide in pixels (0 = auto-calculate from viewport)
	 * @param OnComplete - Optional callback when animation finishes
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Animation", meta = (AdvancedDisplay = "OnComplete", DefaultValues = "Duration=0.0,Distance=0.0"))
	static void SlideIn(UUserWidget* Widget, EProjectUISlideDirection Direction, float Duration, float Distance, FOnAnimationComplete OnComplete);

	/** Convenience overload without completion callback. */
	static void SlideIn(UUserWidget* Widget, EProjectUISlideDirection Direction, float Duration = 0.0f, float Distance = 0.0f);

	/**
	 * Slide out a widget in a direction
	 *
	 * @param Widget - Widget to animate
	 * @param Direction - Direction to slide towards
	 * @param Duration - Animation duration in seconds (0 = use theme default)
	 * @param Distance - Distance to slide in pixels (0 = auto-calculate from viewport)
	 * @param OnComplete - Optional callback when animation finishes
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Animation", meta = (AdvancedDisplay = "OnComplete", DefaultValues = "Duration=0.0,Distance=0.0"))
	static void SlideOut(UUserWidget* Widget, EProjectUISlideDirection Direction, float Duration, float Distance, FOnAnimationComplete OnComplete);

	/** Convenience overload without completion callback. */
	static void SlideOut(UUserWidget* Widget, EProjectUISlideDirection Direction, float Duration = 0.0f, float Distance = 0.0f);

	/**
	 * Scale in a widget from small to normal size
	 *
	 * @param Widget - Widget to animate
	 * @param Duration - Animation duration in seconds (0 = use theme default)
	 * @param StartScale - Starting scale (default 0.5 = 50% size)
	 * @param OnComplete - Optional callback when animation finishes
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Animation", meta = (AdvancedDisplay = "OnComplete", DefaultValues = "Duration=0.0,StartScale=0.5"))
	static void ScaleIn(UUserWidget* Widget, float Duration, float StartScale, FOnAnimationComplete OnComplete);

	/** Convenience overload without completion callback. */
	static void ScaleIn(UUserWidget* Widget, float Duration = 0.0f, float StartScale = 0.5f);

	/**
	 * Scale out a widget from normal size to small
	 *
	 * @param Widget - Widget to animate
	 * @param Duration - Animation duration in seconds (0 = use theme default)
	 * @param EndScale - Ending scale (default 0.5 = 50% size)
	 * @param OnComplete - Optional callback when animation finishes
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Animation", meta = (AdvancedDisplay = "OnComplete", DefaultValues = "Duration=0.0,EndScale=0.5"))
	static void ScaleOut(UUserWidget* Widget, float Duration, float EndScale, FOnAnimationComplete OnComplete);

	/** Convenience overload without completion callback. */
	static void ScaleOut(UUserWidget* Widget, float Duration = 0.0f, float EndScale = 0.5f);

	/**
	 * Transition in a widget with combined effects
	 *
	 * @param Widget - Widget to animate
	 * @param TransitionType - Type of transition (Fade, Slide, Scale, etc.)
	 * @param Direction - Direction for slide transitions (ignored for other types)
	 * @param Duration - Animation duration in seconds (0 = use theme default)
	 * @param OnComplete - Optional callback when animation finishes
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Animation", meta = (AdvancedDisplay = "OnComplete", DefaultValues = "Direction=Bottom,Duration=0.0"))
	static void TransitionIn(UUserWidget* Widget, EProjectUITransitionType TransitionType, EProjectUISlideDirection Direction, float Duration, FOnAnimationComplete OnComplete);

	/** Convenience overload without completion callback. */
	static void TransitionIn(UUserWidget* Widget, EProjectUITransitionType TransitionType, EProjectUISlideDirection Direction = EProjectUISlideDirection::Bottom, float Duration = 0.0f);

	/** Convenience overload with default direction. */
	static void TransitionIn(UUserWidget* Widget, EProjectUITransitionType TransitionType, float Duration = 0.0f);

	/**
	 * Transition out a widget with combined effects
	 *
	 * @param Widget - Widget to animate
	 * @param TransitionType - Type of transition (Fade, Slide, Scale, etc.)
	 * @param Direction - Direction for slide transitions (ignored for other types)
	 * @param Duration - Animation duration in seconds (0 = use theme default)
	 * @param OnComplete - Optional callback when animation finishes
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Animation", meta = (AdvancedDisplay = "OnComplete", DefaultValues = "Direction=Bottom,Duration=0.0"))
	static void TransitionOut(UUserWidget* Widget, EProjectUITransitionType TransitionType, EProjectUISlideDirection Direction, float Duration, FOnAnimationComplete OnComplete);

	/** Convenience overload without completion callback. */
	static void TransitionOut(UUserWidget* Widget, EProjectUITransitionType TransitionType, EProjectUISlideDirection Direction = EProjectUISlideDirection::Bottom, float Duration = 0.0f);

	/** Convenience overload with default direction. */
	static void TransitionOut(UUserWidget* Widget, EProjectUITransitionType TransitionType, float Duration = 0.0f);

protected:
	/**
	 * Get default animation duration from theme
	 * Falls back to 0.25s if theme unavailable
	 */
	static float GetDefaultDuration(UUserWidget* Widget);

	/**
	 * Get slide offset vector for a direction
	 * Calculates appropriate offset based on viewport size
	 */
	static FVector2D GetSlideOffset(UUserWidget* Widget, EProjectUISlideDirection Direction, float Distance);

	/**
	 * Internal: Animate widget render transform with easing
	 */
	static void AnimateRenderTransform(UUserWidget* Widget, const FWidgetTransform& StartTransform, const FWidgetTransform& EndTransform, float Duration, EProjectEasingType Easing, FOnAnimationComplete OnComplete);

	/**
	 * Internal: Animate widget render opacity with easing
	 */
	static void AnimateRenderOpacity(UUserWidget* Widget, float StartOpacity, float EndOpacity, float Duration, EProjectEasingType Easing, FOnAnimationComplete OnComplete);
};
