// Copyright ALIS. All Rights Reserved.
// ProjectWidgetLayoutLoader - Main orchestrator for JSON layout loading

#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Theme/ProjectUIThemeData.h"
#include "ProjectPaths.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Effects/ProjectEffectWidget.h"
#include "Dialogs/ProjectDialogWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWidgetLoader, Log, All);

// Forward declarations for internal implementation namespaces (SOLID: composition)
namespace LayoutWidgetRegistry
{
	UWidget* CreateWidgetByType(const FString& Type, UObject* Outer, FName WidgetName);
	FAnchors GetAnchorPreset(const FString& AnchorName);
	TArray<FString> GetRegisteredWidgetTypes();
}

namespace LayoutThemeResolver
{
	FLinearColor GetColor(const FString& ColorName, UProjectUIThemeData* Theme);
	FSlateFontInfo GetFont(const FString& FontName, UProjectUIThemeData* Theme);
	FLinearColor GetVariantColor(const FString& Variant, UProjectUIThemeData* Theme);
}

namespace LayoutSlotConfig
{
	FVector2D ParseVector2D(const TSharedPtr<FJsonObject>& JsonObject);
	void ApplyCanvasPanelSlot(UWidget* Widget, UPanelWidget* Parent, const TSharedPtr<FJsonObject>& JsonObject);
}

