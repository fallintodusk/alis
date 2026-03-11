// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectToastSubsystem.generated.h"

class UW_ToastNotification;
class UPanelWidget;

/**
 * Toast message data - queued for display.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FToastMessage
{
	GENERATED_BODY()

	/** Message text to display. */
	UPROPERTY(BlueprintReadWrite, Category = "Toast")
	FText Message;

	/** Display duration in seconds (after fade-in completes). */
	UPROPERTY(BlueprintReadWrite, Category = "Toast")
	float Duration = 3.0f;

	/** Toast type for styling (Info, Warning, Error). */
	UPROPERTY(BlueprintReadWrite, Category = "Toast")
	FName Type = NAME_None;

	FToastMessage() = default;
	FToastMessage(const FText& InMessage, float InDuration = 3.0f, FName InType = NAME_None)
		: Message(InMessage), Duration(InDuration), Type(InType) {}
};

/**
 * Toast Notification Subsystem - manages toast message queue and display.
 *
 * Features:
 * - Queue-based display (one toast at a time)
 * - Configurable duration per toast
 * - Fade in/out animations
 * - Type-based styling (Info, Warning, Error)
 *
 * Usage:
 * @code
 * UProjectToastSubsystem* ToastSub = GetGameInstance()->GetSubsystem<UProjectToastSubsystem>();
 * ToastSub->ShowToast(LOCTEXT("WeightError", "Too heavy - cannot carry more"), 3.0f, "Error");
 * @endcode
 */
UCLASS()
class PROJECTUI_API UProjectToastSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Show a toast notification.
	 * If another toast is showing, this one will be queued.
	 *
	 * @param Message - Text to display
	 * @param Duration - How long to show (seconds, after fade-in)
	 * @param Type - Toast type for styling ("Info", "Warning", "Error")
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Toast")
	void ShowToast(const FText& Message, float Duration = 3.0f, FName Type = NAME_None);

	/**
	 * Show a toast notification with full struct.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Toast")
	void ShowToastMessage(const FToastMessage& ToastMessage);

	/**
	 * Clear all pending toasts (does not affect currently showing toast).
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Toast")
	void ClearQueue();

	/**
	 * Dismiss the current toast immediately (starts fade-out).
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Toast")
	void DismissCurrent();

	/**
	 * Set the toast container widget.
	 * Called by HUD layout when it initializes.
	 */
	void SetToastContainer(UPanelWidget* InContainer, APlayerController* OwningPlayer);

	/**
	 * Get the toast widget (for debugging).
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Toast")
	UW_ToastNotification* GetToastWidget() const { return ToastWidget; }

private:
	/** Queue of pending toast messages. */
	TArray<FToastMessage> ToastQueue;

	/** Current toast widget instance. */
	UPROPERTY()
	TObjectPtr<UW_ToastNotification> ToastWidget;

	/** Container for toast widget. */
	TWeakObjectPtr<UPanelWidget> ToastContainer;

	/** Timer handle for auto-dismiss. */
	FTimerHandle DismissTimerHandle;

	/** Process the next toast in queue. */
	void ProcessNextToast();

	/** Called when current toast finishes fading out. */
	void OnToastDismissed();

	/** Called when dismiss timer expires. */
	void OnDismissTimerExpired();
};
