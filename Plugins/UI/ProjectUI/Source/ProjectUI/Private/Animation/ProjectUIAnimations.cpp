// Copyright ALIS. All Rights Reserved.

#include "Animation/ProjectUIAnimations.h"
#include "Animation/ProjectEasingFunctions.h"
#include "ProjectUIThemeManager.h"
#include "Theme/ProjectUIThemeData.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"

void UProjectUIAnimations::FadeIn(UUserWidget* Widget, float Duration)
{
	FadeIn(Widget, Duration, EProjectEasingType::EaseOut, FOnAnimationComplete());
}

void UProjectUIAnimations::FadeOut(UUserWidget* Widget, float Duration)
{
	FadeOut(Widget, Duration, EProjectEasingType::EaseIn, FOnAnimationComplete());
}

void UProjectUIAnimations::SlideIn(UUserWidget* Widget, EProjectUISlideDirection Direction, float Duration, float Distance)
{
	SlideIn(Widget, Direction, Duration, Distance, FOnAnimationComplete());
}

void UProjectUIAnimations::SlideOut(UUserWidget* Widget, EProjectUISlideDirection Direction, float Duration, float Distance)
{
	SlideOut(Widget, Direction, Duration, Distance, FOnAnimationComplete());
}

void UProjectUIAnimations::ScaleIn(UUserWidget* Widget, float Duration, float StartScale)
{
	ScaleIn(Widget, Duration, StartScale, FOnAnimationComplete());
}

void UProjectUIAnimations::ScaleOut(UUserWidget* Widget, float Duration, float EndScale)
{
	ScaleOut(Widget, Duration, EndScale, FOnAnimationComplete());
}

void UProjectUIAnimations::TransitionIn(UUserWidget* Widget, EProjectUITransitionType TransitionType, EProjectUISlideDirection Direction, float Duration)
{
	TransitionIn(Widget, TransitionType, Direction, Duration, FOnAnimationComplete());
}

void UProjectUIAnimations::TransitionIn(UUserWidget* Widget, EProjectUITransitionType TransitionType, float Duration)
{
	TransitionIn(Widget, TransitionType, EProjectUISlideDirection::Bottom, Duration, FOnAnimationComplete());
}

void UProjectUIAnimations::TransitionOut(UUserWidget* Widget, EProjectUITransitionType TransitionType, EProjectUISlideDirection Direction, float Duration)
{
	TransitionOut(Widget, TransitionType, Direction, Duration, FOnAnimationComplete());
}

void UProjectUIAnimations::TransitionOut(UUserWidget* Widget, EProjectUITransitionType TransitionType, float Duration)
{
	TransitionOut(Widget, TransitionType, EProjectUISlideDirection::Bottom, Duration, FOnAnimationComplete());
}

void UProjectUIAnimations::FadeIn(UUserWidget* Widget, float Duration, EProjectEasingType Easing, FOnAnimationComplete OnComplete)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("FadeIn called with null widget"));
		return;
	}

	if (Duration <= 0.0f)
	{
		Duration = GetDefaultDuration(Widget);
	}

	AnimateRenderOpacity(Widget, 0.0f, 1.0f, Duration, Easing, OnComplete);
}

void UProjectUIAnimations::FadeOut(UUserWidget* Widget, float Duration, EProjectEasingType Easing, FOnAnimationComplete OnComplete)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("FadeOut called with null widget"));
		return;
	}

	if (Duration <= 0.0f)
	{
		Duration = GetDefaultDuration(Widget);
	}

	AnimateRenderOpacity(Widget, 1.0f, 0.0f, Duration, Easing, OnComplete);
}

void UProjectUIAnimations::SlideIn(UUserWidget* Widget, EProjectUISlideDirection Direction, float Duration, float Distance, FOnAnimationComplete OnComplete)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("SlideIn called with null widget"));
		return;
	}

	if (Duration <= 0.0f)
	{
		Duration = GetDefaultDuration(Widget);
	}

	FVector2D Offset = GetSlideOffset(Widget, Direction, Distance);

	FWidgetTransform StartTransform;
	StartTransform.Translation = Offset;

	FWidgetTransform EndTransform;
	EndTransform.Translation = FVector2D::ZeroVector;

	AnimateRenderTransform(Widget, StartTransform, EndTransform, Duration, EProjectEasingType::EaseOut, OnComplete);
}

void UProjectUIAnimations::SlideOut(UUserWidget* Widget, EProjectUISlideDirection Direction, float Duration, float Distance, FOnAnimationComplete OnComplete)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("SlideOut called with null widget"));
		return;
	}

	if (Duration <= 0.0f)
	{
		Duration = GetDefaultDuration(Widget);
	}

	FVector2D Offset = GetSlideOffset(Widget, Direction, Distance);

	FWidgetTransform StartTransform;
	StartTransform.Translation = FVector2D::ZeroVector;

	FWidgetTransform EndTransform;
	EndTransform.Translation = Offset;

	AnimateRenderTransform(Widget, StartTransform, EndTransform, Duration, EProjectEasingType::EaseIn, OnComplete);
}

