// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "Kismet/KismetMathLibrary.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "ProjectUIThemeData.generated.h"

/**
 * Color palette for UI theming
 * Defines semantic color roles for consistent styling
 */
USTRUCT(BlueprintType)
struct FProjectUIColorPalette
{
	GENERATED_BODY()

	/** Primary brand color (buttons, highlights) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Primary = FLinearColor(0.0f, 0.47f, 0.84f, 1.0f); // Blue

	/** Secondary accent color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Secondary = FLinearColor(0.29f, 0.56f, 0.89f, 1.0f); // Light Blue

	/** Success state color (confirmations, positive feedback) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Success = FLinearColor(0.3f, 0.69f, 0.31f, 1.0f); // Green

	/** Warning state color (cautions, attention needed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Warning = FLinearColor(1.0f, 0.76f, 0.03f, 1.0f); // Orange

	/** Error state color (failures, invalid input) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Error = FLinearColor(0.96f, 0.26f, 0.21f, 1.0f); // Red

	/** Background color (panels, containers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Background = FLinearColor(0.02f, 0.02f, 0.02f, 0.95f); // Dark Gray

	/** Surface color (cards, overlays) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Surface = FLinearColor(0.12f, 0.12f, 0.12f, 0.9f); // Gray

	/** Primary text color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor TextPrimary = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // White

	/** Secondary text color (descriptions, labels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor TextSecondary = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f); // Light Gray

	/** Disabled text color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor TextDisabled = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f); // Dark Gray

	/** Border/divider color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Border = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f); // Medium Gray
};

/**
 * Typography settings for UI text
 * Defines font families, sizes, and styles
 */
USTRUCT(BlueprintType)
struct FProjectUITypography
{
	GENERATED_BODY()

	/** Large heading font (titles, headers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo HeadingLarge;

	/** Medium heading font (section titles) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo HeadingMedium;

	/** Small heading font (subtitles) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo HeadingSmall;

	/** Body text font (paragraphs, descriptions) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo BodyLarge;

	/** Standard body text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo BodyMedium;

	/** Small body text (captions, footnotes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo BodySmall;

	/** Button text font */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo Button;

	/** Label text font (form labels, tags) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
	FSlateFontInfo Label;

	FProjectUITypography()
	{
		// Default font sizes (designers should override in data assets)
		HeadingLarge.Size = 32;
		HeadingMedium.Size = 24;
		HeadingSmall.Size = 18;
		BodyLarge.Size = 16;
		BodyMedium.Size = 14;
		BodySmall.Size = 12;
		Button.Size = 16;
		Label.Size = 14;
	}
};

/**
 * Spacing/layout settings for consistent UI dimensions
 */
USTRUCT(BlueprintType)
struct FProjectUISpacing
{
	GENERATED_BODY()

	/** Extra small spacing (2-4px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing")
	float ExtraSmall = 4.0f;

	/** Small spacing (8px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing")
	float Small = 8.0f;

	/** Medium spacing (16px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing")
	float Medium = 16.0f;

	/** Large spacing (24px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing")
	float Large = 24.0f;

	/** Extra large spacing (32px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing")
	float ExtraLarge = 32.0f;

	/** Border radius for rounded corners */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing")
	float BorderRadius = 4.0f;

	/** Standard border thickness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing")
	float BorderThickness = 2.0f;
};

/**
 * Gradient definition for UI backgrounds and effects
 * Supports linear gradients with multiple color stops
 */
USTRUCT(BlueprintType)
struct FProjectUIGradient
{
	GENERATED_BODY()

	/** Gradient color stops (min 2 required) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gradient")
	TArray<FLinearColor> ColorStops;

	/** Gradient angle in degrees (0 = horizontal, 90 = vertical) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gradient", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float AngleDegrees = 90.0f;

	FProjectUIGradient()
	{
		// Default: Blue to Dark Blue gradient
		ColorStops = {
			FLinearColor(0.0f, 0.47f, 0.84f, 1.0f), // Primary Blue
			FLinearColor(0.0f, 0.27f, 0.54f, 1.0f)  // Darker Blue
		};
	}
};

/**
 * Animation timing settings for UI transitions
 */
USTRUCT(BlueprintType)
struct FProjectUIAnimationSettings
{
	GENERATED_BODY()