namespace LayoutPropertyAppliers
{
	UTexture2D* LoadTexture(const FString& TexturePath);
	void ApplyButtonProperties(UButton* Button, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
	void ApplyButtonVariantStyle(UButton* Button, const FString& Variant, UProjectUIThemeData* Theme);
	void ApplyTextBlockProperties(UTextBlock* TextBlock, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
	void ApplyImageProperties(UImage* Image, const TSharedPtr<FJsonObject>& JsonObject);
	void ApplyProgressBarProperties(UProgressBar* ProgressBar, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
	void ApplyBorderProperties(UBorder* Border, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
	void ApplyEffectProperties(class UProjectEffectWidget* Effect, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
	void ApplyComboBoxProperties(UComboBoxString* ComboBox, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
	void ApplyCheckBoxProperties(UCheckBox* CheckBox, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
	void ApplySliderProperties(USlider* Slider, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
}

namespace LayoutDialogAppliers
{
	void ApplyDialogProperties(UProjectDialogWidget* Dialog, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme);
}

// =============================================================================
// Public API
// =============================================================================

UWidget* UProjectWidgetLayoutLoader::LoadLayoutFromFile(UObject* Outer, const FString& JsonFilePath, UProjectUIThemeData* Theme)
{
	if (!Outer)
	{
		UE_LOG(LogProjectWidgetLoader, Error, TEXT("LoadLayoutFromFile: Outer is null"));
		return nullptr;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
	{
		UE_LOG(LogProjectWidgetLoader, Error, TEXT("Failed to load JSON file: %s"), *JsonFilePath);
		return nullptr;
	}

	UE_LOG(LogProjectWidgetLoader, Display, TEXT("Loaded JSON from: %s"), *JsonFilePath);
	UWidget* RootWidget = LoadLayoutFromString(Outer, JsonString, Theme);

	if (RootWidget)
	{
		UE_LOG(LogProjectWidgetLoader, Display, TEXT("Layout loaded successfully. Root widget: %s"), *RootWidget->GetName());
	}
	return RootWidget;
}

UWidget* UProjectWidgetLayoutLoader::LoadLayoutFromString(UObject* Outer, const FString& JsonString, UProjectUIThemeData* Theme)
{
	if (!Outer)
	{
		UE_LOG(LogProjectWidgetLoader, Error, TEXT("LoadLayoutFromString: Outer is null"));
		return nullptr;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogProjectWidgetLoader, Error, TEXT("Failed to parse JSON"));
		return nullptr;
	}

	TSharedPtr<FJsonObject> RootObject = JsonObject->GetObjectField(TEXT("root"));
	if (!RootObject.IsValid())
	{
		UE_LOG(LogProjectWidgetLoader, Error, TEXT("JSON missing 'root' object"));
		return nullptr;
	}

	return CreateWidgetFromJson(Outer, RootObject, Theme);
}

UWidget* UProjectWidgetLayoutLoader::CreateWidgetFromJson(UObject* Outer, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
{
	if (!JsonObject.IsValid())
	{
		UE_LOG(LogProjectWidgetLoader, Error, TEXT("Invalid JSON object"));
		return nullptr;
	}

	FString Type;
	if (!JsonObject->TryGetStringField(TEXT("type"), Type))
	{
		UE_LOG(LogProjectWidgetLoader, Error, TEXT("Widget JSON missing 'type' field"));
		return nullptr;
	}

	FString WidgetName;
	JsonObject->TryGetStringField(TEXT("name"), WidgetName);
	FName WidgetFName = WidgetName.IsEmpty() ? NAME_None : FName(*WidgetName);

	UWidget* Widget = LayoutWidgetRegistry::CreateWidgetByType(Type, Outer, WidgetFName);
	if (!Widget)
	{
		UE_LOG(LogProjectWidgetLoader, Error, TEXT("Unknown or failed widget type: %s (name: %s). Registered types: %s"),
			*Type, *WidgetName, *FString::Join(LayoutWidgetRegistry::GetRegisteredWidgetTypes(), TEXT(", ")));
		return nullptr;
	}

	UE_LOG(LogProjectWidgetLoader, Verbose, TEXT("Created widget: %s (type: %s)"), *Widget->GetName(), *Type);

	ApplyWidgetProperties(Widget, JsonObject, Theme);
	ProcessChildren(Outer, Widget, JsonObject, Theme);

	return Widget;
}

void UProjectWidgetLayoutLoader::ApplyWidgetProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
{
	if (!Widget || !JsonObject.IsValid())
	{
		return;
	}

	// Visibility (common to all widgets)
	bool bVisible = true;
	if (JsonObject->TryGetBoolField(TEXT("visible"), bVisible))
	{
		Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	// Delegate to type-specific appliers
	if (UButton* Button = Cast<UButton>(Widget))
	{
		LayoutPropertyAppliers::ApplyButtonProperties(Button, JsonObject, Theme);
	}
	else if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
	{
		LayoutPropertyAppliers::ApplyTextBlockProperties(TextBlock, JsonObject, Theme);
	}
	else if (UImage* Image = Cast<UImage>(Widget))
	{
		LayoutPropertyAppliers::ApplyImageProperties(Image, JsonObject);
	}
	else if (UProgressBar* ProgressBar = Cast<UProgressBar>(Widget))
	{
		LayoutPropertyAppliers::ApplyProgressBarProperties(ProgressBar, JsonObject, Theme);
	}
	else if (UBorder* Border = Cast<UBorder>(Widget))
	{
		LayoutPropertyAppliers::ApplyBorderProperties(Border, JsonObject, Theme);
	}
	else if (UProjectEffectWidget* Effect = Cast<UProjectEffectWidget>(Widget))
	{
		LayoutPropertyAppliers::ApplyEffectProperties(Effect, JsonObject, Theme);
	}
	else if (UProjectDialogWidget* Dialog = Cast<UProjectDialogWidget>(Widget))
	{
		LayoutDialogAppliers::ApplyDialogProperties(Dialog, JsonObject, Theme);
	}
	else if (UComboBoxString* ComboBox = Cast<UComboBoxString>(Widget))
	{
		LayoutPropertyAppliers::ApplyComboBoxProperties(ComboBox, JsonObject, Theme);
	}
	else if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget))
	{
		LayoutPropertyAppliers::ApplyCheckBoxProperties(CheckBox, JsonObject, Theme);
	}
	else if (USlider* Slider = Cast<USlider>(Widget))
	{
		LayoutPropertyAppliers::ApplySliderProperties(Slider, JsonObject, Theme);
	}
}

// =============================================================================
// Path Helpers
// =============================================================================

FString UProjectWidgetLayoutLoader::GetUIConfigPath(const FString& RelativePath)
{
	// Legacy: Redirect to ProjectUI plugin data
	return FProjectPaths::GetPluginDataDir(TEXT("ProjectUI")) / RelativePath;
}

FString UProjectWidgetLayoutLoader::GetPluginUIConfigPath(const FString& PluginName, const FString& RelativePath)
{
	// Auto-resolve plugin data path from IPluginManager
	FString DataDir = FProjectPaths::GetPluginDataDir(PluginName);
	return DataDir.IsEmpty() ? FString() : (DataDir / RelativePath);
}

// =============================================================================
// Children Processing
// =============================================================================

void UProjectWidgetLayoutLoader::ProcessChildren(UObject* Outer, UWidget* Widget, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
{
	UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget);
	if (!PanelWidget)
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* ChildrenArray;
	if (!JsonObject->TryGetArrayField(TEXT("children"), ChildrenArray))
	{
		return;
	}

	auto ParsePaddingObject = [](const TSharedPtr<FJsonObject>& PaddingObject, FMargin& OutPadding) -> bool
	{
		if (!PaddingObject.IsValid())
		{
			return false;
		}

		bool bHasAnyField = false;
		double Value = 0.0;
		if (PaddingObject->TryGetNumberField(TEXT("left"), Value))
		{
			OutPadding.Left = static_cast<float>(Value);
			bHasAnyField = true;
		}
		if (PaddingObject->TryGetNumberField(TEXT("top"), Value))
		{
			OutPadding.Top = static_cast<float>(Value);
			bHasAnyField = true;
		}
		if (PaddingObject->TryGetNumberField(TEXT("right"), Value))
		{
			OutPadding.Right = static_cast<float>(Value);
			bHasAnyField = true;
		}
		if (PaddingObject->TryGetNumberField(TEXT("bottom"), Value))
		{
			OutPadding.Bottom = static_cast<float>(Value);
			bHasAnyField = true;
		}

		return bHasAnyField;
	};

	// Check if parent is a box panel (HorizontalBox/VerticalBox) - needs SizeBox wrapper for explicit sizes
	const bool bIsHorizontalBox = Cast<UHorizontalBox>(PanelWidget) != nullptr;
	const bool bIsVerticalBox = Cast<UVerticalBox>(PanelWidget) != nullptr;
	const bool bIsBoxPanel = bIsHorizontalBox || bIsVerticalBox;
	double SpacingValue = 0.0;
	const bool bHasSpacing = bIsBoxPanel && JsonObject->TryGetNumberField(TEXT("spacing"), SpacingValue);
	const float PanelSpacing = bHasSpacing ? FMath::Max(0.0f, static_cast<float>(SpacingValue)) : 0.0f;

	TArray<TSharedPtr<FJsonObject>> ValidChildren;
	ValidChildren.Reserve(ChildrenArray->Num());
	for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenArray)
	{
		const TSharedPtr<FJsonObject> ChildObject = ChildValue.IsValid() ? ChildValue->AsObject() : nullptr;
		if (ChildObject.IsValid())
		{
			ValidChildren.Add(ChildObject);
		}
	}

	int32 ChildCount = 0;
	for (int32 ChildIndex = 0; ChildIndex < ValidChildren.Num(); ++ChildIndex)
	{
		const TSharedPtr<FJsonObject>& ChildObject = ValidChildren[ChildIndex];

		UWidget* ChildWidget = CreateWidgetFromJson(Outer, ChildObject, Theme);
		if (!ChildWidget)
		{
			continue;
		}

		// Determine which widget is added to the parent (may be a SizeBox wrapper)
		const TSharedPtr<FJsonObject>* SizeObject = nullptr;
		UWidget* SlotOwner = ChildWidget;
		UWidget* CanvasChild = ChildWidget;
		bool bAddedToParent = false;

		// Canvas panel: partial size + autoSize -> SizeBox wrapper for the explicit dimension
		if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(PanelWidget))
		{
			bool bAutoSize = false;
			const bool bHasAutoSize = ChildObject->TryGetBoolField(TEXT("autoSize"), bAutoSize);
			const TSharedPtr<FJsonObject>* PartialSizeObj = nullptr;
			const bool bHasSize = ChildObject->TryGetObjectField(TEXT("size"), PartialSizeObj);

			if (bHasAutoSize && bAutoSize && bHasSize)
			{
				const FVector2D Size = LayoutSlotConfig::ParseVector2D(*PartialSizeObj);
				const bool bPartial = (Size.X > 0) != (Size.Y > 0);

				if (bPartial)
				{
					FName SizeBoxName = FName(*FString::Printf(TEXT("%s_AutoSizer"), *ChildWidget->GetName()));
					USizeBox* SizeBox = Cast<USizeBox>(LayoutWidgetRegistry::CreateWidgetByType(TEXT("SizeBox"), Outer, SizeBoxName));
					if (SizeBox)
					{
						if (Size.X > 0) { SizeBox->SetWidthOverride(Size.X); }
						if (Size.Y > 0) { SizeBox->SetHeightOverride(Size.Y); }
						SizeBox->AddChild(ChildWidget);
						Canvas->AddChild(SizeBox);
						CanvasChild = SizeBox;
						bAddedToParent = true;

						UE_LOG(LogProjectWidgetLoader, Display, TEXT("Canvas auto-size wrap: %s -> %s_AutoSizer (%.0f,%.0f) in %s"),
							*ChildWidget->GetName(), *ChildWidget->GetName(), Size.X, Size.Y, *PanelWidget->GetName());
					}
				}
			}
		}

		// For box panels (HorizontalBox/VerticalBox), wrap children with explicit size in SizeBox
		// This is because BoxSlots use SizeRule (Auto/Fill), not explicit pixel sizes
		if (!bAddedToParent && bIsBoxPanel && ChildObject->TryGetObjectField(TEXT("size"), SizeObject))
		{
			FVector2D Size = LayoutSlotConfig::ParseVector2D(*SizeObject);

			// Create SizeBox wrapper
			FName SizeBoxName = FName(*FString::Printf(TEXT("%s_SizeBox"), *ChildWidget->GetName()));
			USizeBox* SizeBox = Cast<USizeBox>(LayoutWidgetRegistry::CreateWidgetByType(TEXT("SizeBox"), Outer, SizeBoxName));

			if (SizeBox)
			{
				// Set size overrides
				if (Size.X > 0)
				{
					SizeBox->SetWidthOverride(Size.X);
				}
				if (Size.Y > 0)
				{
					SizeBox->SetHeightOverride(Size.Y);
				}

				// Add child to SizeBox, then SizeBox to parent
				SizeBox->AddChild(ChildWidget);
				PanelWidget->AddChild(SizeBox);
				SlotOwner = SizeBox;
				bAddedToParent = true;

				UE_LOG(LogProjectWidgetLoader, Display, TEXT("SizeBox wrap: %s -> %s_SizeBox (%.0fx%.0f) in %s"),
					*ChildWidget->GetName(), *ChildWidget->GetName(), Size.X, Size.Y, *PanelWidget->GetName());
			}
			else
			{
				// Fallback: add without wrapper
				PanelWidget->AddChild(ChildWidget);
				bAddedToParent = true;
				UE_LOG(LogProjectWidgetLoader, Warning, TEXT("SizeBox FAILED for %s in %s - added directly"),
					*ChildWidget->GetName(), *PanelWidget->GetName());
			}
		}

		if (!bAddedToParent)
		{
			// Standard path: add child directly (no explicit size or not a box panel)
			PanelWidget->AddChild(ChildWidget);
			UE_LOG(LogProjectWidgetLoader, Verbose, TEXT("AddChild: %s -> %s (no size wrap)"),
				*ChildWidget->GetName(), *PanelWidget->GetName());
		}
		ChildCount++;

		// Apply canvas slot properties (only for CanvasPanel children)
		// Use CanvasChild (may be SizeBox wrapper) so the slot is on the actual canvas child
		if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(PanelWidget))
		{
			LayoutSlotConfig::ApplyCanvasPanelSlot(CanvasChild, Canvas, ChildObject);
		}

		// Apply box slot properties (HBox/VBox sizeRule, alignment, padding, spacing)
		if (bIsBoxPanel)
		{
			FMargin EffectivePadding(0.0f);
			bool bHasEffectivePadding = false;

			const TSharedPtr<FJsonObject>* SlotObject = nullptr;
			const bool bHasSlotObject = ChildObject->TryGetObjectField(TEXT("slot"), SlotObject)
				&& SlotObject && SlotObject->IsValid();

			if (bHasSlotObject)
			{
				FString SizeRuleStr;
				if ((*SlotObject)->TryGetStringField(TEXT("sizeRule"), SizeRuleStr) && SizeRuleStr == TEXT("Fill"))
				{
					if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(SlotOwner->Slot))
					{
						HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
						UE_LOG(LogProjectWidgetLoader, Verbose, TEXT("  [%s] HBox slot: Fill"), *ChildWidget->GetName());
					}
					else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(SlotOwner->Slot))
					{
						VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
						UE_LOG(LogProjectWidgetLoader, Verbose, TEXT("  [%s] VBox slot: Fill"), *ChildWidget->GetName());
					}
				}

				// Cross-axis alignment: verticalAlignment for HBox slots, horizontalAlignment for VBox slots
				FString VAlignStr;
				if ((*SlotObject)->TryGetStringField(TEXT("verticalAlignment"), VAlignStr))
				{
					if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(SlotOwner->Slot))
					{
						EVerticalAlignment VAlign = VAlignStr == TEXT("Top") ? VAlign_Top
							: VAlignStr == TEXT("Center") ? VAlign_Center
							: VAlignStr == TEXT("Bottom") ? VAlign_Bottom
							: VAlign_Fill;
						HSlot->SetVerticalAlignment(VAlign);
						UE_LOG(LogProjectWidgetLoader, Verbose, TEXT("  [%s] HBox slot VAlign: %s"), *ChildWidget->GetName(), *VAlignStr);
					}
				}

				FString HAlignStr;
				if ((*SlotObject)->TryGetStringField(TEXT("horizontalAlignment"), HAlignStr))
				{
					if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(SlotOwner->Slot))
					{
						EHorizontalAlignment HAlign = HAlignStr == TEXT("Left") ? HAlign_Left
							: HAlignStr == TEXT("Center") ? HAlign_Center
							: HAlignStr == TEXT("Right") ? HAlign_Right
							: HAlign_Fill;
						VSlot->SetHorizontalAlignment(HAlign);
						UE_LOG(LogProjectWidgetLoader, Verbose, TEXT("  [%s] VBox slot HAlign: %s"), *ChildWidget->GetName(), *HAlignStr);
					}
				}

				const TSharedPtr<FJsonObject>* SlotPaddingObject = nullptr;
				if ((*SlotObject)->TryGetObjectField(TEXT("padding"), SlotPaddingObject) && SlotPaddingObject && SlotPaddingObject->IsValid())
				{
					bHasEffectivePadding = ParsePaddingObject(*SlotPaddingObject, EffectivePadding);
				}
			}

			// Global box-panel spacing: apply as inter-item slot padding.
			if (PanelSpacing > 0.0f && ChildIndex < ValidChildren.Num() - 1)
			{
				if (bIsHorizontalBox)
				{
					EffectivePadding.Right += PanelSpacing;
					bHasEffectivePadding = true;
				}
				else if (bIsVerticalBox)
				{
					EffectivePadding.Bottom += PanelSpacing;
					bHasEffectivePadding = true;
				}
			}

			if (bHasEffectivePadding)
			{
				if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(SlotOwner->Slot))
				{
					HSlot->SetPadding(EffectivePadding);
				}
				else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(SlotOwner->Slot))
				{
					VSlot->SetPadding(EffectivePadding);
				}
			}
		}
	}

	UE_LOG(LogProjectWidgetLoader, Verbose, TEXT("Panel %s: added %d children"), *Widget->GetName(), ChildCount);
}

