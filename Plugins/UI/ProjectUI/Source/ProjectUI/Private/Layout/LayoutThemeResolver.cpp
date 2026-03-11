// Copyright ALIS. All Rights Reserved.
// LayoutThemeResolver - Theme color/font/variant resolution (SOLID: Single Responsibility)

#include "Theme/ProjectUIThemeData.h"
#include "ProjectUI.h"
#include "Misc/Paths.h"
#include "Fonts/CompositeFont.h"

DEFINE_LOG_CATEGORY_STATIC(LogLayoutTheme, Log, All);

namespace LayoutThemeResolver
{	// UE 5.7: FSlateFontInfo(Path, Size) deprecated - use TSharedPtr<FCompositeFont>
	static FSlateFontInfo MakeSlateFontFromFile(const FString& FontPath, int32 Size)
	{
		TSharedPtr<const FCompositeFont> CompositeFont = MakeShareable(new FStandaloneCompositeFont(
			NAME_None,
			FontPath,
			EFontHinting::Default,
			EFontLoadingPolicy::LazyLoad
		));
		return FSlateFontInfo(CompositeFont, Size);
	}
	// Cache for Roboto fallback font
	static TSharedPtr<const FCompositeFont> RobotoComposite;
	static FSlateFontInfo MakeRobotoFont(int32 Size)
	{
		if (!RobotoComposite.IsValid())
		{
			const FString RobotoPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf");
			RobotoComposite = MakeShareable(new FStandaloneCompositeFont(
				NAME_None,
				RobotoPath,
				EFontHinting::Default,
				EFontLoadingPolicy::LazyLoad
			));
		}
		return FSlateFontInfo(RobotoComposite, Size);
	}
	FLinearColor GetColor(const FString& ColorName, UProjectUIThemeData* Theme)
	{
		if (!Theme)
		{
			// Fallback colors when no theme
			static const TMap<FString, FLinearColor> FallbackColors = {
				{TEXT("Primary"), FLinearColor(0.2f, 0.4f, 0.8f, 1.0f)},
				{TEXT("Secondary"), FLinearColor(0.4f, 0.4f, 0.5f, 1.0f)},
				{TEXT("Background"), FLinearColor(0.1f, 0.1f, 0.15f, 1.0f)},
				{TEXT("Surface"), FLinearColor(0.15f, 0.15f, 0.2f, 1.0f)},
				{TEXT("Error"), FLinearColor(0.8f, 0.2f, 0.2f, 1.0f)},
				{TEXT("Warning"), FLinearColor(0.8f, 0.6f, 0.2f, 1.0f)},
				{TEXT("Success"), FLinearColor(0.2f, 0.7f, 0.3f, 1.0f)},
				{TEXT("Text"), FLinearColor(0.9f, 0.9f, 0.9f, 1.0f)},
				{TEXT("TextSecondary"), FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)},
				{TEXT("Border"), FLinearColor(0.3f, 0.3f, 0.35f, 1.0f)},
				{TEXT("Transparent"), FLinearColor::Transparent}
			};
			if (const FLinearColor* Color = FallbackColors.Find(ColorName))
			{
				return *Color;
			}
			return FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);
		}

		// Theme color lookup
		static const TMap<FString, int32> ColorIndices = {
			{TEXT("Primary"), 0},
			{TEXT("Secondary"), 1},
			{TEXT("Background"), 2},
			{TEXT("Surface"), 3},
			{TEXT("Error"), 4},
			{TEXT("Warning"), 5},
			{TEXT("Success"), 6},
			{TEXT("Text"), 7},
			{TEXT("TextSecondary"), 8},
			{TEXT("Border"), 9},
			{TEXT("Transparent"), 10}
		};

		if (const int32* Index = ColorIndices.Find(ColorName))
		{
			switch (*Index)
			{
				case 0: return Theme->Colors.Primary;
				case 1: return Theme->Colors.Secondary;
				case 2: return Theme->Colors.Background;
				case 3: return Theme->Colors.Surface;
				case 4: return Theme->Colors.Error;
				case 5: return Theme->Colors.Warning;
				case 6: return Theme->Colors.Success;
				case 7: return Theme->Colors.TextPrimary;
				case 8: return Theme->Colors.TextSecondary;
				case 9: return Theme->Colors.Border;
				case 10: return FLinearColor::Transparent;
				default: break;
			}
		}

		UE_LOG(LogLayoutTheme, Warning, TEXT("Unknown theme color: %s"), *ColorName);
		return FLinearColor::White;
	}

	FSlateFontInfo GetFont(const FString& FontName, UProjectUIThemeData* Theme)
	{
		// Fallback font sizes
		static const TMap<FString, int32> FallbackFontSizes = {
			{TEXT("HeadingLarge"), 36},
			{TEXT("HeadingMedium"), 28},
			{TEXT("HeadingSmall"), 22},
			{TEXT("BodyLarge"), 18},
			{TEXT("BodyMedium"), 14},
			{TEXT("BodySmall"), 12},
			{TEXT("Button"), 16},
			{TEXT("Label"), 12},
			{TEXT("Icon"), 18},
			{TEXT("GameIcon"), 18}
		};

		auto MakeFallbackFont = [](const FString& Name, int32 OverrideSize = 0) -> FSlateFontInfo
		{
			int32 FontSize = OverrideSize > 0 ? OverrideSize : 14;
			if (OverrideSize == 0)
			{
				if (const int32* Size = FallbackFontSizes.Find(Name))
				{
					FontSize = *Size;
				}
			}

			// Select font file based on style name
			const FString& FontsDir = FProjectUIModule::GetFontsDir();

			// Icon fonts: MDI for general UI, Game Icons for equipment/items
			if (Name == TEXT("Icon"))
			{
				static const TCHAR* IconFontFiles[] = {
					TEXT("materialdesignicons-webfont.ttf"),
				};

				for (const TCHAR* IconFile : IconFontFiles)
				{
					const FString IconPath = FPaths::Combine(FontsDir, IconFile);
					if (FPaths::FileExists(IconPath))
					{
						return MakeSlateFontFromFile(IconPath, FontSize);
					}
				}
			}
			else if (Name == TEXT("GameIcon"))
			{
				static const TCHAR* GameIconFontFiles[] = {
					TEXT("game-icons.ttf"),
				};

				for (const TCHAR* IconFile : GameIconFontFiles)
				{
					const FString IconPath = FPaths::Combine(FontsDir, IconFile);
					if (FPaths::FileExists(IconPath))
					{
						return MakeSlateFontFromFile(IconPath, FontSize);
					}
				}
			}
			else
			{
				// Text fonts: Headings use SemiBold, Buttons use Medium, Body uses Regular
				const TCHAR* FontFile = TEXT("Inter-Regular.ttf");
				if (Name.StartsWith(TEXT("Heading")))
				{
					FontFile = TEXT("Inter-SemiBold.ttf");
				}
				else if (Name == TEXT("Button"))
				{
					FontFile = TEXT("Inter-Medium.ttf");
				}

				if (!FontsDir.IsEmpty())
				{
					const FString FontPath = FPaths::Combine(FontsDir, FontFile);
					if (FPaths::FileExists(FontPath))
					{
						return MakeSlateFontFromFile(FontPath, FontSize);
					}
				}
			}

			// Fallback to engine Roboto if fonts not found
			return MakeRobotoFont(FontSize);
		};

		if (!Theme)
		{
			return MakeFallbackFont(FontName);
		}

		FSlateFontInfo ThemeFont;
		bool bFoundFont = false;

		if (FontName == TEXT("HeadingLarge")) { ThemeFont = Theme->Typography.HeadingLarge; bFoundFont = true; }
		else if (FontName == TEXT("HeadingMedium")) { ThemeFont = Theme->Typography.HeadingMedium; bFoundFont = true; }
		else if (FontName == TEXT("HeadingSmall")) { ThemeFont = Theme->Typography.HeadingSmall; bFoundFont = true; }
		else if (FontName == TEXT("BodyLarge")) { ThemeFont = Theme->Typography.BodyLarge; bFoundFont = true; }
		else if (FontName == TEXT("BodyMedium")) { ThemeFont = Theme->Typography.BodyMedium; bFoundFont = true; }
		else if (FontName == TEXT("BodySmall")) { ThemeFont = Theme->Typography.BodySmall; bFoundFont = true; }
		else if (FontName == TEXT("Button")) { ThemeFont = Theme->Typography.Button; bFoundFont = true; }
		else if (FontName == TEXT("Label")) { ThemeFont = Theme->Typography.Label; bFoundFont = true; }
		else if (FontName == TEXT("Icon") || FontName == TEXT("GameIcon"))
		{
			// Icon fonts use fallback path which searches for font files on disk
			return MakeFallbackFont(FontName);
		}

		if (!bFoundFont)
		{
			UE_LOG(LogLayoutTheme, Warning, TEXT("Unknown theme font: %s"), *FontName);
			return MakeFallbackFont(FontName);
		}

		if (!ThemeFont.HasValidFont())
		{
			return MakeFallbackFont(FontName, ThemeFont.Size > 0 ? ThemeFont.Size : 0);
		}

		return ThemeFont;
	}

	FLinearColor GetVariantColor(const FString& Variant, UProjectUIThemeData* Theme)
	{
		FString VariantLower = Variant.ToLower();

		if (!Theme)
		{
			static const TMap<FString, FLinearColor> FallbackVariants = {
				{TEXT("primary"), FLinearColor(0.2f, 0.4f, 0.8f, 1.0f)},
				{TEXT("secondary"), FLinearColor(0.4f, 0.4f, 0.5f, 1.0f)},
				{TEXT("success"), FLinearColor(0.2f, 0.7f, 0.3f, 1.0f)},
				{TEXT("warning"), FLinearColor(0.8f, 0.6f, 0.2f, 1.0f)},
				{TEXT("error"), FLinearColor(0.8f, 0.2f, 0.2f, 1.0f)},
				{TEXT("danger"), FLinearColor(0.8f, 0.2f, 0.2f, 1.0f)},
				{TEXT("text"), FLinearColor::Transparent}
			};
			if (const FLinearColor* Color = FallbackVariants.Find(VariantLower))
			{
				return *Color;
			}
			return FLinearColor(0.2f, 0.4f, 0.8f, 1.0f);
		}

		static const TMap<FString, int32> VariantIndices = {
			{TEXT("primary"), 0},
			{TEXT("secondary"), 1},
			{TEXT("success"), 2},
			{TEXT("warning"), 3},
			{TEXT("error"), 4},
			{TEXT("danger"), 4},
			{TEXT("text"), 5}
		};

		if (const int32* Index = VariantIndices.Find(VariantLower))
		{
			switch (*Index)
			{
				case 0: return Theme->Colors.Primary;
				case 1: return Theme->Colors.Secondary;
				case 2: return Theme->Colors.Success;
				case 3: return Theme->Colors.Warning;
				case 4: return Theme->Colors.Error;
				case 5: return FLinearColor::Transparent;
				default: break;
			}
		}

		UE_LOG(LogLayoutTheme, Warning, TEXT("Unknown button variant: %s, using Primary"), *Variant);
		return Theme->Colors.Primary;
	}
}
