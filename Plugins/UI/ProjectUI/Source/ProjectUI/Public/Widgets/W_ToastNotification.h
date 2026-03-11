// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "W_ToastNotification.generated.h"

class UTextBlock;
class UBorder;

DECLARE_DELEGATE(FOnToastDismissed);

/**
 * Toast Notification Widget - displays a temporary message with fade in/out.
 *
 * Features:
 * - Fade in/out animations
 * - Type-based styling (Info, Warning, Error)
 * - Auto-layout from JSON config
 *
 * Layout structure (ToastLayout.json):
 * - Border (background)
 *   - TextBlock "MessageText"
 */
UCLASS()
class PROJECTUI_API UW_ToastNotification : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_ToastNotification(const FObjectInitializer& ObjectInitializer);

	/**
	 * Show the toast with a message.
	 *
	 * @param Message - Text to display
	 * @param Type - Toast type for styling ("Info", "Warning", "Error")
	 */
	UFUNCTION(BlueprintCallable, Category = "Toast")
	void Show(const FText& Message, FName Type = NAME_None);

	/**
	 * Dismiss the toast (starts fade-out).
	 */
	UFUNCTION(BlueprintCallable, Category = "Toast")
	void Dismiss();

	/**
	 * Check if currently showing (visible and not fading out).
	 */
	UFUNCTION(BlueprintPure, Category = "Toast")
	bool IsShowing() const { return bIsShowing; }

	/** Called when toast finishes fading out. */
	FOnToastDismissed OnDismissed;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Message text widget (found by name). */
	TWeakObjectPtr<UTextBlock> MessageText;

	/** Background border (found by name). */
	TWeakObjectPtr<UBorder> Background;

	/** Animation state. */
	enum class EToastState : uint8
	{
		Hidden,
		FadingIn,
		Showing,
		FadingOut
	};

	EToastState CurrentState = EToastState::Hidden;
	bool bIsShowing = false;

	/** Current opacity (0-1). */
	float CurrentOpacity = 0.f;

	/** Animation speeds. */
	static constexpr float FadeInSpeed = 4.0f;
	static constexpr float FadeOutSpeed = 3.0f;

	/** Current toast type for styling. */
	FName CurrentType;

	/** Apply visual styling based on type. */
	void ApplyTypeStyle(FName Type);

	/** Update opacity on all elements. */
	void UpdateOpacity(float Opacity);
};
