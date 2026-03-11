// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ProjectSettingsViewModel.generated.h"

/**
 * ViewModel for settings management
 * Manages game settings data for W_Settings widget
 */
UCLASS(BlueprintType)
class PROJECTMENUMAIN_API UProjectSettingsViewModel : public UObject
{
	GENERATED_BODY()

public:
	UProjectSettingsViewModel();

	// Stub: Settings access
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void LoadSettings();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SaveSettings();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ApplySettings();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ResetToCurrentSettings();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ResetToDefaults();

protected:
	// Stub: Settings categories
	UPROPERTY(BlueprintReadOnly, Category = "Settings")
	TMap<FString, FString> SettingsData;
};