// =============================================================================
// Slot Properties (delegate to internal)
// =============================================================================

void UProjectWidgetLayoutLoader::ApplyCanvasPanelSlot(UWidget* Widget, UPanelWidget* Parent, const TSharedPtr<FJsonObject>& JsonObject)
{
	LayoutSlotConfig::ApplyCanvasPanelSlot(Widget, Parent, JsonObject);
}

// =============================================================================
// Widget Property Appliers (delegate to internal)
// =============================================================================

void UProjectWidgetLayoutLoader::ApplyButtonProperties(UButton* Button, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
{
	LayoutPropertyAppliers::ApplyButtonProperties(Button, JsonObject, Theme);
}

void UProjectWidgetLayoutLoader::ApplyTextBlockProperties(UTextBlock* TextBlock, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
{
	LayoutPropertyAppliers::ApplyTextBlockProperties(TextBlock, JsonObject, Theme);
}

void UProjectWidgetLayoutLoader::ApplyImageProperties(UImage* Image, const TSharedPtr<FJsonObject>& JsonObject)
{
	LayoutPropertyAppliers::ApplyImageProperties(Image, JsonObject);
}

void UProjectWidgetLayoutLoader::ApplyProgressBarProperties(UProgressBar* ProgressBar, const TSharedPtr<FJsonObject>& JsonObject, UProjectUIThemeData* Theme)
{
	LayoutPropertyAppliers::ApplyProgressBarProperties(ProgressBar, JsonObject, Theme);
}

void UProjectWidgetLayoutLoader::ApplyButtonVariantStyle(UButton* Button, const FString& Variant, UProjectUIThemeData* Theme)
{
	LayoutPropertyAppliers::ApplyButtonVariantStyle(Button, Variant, Theme);
}

// =============================================================================
// Parsing Helpers
// =============================================================================

FVector2D UProjectWidgetLayoutLoader::ParseVector2D(const TSharedPtr<FJsonObject>& JsonObject)
{
	return LayoutSlotConfig::ParseVector2D(JsonObject);
}

FLinearColor UProjectWidgetLayoutLoader::ParseColor(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		return FLinearColor::White;
	}

	double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
	JsonObject->TryGetNumberField(TEXT("r"), R);
	JsonObject->TryGetNumberField(TEXT("g"), G);
	JsonObject->TryGetNumberField(TEXT("b"), B);
	JsonObject->TryGetNumberField(TEXT("a"), A);

	return FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
}

// =============================================================================
// Legacy API Wrappers
// =============================================================================

FLinearColor UProjectWidgetLayoutLoader::ResolveThemeColor(const FString& ColorName, UProjectUIThemeData* Theme)
{
	return LayoutThemeResolver::GetColor(ColorName, Theme);
}

FSlateFontInfo UProjectWidgetLayoutLoader::ResolveThemeFont(const FString& FontName, UProjectUIThemeData* Theme)
{
	return LayoutThemeResolver::GetFont(FontName, Theme);
}

UTexture2D* UProjectWidgetLayoutLoader::LoadTexture(const FString& TexturePath)
{
	return LayoutPropertyAppliers::LoadTexture(TexturePath);
}
