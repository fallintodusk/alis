// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_MainMenu.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "ProjectWidgetHelpers.h"
#include "ViewModels/ProjectMenuViewModel.h"
#include "Components/Widget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Dom/JsonObject.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectSettingsRootWidget.h"
#include "Theme/ProjectUIThemeManager.h"
#include "Theme/ProjectUIThemeData.h"
#include "Dialogs/ProjectDialogWidget.h"
#include "Subsystems/MenuMainComposerSubsystem.h"

// DEBUG: Set to 1 to bypass JSON and show simple red text (tests if UMG renders at all)
// If red text appears: JSON/theme is the problem
// If black screen persists: viewport/widget lifecycle is the problem
#define MAIN_MENU_DEBUG_MODE 0

DEFINE_LOG_CATEGORY_STATIC(LogW_MainMenu, Log, All);

UW_MainMenu::UW_MainMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set JSON config path - base class handles loading and hot reload automatically
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectMenuMain"), TEXT("MainMenu.json"));
	// NativeTick is called automatically for widgets in viewport
}

void UW_MainMenu::NativeConstruct()
{
	UE_LOG(LogW_MainMenu, Display, TEXT("[W_MainMenu] NativeConstruct"));

	// Create ViewModel BEFORE calling Super (which loads JSON and calls BindCallbacks)
	MenuViewModel = NewObject<UProjectMenuViewModel>(this);
	if (MenuViewModel)
	{
		// Subscribe to ViewModel property changes
		MenuViewModel->OnPropertyChangedNative.AddLambda([this](FName PropertyName)
		{
			if (PropertyName == GET_MEMBER_NAME_CHECKED(UProjectMenuViewModel, CurrentScreen))
			{
				OnCurrentScreenChanged();
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UProjectMenuViewModel, bShowQuitConfirmation))
			{
				OnQuitConfirmationChanged();
			}
		});
		MenuViewModel->Initialize(GetGameInstance());
	}

	// Base class handles: JSON loading, BindCallbacks(), ConfigWatcher setup
	Super::NativeConstruct();

	// Initialize shared popup-hosted settings content once layout is available.
	EnsureSettingsPopupInitialized();

#if MAIN_MENU_DEBUG_MODE
	// DEBUG: Override with simple red text to test if UMG renders at all
	UE_LOG(LogW_MainMenu, Warning, TEXT("=== DEBUG MODE: Overriding with simple TextBlock ==="));
	if (WidgetTree)
	{
		UTextBlock* DebugText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DebugText"));
		if (DebugText)
		{
			DebugText->SetText(FText::FromString(TEXT("DEBUG MAIN MENU - IF YOU SEE THIS, UMG WORKS!")));
			DebugText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
			FSlateFontInfo Font = DebugText->GetFont();
			Font.Size = 48;
			DebugText->SetFont(Font);

			// Override the root widget completely
			WidgetTree->RootWidget = DebugText;
			RootWidget = DebugText;

			UE_LOG(LogW_MainMenu, Warning, TEXT("DEBUG: RootWidget set to DebugText (red 48pt text)"));
		}
	}
	else
	{
		UE_LOG(LogW_MainMenu, Error, TEXT("DEBUG: WidgetTree is NULL - this is the problem!"));
	}
#endif
}

void UW_MainMenu::NativeDestruct()
{
	SettingsPopupPresenter.Hide();

	// Base class handles: ConfigWatcher cleanup
	Super::NativeDestruct();
}

void UW_MainMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// One-frame-late diagnostic to log actual post-layout size
	if (bPendingDiagnosticTick)
	{
		bPendingDiagnosticTick = false;

		const FVector2D LocalSize = MyGeometry.GetLocalSize();
		UE_LOG(LogW_MainMenu, Display, TEXT("Post-layout size: %.0f x %.0f"), LocalSize.X, LocalSize.Y);

		if (WidgetTree && WidgetTree->RootWidget)
		{
			UE_LOG(LogW_MainMenu, Display, TEXT("RootWidget: %s"), *WidgetTree->RootWidget->GetName());
		}
	}
}

