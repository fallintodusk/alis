#include "DataAssets/Theme/DA_MinimalTheme.h"

#include "UObject/UObjectGlobals.h"

namespace MinimalTheme
{
	static void ConfigureFont(FSlateFontInfo& FontInfo, int32 Size)
	{
		// Use default UE font - just set size, Slate handles the rest
		FontInfo.Size = Size;
	}
}

UDA_MinimalTheme::UDA_MinimalTheme()
{
	ThemeName = FText::FromString(TEXT("Minimal Theme"));
	ThemeDescription = FText::FromString(TEXT("Neutral palette for prototyping and accessibility reviews."));

	Colors.Primary = FLinearColor(0.18f, 0.18f, 0.18f, 1.0f);
	Colors.Secondary = FLinearColor(0.32f, 0.32f, 0.32f, 1.0f);
	Colors.Success = FLinearColor(0.23f, 0.62f, 0.43f, 1.0f);
	Colors.Warning = FLinearColor(0.89f, 0.59f, 0.04f, 1.0f);
	Colors.Error = FLinearColor(0.80f, 0.22f, 0.24f, 1.0f);
	Colors.Background = FLinearColor(0.96f, 0.96f, 0.96f, 1.0f);
	Colors.Surface = FLinearColor(0.90f, 0.90f, 0.90f, 1.0f);
	Colors.TextPrimary = FLinearColor(0.12f, 0.12f, 0.12f, 1.0f);
	Colors.TextSecondary = FLinearColor(0.32f, 0.32f, 0.32f, 1.0f);
	Colors.TextDisabled = FLinearColor(0.65f, 0.65f, 0.65f, 1.0f);
	Colors.Border = FLinearColor(0.78f, 0.78f, 0.78f, 1.0f);

	Spacing.ExtraSmall = 2.0f;
	Spacing.Small = 6.0f;
	Spacing.Medium = 12.0f;
	Spacing.Large = 20.0f;
	Spacing.ExtraLarge = 28.0f;
	Spacing.BorderRadius = 2.0f;
	Spacing.BorderThickness = 1.0f;

	Animation.Fast = 0.12f;
	Animation.Normal = 0.20f;
	Animation.Slow = 0.32f;
	Animation.EasingFunction = EEasingFunc::EaseInOut;

	MinimalTheme::ConfigureFont(Typography.HeadingLarge, 28);
	MinimalTheme::ConfigureFont(Typography.HeadingMedium, 22);
	MinimalTheme::ConfigureFont(Typography.HeadingSmall, 18);
	MinimalTheme::ConfigureFont(Typography.BodyLarge, 15);
	MinimalTheme::ConfigureFont(Typography.BodyMedium, 13);
	MinimalTheme::ConfigureFont(Typography.BodySmall, 11);
	MinimalTheme::ConfigureFont(Typography.Button, 14);
	MinimalTheme::ConfigureFont(Typography.Label, 12);
}
