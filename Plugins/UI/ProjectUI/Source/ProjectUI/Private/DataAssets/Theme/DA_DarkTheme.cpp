#include "DataAssets/Theme/DA_DarkTheme.h"

#include "UObject/UObjectGlobals.h"
#include "Misc/Paths.h"
#include "Fonts/CompositeFont.h"

namespace DarkTheme
{
	// UE 5.7: FSlateFontInfo(Path, Size) deprecated - use TSharedPtr<FCompositeFont>
	static TSharedPtr<const FCompositeFont> MakeSingleFileCompositeFont(const FString& FontPath)
	{
		return MakeShareable(new FStandaloneCompositeFont(
			NAME_None,
			FontPath,
			EFontHinting::Default,
			EFontLoadingPolicy::LazyLoad
		));
	}

	// Cache Roboto composite to avoid realloc every call
	static TSharedPtr<const FCompositeFont> RobotoComposite;

	static void ConfigureFont(FSlateFontInfo& FontInfo, int32 Size)
	{
		// Pin to Roboto path so extended glyphs (icons) render reliably
		if (!RobotoComposite.IsValid())
		{
			const FString RobotoPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf");
			RobotoComposite = MakeSingleFileCompositeFont(RobotoPath);
		}
		FontInfo = FSlateFontInfo(RobotoComposite, Size);
	}
}

UDA_DarkTheme::UDA_DarkTheme()
{
	ThemeName = FText::FromString(TEXT("Dark Theme"));
	ThemeDescription = FText::FromString(TEXT("High contrast palette tailored for dark environments."));

	Colors.Primary = FLinearColor(0.97f, 0.50f, 0.14f, 1.0f);
	Colors.Secondary = FLinearColor(0.23f, 0.25f, 0.30f, 1.0f);
	Colors.Success = FLinearColor(0.28f, 0.75f, 0.46f, 1.0f);
	Colors.Warning = FLinearColor(1.0f, 0.64f, 0.22f, 1.0f);
	Colors.Error = FLinearColor(0.90f, 0.18f, 0.32f, 1.0f);
	Colors.Background = FLinearColor(0.02f, 0.02f, 0.03f, 0.98f);
	Colors.Surface = FLinearColor(0.07f, 0.07f, 0.10f, 0.94f);
	Colors.TextPrimary = FLinearColor(0.96f, 0.96f, 0.98f, 1.0f);
	Colors.TextSecondary = FLinearColor(0.72f, 0.74f, 0.78f, 1.0f);
	Colors.TextDisabled = FLinearColor(0.42f, 0.44f, 0.48f, 1.0f);
	Colors.Border = FLinearColor(0.36f, 0.21f, 0.12f, 1.0f);

	Spacing.ExtraSmall = 4.0f;
	Spacing.Small = 8.0f;
	Spacing.Medium = 14.0f;
	Spacing.Large = 22.0f;
	Spacing.ExtraLarge = 30.0f;
	Spacing.BorderRadius = 8.0f;
	Spacing.BorderThickness = 2.0f;

	Animation.Fast = 0.18f;
	Animation.Normal = 0.28f;
	Animation.Slow = 0.45f;
	Animation.EasingFunction = EEasingFunc::EaseOut;

	DarkTheme::ConfigureFont(Typography.HeadingLarge, 34);
	DarkTheme::ConfigureFont(Typography.HeadingMedium, 26);
	DarkTheme::ConfigureFont(Typography.HeadingSmall, 20);
	DarkTheme::ConfigureFont(Typography.BodyLarge, 18);
	DarkTheme::ConfigureFont(Typography.BodyMedium, 15);
	DarkTheme::ConfigureFont(Typography.BodySmall, 13);
	DarkTheme::ConfigureFont(Typography.Button, 17);
	DarkTheme::ConfigureFont(Typography.Label, 14);
}