	/** Fast animation duration (100-200ms) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float Fast = 0.15f;

	/** Normal animation duration (200-300ms) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float Normal = 0.25f;

	/** Slow animation duration (300-500ms) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float Slow = 0.4f;

	/** Easing curve for animations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TEnumAsByte<EEasingFunc::Type> EasingFunction = EEasingFunc::EaseInOut;
};

/**
 * Theme Data Asset
 *
 * Data-driven theme configuration defining all visual styling for UI.
 * Create multiple theme assets for different visual styles (e.g., Light, Dark, HighContrast).
 *
 * Usage:
 * 1. Create theme asset in editor: Right-click → Miscellaneous → Data Asset → UProjectUIThemeData
 * 2. Configure colors, fonts, spacing in editor
 * 3. Set active theme via UProjectUIThemeManager
 * 4. Widgets query active theme for styling
 *
 * Design Goals:
 * - Data-driven: All styling in data assets, no hardcoded colors/sizes
 * - Designer-friendly: Non-programmers can create themes
 * - Runtime switchable: Change themes without restarting
 * - Consistent: Single source of truth for all UI styling
 */
UCLASS(BlueprintType)
class PROJECTUI_API UProjectUIThemeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Theme display name (e.g., "Dark Theme", "Light Theme") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	FText ThemeName;

	/** Theme description */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	FText ThemeDescription;

	/** Color palette */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	FProjectUIColorPalette Colors;

	/** Typography settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	FProjectUITypography Typography;

	/** Spacing/layout settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	FProjectUISpacing Spacing;

	/** Animation settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	FProjectUIAnimationSettings Animation;

	/**
	 * Predefined gradient presets for backgrounds, buttons, panels
	 * Designers can reference these by name in UI configs
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Gradients")
	TMap<FName, FProjectUIGradient> GradientPresets;

	/**
	 * Get a gradient preset by name
	 * @param PresetName - Name of the gradient preset (e.g., "Primary", "Background")
	 * @param OutGradient - The gradient data if found
	 * @return true if gradient was found
	 */
	UFUNCTION(BlueprintPure, Category = "Theme")
	bool GetGradientPreset(FName PresetName, FProjectUIGradient& OutGradient) const
	{
		if (const FProjectUIGradient* Found = GradientPresets.Find(PresetName))
		{
			OutGradient = *Found;
			return true;
		}
		return false;
	}

	// PrimaryDataAsset overrides for Asset Manager integration
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("ProjectUITheme", GetFName());
	}

	/** Initialize default gradient presets */
	UProjectUIThemeData()
	{
		// Default gradient presets
		FProjectUIGradient PrimaryGradient;
		PrimaryGradient.ColorStops = {
			FLinearColor(0.0f, 0.47f, 0.84f, 1.0f), // Primary Blue
			FLinearColor(0.0f, 0.27f, 0.54f, 1.0f)  // Darker Blue
		};
		PrimaryGradient.AngleDegrees = 135.0f; // Diagonal

		FProjectUIGradient BackgroundGradient;
		BackgroundGradient.ColorStops = {
			FLinearColor(0.02f, 0.02f, 0.02f, 0.95f), // Dark
			FLinearColor(0.05f, 0.05f, 0.05f, 0.95f)  // Slightly lighter
		};
		BackgroundGradient.AngleDegrees = 180.0f; // Top to bottom

		FProjectUIGradient SuccessGradient;
		SuccessGradient.ColorStops = {
			FLinearColor(0.3f, 0.69f, 0.31f, 1.0f), // Green
			FLinearColor(0.2f, 0.49f, 0.21f, 1.0f)  // Darker Green
		};
		SuccessGradient.AngleDegrees = 135.0f;

		FProjectUIGradient ErrorGradient;
		ErrorGradient.ColorStops = {
			FLinearColor(0.96f, 0.26f, 0.21f, 1.0f), // Red
			FLinearColor(0.76f, 0.16f, 0.11f, 1.0f)  // Darker Red
		};
		ErrorGradient.AngleDegrees = 135.0f;

		GradientPresets.Add("Primary", PrimaryGradient);
		GradientPresets.Add("Background", BackgroundGradient);
		GradientPresets.Add("Success", SuccessGradient);
		GradientPresets.Add("Error", ErrorGradient);
	}
};
