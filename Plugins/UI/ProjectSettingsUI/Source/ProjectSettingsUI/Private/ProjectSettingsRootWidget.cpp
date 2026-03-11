// Copyright ALIS. All Rights Reserved.

#include "ProjectSettingsRootWidget.h"
#include "ProjectSettingsService.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/PanelWidget.h"
#include "Components/Slider.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Misc/FileHelper.h"
#include "ProjectWidgetHelpers.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Theme/ProjectUIThemeManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectSettings, Log, All);

namespace SettingsRoot
{
	static TMap<FString, FString> BuildActionMap(const FString& ConfigFilePath)
	{
		TMap<FString, FString> ButtonActions;

		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *ConfigFilePath))
		{
			return ButtonActions;
		}

		TSharedPtr<FJsonObject> Parsed;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
		{
			return ButtonActions;
		}

		const TSharedPtr<FJsonObject> RootObject = Parsed->GetObjectField(TEXT("root"));

		auto Collect = [&](const TSharedPtr<FJsonObject>& Node, const auto& Self) -> void
		{
			if (!Node.IsValid())
			{
				return;
			}

			FString Type;
			Node->TryGetStringField(TEXT("type"), Type);

			if (Type.Equals(TEXT("Button")))
			{
				FString Name;
				FString Action;
				if (Node->TryGetStringField(TEXT("name"), Name) && Node->TryGetStringField(TEXT("action"), Action))
				{
					ButtonActions.Add(Name, Action);
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
			if (Node->TryGetArrayField(TEXT("children"), Children))
			{
				for (const TSharedPtr<FJsonValue>& ChildValue : *Children)
				{
					Self(ChildValue->AsObject(), Self);
				}
			}
		};

		Collect(RootObject, Collect);
		return ButtonActions;
	}
}

UProjectSettingsRootWidget::UProjectSettingsRootWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectSettingsUI"), TEXT("Settings.json"));
}

void UProjectSettingsRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CacheControls();
	PopulateResolutionOptions();

	// Load settings from service and sync to UI
	SavedSettings = UProjectSettingsService::Get()->LoadSettings();
	SyncUIFromSettings(SavedSettings);

	ShowGraphics();
}

void UProjectSettingsRootWidget::BindCallbacks()
{
	const TMap<FString, FString> ButtonActions = SettingsRoot::BuildActionMap(ConfigFilePath);

	TArray<UWidget*> Stack;
	Stack.Add(RootWidget);

	auto BindAction = [this](const FString& Action, UButton* Button) -> bool
	{
		const FString ActionKey = Action.ToLower();

		// Use AddUniqueDynamic for safety on layout reload
		if (ActionKey == TEXT("applysettings"))
		{
			Button->OnClicked.AddUniqueDynamic(this, &UProjectSettingsRootWidget::ApplySettings);
			return true;
		}
		if (ActionKey == TEXT("revertsettings"))
		{
			Button->OnClicked.AddUniqueDynamic(this, &UProjectSettingsRootWidget::RevertSettings);
			return true;
		}
		if (ActionKey == TEXT("resetdefaults"))
		{
			Button->OnClicked.AddUniqueDynamic(this, &UProjectSettingsRootWidget::ResetDefaults);
			return true;
		}
		if (ActionKey == TEXT("tabgraphics"))
		{
			Button->OnClicked.AddUniqueDynamic(this, &UProjectSettingsRootWidget::ShowGraphics);
			return true;
		}
		if (ActionKey == TEXT("tabaudio"))
		{
			Button->OnClicked.AddUniqueDynamic(this, &UProjectSettingsRootWidget::ShowAudio);
			return true;
		}
		if (ActionKey == TEXT("tabgame"))
		{
			Button->OnClicked.AddUniqueDynamic(this, &UProjectSettingsRootWidget::ShowGame);
			return true;
		}

		UE_LOG(LogProjectSettings, Warning, TEXT("No handler mapped for action '%s'"), *Action);
		return false;
	};

	int32 BoundCount = 0;

	while (Stack.Num() > 0)
	{
		UWidget* Current = Stack.Pop(EAllowShrinking::No);
		if (!Current)
		{
			continue;
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Current))
		{
			const int32 ChildCount = Panel->GetChildrenCount();
			for (int32 Index = 0; Index < ChildCount; ++Index)
			{
				if (UWidget* Child = Panel->GetChildAt(Index))
				{
					Stack.Add(Child);
				}
			}
		}

		if (UButton* Button = Cast<UButton>(Current))
		{
			const FString WidgetName = Button->GetName();
			if (const FString* Action = ButtonActions.Find(WidgetName))
			{
				BoundCount += BindAction(*Action, Button) ? 1 : 0;
			}
		}
	}

	UE_LOG(LogProjectSettings, Display, TEXT("Bound %d button callbacks"), BoundCount);
}

