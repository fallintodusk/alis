// Copyright ALIS. All Rights Reserved.
// LayoutWidgetRegistry - Widget factory and anchor preset registry (SOLID: Open/Closed)

#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/EditableText.h"
#include "Components/ListView.h"
#include "Components/Spacer.h"
#include "Components/Border.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Effects/ProjectFireSparksEffect.h"
#include "Dialogs/ProjectDialogWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogLayoutRegistry, Log, All);

// Widget factory function signature
using FWidgetFactory = TFunction<UWidget*(UObject*, FName)>;

namespace LayoutWidgetRegistry
{
	// Widget type -> Factory function
	static TMap<FString, FWidgetFactory> WidgetFactories;

	// Anchor name -> FAnchors preset
	static TMap<FString, FAnchors> AnchorPresets;

	// Flag to ensure one-time initialization
	static bool bInitialized = false;

	// Template factory for common widget creation pattern
	template<typename T>
	static UWidget* CreateWidget(UObject* Outer, FName WidgetName)
	{
		// Try to use WidgetTree for proper UMG lifecycle management
		if (UUserWidget* OwningWidget = Cast<UUserWidget>(Outer))
		{
			if (OwningWidget->WidgetTree)
			{
				return OwningWidget->WidgetTree->ConstructWidget<T>(T::StaticClass(), WidgetName);
			}
		}
		// Fallback to NewObject (for non-UserWidget contexts)
		return NewObject<T>(Outer, WidgetName);
	}

	void Initialize()
	{
		if (bInitialized)
		{
			return;
		}

		// Widget Factories (extensible via TMap)
		WidgetFactories.Add(TEXT("CanvasPanel"), &CreateWidget<UCanvasPanel>);
		WidgetFactories.Add(TEXT("VerticalBox"), &CreateWidget<UVerticalBox>);
		WidgetFactories.Add(TEXT("HorizontalBox"), &CreateWidget<UHorizontalBox>);
		WidgetFactories.Add(TEXT("Overlay"), &CreateWidget<UOverlay>);
		WidgetFactories.Add(TEXT("Button"), &CreateWidget<UButton>);
		WidgetFactories.Add(TEXT("TextBlock"), &CreateWidget<UTextBlock>);
		WidgetFactories.Add(TEXT("Image"), &CreateWidget<UImage>);
		WidgetFactories.Add(TEXT("ProgressBar"), &CreateWidget<UProgressBar>);
		WidgetFactories.Add(TEXT("EditableText"), &CreateWidget<UEditableText>);
		WidgetFactories.Add(TEXT("ListView"), &CreateWidget<UListView>);
		WidgetFactories.Add(TEXT("Spacer"), &CreateWidget<USpacer>);
		WidgetFactories.Add(TEXT("Border"), &CreateWidget<UBorder>);
		WidgetFactories.Add(TEXT("ComboBox"), &CreateWidget<UComboBoxString>);
		WidgetFactories.Add(TEXT("CheckBox"), &CreateWidget<UCheckBox>);
		WidgetFactories.Add(TEXT("Slider"), &CreateWidget<USlider>);
		WidgetFactories.Add(TEXT("SizeBox"), &CreateWidget<USizeBox>);
		WidgetFactories.Add(TEXT("ScrollBox"), &CreateWidget<UScrollBox>);

		// Effect Widgets (procedural UI effects - no uassets needed)
		WidgetFactories.Add(TEXT("FireSparks"), &CreateWidget<UProjectFireSparksEffect>);

		// Dialog Widgets (modal overlays with buttons)
		WidgetFactories.Add(TEXT("Dialog"), &CreateWidget<UProjectDialogWidget>);

		// Anchor Presets (data-driven lookup)
		AnchorPresets.Add(TEXT("Fill"), FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		AnchorPresets.Add(TEXT("TopLeft"), FAnchors(0.0f, 0.0f));
		AnchorPresets.Add(TEXT("TopCenter"), FAnchors(0.5f, 0.0f));
		AnchorPresets.Add(TEXT("TopRight"), FAnchors(1.0f, 0.0f));
		AnchorPresets.Add(TEXT("CenterLeft"), FAnchors(0.0f, 0.5f));
		AnchorPresets.Add(TEXT("Center"), FAnchors(0.5f, 0.5f));
		AnchorPresets.Add(TEXT("CenterRight"), FAnchors(1.0f, 0.5f));
		AnchorPresets.Add(TEXT("BottomLeft"), FAnchors(0.0f, 1.0f));
		AnchorPresets.Add(TEXT("BottomCenter"), FAnchors(0.5f, 1.0f));
		AnchorPresets.Add(TEXT("BottomRight"), FAnchors(1.0f, 1.0f));

		bInitialized = true;

		UE_LOG(LogLayoutRegistry, Log, TEXT("Registry initialized: %d widget types, %d anchor presets"),
			WidgetFactories.Num(), AnchorPresets.Num());
	}

	UWidget* CreateWidgetByType(const FString& Type, UObject* Outer, FName WidgetName)
	{
		Initialize();

		if (const FWidgetFactory* Factory = WidgetFactories.Find(Type))
		{
			return (*Factory)(Outer, WidgetName);
		}
		return nullptr;
	}

	FAnchors GetAnchorPreset(const FString& AnchorName)
	{
		Initialize();

		if (const FAnchors* Preset = AnchorPresets.Find(AnchorName))
		{
			return *Preset;
		}

		UE_LOG(LogLayoutRegistry, Warning, TEXT("Unknown anchor preset: %s, using TopLeft"), *AnchorName);
		return FAnchors(0.0f, 0.0f);
	}

	TArray<FString> GetRegisteredWidgetTypes()
	{
		Initialize();
		TArray<FString> Types;
		WidgetFactories.GetKeys(Types);
		return Types;
	}
}
