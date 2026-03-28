// Copyright ALIS. All Rights Reserved.
// LayoutPropertyAppliers - Widget-specific property application (SOLID: Single Responsibility)

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Engine/Texture2D.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Theme/ProjectUIThemeData.h"
#include "Effects/ProjectEffectWidget.h"
#include "Effects/ProjectEffectRegistry.h"
#include "Widgets/ProjectRadialProgress.h"

DEFINE_LOG_CATEGORY_STATIC(LogLayoutAppliers, Log, All);

// Forward declarations - defined in LayoutThemeResolver.cpp
namespace LayoutThemeResolver
{
	FLinearColor GetColor(const FString& ColorName, UProjectUIThemeData* Theme);
	FSlateFontInfo GetFont(const FString& FontName, UProjectUIThemeData* Theme);
	FLinearColor GetVariantColor(const FString& Variant, UProjectUIThemeData* Theme);
}

namespace LayoutPropertyAppliers
{
	UTexture2D* LoadTexture(const FString& TexturePath)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
		if (!Texture)
		{
			UE_LOG(LogLayoutAppliers, Warning, TEXT("Failed to load texture: %s"), *TexturePath);
		}
		return Texture;
	}

	void ApplyButtonVariantStyle(UButton* Button, const FString& Variant, UProjectUIThemeData* Theme)
	{
		if (!Button)
		{
			return;
		}

		FLinearColor BaseColor = LayoutThemeResolver::GetVariantColor(Variant, Theme);

		FButtonStyle ButtonStyle = Button->GetStyle();

		ButtonStyle.Normal.TintColor = FSlateColor(BaseColor);

		FLinearColor HoveredColor = BaseColor * 1.1f;
		HoveredColor.A = BaseColor.A;
		ButtonStyle.Hovered.TintColor = FSlateColor(HoveredColor);

		FLinearColor PressedColor = BaseColor * 0.8f;
		PressedColor.A = BaseColor.A;
		ButtonStyle.Pressed.TintColor = FSlateColor(PressedColor);

		FLinearColor DisabledColor = BaseColor * 0.5f;
		DisabledColor.A = BaseColor.A * 0.5f;
		ButtonStyle.Disabled.TintColor = FSlateColor(DisabledColor);

		Button->SetStyle(ButtonStyle);

		UE_LOG(LogLayoutAppliers, Verbose, TEXT("Applied button variant: %s"), *Variant);
	}

	void ApplyButtonProperties(UButton* Button, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
	{
		if (!Button)
		{
			return;
		}

		// Button label (text + optional icon)
		FString Text;
		JsonObject->TryGetStringField(TEXT("text"), Text);

		FString Icon;
		JsonObject->TryGetStringField(TEXT("icon"), Icon);

		const bool bHasText = !Text.IsEmpty();
		const bool bHasIcon = !Icon.IsEmpty();

		if (bHasText || bHasIcon)
		{
			UUserWidget* OwningWidget = Button->GetTypedOuter<UUserWidget>();
			UWidgetTree* WidgetTree = OwningWidget ? OwningWidget->WidgetTree : nullptr;

			// Create HorizontalBox to hold icon + text separately (each with own font)
			UHorizontalBox* HBox = nullptr;
			if (WidgetTree)
			{
				HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			}
			else
			{
				HBox = NewObject<UHorizontalBox>(Button);
			}

			// Add icon TextBlock with icon font
			if (bHasIcon)
			{
				UTextBlock* IconBlock = WidgetTree
					? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
					: NewObject<UTextBlock>(HBox);
				IconBlock->SetText(FText::FromString(Icon));
				IconBlock->SetFont(LayoutThemeResolver::GetFont(TEXT("Icon"), Theme));

				UHorizontalBoxSlot* IconSlot = HBox->AddChildToHorizontalBox(IconBlock);
				IconSlot->SetPadding(FMargin(0.f, 0.f, bHasText ? 8.f : 0.f, 0.f));
				IconSlot->SetVerticalAlignment(VAlign_Center);
			}

			// Add text TextBlock with text font
			if (bHasText)
			{
				UTextBlock* TextBlock = WidgetTree
					? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
					: NewObject<UTextBlock>(HBox);
				TextBlock->SetText(FText::FromString(Text));

				FString FontName;
				if (JsonObject->TryGetStringField(TEXT("font"), FontName))
				{
					TextBlock->SetFont(LayoutThemeResolver::GetFont(FontName, Theme));
				}

				UHorizontalBoxSlot* TextSlot = HBox->AddChildToHorizontalBox(TextBlock);
				TextSlot->SetVerticalAlignment(VAlign_Center);
			}

			Button->AddChild(HBox);
		}

		// Variant styling
		FString Variant;
		if (JsonObject->TryGetStringField(TEXT("variant"), Variant))
		{
			ApplyButtonVariantStyle(Button, Variant, Theme);
		}
	}

	void ApplyTextBlockProperties(UTextBlock* TextBlock, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
	{
		if (!TextBlock)
		{
			return;
		}

		FString Text;
		if (JsonObject->TryGetStringField(TEXT("text"), Text))
		{
			TextBlock->SetText(FText::FromString(Text));
		}

		// Set min width from size for justification to work
		const TSharedPtr<FJsonObject>* SizeObject;
		if (JsonObject->TryGetObjectField(TEXT("size"), SizeObject))
		{
			double Width = 0;
			if ((*SizeObject)->TryGetNumberField(TEXT("x"), Width) && Width > 0)
			{
				TextBlock->SetMinDesiredWidth(static_cast<float>(Width));
				UE_LOG(LogLayoutAppliers, Display, TEXT("  [%s] TextBlock MinDesiredWidth=%.0f"), *TextBlock->GetName(), Width);
			}
		}

		// Font
		FString FontName;
		if (JsonObject->TryGetStringField(TEXT("font"), FontName))
		{
			TextBlock->SetFont(LayoutThemeResolver::GetFont(FontName, Theme));
		}

		double FontSize = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("font_size"), FontSize))
		{
			FSlateFontInfo FontInfo = TextBlock->GetFont();
			FontInfo.Size = FMath::Max(1, static_cast<int32>(FontSize));
			TextBlock->SetFont(FontInfo);
		}

		double WrapTextAt = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("wrap_text_at"), WrapTextAt))
		{
			TextBlock->SetWrapTextAt(static_cast<float>(FMath::Max(0.0, WrapTextAt)));
		}

		bool bAutoWrapText = false;
		if (JsonObject->TryGetBoolField(TEXT("auto_wrap_text"), bAutoWrapText))
		{
			TextBlock->SetAutoWrapText(bAutoWrapText);
		}

		// Color - supports named theme color or custom RGBA
		const TSharedPtr<FJsonObject>* CustomColorObject;
		if (JsonObject->TryGetObjectField(TEXT("customColor"), CustomColorObject))
		{
			double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
			(*CustomColorObject)->TryGetNumberField(TEXT("r"), R);
			(*CustomColorObject)->TryGetNumberField(TEXT("g"), G);
			(*CustomColorObject)->TryGetNumberField(TEXT("b"), B);
			(*CustomColorObject)->TryGetNumberField(TEXT("a"), A);
			TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(
				static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A))));
		}
		else
		{
			FString ColorName;
			if (JsonObject->TryGetStringField(TEXT("color"), ColorName))
			{
				TextBlock->SetColorAndOpacity(FSlateColor(LayoutThemeResolver::GetColor(ColorName, Theme)));
			}
			else if (JsonObject->HasField(TEXT("color")))
			{
				// "color" exists but isn't string - likely meant "customColor" for RGBA object
				UE_LOG(LogLayoutAppliers, Warning, TEXT("TextBlock '%s': 'color' must be theme color string (e.g. \"Text\"). Use 'customColor' for RGBA object."),
					*TextBlock->GetName());
			}
		}

		// Justification
		FString Justification;
		if (JsonObject->TryGetStringField(TEXT("justification"), Justification))
		{
			UE_LOG(LogLayoutAppliers, Display, TEXT("  [%s] TextBlock Justification=%s"), *TextBlock->GetName(), *Justification);
			if (Justification.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
			{
				TextBlock->SetJustification(ETextJustify::Center);
			}
			else if (Justification.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
			{
				TextBlock->SetJustification(ETextJustify::Right);
			}
			else
			{
				TextBlock->SetJustification(ETextJustify::Left);
			}
		}
	}

	void ApplyImageProperties(UImage* Image, const TSharedPtr<FJsonObject>& JsonObject)
	{
		if (!Image)
		{
			return;
		}

		FString TexturePath;
		if (JsonObject->TryGetStringField(TEXT("texture"), TexturePath))
		{
			if (UTexture2D* Texture = LoadTexture(TexturePath))
			{
				Image->SetBrushFromTexture(Texture);
			}
		}
	}

	void ApplyProgressBarProperties(UProgressBar* ProgressBar, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
	{
		if (!ProgressBar)
		{
			return;
		}

		double Percent = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("percent"), Percent))
		{
			ProgressBar->SetPercent(static_cast<float>(Percent));
		}

		FString ColorName;
		if (JsonObject->TryGetStringField(TEXT("fillColor"), ColorName))
		{
			ProgressBar->SetFillColorAndOpacity(LayoutThemeResolver::GetColor(ColorName, Theme));
		}
	}

	void ApplyRadialProgressProperties(UProjectRadialProgress* RadialProgress, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
	{
		if (!RadialProgress)
		{
			return;
		}

		double Percent = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("percent"), Percent))
		{
			RadialProgress->SetPercent(static_cast<float>(Percent));
		}

		double Thickness = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("thickness"), Thickness))
		{
			RadialProgress->SetThickness(static_cast<float>(Thickness));
		}

		FString FillColorName;
		if (JsonObject->TryGetStringField(TEXT("fillColor"), FillColorName))
		{
			RadialProgress->SetFillColor(LayoutThemeResolver::GetColor(FillColorName, Theme));
		}

		FString TrackColorName;
		if (JsonObject->TryGetStringField(TEXT("trackColor"), TrackColorName))
		{
			RadialProgress->SetTrackColor(LayoutThemeResolver::GetColor(TrackColorName, Theme));
		}

		double StartAngleDegrees = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("startAngleDegrees"), StartAngleDegrees))
		{
			RadialProgress->SetStartAngleDegrees(static_cast<float>(StartAngleDegrees));
		}

		bool bClockwise = true;
		if (JsonObject->TryGetBoolField(TEXT("clockwise"), bClockwise))
		{
			RadialProgress->SetClockwise(bClockwise);
		}

		bool bShowTrack = true;
		if (JsonObject->TryGetBoolField(TEXT("showTrack"), bShowTrack))
		{
			RadialProgress->SetShowTrack(bShowTrack);
		}
	}

	void ApplyBorderProperties(UBorder* Border, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
	{
		if (!Border)
		{
			return;
		}

		// Background color (expects theme color name string, e.g. "Surface", "Background")
		FString BackgroundColor;
		if (JsonObject->TryGetStringField(TEXT("background"), BackgroundColor))
		{
			FLinearColor Color = LayoutThemeResolver::GetColor(BackgroundColor, Theme);
			Border->SetBrushColor(Color);
		}
		else if (JsonObject->HasField(TEXT("background")))
		{
			// Field exists but wrong type - warn developer
			UE_LOG(LogLayoutAppliers, Warning, TEXT("Border '%s': 'background' must be theme color string (e.g. \"Surface\"), not object"),
				*Border->GetName());
		}

		// Opacity
		double Opacity = 1.0;
		if (JsonObject->TryGetNumberField(TEXT("opacity"), Opacity))
		{
			Border->SetRenderOpacity(static_cast<float>(Opacity));
		}

		// Padding
		double Padding = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("padding"), Padding))
		{
			Border->SetPadding(FMargin(static_cast<float>(Padding)));
		}
	}

	void ApplyEffectProperties(UProjectEffectWidget* Effect, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
	{
		if (!Effect)
		{
			return;
		}

		// Delegate to effect registry for type-specific property handling
		UProjectEffectRegistry::ApplyEffectProperties(Effect, JsonObject, Theme);

		UE_LOG(LogLayoutAppliers, Verbose, TEXT("Applied effect properties: %s"), *Effect->GetEffectTypeName());
	}

	void ApplyComboBoxProperties(UComboBoxString* ComboBox, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
	{
		if (!ComboBox)
		{
			return;
		}

		// Options array
		const TArray<TSharedPtr<FJsonValue>>* OptionsArray;
		if (JsonObject->TryGetArrayField(TEXT("options"), OptionsArray))
		{
			for (const TSharedPtr<FJsonValue>& OptionValue : *OptionsArray)
			{
				FString Option = OptionValue->AsString();
				if (!Option.IsEmpty())
				{
					ComboBox->AddOption(Option);
				}
			}
		}

		// Default selected option
		FString Selected;
		if (JsonObject->TryGetStringField(TEXT("selected"), Selected))
		{
			ComboBox->SetSelectedOption(Selected);
		}
		else if (ComboBox->GetOptionCount() > 0)
		{
			ComboBox->SetSelectedIndex(0);
		}

		// Note: UComboBoxString font requires ItemStyle setup which is complex
		// ComboBox uses default engine style - acceptable for KISS approach
	}

	void ApplyCheckBoxProperties(UCheckBox* CheckBox, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
	{
		if (!CheckBox)
		{
			return;
		}

		// Initial checked state
		bool bChecked = false;
		if (JsonObject->TryGetBoolField(TEXT("checked"), bChecked))
		{
			CheckBox->SetIsChecked(bChecked);
		}

		// Label text (create as sibling in parent - caller handles this)
	}

	void ApplySliderProperties(USlider* Slider, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
	{
		if (!Slider)
		{
			return;
		}

		// Initial value (0..1)
		double Value = 0.5;
		if (JsonObject->TryGetNumberField(TEXT("value"), Value))
		{
			Slider->SetValue(FMath::Clamp(static_cast<float>(Value), 0.0f, 1.0f));
		}

		// Min/Max (for display, slider always 0..1 internally)
		double MinValue = 0.0;
		double MaxValue = 1.0;
		JsonObject->TryGetNumberField(TEXT("min"), MinValue);
		JsonObject->TryGetNumberField(TEXT("max"), MaxValue);
		Slider->SetMinValue(static_cast<float>(MinValue));
		Slider->SetMaxValue(static_cast<float>(MaxValue));

		// Step size
		double StepSize = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("step"), StepSize))
		{
			Slider->SetStepSize(static_cast<float>(StepSize));
		}

		// Colors
		FString BarColor;
		if (JsonObject->TryGetStringField(TEXT("barColor"), BarColor))
		{
			FLinearColor Color = LayoutThemeResolver::GetColor(BarColor, Theme);
			Slider->SetSliderBarColor(Color);
		}

		FString HandleColor;
		if (JsonObject->TryGetStringField(TEXT("handleColor"), HandleColor))
		{
			FLinearColor Color = LayoutThemeResolver::GetColor(HandleColor, Theme);
			Slider->SetSliderHandleColor(Color);
		}
	}
}