void UProjectSettingsRootWidget::CacheControls()
{
	if (!RootWidget)
	{
		return;
	}

	// Tab buttons
	TabGraphicsButton = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(
		RootWidget, TEXT("Button_Tab_Graphics"), true, true);
	TabAudioButton = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(
		RootWidget, TEXT("Button_Tab_Audio"), true, true);
	TabGameButton = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(
		RootWidget, TEXT("Button_Tab_Game"), true, true);

	// Display
	ResolutionCombo = UProjectWidgetHelpers::FindWidgetByNameTyped<UComboBoxString>(
		RootWidget, TEXT("Combo_Resolution"), true, true);
	FullscreenCheckBox = UProjectWidgetHelpers::FindWidgetByNameTyped<UCheckBox>(
		RootWidget, TEXT("CheckBox_Fullscreen"), true, true);
	VSyncCheckBox = UProjectWidgetHelpers::FindWidgetByNameTyped<UCheckBox>(
		RootWidget, TEXT("CheckBox_VSync"), true, true);

	// Scalability
	ShadowCombo = UProjectWidgetHelpers::FindWidgetByNameTyped<UComboBoxString>(
		RootWidget, TEXT("Combo_Shadows"), true, true);
	TextureCombo = UProjectWidgetHelpers::FindWidgetByNameTyped<UComboBoxString>(
		RootWidget, TEXT("Combo_Textures"), true, true);
	EffectsCombo = UProjectWidgetHelpers::FindWidgetByNameTyped<UComboBoxString>(
		RootWidget, TEXT("Combo_Effects"), true, true);
	ViewDistanceCombo = UProjectWidgetHelpers::FindWidgetByNameTyped<UComboBoxString>(
		RootWidget, TEXT("Combo_ViewDistance"), true, true);
	AntiAliasingCombo = UProjectWidgetHelpers::FindWidgetByNameTyped<UComboBoxString>(
		RootWidget, TEXT("Combo_AntiAliasing"), true, true);

	// Populate scalability options
	PopulateScalabilityOptions(ShadowCombo);
	PopulateScalabilityOptions(TextureCombo);
	PopulateScalabilityOptions(EffectsCombo);
	PopulateScalabilityOptions(ViewDistanceCombo);
	PopulateScalabilityOptions(AntiAliasingCombo);

	// Audio
	MasterVolumeSlider = UProjectWidgetHelpers::FindWidgetByNameTyped<USlider>(
		RootWidget, TEXT("Slider_MasterVolume"), true, true);
	MusicVolumeSlider = UProjectWidgetHelpers::FindWidgetByNameTyped<USlider>(
		RootWidget, TEXT("Slider_MusicVolume"), true, true);
	EffectsVolumeSlider = UProjectWidgetHelpers::FindWidgetByNameTyped<USlider>(
		RootWidget, TEXT("Slider_EffectsVolume"), true, true);

	// Game
	InvertMouseCheckBox = UProjectWidgetHelpers::FindWidgetByNameTyped<UCheckBox>(
		RootWidget, TEXT("CheckBox_InvertMouse"), true, true);
	ShowHintsCheckBox = UProjectWidgetHelpers::FindWidgetByNameTyped<UCheckBox>(
		RootWidget, TEXT("CheckBox_ShowHints"), true, true);
}

void UProjectSettingsRootWidget::PopulateResolutionOptions()
{
	if (!ResolutionCombo)
	{
		return;
	}

	ResolutionCombo->ClearOptions();

	UProjectSettingsService* Service = UProjectSettingsService::Get();
	const TArray<FString> Resolutions = Service->GetSupportedResolutions();

	for (const FString& Res : Resolutions)
	{
		ResolutionCombo->AddOption(Res);
	}

	// Ensure current resolution is in list
	const FString CurrentRes = Service->GetCurrentResolution();
	if (ResolutionCombo->FindOptionIndex(CurrentRes) == -1)
	{
		ResolutionCombo->AddOption(CurrentRes);
	}

	UE_LOG(LogProjectSettings, Display, TEXT("Populated %d resolution options"), Resolutions.Num());
}