void UW_MainMenu::BindCallbacks()
{
	if (!RootWidget)
	{
		UE_LOG(LogW_MainMenu, Warning, TEXT("BindCallbacks: RootWidget is null"));
		return;
	}

	// Build ButtonName -> Action map from JSON to avoid hardcoded widget name checks
	TMap<FString, FString> ButtonActions;
	{
		FString JsonString;
		if (FFileHelper::LoadFileToString(JsonString, *ConfigFilePath))
		{
			TSharedPtr<FJsonObject> Parsed;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
			if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
			{
				const TSharedPtr<FJsonObject> RootObject = Parsed->GetObjectField(TEXT("root"));
				auto CollectActions = [&](const TSharedPtr<FJsonObject>& Node, const auto& Self) -> void
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

				CollectActions(RootObject, CollectActions);
			}
			else
			{
				UE_LOG(LogW_MainMenu, Warning, TEXT("BindCallbacks: Failed to parse layout JSON for actions"));
			}
		}
		else
		{
			UE_LOG(LogW_MainMenu, Warning, TEXT("BindCallbacks: Cannot load layout file for actions: %s"), *ConfigFilePath);
		}
	}

	TArray<UWidget*> Stack;
	Stack.Add(RootWidget);

	int32 BoundCount = 0;

	while (Stack.Num() > 0)
	{
		UWidget* Current = Stack.Pop(EAllowShrinking::No);
		if (!Current)
		{
			continue;
		}

		// Recurse into panel children to support nested layouts
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

		// Bind Dialog widgets - route button actions through OnButtonClicked delegate
		if (UProjectDialogWidget* Dialog = Cast<UProjectDialogWidget>(Current))
		{
			Dialog->OnButtonClicked.AddUniqueDynamic(this, &UW_MainMenu::OnDialogButtonClicked);
			UE_LOG(LogW_MainMenu, Display, TEXT("BindCallbacks: Bound dialog widget '%s'"), *Dialog->GetName());
			BoundCount++;
		}

		// Bind buttons based on name tokens (AddUniqueDynamic for safety on layout reload)
		if (UButton* Button = Cast<UButton>(Current))
		{
			const FString WidgetName = Button->GetName();
			if (const FString* Action = ButtonActions.Find(WidgetName))
			{
				const FString ActionKey = Action->ToLower();

				if (ActionKey == TEXT("play"))
				{
					Button->OnClicked.AddUniqueDynamic(this, &UW_MainMenu::NavigateToMapBrowser);
					BoundCount++;
				}
				else if (ActionKey == TEXT("settings"))
				{
					Button->OnClicked.AddUniqueDynamic(this, &UW_MainMenu::NavigateToSettings);
					BoundCount++;
				}
				else if (ActionKey == TEXT("credits"))
				{
					Button->OnClicked.AddUniqueDynamic(this, &UW_MainMenu::NavigateToCredits);
					BoundCount++;
				}
				else if (ActionKey == TEXT("quit"))
				{
					Button->OnClicked.AddUniqueDynamic(this, &UW_MainMenu::RequestQuit);
					BoundCount++;
				}
				else if (ActionKey == TEXT("loadcity17"))
				{
					Button->OnClicked.AddUniqueDynamic(this, &UW_MainMenu::SelectMapCity17);
					BoundCount++;
				}
				else if (ActionKey == TEXT("confirmquit"))
				{
					Button->OnClicked.AddUniqueDynamic(this, &UW_MainMenu::ConfirmQuit);
					BoundCount++;
				}
				else if (ActionKey == TEXT("cancelquit"))
				{
					Button->OnClicked.AddUniqueDynamic(this, &UW_MainMenu::CancelQuit);
					BoundCount++;
				}
				else
				{
					UE_LOG(LogW_MainMenu, Warning, TEXT("BindCallbacks: No handler mapped for action '%s' (widget: %s)"), **Action, *WidgetName);
				}
			}
		}
	}

	UE_LOG(LogW_MainMenu, Display, TEXT("BindCallbacks: Bound %d button callbacks (recursive scan)"), BoundCount);
}

// Navigation callbacks
void UW_MainMenu::NavigateToMapBrowser()
{
	UE_LOG(LogW_MainMenu, Display, TEXT("NavigateToMapBrowser called"));
	if (MenuViewModel) MenuViewModel->NavigateToMapBrowser();
}

