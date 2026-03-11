#include "DataAssets/Theme/DA_DefaultTheme.h"

#include "UObject/UObjectGlobals.h"

namespace DefaultTheme
{
	static void ConfigureFont(FSlateFontInfo& FontInfo, int32 Size)
	{
		// Use default UE font - just set size, Slate handles the rest
		FontInfo.Size = Size;
	}
}

UDA_DefaultTheme::UDA_DefaultTheme()
{
	ThemeName = FText::FromString(TEXT("Default Theme"));
	ThemeDescription = FText::FromString(TEXT("Primary development palette with blue highlights."));

	Colors.Primary = FLinearColor(0.02f, 0.40f, 0.76f, 1.0f);
	Colors.Secondary = FLinearColor(0.33f, 0.58f, 0.92f, 1.0f);
	Colors.Success = FLinearColor(0.26f, 0.68f, 0.36f, 1.0f);
	Colors.Warning = FLinearColor(1.0f, 0.74f, 0.03f, 1.0f);
	Colors.Error = FLinearColor(0.93f, 0.25f, 0.20f, 1.0f);
	Colors.Background = FLinearColor(0.05f, 0.06f, 0.09f, 0.95f);
	Colors.Surface = FLinearColor(0.12f, 0.15f, 0.19f, 0.90f);
	Colors.TextPrimary = FLinearColor(0.96f, 0.98f, 1.0f, 1.0f);
	Colors.TextSecondary = FLinearColor(0.73f, 0.79f, 0.88f, 1.0f);
	Colors.TextDisabled = FLinearColor(0.45f, 0.48f, 0.55f, 1.0f);
	Colors.Border = FLinearColor(0.24f, 0.30f, 0.38f, 1.0f);

	Spacing.ExtraSmall = 4.0f;
	Spacing.Small = 8.0f;
	Spacing.Medium = 16.0f;
	Spacing.Large = 24.0f;
	Spacing.ExtraLarge = 32.0f;
	Spacing.BorderRadius = 6.0f;
	Spacing.BorderThickness = 2.0f;

	Animation.Fast = 0.15f;
	Animation.Normal = 0.25f;
	Animation.Slow = 0.40f;
	Animation.EasingFunction = EEasingFunc::EaseInOut;

	DefaultTheme::ConfigureFont(Typography.HeadingLarge, 32);
	DefaultTheme::ConfigureFont(Typography.HeadingMedium, 24);
	DefaultTheme::ConfigureFont(Typography.HeadingSmall, 18);
	DefaultTheme::ConfigureFont(Typography.BodyLarge, 16);
	DefaultTheme::ConfigureFont(Typography.BodyMedium, 14);
	DefaultTheme::ConfigureFont(Typography.BodySmall, 12);
	DefaultTheme::ConfigureFont(Typography.Button, 16);
	DefaultTheme::ConfigureFont(Typography.Label, 14);
}
