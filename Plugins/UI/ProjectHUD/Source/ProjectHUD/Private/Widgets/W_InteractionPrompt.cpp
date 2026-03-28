// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_InteractionPrompt.h"
#include "MVVM/InteractionPromptViewModel.h"
#include "Components/TextBlock.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "ProjectWidgetHelpers.h"
#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "Widgets/ProjectRadialProgress.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteractionPrompt, Log, All);

UW_InteractionPrompt::UW_InteractionPrompt(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectHUD"), TEXT("InteractionPrompt.json"));
}

void UW_InteractionPrompt::NativeConstruct()
{
	Super::NativeConstruct();

	// Find text block by name from JSON-built layout
	if (RootWidget)
	{
		PromptText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("PromptText"));
		PromptProgress = UProjectWidgetHelpers::FindWidgetByNameTyped<UProjectRadialProgress>(RootWidget, TEXT("PromptProgress"));
		UE_LOG(LogInteractionPrompt, Log, TEXT("NativeConstruct: RootWidget=%s PromptText=%s"),
			*RootWidget->GetName(),
			PromptText.IsValid() ? *PromptText->GetName() : TEXT("NOT FOUND"));
	}
	else
	{
		UE_LOG(LogInteractionPrompt, Warning, TEXT("NativeConstruct: RootWidget is NULL"));
	}

	if (!PromptText.IsValid())
	{
		UE_LOG(LogInteractionPrompt, Warning, TEXT("W_InteractionPrompt: PromptText not found in layout"));
	}

	// Start hidden (no focus yet)
	SetVisibility(ESlateVisibility::Collapsed);

	// If ViewModel already set, refresh display
	if (InteractionVM)
	{
		RefreshFromViewModel();
	}
}

void UW_InteractionPrompt::NativeDestruct()
{
	// Clean up ViewModel (unbind and shutdown to remove service delegates)
	if (InteractionVM)
	{
		InteractionVM->OnPropertyChangedNative.RemoveAll(this);
		InteractionVM->Shutdown();
	}

	Super::NativeDestruct();
}

void UW_InteractionPrompt::SetInteractionViewModel(UInteractionPromptViewModel* InViewModel)
{
	// Unbind from old ViewModel
	if (InteractionVM)
	{
		InteractionVM->OnPropertyChangedNative.RemoveAll(this);
	}

	InteractionVM = InViewModel;

	// Bind to new ViewModel
	if (InteractionVM)
	{
		InteractionVM->OnPropertyChangedNative.AddUObject(this, &UW_InteractionPrompt::HandleViewModelPropertyChanged);
		RefreshFromViewModel();
		UE_LOG(LogInteractionPrompt, Log, TEXT("SetInteractionViewModel: Bound to ViewModel"));
	}
}

void UW_InteractionPrompt::HandleViewModelPropertyChanged(FName PropertyName)
{
	// Refresh on any relevant property change
	if (PropertyName == TEXT("bHasFocus")
		|| PropertyName == TEXT("FormattedPrompt")
		|| PropertyName == TEXT("bShowProgress")
		|| PropertyName == TEXT("ProgressPercent"))
	{
		RefreshFromViewModel();
	}
}

void UW_InteractionPrompt::RefreshFromViewModel()
{
	if (!InteractionVM)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bHasFocus = InteractionVM->GetbHasFocus();

	if (bHasFocus)
	{
		// Show prompt with formatted text
		if (PromptText.IsValid())
		{
			PromptText->SetText(InteractionVM->GetFormattedPrompt());
		}
		if (PromptProgress.IsValid())
		{
			PromptProgress->SetPercent(InteractionVM->GetProgressPercent());
			PromptProgress->SetVisibility(
				InteractionVM->GetbShowProgress()
					? ESlateVisibility::Visible
					: ESlateVisibility::Collapsed);
		}
		SetVisibility(ESlateVisibility::HitTestInvisible);

		PROJECT_UI_DEBUG(Info, TEXT("InteractionPrompt SHOW: Label='%s'"),
			*InteractionVM->GetFocusLabel().ToString());
	}
	else
	{
		// Hide prompt
		if (PromptProgress.IsValid())
		{
			PromptProgress->SetVisibility(ESlateVisibility::Collapsed);
			PromptProgress->SetPercent(0.0f);
		}
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