void UW_MainMenu::NavigateToSettings()
{
	UE_LOG(LogW_MainMenu, Display, TEXT("NavigateToSettings called"));
	if (MenuViewModel) MenuViewModel->NavigateToSettings();
}

void UW_MainMenu::NavigateToCredits()
{
	UE_LOG(LogW_MainMenu, Display, TEXT("NavigateToCredits called"));
	if (MenuViewModel) MenuViewModel->NavigateToCredits();
}

void UW_MainMenu::BackToMain()
{
	UE_LOG(LogW_MainMenu, Display, TEXT("BackToMain called"));
	if (MenuViewModel) MenuViewModel->NavigateToMain();
}

void UW_MainMenu::SelectMapCity17()
{
	UE_LOG(LogW_MainMenu, Display, TEXT("SelectMapCity17 - requesting via Composer"));

	if (UMenuMainComposerSubsystem* Composer = GetGameInstance()->GetSubsystem<UMenuMainComposerSubsystem>())
	{
		Composer->RequestStartGame(TEXT("City17"), TEXT("SinglePlayer"));
	}
	else
	{
		UE_LOG(LogW_MainMenu, Error, TEXT("SelectMapCity17: MenuMainComposerSubsystem not available"));
	}
}

void UW_MainMenu::RequestQuit()
{
	UE_LOG(LogW_MainMenu, Display, TEXT("RequestQuit called"));
	if (MenuViewModel) MenuViewModel->RequestQuit();
}

void UW_MainMenu::ConfirmQuit()
{
	UE_LOG(LogW_MainMenu, Display, TEXT("ConfirmQuit called - quitting application"));
	if (MenuViewModel) MenuViewModel->ConfirmQuit();
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

void UW_MainMenu::CancelQuit()
{
	UE_LOG(LogW_MainMenu, Display, TEXT("CancelQuit called"));
	if (MenuViewModel) MenuViewModel->CancelQuit();
}

void UW_MainMenu::OnDialogButtonClicked(const FString& Action)
{
	UE_LOG(LogW_MainMenu, Display, TEXT("Dialog button clicked: %s"), *Action);

	const FString ActionKey = Action.ToLower();

	if (ActionKey == TEXT("confirmquit"))
	{
		ConfirmQuit();
	}
	else if (ActionKey == TEXT("cancelquit"))
	{
		CancelQuit();
	}
	else
	{
		UE_LOG(LogW_MainMenu, Warning, TEXT("Unknown dialog action: %s"), *Action);
	}
}

// ViewModel event handlers
void UW_MainMenu::UpdateMenuButtonHighlight(EMenuScreen CurrentScreen)
{
	if (!RootWidget)
	{
		return;
	}

	// Map screen to corresponding button name
	struct FScreenButtonMapping
	{
		EMenuScreen Screen;
		FString ButtonName;
	};

	TArray<FScreenButtonMapping> ButtonMappings =
	{
		{ EMenuScreen::MapBrowser, TEXT("Button_Play") },
		{ EMenuScreen::Settings, TEXT("Button_Settings") },
		{ EMenuScreen::Credits, TEXT("Button_Credits") }
	};

	// Get theme for variant styling
	UProjectUIThemeData* Theme = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UProjectUIThemeManager* ThemeManager = GI->GetSubsystem<UProjectUIThemeManager>())
		{
			Theme = ThemeManager->GetActiveTheme();
		}
	}

	// Update all menu buttons
	for (const FScreenButtonMapping& Mapping : ButtonMappings)
	{
		UWidget* Widget = UProjectWidgetHelpers::FindWidgetByName(RootWidget, FName(*Mapping.ButtonName), true, true);
		if (UButton* Button = Cast<UButton>(Widget))
		{
			const bool bIsActive = (Mapping.Screen == CurrentScreen);
			const FString Variant = bIsActive ? TEXT("Primary") : TEXT("Secondary");
			UProjectWidgetLayoutLoader::ApplyButtonVariantStyle(Button, Variant, Theme);
		}
	}
}

