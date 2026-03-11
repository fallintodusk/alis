// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"
#include "ProjectUISettings.generated.h"

/**
 * Developer settings for ProjectUI system
 * Accessible via Project Settings → Project → Project UI
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Project UI"))
class PROJECTUI_API UProjectUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UProjectUISettings();

	/**
	 * Default theme asset to load on subsystem initialization
	 * Example: /Game/Project/UI/Themes/DA_DefaultDarkTheme
	 * Leave empty to use embedded fallback theme
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Theme", meta = (AllowedClasses = "/Script/ProjectUI.ProjectUIThemeData"))
	FSoftObjectPath DefaultThemePath;

	/**
	 * HUD layout widget class (slot host).
	 * Example: /Script/ProjectHUD.W_HUDLayout
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (AllowedClasses = "/Script/UMG.UserWidget"))
	FSoftClassPath HUDLayoutClass;

	/**
	 * Enable theme hot reload in editor
	 * Allows themes to be reloaded at runtime during development
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Theme")
	bool bEnableThemeHotReload = true;

	/**
	 * Enable verbose UI logging for debugging
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bVerboseUILogging = false;

public:
	//~UDeveloperSettings interface
#if WITH_EDITOR
	virtual FName GetCategoryName() const override;
	virtual FText GetSectionText() const override;
#endif
};
