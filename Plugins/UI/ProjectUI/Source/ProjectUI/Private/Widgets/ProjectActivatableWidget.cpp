// Copyright ALIS. All Rights Reserved.

#include "Widgets/ProjectActivatableWidget.h"
#include "MVVM/ProjectViewModel.h"
#include "Theme/ProjectUIThemeManager.h"
#include "Theme/ProjectUIThemeData.h"

DEFINE_LOG_CATEGORY(LogProjectActivatableWidget);

UProjectActivatableWidget::UProjectActivatableWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TOptional<FUIInputConfig> UProjectActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputModeOverride)
	{
	case EProjectWidgetInputMode::Game:
		// Game input only - cursor hidden, input goes to game
		return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently);

	case EProjectWidgetInputMode::Menu:
		// Menu input only - cursor visible, blocks game input
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);

	case EProjectWidgetInputMode::GameAndMenu:
		// Both game and UI can receive input
		return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture);

	case EProjectWidgetInputMode::Default:
	default:
		// No override - use CommonUI default behavior
		return Super::GetDesiredInputConfig();
	}
}

void UProjectActivatableWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bIsConstructed = true;

	// Bind to theme changes
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UProjectUIThemeManager* ThemeManager = GI->GetSubsystem<UProjectUIThemeManager>())
		{
			ThemeManager->OnThemeChanged.AddUniqueDynamic(this, &UProjectActivatableWidget::HandleThemeChanged);

			// Apply current theme
			if (UProjectUIThemeData* CurrentTheme = ThemeManager->GetActiveTheme())
			{
				OnThemeChanged(CurrentTheme);
			}
		}
	}

	// Apply ViewModel if already set before construction
	TryApplyViewModel();

	UE_LOG(LogProjectActivatableWidget, Verbose, TEXT("NativeConstruct: %s"),
		*GetClass()->GetName());
}

void UProjectActivatableWidget::NativeDestruct()
{
	// Unbind from ViewModel
	if (ViewModel)
	{
		ViewModel->OnPropertyChanged.RemoveDynamic(this, &UProjectActivatableWidget::HandleViewModelPropertyChanged);
	}

	// Unbind from theme
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UProjectUIThemeManager* ThemeManager = GI->GetSubsystem<UProjectUIThemeManager>())
		{
			ThemeManager->OnThemeChanged.RemoveDynamic(this, &UProjectActivatableWidget::HandleThemeChanged);
		}
	}

	bIsConstructed = false;

	Super::NativeDestruct();
}

void UProjectActivatableWidget::SetViewModel(UProjectViewModel* InViewModel)
{
	if (ViewModel == InViewModel)
	{
		return;
	}

	UProjectViewModel* OldViewModel = ViewModel;

	// Unbind from old ViewModel
	if (OldViewModel)
	{
		OldViewModel->OnPropertyChanged.RemoveDynamic(this, &UProjectActivatableWidget::HandleViewModelPropertyChanged);
	}

	ViewModel = InViewModel;

	// Bind to new ViewModel
	if (ViewModel)
	{
		ViewModel->OnPropertyChanged.AddUniqueDynamic(this, &UProjectActivatableWidget::HandleViewModelPropertyChanged);
	}

	// Notify derived classes
	OnViewModelChanged(OldViewModel, ViewModel);

	// Apply if already constructed
	TryApplyViewModel();

	UE_LOG(LogProjectActivatableWidget, Verbose, TEXT("SetViewModel: %s -> %s"),
		OldViewModel ? *OldViewModel->GetClass()->GetName() : TEXT("null"),
		ViewModel ? *ViewModel->GetClass()->GetName() : TEXT("null"));
}

void UProjectActivatableWidget::TryApplyViewModel()
{
	// Only refresh if both constructed and ViewModel set
	// This guards against timing issues where SetViewModel is called before NativeConstruct
	if (bIsConstructed && ViewModel)
	{
		RefreshFromViewModel();
	}
}

void UProjectActivatableWidget::RefreshFromViewModel_Implementation()
{
	// Default: no-op. Derived classes override to update UI elements.
}

void UProjectActivatableWidget::OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel)
{
	// Default: no-op. Derived classes override to bind to ViewModel properties.
}

void UProjectActivatableWidget::HandleViewModelPropertyChanged(FName PropertyName)
{
	RefreshFromViewModel();
}

void UProjectActivatableWidget::OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme)
{
	// Default: no-op. Derived classes override to apply theme.
}

void UProjectActivatableWidget::HandleThemeChanged(UProjectUIThemeData* NewTheme)
{
	OnThemeChanged(NewTheme);
}

UProjectUIThemeData* UProjectActivatableWidget::GetActiveTheme() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UProjectUIThemeManager* ThemeManager = GI->GetSubsystem<UProjectUIThemeManager>())
		{
			return ThemeManager->GetActiveTheme();
		}
	}
	return nullptr;
}
