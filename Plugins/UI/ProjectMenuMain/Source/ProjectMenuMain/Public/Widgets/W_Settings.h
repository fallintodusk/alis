// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "W_Settings.generated.h"

class UProjectSettingsViewModel;

/**
 * Settings menu widget - loads layout from Config/UI/Settings.json
 *
 * Uses base class JSON layout loading (no duplication needed):
 * - Set ConfigFilePath in constructor
 * - Override BindCallbacks() to wire button handlers
 * - Hot reload automatic in editor
 *
 * Layout: Category tabs (Graphics, Audio, Gameplay) + action buttons
 */
UCLASS()
class PROJECTMENUMAIN_API UW_Settings : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_Settings(const FObjectInitializer& ObjectInitializer);

protected:
	// UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// UProjectUserWidget interface - bind button handlers
	virtual void BindCallbacks() override;

	// Settings action callbacks
	UFUNCTION()
	void OnApplyClicked();

	UFUNCTION()
	void OnResetClicked();

	UFUNCTION()
	void OnDefaultClicked();

	UFUNCTION()
	void OnBackClicked();

	// Tab switching callbacks
	UFUNCTION()
	void OnGraphicsTabClicked();

	UFUNCTION()
	void OnAudioTabClicked();

	UFUNCTION()
	void OnGameplayTabClicked();

	/**
	 * Show specified settings category.
	 */
	void ShowCategory(const FString& InCategoryName);

protected:
	/** ViewModel for settings state (optional) */
	UPROPERTY(Transient)
	TObjectPtr<UProjectSettingsViewModel> SettingsViewModel;

	/** Current active category */
	FString CurrentCategory;
};
