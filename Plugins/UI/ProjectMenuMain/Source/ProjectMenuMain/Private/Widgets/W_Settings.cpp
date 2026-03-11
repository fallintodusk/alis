// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_Settings.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "ProjectWidgetHelpers.h"
#include "ViewModels/ProjectSettingsViewModel.h"
#include "Components/Widget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"

DEFINE_LOG_CATEGORY_STATIC(LogW_Settings, Log, All);

UW_Settings::UW_Settings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set JSON config path - base class handles loading and hot reload automatically
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectMenuMain"), TEXT("Settings.json"));
}

void UW_Settings::NativeConstruct()
{
	UE_LOG(LogW_Settings, Display, TEXT("[W_Settings] NativeConstruct"));

	// Create ViewModel BEFORE calling Super (which loads JSON and calls BindCallbacks)
	// SettingsViewModel = NewObject<UProjectSettingsViewModel>(this);

	// Default category (set before base class calls BindCallbacks)
	CurrentCategory = TEXT("Graphics");

	// Base class handles: JSON loading, BindCallbacks(), ConfigWatcher setup
	Super::NativeConstruct();

	// After layout is loaded, show default category
	ShowCategory(CurrentCategory);
}

void UW_Settings::NativeDestruct()
{
	// Base class handles: ConfigWatcher cleanup
	Super::NativeDestruct();
}

void UW_Settings::BindCallbacks()
{
	if (!RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = Cast<UCanvasPanel>(RootWidget);
	if (!Canvas)
	{
		return;
	}

	for (int32 i = 0; i < Canvas->GetChildrenCount(); ++i)
	{
		UWidget* Child = Canvas->GetChildAt(i);
		if (!Child) continue;

		FString WidgetName = Child->GetName();

		// Action buttons (AddUniqueDynamic for safety on layout reload)
		if (WidgetName.Contains(TEXT("Button_Apply")))
		{
			if (UButton* Button = Cast<UButton>(Child))
			{
				Button->OnClicked.AddUniqueDynamic(this, &UW_Settings::OnApplyClicked);
				UE_LOG(LogW_Settings, Display, TEXT("Bound OnApplyClicked to %s"), *WidgetName);
			}
		}
		else if (WidgetName.Contains(TEXT("Button_Reset")))
		{
			if (UButton* Button = Cast<UButton>(Child))
			{
				Button->OnClicked.AddUniqueDynamic(this, &UW_Settings::OnResetClicked);
				UE_LOG(LogW_Settings, Display, TEXT("Bound OnResetClicked to %s"), *WidgetName);
			}
		}
		else if (WidgetName.Contains(TEXT("Button_Default")))
		{
			if (UButton* Button = Cast<UButton>(Child))
			{
				Button->OnClicked.AddUniqueDynamic(this, &UW_Settings::OnDefaultClicked);
				UE_LOG(LogW_Settings, Display, TEXT("Bound OnDefaultClicked to %s"), *WidgetName);
			}
		}
		else if (WidgetName.Contains(TEXT("Button_Back")))
		{
			if (UButton* Button = Cast<UButton>(Child))
			{
				Button->OnClicked.AddUniqueDynamic(this, &UW_Settings::OnBackClicked);
				UE_LOG(LogW_Settings, Display, TEXT("Bound OnBackClicked to %s"), *WidgetName);
			}
		}
		// Tab buttons
		else if (WidgetName.Contains(TEXT("Button_GraphicsTab")))
		{
			if (UButton* Button = Cast<UButton>(Child))
			{
				Button->OnClicked.AddUniqueDynamic(this, &UW_Settings::OnGraphicsTabClicked);
				UE_LOG(LogW_Settings, Display, TEXT("Bound OnGraphicsTabClicked to %s"), *WidgetName);
			}
		}
		else if (WidgetName.Contains(TEXT("Button_AudioTab")))
		{
			if (UButton* Button = Cast<UButton>(Child))
			{
				Button->OnClicked.AddUniqueDynamic(this, &UW_Settings::OnAudioTabClicked);
				UE_LOG(LogW_Settings, Display, TEXT("Bound OnAudioTabClicked to %s"), *WidgetName);
			}
		}
		else if (WidgetName.Contains(TEXT("Button_GameplayTab")))
		{
			if (UButton* Button = Cast<UButton>(Child))
			{
				Button->OnClicked.AddUniqueDynamic(this, &UW_Settings::OnGameplayTabClicked);
				UE_LOG(LogW_Settings, Display, TEXT("Bound OnGameplayTabClicked to %s"), *WidgetName);
			}
		}
	}
}

// Action callbacks
void UW_Settings::OnApplyClicked()
{
	UE_LOG(LogW_Settings, Display, TEXT("OnApplyClicked - applying settings"));
	if (SettingsViewModel) SettingsViewModel->ApplySettings();
}

void UW_Settings::OnResetClicked()
{
	UE_LOG(LogW_Settings, Display, TEXT("OnResetClicked - resetting to current settings"));
	if (SettingsViewModel) SettingsViewModel->ResetToCurrentSettings();
}

void UW_Settings::OnDefaultClicked()
{
	UE_LOG(LogW_Settings, Display, TEXT("OnDefaultClicked - resetting to defaults"));
	if (SettingsViewModel) SettingsViewModel->ResetToDefaults();
}

void UW_Settings::OnBackClicked()
{
	UE_LOG(LogW_Settings, Display, TEXT("OnBackClicked - returning to main menu"));
	// TODO: Integrate with navigation system
}

// Tab callbacks
void UW_Settings::OnGraphicsTabClicked()
{
	UE_LOG(LogW_Settings, Display, TEXT("OnGraphicsTabClicked"));
	ShowCategory(TEXT("Graphics"));
}

void UW_Settings::OnAudioTabClicked()
{
	UE_LOG(LogW_Settings, Display, TEXT("OnAudioTabClicked"));
	ShowCategory(TEXT("Audio"));
}

void UW_Settings::OnGameplayTabClicked()
{
	UE_LOG(LogW_Settings, Display, TEXT("OnGameplayTabClicked"));
	ShowCategory(TEXT("Gameplay"));
}

void UW_Settings::ShowCategory(const FString& InCategoryName)
{
	CurrentCategory = InCategoryName;

	if (!RootWidget)
	{
		return;
	}

	TArray<FString> CategoryNames = { TEXT("Graphics"), TEXT("Audio"), TEXT("Gameplay") };

	// Hide all, show selected
	for (const FString& Category : CategoryNames)
	{
		FString PanelName = FString::Printf(TEXT("%sPanel"), *Category);
		UWidget* Panel = UProjectWidgetHelpers::FindWidgetByName(RootWidget, FName(*PanelName), true, true);

		if (Panel)
		{
			bool bShow = (Category == InCategoryName);
			Panel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			if (bShow)
			{
				UE_LOG(LogW_Settings, Display, TEXT("Showing category: %s"), *InCategoryName);
			}
		}
	}
}