void UProjectUIAnimations::ScaleIn(UUserWidget* Widget, float Duration, float StartScale, FOnAnimationComplete OnComplete)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("ScaleIn called with null widget"));
		return;
	}

	if (Duration <= 0.0f)
	{
		Duration = GetDefaultDuration(Widget);
	}

	FWidgetTransform StartTransform;
	StartTransform.Scale = FVector2D(StartScale, StartScale);

	FWidgetTransform EndTransform;
	EndTransform.Scale = FVector2D(1.0f, 1.0f);

	AnimateRenderTransform(Widget, StartTransform, EndTransform, Duration, EProjectEasingType::BackOut, OnComplete);
}

void UProjectUIAnimations::ScaleOut(UUserWidget* Widget, float Duration, float EndScale, FOnAnimationComplete OnComplete)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("ScaleOut called with null widget"));
		return;
	}

	if (Duration <= 0.0f)
	{
		Duration = GetDefaultDuration(Widget);
	}

	FWidgetTransform StartTransform;
	StartTransform.Scale = FVector2D(1.0f, 1.0f);

	FWidgetTransform EndTransform;
	EndTransform.Scale = FVector2D(EndScale, EndScale);

	AnimateRenderTransform(Widget, StartTransform, EndTransform, Duration, EProjectEasingType::BackIn, OnComplete);
}

void UProjectUIAnimations::TransitionIn(UUserWidget* Widget, EProjectUITransitionType TransitionType, EProjectUISlideDirection Direction, float Duration, FOnAnimationComplete OnComplete)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransitionIn called with null widget"));
		return;
	}

	if (Duration <= 0.0f)
	{
		Duration = GetDefaultDuration(Widget);
	}

	switch (TransitionType)
	{
	case EProjectUITransitionType::Fade:
		FadeIn(Widget, Duration, EProjectEasingType::EaseOut, OnComplete);
		break;

	case EProjectUITransitionType::Slide:
		SlideIn(Widget, Direction, Duration, 0.0f, OnComplete);
		break;

	case EProjectUITransitionType::Scale:
		ScaleIn(Widget, Duration, 0.5f, OnComplete);
		break;

	case EProjectUITransitionType::FadeScale:
		// Combine fade and scale: set initial opacity to 0, then animate both
		Widget->SetRenderOpacity(0.0f);
		FadeIn(Widget, Duration, EProjectEasingType::EaseOut, FOnAnimationComplete());
		ScaleIn(Widget, Duration, 0.8f, OnComplete); // Subtle scale (80% to 100%)
		break;

	case EProjectUITransitionType::FadeSlide:
		// Combine fade and slide
		Widget->SetRenderOpacity(0.0f);
		FadeIn(Widget, Duration, EProjectEasingType::EaseOut, FOnAnimationComplete());
		SlideIn(Widget, Direction, Duration, 0.0f, OnComplete);
		break;
	}
}

void UProjectUIAnimations::TransitionOut(UUserWidget* Widget, EProjectUITransitionType TransitionType, EProjectUISlideDirection Direction, float Duration, FOnAnimationComplete OnComplete)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransitionOut called with null widget"));
		return;
	}

	if (Duration <= 0.0f)
	{
		Duration = GetDefaultDuration(Widget);
	}

	switch (TransitionType)
	{
	case EProjectUITransitionType::Fade:
		FadeOut(Widget, Duration, EProjectEasingType::EaseIn, OnComplete);
		break;

	case EProjectUITransitionType::Slide:
		SlideOut(Widget, Direction, Duration, 0.0f, OnComplete);
		break;

	case EProjectUITransitionType::Scale:
		ScaleOut(Widget, Duration, 0.5f, OnComplete);
		break;

	case EProjectUITransitionType::FadeScale:
		// Combine fade and scale
		FadeOut(Widget, Duration, EProjectEasingType::EaseIn, OnComplete);
		ScaleOut(Widget, Duration, 0.8f, FOnAnimationComplete()); // Subtle scale
		break;

	case EProjectUITransitionType::FadeSlide:
		// Combine fade and slide
		FadeOut(Widget, Duration, EProjectEasingType::EaseIn, OnComplete);
		SlideOut(Widget, Direction, Duration, 0.0f, FOnAnimationComplete());
		break;
	}
}

float UProjectUIAnimations::GetDefaultDuration(UUserWidget* Widget)
{
	if (!Widget)
	{
		return 0.25f; // Fallback default
	}

	UGameInstance* GameInstance = Widget->GetGameInstance();
	if (!GameInstance)
	{
		return 0.25f;
	}

	UProjectUIThemeManager* ThemeManager = GameInstance->GetSubsystem<UProjectUIThemeManager>();
	if (!ThemeManager)
	{
		return 0.25f;
	}

	UProjectUIThemeData* Theme = ThemeManager->GetActiveTheme();
	if (!Theme)
	{
		return 0.25f;
	}

	return Theme->Animation.Normal;
}

