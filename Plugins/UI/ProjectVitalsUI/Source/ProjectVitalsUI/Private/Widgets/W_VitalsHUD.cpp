// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_VitalsHUD.h"
#include "MVVM/VitalsViewModel.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/OverlaySlot.h"
#include "Theme/ProjectUIThemeData.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Presentation/ProjectUIWidgetBinder.h"

DEFINE_LOG_CATEGORY(LogVitalsHUD);

UW_VitalsHUD::UW_VitalsHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set path to JSON layout config
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectVitalsUI"), TEXT("VitalsHUD.json"));
}

void UW_VitalsHUD::SetViewModel(UProjectViewModel* InViewModel)
{
	UE_LOG(LogVitalsHUD, Log, TEXT("SetViewModel: InViewModel=%s"),
		InViewModel ? *InViewModel->GetClass()->GetName() : TEXT("null"));

	// Widget receives already-initialized ViewModel from UI Factory.
	// Factory calls ViewModel::Initialize(Widget) which resolves PC->Pawn->ASC.
	// Widget should NEVER access game entities directly.
	if (UVitalsViewModel* NewVitalsVM = Cast<UVitalsViewModel>(InViewModel))
	{
		SetVitalsViewModel(NewVitalsVM);
		UE_LOG(LogVitalsHUD, Log, TEXT("SetViewModel: ViewModel bound, ConditionPercent=%.2f"),
			NewVitalsVM->GetConditionPercent());
	}
	else
	{
		UE_LOG(LogVitalsHUD, Warning, TEXT("SetViewModel: Expected VitalsViewModel, got %s"),
			InViewModel ? *InViewModel->GetClass()->GetName() : TEXT("null"));
	}
}

void UW_VitalsHUD::SetVitalsViewModel(UVitalsViewModel* InViewModel)
{
	if (VitalsVM == InViewModel)
	{
		return;
	}

	// Unbind from old ViewModel
	if (VitalsVM)
	{
		VitalsVM->OnPropertyChanged.RemoveDynamic(this, &UW_VitalsHUD::HandleVitalsViewModelPropertyChanged);
	}

	VitalsVM = InViewModel;

	// Bind to new ViewModel
	if (VitalsVM)
	{
		VitalsVM->OnPropertyChanged.AddUniqueDynamic(this, &UW_VitalsHUD::HandleVitalsViewModelPropertyChanged);
		RefreshFromViewModel();
	}
}