void UProjectSettingsRootWidget::PopulateScalabilityOptions(UComboBoxString* Combo)
{
	if (!Combo)
	{
		return;
	}

	Combo->ClearOptions();
	Combo->AddOption(TEXT("Low"));
	Combo->AddOption(TEXT("Medium"));
	Combo->AddOption(TEXT("High"));
	Combo->AddOption(TEXT("Epic"));
}

void UProjectSettingsRootWidget::SyncUIFromSettings(const FProjectUserSettings& Settings)
{
	// Display
	if (ResolutionCombo)
	{
		if (ResolutionCombo->FindOptionIndex(Settings.Resolution) == -1)
		{
			ResolutionCombo->AddOption(Settings.Resolution);
		}
		ResolutionCombo->SetSelectedOption(Settings.Resolution);
	}

	if (FullscreenCheckBox)
	{
		FullscreenCheckBox->SetIsChecked(Settings.bFullscreen);
	}

	if (VSyncCheckBox)
	{
		VSyncCheckBox->SetIsChecked(Settings.bVSync);
	}

	// Scalability
	if (ShadowCombo)
	{
		ShadowCombo->SetSelectedOption(FProjectUserSettings::ScalabilityToString(Settings.ShadowQuality));
	}
	if (TextureCombo)
	{
		TextureCombo->SetSelectedOption(FProjectUserSettings::ScalabilityToString(Settings.TextureQuality));
	}
	if (EffectsCombo)
	{
		EffectsCombo->SetSelectedOption(FProjectUserSettings::ScalabilityToString(Settings.EffectsQuality));
	}
	if (ViewDistanceCombo)
	{
		ViewDistanceCombo->SetSelectedOption(FProjectUserSettings::ScalabilityToString(Settings.ViewDistance));
	}
	if (AntiAliasingCombo)
	{
		AntiAliasingCombo->SetSelectedOption(FProjectUserSettings::ScalabilityToString(Settings.AntiAliasing));
	}

	// Audio
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->SetValue(Settings.MasterVolume);
	}
	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->SetValue(Settings.MusicVolume);
	}
	if (EffectsVolumeSlider)
	{
		EffectsVolumeSlider->SetValue(Settings.EffectsVolume);
	}

	// Game
	if (InvertMouseCheckBox)
	{
		InvertMouseCheckBox->SetIsChecked(Settings.bInvertMouseY);
	}
	if (ShowHintsCheckBox)
	{
		ShowHintsCheckBox->SetIsChecked(Settings.bShowHints);
	}

	UE_LOG(LogProjectSettings, Display, TEXT("UI synced from settings"));
}

FProjectUserSettings UProjectSettingsRootWidget::GetPendingSettings() const
{
	FProjectUserSettings Pending;

	// Display
	if (ResolutionCombo)
	{
		Pending.Resolution = ResolutionCombo->GetSelectedOption();
	}
	if (FullscreenCheckBox)
	{
		Pending.bFullscreen = FullscreenCheckBox->IsChecked();
	}
	if (VSyncCheckBox)
	{
		Pending.bVSync = VSyncCheckBox->IsChecked();
	}

	// Scalability
	if (ShadowCombo)
	{
		Pending.ShadowQuality = FProjectUserSettings::StringToScalability(ShadowCombo->GetSelectedOption());
	}
	if (TextureCombo)
	{
		Pending.TextureQuality = FProjectUserSettings::StringToScalability(TextureCombo->GetSelectedOption());
	}
	if (EffectsCombo)
	{
		Pending.EffectsQuality = FProjectUserSettings::StringToScalability(EffectsCombo->GetSelectedOption());
	}
	if (ViewDistanceCombo)
	{
		Pending.ViewDistance = FProjectUserSettings::StringToScalability(ViewDistanceCombo->GetSelectedOption());
	}
	if (AntiAliasingCombo)
	{
		Pending.AntiAliasing = FProjectUserSettings::StringToScalability(AntiAliasingCombo->GetSelectedOption());
	}

	// Audio
	if (MasterVolumeSlider)
	{
		Pending.MasterVolume = MasterVolumeSlider->GetValue();
	}
	if (MusicVolumeSlider)
	{
		Pending.MusicVolume = MusicVolumeSlider->GetValue();
	}
	if (EffectsVolumeSlider)
	{
		Pending.EffectsVolume = EffectsVolumeSlider->GetValue();
	}

	// Game
	if (InvertMouseCheckBox)
	{
		Pending.bInvertMouseY = InvertMouseCheckBox->IsChecked();
	}
	if (ShowHintsCheckBox)
	{
		Pending.bShowHints = ShowHintsCheckBox->IsChecked();
	}

	return Pending;
}

