// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectUserSettings.h"
#include "ProjectSettingsDelta.h"
#include "ProjectSettingsService.generated.h"

/**
 * Settings persistence and application service.
 * Single responsibility: Load, save, and apply settings to engine systems.
 * Uses FProjectSettingsDelta for efficient delta-based application.
 */
UCLASS()
class PROJECTSETTINGSUI_API UProjectSettingsService : public UObject
{
	GENERATED_BODY()

public:
	static UProjectSettingsService* Get();

	/** Load all settings from persistent storage */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	FProjectUserSettings LoadSettings();

	/** Apply delta - only touches changed settings (efficient, no CVar spam) */
	void Apply(const FProjectUserSettings& NewSettings, const FProjectSettingsDelta& Delta);

	/** Reset all settings to defaults */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	FProjectUserSettings ResetToDefaults();

	/** Get list of supported screen resolutions */
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	TArray<FString> GetSupportedResolutions();

	/** Get current screen resolution as string */
	UFUNCTION(BlueprintPure, Category = "Settings|Graphics")
	FString GetCurrentResolution();

private:
	// Load helpers
	void LoadGraphicsSettings(FProjectUserSettings& Out);
	void LoadAudioSettings(FProjectUserSettings& Out);
	void LoadGameSettings(FProjectUserSettings& Out);

	// Apply helpers - use delta flags for conditional application
	void ApplyGraphics(const FProjectUserSettings& Settings, const FProjectSettingsDelta& Delta);
	void ApplyAudio(const FProjectUserSettings& Settings, const FProjectSettingsDelta& Delta);
	void ApplyGame(const FProjectUserSettings& Settings, const FProjectSettingsDelta& Delta);

	// Config section names (for custom INI settings)
	static const TCHAR* AudioSection;
	static const TCHAR* GameSection;

	// Singleton
	static TWeakObjectPtr<UProjectSettingsService> Instance;
};