void UW_VitalsHUD::NativeConstruct()
{
	UE_LOG(LogVitalsHUD, Log, TEXT(""));
	UE_LOG(LogVitalsHUD, Log, TEXT("========================================"));
	UE_LOG(LogVitalsHUD, Log, TEXT("=== W_VitalsHUD NATIVE CONSTRUCT ==="));
	UE_LOG(LogVitalsHUD, Log, TEXT("========================================"));

	Super::NativeConstruct();

	// Cache widget references from JSON layout using helper (no BindWidget for JSON-built widgets)
	if (RootWidget)
	{
		FProjectUIWidgetBinder Binder(RootWidget, GetClass()->GetName());
		ConditionBar = Binder.FindRequired<UProgressBar>(TEXT("ConditionBar"));
		WarningIcons = Binder.FindRequired<UTextBlock>(TEXT("WarningIcons"));
		ExpandButton = Binder.FindOptional<UButton>(TEXT("ExpandButton"));
		Binder.LogMissingRequired(TEXT("UW_VitalsHUD::NativeConstruct"));

		// Set icon font for warning symbols (game-icons.ttf PUA codepoints)
		if (WarningIcons)
		{
			WarningIcons->SetFont(UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("GameIcon"), CurrentTheme));
		}

		// Bind button here (after caching) - BindCallbacks runs before member pointers are populated
		if (ExpandButton)
		{
			ExpandButton->OnClicked.AddUniqueDynamic(this, &UW_VitalsHUD::HandleExpandClicked);
		}

		UE_LOG(LogVitalsHUD, Log, TEXT("RootWidget: %s (%s) Visibility=%s"),
			*RootWidget->GetName(),
			*RootWidget->GetClass()->GetName(),
			*StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(RootWidget->GetVisibility())));

		FVector2D RootDesired = RootWidget->GetDesiredSize();
		FVector2D RootCached = RootWidget->GetCachedGeometry().GetLocalSize();
		UE_LOG(LogVitalsHUD, Log, TEXT("RootWidget Size: Desired=(%.0f,%.0f) Cached=(%.0f,%.0f)"),
			RootDesired.X, RootDesired.Y, RootCached.X, RootCached.Y);
	}
	else
	{
		UE_LOG(LogVitalsHUD, Error, TEXT("RootWidget is NULL - layout not loaded!"));
	}

	UE_LOG(LogVitalsHUD, Log, TEXT("Widget Components: ConditionBar=%s WarningIcons=%s ExpandButton=%s"),
		ConditionBar ? TEXT("OK") : TEXT("NULL"),
		WarningIcons ? TEXT("OK") : TEXT("NULL"),
		ExpandButton ? TEXT("OK") : TEXT("NULL"));
	UE_LOG(LogVitalsHUD, Log, TEXT("ConfigFilePath: %s"), *ConfigFilePath);

	// Log THIS widget's slot configuration (how HUD container added us)
	UE_LOG(LogVitalsHUD, Log, TEXT("--- THIS WIDGET SLOT INFO ---"));
	if (Slot)
	{
		if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
		{
			// Alignment enums: 0=Left/Top, 1=Center, 2=Right/Bottom, 3=Fill
			EHorizontalAlignment HAlign = OverlaySlot->GetHorizontalAlignment();
			EVerticalAlignment VAlign = OverlaySlot->GetVerticalAlignment();
			const TCHAR* HAlignName = HAlign == HAlign_Fill ? TEXT("Fill") : (HAlign == HAlign_Center ? TEXT("Center") : (HAlign == HAlign_Left ? TEXT("Left") : TEXT("Right")));
			const TCHAR* VAlignName = VAlign == VAlign_Fill ? TEXT("Fill") : (VAlign == VAlign_Center ? TEXT("Center") : (VAlign == VAlign_Top ? TEXT("Top") : TEXT("Bottom")));

			UE_LOG(LogVitalsHUD, Log, TEXT("In OverlaySlot: HAlign=%d (%s), VAlign=%d (%s)"),
				static_cast<int32>(HAlign), HAlignName,
				static_cast<int32>(VAlign), VAlignName);

			if (HAlign != HAlign_Fill || VAlign != VAlign_Fill)
			{
				// This is informational - parent AttachWidgetToSlot sets Fill AFTER NativeConstruct completes
				UE_LOG(LogVitalsHUD, Log, TEXT("Note: Slot align %d,%d at construct (parent will set Fill=3,3 after)"),
					static_cast<int32>(HAlign), static_cast<int32>(VAlign));
			}
			else
			{
				UE_LOG(LogVitalsHUD, Log, TEXT("[OK] OverlaySlot is Fill - widget will expand to parent size"));
			}
		}
		else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			FVector2D SlotSize = CanvasSlot->GetSize();
			UE_LOG(LogVitalsHUD, Log, TEXT("In CanvasPanelSlot: AutoSize=%s SlotSize=(%.0f,%.0f) ZOrder=%d"),
				CanvasSlot->GetAutoSize() ? TEXT("true") : TEXT("false"),
				SlotSize.X, SlotSize.Y,
				CanvasSlot->GetZOrder());
			if (!CanvasSlot->GetAutoSize() && SlotSize.IsNearlyZero())
			{
				UE_LOG(LogVitalsHUD, Warning, TEXT("[ISSUE] CanvasSlot AutoSize=false with 0x0 size - widget INVISIBLE!"));
			}
		}
		else
		{
			UE_LOG(LogVitalsHUD, Log, TEXT("In slot type: %s"), *Slot->GetClass()->GetName());
		}
	}
	else
	{
		UE_LOG(LogVitalsHUD, Log, TEXT("No Slot (is root widget or not added to parent)"));
	}

	// Log parent info
	UWidget* ParentWidget = GetParent();
	UE_LOG(LogVitalsHUD, Log, TEXT("--- PARENT INFO ---"));
	if (ParentWidget)
	{
		FVector2D ParentCached = ParentWidget->GetCachedGeometry().GetLocalSize();
		UE_LOG(LogVitalsHUD, Log, TEXT("Parent: %s (%s) CachedSize=(%.0f,%.0f)"),
			*ParentWidget->GetName(),
			*ParentWidget->GetClass()->GetName(),
			ParentCached.X, ParentCached.Y);
	}
	else
	{
		UE_LOG(LogVitalsHUD, Log, TEXT("Parent: NULL"));
	}

	// Log own geometry
	FVector2D OwnDesired = GetDesiredSize();
	FVector2D OwnCached = GetCachedGeometry().GetLocalSize();
	UE_LOG(LogVitalsHUD, Log, TEXT("--- OWN GEOMETRY ---"));
	UE_LOG(LogVitalsHUD, Log, TEXT("Visibility: %s IsInViewport: %s"),
		*StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(GetVisibility())),
		IsInViewport() ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogVitalsHUD, Log, TEXT("DesiredSize: (%.0f,%.0f) CachedSize: (%.0f,%.0f) %s"),
		OwnDesired.X, OwnDesired.Y,
		OwnCached.X, OwnCached.Y,
		OwnCached.IsNearlyZero() ? TEXT("[ZERO - normal at construct time, check deferred diagnostics]") : TEXT(""));

	UE_LOG(LogVitalsHUD, Log, TEXT("========================================"));
	UE_LOG(LogVitalsHUD, Log, TEXT("=== W_VitalsHUD CONSTRUCT COMPLETE ==="));
	UE_LOG(LogVitalsHUD, Log, TEXT("========================================"));
	UE_LOG(LogVitalsHUD, Log, TEXT(""));
}

