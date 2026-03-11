// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectUserSettings.h"
#include "Widgets/ProjectUserWidget.h"
#include "ProjectSettingsRootWidget.generated.h"

class UButton;
class UComboBoxString;
class UCheckBox;
class USlider;

/**
 * Root widget for settings UI.
 * UI-only concerns: control caching, UI <-> FProjectUserSettings mapping, tab switching.
 * All persistence delegated to UProjectSettingsService (SOLID: Single Responsibility).
 */
UCLASS()
class PROJECTSETTINGSUI_API UProjectSettingsRootWidget : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UProjectSettingsRootWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void BindCallbacks() override;

	// Button actions
	UFUNCTION()
	void ApplySettings();

	UFUNCTION()
	void RevertSettings();

	UFUNCTION()
	void ResetDefaults();

	UFUNCTION()
	void ShowGraphics();

	UFUNCTION()
	void ShowAudio();

	UFUNCTION()
	void ShowGame();

	// Tab switching and highlighting
	void ToggleTab(const FString& TargetName);
	void UpdateTabHighlight(const FString& ActiveTabButton);

	// UI operations
	void CacheControls();
	void PopulateResolutionOptions();
	void PopulateScalabilityOptions(UComboBoxString* Combo);
	void SyncUIFromSettings(const FProjectUserSettings& Settings);

	// Cached widget pointers - Tab buttons
	UPROPERTY()
	TObjectPtr<UButton> TabGraphicsButton;

	UPROPERTY()
	TObjectPtr<UButton> TabAudioButton;

	UPROPERTY()
	TObjectPtr<UButton> TabGameButton;

	// Cached widget pointers - Display
	UPROPERTY()
	TObjectPtr<UComboBoxString> ResolutionCombo;

	UPROPERTY()
	TObjectPtr<UCheckBox> FullscreenCheckBox;

	UPROPERTY()
	TObjectPtr<UCheckBox> VSyncCheckBox;

	// Cached widget pointers - Scalability
	UPROPERTY()
	TObjectPtr<UComboBoxString> ShadowCombo;

	UPROPERTY()
	TObjectPtr<UComboBoxString> TextureCombo;

	UPROPERTY()
	TObjectPtr<UComboBoxString> EffectsCombo;

	UPROPERTY()
	TObjectPtr<UComboBoxString> ViewDistanceCombo;

	UPROPERTY()
	TObjectPtr<UComboBoxString> AntiAliasingCombo;

	// Cached widget pointers - Audio
	UPROPERTY()
	TObjectPtr<USlider> MasterVolumeSlider;

	UPROPERTY()
	TObjectPtr<USlider> MusicVolumeSlider;

	UPROPERTY()
	TObjectPtr<USlider> EffectsVolumeSlider;

	// Cached widget pointers - Game
	UPROPERTY()
	TObjectPtr<UCheckBox> InvertMouseCheckBox;

	UPROPERTY()
	TObjectPtr<UCheckBox> ShowHintsCheckBox;

	// Settings state: last saved/loaded state (for change detection)
	FProjectUserSettings SavedSettings;

	// Get current UI state as settings struct
	FProjectUserSettings GetPendingSettings() const;

	// Check if current UI state differs from saved
	bool HasPendingChanges() const;
};