FVector2D UProjectUIAnimations::GetSlideOffset(UUserWidget* Widget, EProjectUISlideDirection Direction, float Distance)
{
	FVector2D Offset = FVector2D::ZeroVector;

	// If distance not specified, use viewport dimensions
	if (Distance <= 0.0f)
	{
		FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(Widget);
		Distance = FMath::Max(ViewportSize.X, ViewportSize.Y); // Use larger dimension
	}

	switch (Direction)
	{
	case EProjectUISlideDirection::Left:
		Offset.X = -Distance;
		break;

	case EProjectUISlideDirection::Right:
		Offset.X = Distance;
		break;

	case EProjectUISlideDirection::Top:
		Offset.Y = -Distance;
		break;

	case EProjectUISlideDirection::Bottom:
		Offset.Y = Distance;
		break;
	}

	return Offset;
}

void UProjectUIAnimations::AnimateRenderTransform(UUserWidget* Widget, const FWidgetTransform& StartTransform, const FWidgetTransform& EndTransform, float Duration, EProjectEasingType Easing, FOnAnimationComplete OnComplete)
{
	if (!Widget || Duration <= 0.0f)
	{
		if (Widget)
		{
			Widget->SetRenderTransform(EndTransform);
		}
		OnComplete.ExecuteIfBound();
		return;
	}

	// Set initial transform
	Widget->SetRenderTransform(StartTransform);

	// Animate using timer with easing curve
	float ElapsedTime = 0.0f;
	const float TickInterval = 1.0f / 60.0f; // 60 FPS

	FTimerHandle TimerHandle;
	FTimerDelegate TimerDelegate;

	// Capture variables for lambda
	TimerDelegate.BindLambda([Widget, StartTransform, EndTransform, Duration, Easing, OnComplete, TickInterval, ElapsedTime, &TimerHandle]() mutable
	{
		ElapsedTime += TickInterval;
		float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);

		// Apply easing curve
		float EasedAlpha = UProjectEasingFunctions::Ease(Alpha, Easing);

		// Interpolate with eased alpha
		FWidgetTransform CurrentTransform;
		CurrentTransform.Translation = FMath::Lerp(StartTransform.Translation, EndTransform.Translation, EasedAlpha);
		CurrentTransform.Scale = FMath::Lerp(StartTransform.Scale, EndTransform.Scale, EasedAlpha);
		CurrentTransform.Shear = FMath::Lerp(StartTransform.Shear, EndTransform.Shear, EasedAlpha);
		CurrentTransform.Angle = FMath::Lerp(StartTransform.Angle, EndTransform.Angle, EasedAlpha);

		if (Widget)
		{
			Widget->SetRenderTransform(CurrentTransform);
		}

		// Check if animation complete
		if (Alpha >= 1.0f)
		{
			if (UWorld* World = Widget->GetWorld())
			{
				World->GetTimerManager().ClearTimer(TimerHandle);
			}
			OnComplete.ExecuteIfBound();
		}
	});

	if (UWorld* World = Widget->GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, TickInterval, true);
	}
}

void UProjectUIAnimations::AnimateRenderOpacity(UUserWidget* Widget, float StartOpacity, float EndOpacity, float Duration, EProjectEasingType Easing, FOnAnimationComplete OnComplete)
{
	if (!Widget || Duration <= 0.0f)
	{
		if (Widget)
		{
			Widget->SetRenderOpacity(EndOpacity);
		}
		OnComplete.ExecuteIfBound();
		return;
	}

	// Set initial opacity
	Widget->SetRenderOpacity(StartOpacity);

	// Animate using timer with easing curve
	float ElapsedTime = 0.0f;
	const float TickInterval = 1.0f / 60.0f; // 60 FPS

	FTimerHandle TimerHandle;
	FTimerDelegate TimerDelegate;

	TimerDelegate.BindLambda([Widget, StartOpacity, EndOpacity, Duration, Easing, OnComplete, TickInterval, ElapsedTime, &TimerHandle]() mutable
	{
		ElapsedTime += TickInterval;
		float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);

		// Apply easing curve
		float EasedAlpha = UProjectEasingFunctions::Ease(Alpha, Easing);

		float CurrentOpacity = FMath::Lerp(StartOpacity, EndOpacity, EasedAlpha);

		if (Widget)
		{
			Widget->SetRenderOpacity(CurrentOpacity);
		}

		// Check if animation complete
		if (Alpha >= 1.0f)
		{
			if (UWorld* World = Widget->GetWorld())
			{
				World->GetTimerManager().ClearTimer(TimerHandle);
			}
			OnComplete.ExecuteIfBound();
		}
	});

	if (UWorld* World = Widget->GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, TickInterval, true);
	}
}