void UW_VitalsHUD::NativeDestruct()
{
	if (VitalsVM)
	{
		VitalsVM->OnPropertyChanged.RemoveDynamic(this, &UW_VitalsHUD::HandleVitalsViewModelPropertyChanged);
	}

	Super::NativeDestruct();
}

void UW_VitalsHUD::HandleVitalsViewModelPropertyChanged(FName PropertyName)
{
	// Refresh UI when any VitalsViewModel property changes
	RefreshFromViewModel();
}

// NOTE: BindCallbacks() override removed - binding happens in NativeConstruct after widget caching

void UW_VitalsHUD::OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel)
{
	Super::OnViewModelChanged_Implementation(OldViewModel, NewViewModel);

	// If generic ViewModel changed, check if it's a VitalsViewModel
	if (UVitalsViewModel* NewVitalsVM = Cast<UVitalsViewModel>(NewViewModel))
	{
		SetVitalsViewModel(NewVitalsVM);
	}
}

void UW_VitalsHUD::RefreshFromViewModel_Implementation()
{
	Super::RefreshFromViewModel_Implementation();

	if (!VitalsVM)
	{
		return;
	}

	// Update condition bar percent
	if (ConditionBar)
	{
		ConditionBar->SetPercent(VitalsVM->GetConditionPercent());
	}

	// Update colors and icons
	UpdateConditionBarColor();
	UpdateWarningIcons();
}

void UW_VitalsHUD::OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme)
{
	Super::OnThemeChanged_Implementation(NewTheme);

	CurrentTheme = NewTheme;

	// Reapply colors with new theme
	UpdateConditionBarColor();
}

void UW_VitalsHUD::HandleExpandClicked()
{
	if (VitalsVM)
	{
		VitalsVM->TogglePanel();
	}
}

void UW_VitalsHUD::UpdateConditionBarColor()
{
	if (!ConditionBar || !VitalsVM)
	{
		return;
	}

	FLinearColor BarColor = GetStateColor(VitalsVM->GetConditionState());
	ConditionBar->SetFillColorAndOpacity(BarColor);
}

void UW_VitalsHUD::UpdateWarningIcons()
{
	if (!WarningIcons || !VitalsVM)
	{
		return;
	}

	FString IconText;

	// Build icon string based on warning flags
	// Priority order: Critical > Low > Status effects

	if (VitalsVM->GetbWarnAnyCritical())
	{
		// Critical: show skull
		IconText.AppendChar(ICON_SKULL);
	}
	else if (VitalsVM->GetbWarnAnyLow())
	{
		// Low: show warning triangle
		IconText.AppendChar(ICON_WARNING);
	}

	// Append status effect icons
	if (VitalsVM->GetbIsBleeding())
	{
		IconText.AppendChar(ICON_BLEEDING);
	}
	if (VitalsVM->GetbIsPoisoned())
	{
		IconText.AppendChar(ICON_BIOHAZARD);
	}
	if (VitalsVM->GetRadiationLevel() > 0.f)
	{
		IconText.AppendChar(ICON_RADIOACTIVE);
	}

	WarningIcons->SetText(FText::FromString(IconText));

	// Set color based on severity
	if (CurrentTheme)
	{
		FSlateColor IconColor;
		if (VitalsVM->GetbWarnAnyCritical())
		{
			IconColor = CurrentTheme->Colors.Error;
		}
		else if (VitalsVM->GetbWarnAnyLow() || VitalsVM->GetbHasStatusEffect())
		{
			IconColor = CurrentTheme->Colors.Warning;
		}
		else
		{
			IconColor = CurrentTheme->Colors.TextSecondary;
		}
		WarningIcons->SetColorAndOpacity(IconColor);
	}
}

FLinearColor UW_VitalsHUD::GetStateColor(EVitalState State) const
{
	if (!CurrentTheme)
	{
		// Fallback colors if no theme
		switch (State)
		{
		case EVitalState::OK:       return FLinearColor::Green;
		case EVitalState::Low:      return FLinearColor(1.f, 0.5f, 0.f); // Orange
		case EVitalState::Critical: return FLinearColor::Red;
		case EVitalState::Empty:    return FLinearColor::Red;
		default:                    return FLinearColor::White;
		}
	}

	switch (State)
	{
	case EVitalState::OK:       return CurrentTheme->Colors.Success;
	case EVitalState::Low:      return CurrentTheme->Colors.Warning;
	case EVitalState::Critical: return CurrentTheme->Colors.Error;
	case EVitalState::Empty:    return CurrentTheme->Colors.Error;
	default:                    return CurrentTheme->Colors.TextPrimary;
	}
}
