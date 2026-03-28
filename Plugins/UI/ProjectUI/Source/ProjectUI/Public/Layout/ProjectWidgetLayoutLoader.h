// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Dom/JsonObject.h"
#include "ProjectWidgetLayoutLoader.generated.h"

class UWidget;
class UUserWidget;
class UPanelWidget;
class UProjectUIThemeData;

/**
 * Utility class for loading UMG widget layouts from JSON configuration files.
 *
 * Architecture:
 * - Registry pattern for widget type creation (Open/Closed principle)
 * - Namespace helpers for theme resolution (single responsibility)
 * - Recursive children processing with logging for observability
 *
 * Usage:
 *   FString ConfigPath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectMenuMain"), TEXT("MainMenu.json"));
 *   UWidget* RootWidget = UProjectWidgetLayoutLoader::LoadLayoutFromFile(this, ConfigPath, ThemeData);
 */
UCLASS()
class PROJECTUI_API UProjectWidgetLayoutLoader : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Load a widget layout from a JSON file.
	 *
	 * @param Outer - Outer object for widget creation (typically the owning UserWidget)
	 * @param JsonFilePath - Absolute path to JSON config file
	 * @param Theme - Theme data for resolving color/font references (optional)
	 * @return Root widget created from JSON, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Layout Loader")
	static UWidget* LoadLayoutFromFile(UObject* Outer, const FString& JsonFilePath, UProjectUIThemeData* Theme = nullptr);

	/**
	 * Load a widget layout from a JSON string.
	 *
	 * @param Outer - Outer object for widget creation
	 * @param JsonString - JSON string containing layout definition
	 * @param Theme - Theme data for resolving color/font references (optional)
	 * @return Root widget created from JSON, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Layout Loader")
	static UWidget* LoadLayoutFromString(UObject* Outer, const FString& JsonString, UProjectUIThemeData* Theme = nullptr);

	/**
	 * Create a widget from a JSON object (recursive, handles children).
	 *
	 * @param Outer - Outer object for widget creation
	 * @param JsonObject - Parsed JSON object defining the widget
	 * @param Theme - Theme data for resolving references
	 * @return Created widget, or nullptr on failure
	 */
	static UWidget* CreateWidgetFromJson(UObject* Outer, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);

	/**
	 * Apply JSON properties to an existing widget.
	 *
	 * @param Widget - Widget to configure
	 * @param JsonObject - JSON object with property values
	 * @param Theme - Theme data for resolving references
	 */
	static void ApplyWidgetProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);

	/**
	 * Get the absolute path to a UI config file.
	 * Redirects to ProjectUI plugin data for legacy compatibility.
	 *
	 * @param RelativePath - Relative path from Plugins/UI/ProjectUI/Content/Data/ (e.g., "MainMenu.json")
	 * @return Absolute path to the config file
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Layout Loader")
	static FString GetUIConfigPath(const FString& RelativePath);

	/**
	 * Get the absolute path to a plugin UI config file.
	 *
	 * @param PluginName - Plugin name (e.g., "ProjectMenuMain")
	 * @param RelativePath - Relative path from Plugins/<Category>/<PluginName>/Content/Data/ (e.g., "MainMenu.json")
	 * @return Absolute path to the config file
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Layout Loader")
	static FString GetPluginUIConfigPath(const FString& PluginName, const FString& RelativePath);

	/**
	 * Resolve a theme color by name.
	 * Supported: Primary, Secondary, Background, Surface, Error, Warning, Success, Text, TextSecondary, Border
	 */
	static FLinearColor ResolveThemeColor(const FString& ColorName, UProjectUIThemeData* Theme);

	/**
	 * Resolve a theme font by name.
	 * Supported: HeadingLarge, HeadingMedium, HeadingSmall, BodyLarge, BodyMedium, BodySmall, Button, Label
	 */
	static FSlateFontInfo ResolveThemeFont(const FString& FontName, UProjectUIThemeData* Theme);

	/**
	 * Apply button variant styling at runtime.
	 * Use to update button appearance based on selection state.
	 * Supported variants: Primary, Secondary, Success, Warning, Error, Text
	 */
	static void ApplyButtonVariantStyle(class UButton* Button, const FString& Variant, UProjectUIThemeData* Theme);

private:
	// Children processing
	static void ProcessChildren(UObject* Outer, UWidget* Widget, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);

	// Slot properties
	static void ApplyCanvasPanelSlot(UWidget* Widget, UPanelWidget* Parent, const TSharedPtr<FJsonObject>& JsonObject);

	// Widget-specific property appliers
	static void ApplyButtonProperties(class UButton* Button, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
	static void ApplyTextBlockProperties(class UTextBlock* TextBlock, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
	static void ApplyImageProperties(class UImage* Image, const TSharedPtr<FJsonObject>& JsonObject);
	static void ApplyProgressBarProperties(class UProgressBar* ProgressBar, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
	static void ApplyRadialProgressProperties(class UProjectRadialProgress* RadialProgress, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);

	// Parsing helpers
	static FVector2D ParseVector2D(const TSharedPtr<FJsonObject>& JsonObject);
	static FLinearColor ParseColor(const TSharedPtr<FJsonObject>& JsonObject);
	static class UTexture2D* LoadTexture(const FString& TexturePath);
};