void UW_MainMenu::OnCurrentScreenChanged()
{
	if (!MenuViewModel || !RootWidget)
	{
		return;
	}

	const EMenuScreen CurrentScreen = MenuViewModel->GetCurrentScreen();
	const FString ScreenName = StaticEnum<EMenuScreen>()->GetNameStringByValue(static_cast<int64>(CurrentScreen));

	// Update button highlighting based on current screen
	UpdateMenuButtonHighlight(CurrentScreen);

	struct FScreenPanelMapping
	{
		EMenuScreen Screen;
		FString PanelName;
	};

	TArray<FScreenPanelMapping> ScreenMappings =
	{
		{ EMenuScreen::Main, TEXT("MainMenuPanel") },
		{ EMenuScreen::MapBrowser, TEXT("MapBrowserPanel") },
		{ EMenuScreen::Settings, TEXT("SettingsPanel") },
		{ EMenuScreen::Credits, TEXT("CreditsPanel") }
	};

	// Hide all, show selected
	for (const FScreenPanelMapping& Mapping : ScreenMappings)
	{
		UWidget* Panel = UProjectWidgetHelpers::FindWidgetByName(RootWidget, FName(*Mapping.PanelName), true, true);
		if (Panel)
		{
			bool bShow = (Mapping.Screen == CurrentScreen);
			Panel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			if (bShow)
			{
				UE_LOG(LogW_MainMenu, Display, TEXT("Screen changed to: %s"), *ScreenName);
			}
		}
	}

	SetSettingsPopupVisible(CurrentScreen == EMenuScreen::Settings);
}

void UW_MainMenu::OnQuitConfirmationChanged()
{
	if (!MenuViewModel || !RootWidget)
	{
		return;
	}

	const bool bShowDialog = MenuViewModel->GetbShowQuitConfirmation();
	UWidget* QuitDialog = UProjectWidgetHelpers::FindWidgetByName(RootWidget, TEXT("Dialog_QuitConfirmation"), true, true);

	if (QuitDialog)
	{
		// Dialog handles its own overlay dimming - always render at full opacity
		QuitDialog->SetRenderOpacity(1.0f);
		QuitDialog->SetVisibility(bShowDialog ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		UE_LOG(LogW_MainMenu, Display, TEXT("Quit dialog: %s"), bShowDialog ? TEXT("Shown") : TEXT("Hidden"));
	}
}

void UW_MainMenu::EnsureSettingsPopupInitialized()
{
	if (SettingsContent.IsValid())
	{
		return;
	}

	if (!RootWidget)
	{
		return;
	}

	if (!SettingsHostCanvas.IsValid())
	{
		SettingsHostCanvas = Cast<UCanvasPanel>(UProjectWidgetHelpers::FindWidgetByName(RootWidget, TEXT("SettingsPanel"), true, true));
	}

	UCanvasPanel* HostCanvas = SettingsHostCanvas.Get();
	if (!HostCanvas)
	{
		UE_LOG(LogW_MainMenu, Warning, TEXT("EnsureSettingsPopupInitialized: SettingsPanel canvas not found"));
		return;
	}

	SettingsPopupPresenter.Initialize(HostCanvas, this, UProjectSettingsRootWidget::StaticClass(), 1, 0);
	SettingsPopupPresenter.ShowClickCatcher(false);

	UUserWidget* SettingsWidget = SettingsPopupPresenter.GetPopupWidget<UUserWidget>();
	if (!SettingsWidget)
	{
		UE_LOG(LogW_MainMenu, Warning, TEXT("EnsureSettingsPopupInitialized: Failed to create ProjectSettingsRootWidget"));
		return;
	}

	SettingsContent = SettingsWidget;

	// Keep same visual placement contract as old manual AddChildToCanvas path.
	if (UCanvasPanelSlot* SettingsSlot = Cast<UCanvasPanelSlot>(SettingsWidget->Slot))
	{
		SettingsSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		SettingsSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		SettingsSlot->SetAutoSize(true);
	}
}

void UW_MainMenu::SetSettingsPopupVisible(bool bVisible)
{
	EnsureSettingsPopupInitialized();
	if (!SettingsContent.IsValid())
	{
		return;
	}

	if (bVisible)
	{
		if (UUserWidget* SettingsWidget = SettingsContent.Get())
		{
			SettingsWidget->SetVisibility(ESlateVisibility::Visible);
		}
		SettingsPopupPresenter.ShowClickCatcher(false);
	}
	else
	{
		SettingsPopupPresenter.Hide();
	}
}