bool UProjectSettingsRootWidget::HasPendingChanges() const
{
	return GetPendingSettings() != SavedSettings;
}

// =============================================================================
// Button Handlers
// =============================================================================

void UProjectSettingsRootWidget::ApplySettings()
{
	const FProjectUserSettings Pending = GetPendingSettings();

	// Compute delta - single source of truth for what changed
	const FProjectSettingsDelta Delta = FProjectSettingsDelta::Compute(SavedSettings, Pending);

	// Apply only changed settings (delta-based, no CVar spam)
	UProjectSettingsService::Get()->Apply(Pending, Delta);

	// Update saved state
	SavedSettings = Pending;
}

void UProjectSettingsRootWidget::RevertSettings()
{
	UE_LOG(LogProjectSettings, Display, TEXT("RevertSettings: Reverting to saved settings"));

	// Reload from service and sync UI
	SavedSettings = UProjectSettingsService::Get()->LoadSettings();
	SyncUIFromSettings(SavedSettings);
}

void UProjectSettingsRootWidget::ResetDefaults()
{
	UE_LOG(LogProjectSettings, Display, TEXT("ResetDefaults: Resetting all settings via service"));

	// Delegate reset to service, get new defaults
	SavedSettings = UProjectSettingsService::Get()->ResetToDefaults();
	SyncUIFromSettings(SavedSettings);
}

void UProjectSettingsRootWidget::ShowGraphics()
{
	ToggleTab(TEXT("Content_Graphics"));
}

void UProjectSettingsRootWidget::ShowAudio()
{
	ToggleTab(TEXT("Content_Audio"));
}

void UProjectSettingsRootWidget::ShowGame()
{
	ToggleTab(TEXT("Content_Game"));
}

void UProjectSettingsRootWidget::ToggleTab(const FString& TargetName)
{
	UWidget* Graphics = UProjectWidgetHelpers::FindWidgetByName(RootWidget, TEXT("Content_Graphics"), true, true);
	UWidget* Audio = UProjectWidgetHelpers::FindWidgetByName(RootWidget, TEXT("Content_Audio"), true, true);
	UWidget* Game = UProjectWidgetHelpers::FindWidgetByName(RootWidget, TEXT("Content_Game"), true, true);

	if (Graphics) Graphics->SetVisibility(TargetName == TEXT("Content_Graphics") ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Audio) Audio->SetVisibility(TargetName == TEXT("Content_Audio") ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Game) Game->SetVisibility(TargetName == TEXT("Content_Game") ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// Update tab button highlighting
	FString ActiveTabButton;
	if (TargetName == TEXT("Content_Graphics"))
	{
		ActiveTabButton = TEXT("Button_Tab_Graphics");
	}
	else if (TargetName == TEXT("Content_Audio"))
	{
		ActiveTabButton = TEXT("Button_Tab_Audio");
	}
	else if (TargetName == TEXT("Content_Game"))
	{
		ActiveTabButton = TEXT("Button_Tab_Game");
	}

	UpdateTabHighlight(ActiveTabButton);
}

void UProjectSettingsRootWidget::UpdateTabHighlight(const FString& ActiveTabButton)
{
	// Get theme for variant styling
	UProjectUIThemeData* Theme = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UProjectUIThemeManager* ThemeManager = GI->GetSubsystem<UProjectUIThemeManager>())
		{
			Theme = ThemeManager->GetActiveTheme();
		}
	}

	// Define tab button mappings
	struct FTabButtonMapping
	{
		FString ButtonName;
		UButton* Button;
	};

	TArray<FTabButtonMapping> TabMappings =
	{
		{ TEXT("Button_Tab_Graphics"), TabGraphicsButton },
		{ TEXT("Button_Tab_Audio"), TabAudioButton },
		{ TEXT("Button_Tab_Game"), TabGameButton }
	};

	// Update all tab buttons - Primary for active, Secondary for inactive
	for (const FTabButtonMapping& Mapping : TabMappings)
	{
		if (Mapping.Button)
		{
			const bool bIsActive = (Mapping.ButtonName == ActiveTabButton);
			const FString Variant = bIsActive ? TEXT("Primary") : TEXT("Secondary");
			UProjectWidgetLayoutLoader::ApplyButtonVariantStyle(Mapping.Button, Variant, Theme);
		}
	}
}
