// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "W_LoadingScreen.generated.h"

/**
 * Loading screen widget - loads layout from Config/UI/LoadingScreen.json
 *
 * Uses base class JSON layout loading (no duplication needed):
 * - Set ConfigFilePath in constructor
 * - Override BindCallbacks() to wire button handlers
 * - Hot reload automatic in editor
 *
 * Layout: Progress bar, phase text, status message, cancel button, error panel
 *
 * Note: This widget is managed by ULoadingScreenSubsystem which handles showing/hiding
 * based on ProjectLoadingSubsystem events. The widget itself is a passive display.
 */
UCLASS()
class PROJECTUI_API UW_LoadingScreen : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_LoadingScreen(const FObjectInitializer& ObjectInitializer);

protected:
	// UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// UProjectUserWidget interface - bind button handlers
	virtual void BindCallbacks() override;

public:
	/**
	 * Update loading progress (0.0 to 1.0).
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Loading Screen")
	void SetProgress(float InProgress);

	/**
	 * Update current loading phase text.
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Loading Screen")
	void SetPhaseText(const FText& InPhaseText);

	/**
	 * Update status message.
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Loading Screen")
	void SetStatusMessage(const FText& InStatusMessage);

	/**
	 * Show error panel with error message.
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Loading Screen")
	void ShowError(const FText& InErrorMessage);

	/**
	 * Hide error panel.
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Loading Screen")
	void HideError();

protected:
	void UpdateProgressBar();
	void UpdatePhaseText();
	void UpdateStatusMessage();

	// Cancel button callback
	UFUNCTION()
	void OnCancelClicked();

	// Diagnostic logging for debugging widget initialization
	void LogWidgetDiagnostics();

protected:
	/** Current loading progress (0.0-1.0) */
	float CurrentProgress = 0.0f;

	/** Current phase text */
	FText CurrentPhaseText;

	/** Current status message */
	FText CurrentStatusMessage;

	/** Current error message (empty if no error) */
	FText CurrentErrorMessage;

	/** Whether error panel is visible */
	bool bErrorVisible = false;
};
