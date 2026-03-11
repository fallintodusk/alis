// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Theme/ProjectUIThemeData.h"
#include "ProjectUIThemeManager.generated.h"

/**
 * Delegate fired when the active theme changes
 * UI widgets bind to this to refresh their styling
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThemeChanged, UProjectUIThemeData*, NewTheme);

/**
 * Delegate for async theme loading completion
 * Called when async theme load finishes (success or failure)
 */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnThemeLoadComplete, bool, bSuccess, UProjectUIThemeData*, LoadedTheme);

/**
 * Theme Manager Subsystem
 *
 * Manages the active UI theme and provides access to theme data.
 * Singleton subsystem accessible from any game code.
 *
 * Responsibilities:
 * - Load and cache available themes
 * - Track the active theme
 * - Notify widgets when theme changes
 * - Provide convenient accessors for theme properties
 *
 * Usage Example:
 * @code
 * // Get active theme
 * UProjectUIThemeManager* ThemeManager = GetGameInstance()->GetSubsystem<UProjectUIThemeManager>();
 * UProjectUIThemeData* Theme = ThemeManager->GetActiveTheme();
 *
 * // Apply theme color to widget
 * MyButton->SetColorAndOpacity(Theme->Colors.Primary);
 *
 * // Listen for theme changes
 * ThemeManager->OnThemeChanged.AddDynamic(this, &UMyWidget::HandleThemeChanged);
 * @endcode
 */
UCLASS()
class PROJECTUI_API UProjectUIThemeManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Subsystem lifecycle */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Get the currently active theme
	 * Always returns a valid theme (falls back to default if none set)
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Theme")
	UProjectUIThemeData* GetActiveTheme() const;

	/**
	 * Set the active theme
	 * Broadcasts OnThemeChanged delegate to notify UI widgets
	 *
	 * @param NewTheme - Theme to activate (must not be null)
	 * @return true if theme was changed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Theme")
	bool SetActiveTheme(UProjectUIThemeData* NewTheme);

	/**
	 * Load a theme by asset path (synchronous)
	 * Useful for loading themes dynamically at runtime
	 * Note: This blocks the game thread until loading completes
	 *
	 * @param ThemePath - Soft object path to theme asset (e.g., "/Game/UI/Themes/DarkTheme.DarkTheme")
	 * @return true if theme was loaded and set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Theme")
	bool LoadAndSetTheme(const FSoftObjectPath& ThemePath);

	/**
	 * Load a theme by asset path (asynchronous)
	 * Loads theme in background without blocking the game thread
	 * Recommended for large themes with many textures/materials
	 *
	 * @param ThemePath - Soft object path to theme asset
	 * @param OnComplete - Delegate called when loading finishes (success or failure)
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Theme")
	void LoadAndSetThemeAsync(const FSoftObjectPath& ThemePath, FOnThemeLoadComplete OnComplete);

	/**
	 * Get all available themes
	 * Queries Asset Manager for all UProjectUIThemeData assets
	 *
	 * @param OutThemes - Array to populate with available themes
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Theme")
	void GetAvailableThemes(TArray<UProjectUIThemeData*>& OutThemes);

	/** Delegate broadcast when active theme changes */
	UPROPERTY(BlueprintAssignable, Category = "Project UI|Theme")
	FOnThemeChanged OnThemeChanged;

	// Convenience accessors for common theme properties

	UFUNCTION(BlueprintPure, Category = "Project UI|Theme")
	FProjectUIColorPalette GetColors() const;

	UFUNCTION(BlueprintPure, Category = "Project UI|Theme")
	FProjectUITypography GetTypography() const;

	UFUNCTION(BlueprintPure, Category = "Project UI|Theme")
	FProjectUISpacing GetSpacing() const;

	UFUNCTION(BlueprintPure, Category = "Project UI|Theme")
	FProjectUIAnimationSettings GetAnimationSettings() const;

protected:
	/**
	 * Load the default theme from project settings
	 * Called during initialization if no theme is set
	 */
	void LoadDefaultTheme();

private:
	/** Currently active theme */
	UPROPERTY()
	TObjectPtr<UProjectUIThemeData> ActiveTheme;

	/** Default fallback theme (embedded in code, always available) */
	UPROPERTY()
	TObjectPtr<UProjectUIThemeData> DefaultTheme;

	/** Create a default theme with reasonable fallback values */
	void CreateDefaultTheme();
};
