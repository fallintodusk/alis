// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_MapBrowser.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "ViewModels/ProjectMapListViewModel.h"
#include "Components/Widget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

DEFINE_LOG_CATEGORY_STATIC(LogW_MapBrowser, Log, All);

UW_MapBrowser::UW_MapBrowser(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set JSON config path - base class handles loading and hot reload automatically
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectMenuMain"), TEXT("MapBrowser.json"));
}

FReply UW_MapBrowser::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	// Enhanced logging for TextInputFramework.dll crash debugging
	UE_LOG(LogW_MapBrowser, Display, TEXT(""));
	UE_LOG(LogW_MapBrowser, Display, TEXT(">>> W_MapBrowser::NativeOnFocusReceived <<<"));
	UE_LOG(LogW_MapBrowser, Display, TEXT("  FocusCause=%d, UserIndex=%d"),
		static_cast<int32>(InFocusEvent.GetCause()),
		InFocusEvent.GetUser());

	// Check if search box has focus (this is the TSF trigger point)
	if (CachedSearchBox && CachedSearchBox->HasKeyboardFocus())
	{
		UE_LOG(LogW_MapBrowser, Warning, TEXT("  !!! SearchBox has keyboard focus - TSF ACTIVE !!!"));
		OnSearchBoxFocusReceived();
	}

	// Call parent (which calls the base UProjectUserWidget logging)
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void UW_MapBrowser::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	// Enhanced logging for TextInputFramework.dll crash debugging
	UE_LOG(LogW_MapBrowser, Display, TEXT(""));
	UE_LOG(LogW_MapBrowser, Display, TEXT("<<< W_MapBrowser::NativeOnFocusLost <<<"));
	UE_LOG(LogW_MapBrowser, Display, TEXT("  FocusCause=%d, UserIndex=%d"),
		static_cast<int32>(InFocusEvent.GetCause()),
		InFocusEvent.GetUser());

	// Check if search box is losing focus (TSF deactivation point)
	if (CachedSearchBox)
	{
		bool bHadFocus = CachedSearchBox->HasKeyboardFocus();
		UE_LOG(LogW_MapBrowser, Display, TEXT("  SearchBox.HasKeyboardFocus=%s"),
			bHadFocus ? TEXT("true") : TEXT("false"));

		if (bHadFocus)
		{
			UE_LOG(LogW_MapBrowser, Warning, TEXT("  !!! SearchBox losing focus - TSF DEACTIVATING !!!"));
			OnSearchBoxFocusLost();
		}
	}

	// Call parent (which calls the base UProjectUserWidget logging)
	Super::NativeOnFocusLost(InFocusEvent);
}

void UW_MapBrowser::NativeConstruct()
{
	UE_LOG(LogW_MapBrowser, Display, TEXT("[W_MapBrowser] NativeConstruct"));

	// Create ViewModel BEFORE calling Super (which loads JSON and calls BindCallbacks)
	MapListViewModel = NewObject<UProjectMapListViewModel>(this);

	// Base class handles: JSON loading, BindCallbacks(), ConfigWatcher setup
	Super::NativeConstruct();

	// After layout is loaded, populate map list
	UpdateMapList();
}

void UW_MapBrowser::NativeDestruct()
{
	// Base class handles: ConfigWatcher cleanup
	Super::NativeDestruct();
}

void UW_MapBrowser::BindCallbacks()
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

		if (WidgetName.Contains(TEXT("SearchBox")))
		{
			if (UEditableTextBox* SearchBox = Cast<UEditableTextBox>(Child))
			{
				// Cache reference for focus tracking
				CachedSearchBox = SearchBox;

				// Bind text change event (AddUniqueDynamic for safety on layout reload)
				SearchBox->OnTextChanged.AddUniqueDynamic(this, &UW_MapBrowser::OnSearchTextChanged);
				UE_LOG(LogW_MapBrowser, Display, TEXT("Bound OnSearchTextChanged to %s"), *WidgetName);

				// Log widget state for TextInputFramework debugging
				UE_LOG(LogW_MapBrowser, Display, TEXT("SearchBox widget state: Visibility=%d"),
					static_cast<int32>(SearchBox->GetVisibility()));
			}
		}
		else if (WidgetName.Contains(TEXT("Button_Load")))
		{
			if (UButton* Button = Cast<UButton>(Child))
			{
				Button->OnClicked.AddUniqueDynamic(this, &UW_MapBrowser::OnLoadClicked);
				UE_LOG(LogW_MapBrowser, Display, TEXT("Bound OnLoadClicked to %s"), *WidgetName);
			}
		}
		else if (WidgetName.Contains(TEXT("Button_Back")))
		{
			if (UButton* Button = Cast<UButton>(Child))
			{
				Button->OnClicked.AddUniqueDynamic(this, &UW_MapBrowser::OnBackClicked);
				UE_LOG(LogW_MapBrowser, Display, TEXT("Bound OnBackClicked to %s"), *WidgetName);
			}
		}
	}
}

void UW_MapBrowser::UpdateMapList()
{
	if (!MapListViewModel)
	{
		UE_LOG(LogW_MapBrowser, Warning, TEXT("UpdateMapList: MapListViewModel is null"));
		return;
	}

	// Refresh the map list from ViewModel
	MapListViewModel->RefreshMapList();

	UE_LOG(LogW_MapBrowser, Display, TEXT("UpdateMapList: %d maps available, selected index: %d"),
		MapListViewModel->GetMapCount(), MapListViewModel->GetSelectedIndex());
}

void UW_MapBrowser::UpdateSelectedMapDetails()
{
	if (!MapListViewModel)
	{
		return;
	}
	// TODO: Update selected map details panel from ViewModel
	UE_LOG(LogW_MapBrowser, Display, TEXT("UpdateSelectedMapDetails called (stub)"));
}

void UW_MapBrowser::OnSearchTextChanged(const FText& NewText)
{
	CurrentSearchFilter = NewText.ToString();
	UE_LOG(LogW_MapBrowser, Display, TEXT("Search filter changed: %s"), *CurrentSearchFilter);

	if (MapListViewModel)
	{
		MapListViewModel->SetSearchFilter(CurrentSearchFilter);
	}
}

void UW_MapBrowser::OnMapSelected(int32 MapIndex)
{
	UE_LOG(LogW_MapBrowser, Display, TEXT("Map selected: Index %d"), MapIndex);

	if (MapListViewModel)
	{
		MapListViewModel->SelectMap(MapIndex);
	}

	UpdateSelectedMapDetails();
}

void UW_MapBrowser::OnLoadClicked()
{
	UE_LOG(LogW_MapBrowser, Display, TEXT(""));
	UE_LOG(LogW_MapBrowser, Display, TEXT("========================================"));
	UE_LOG(LogW_MapBrowser, Display, TEXT("=== W_MapBrowser::OnLoadClicked ==="));
	UE_LOG(LogW_MapBrowser, Display, TEXT("========================================"));

	if (!MapListViewModel)
	{
		UE_LOG(LogW_MapBrowser, Error, TEXT("  FAILED: MapListViewModel is null!"));
		return;
	}

	if (!MapListViewModel->HasSelection())
	{
		UE_LOG(LogW_MapBrowser, Warning, TEXT("  No map selected, nothing to load"));
		return;
	}

	const FProjectMapEntry& SelectedMap = MapListViewModel->GetSelectedMap();
	UE_LOG(LogW_MapBrowser, Display, TEXT("  Starting load for map: %s"), *SelectedMap.DisplayName);
	UE_LOG(LogW_MapBrowser, Display, TEXT("  GameMode: %s"), *SelectedMap.DefaultGameModeClass);
	UE_LOG(LogW_MapBrowser, Display, TEXT("  Mode: %s"), *SelectedMap.DefaultModeName);

	// Start loading via ViewModel (which uses ILoadingService)
	// Pass empty strings to use map defaults for GameMode and Mode
	const bool bSuccess = MapListViewModel->StartLoadingSelectedMap(TEXT(""), TEXT(""));

	if (bSuccess)
	{
		UE_LOG(LogW_MapBrowser, Display, TEXT("  Loading initiated successfully"));
	}
	else
	{
		UE_LOG(LogW_MapBrowser, Error, TEXT("  Loading failed to start - check ViewModel logs for details"));
	}

	UE_LOG(LogW_MapBrowser, Display, TEXT("========================================"));
}

void UW_MapBrowser::OnBackClicked()
{
	UE_LOG(LogW_MapBrowser, Display, TEXT("OnBackClicked called"));
	// TODO: Navigate back to main menu
}

void UW_MapBrowser::OnMapListChanged()
{
	UpdateMapList();
}

void UW_MapBrowser::OnSelectedMapChanged()
{
	UpdateSelectedMapDetails();
}

void UW_MapBrowser::OnSearchBoxFocusReceived()
{
	// Log focus event for TextInputFramework.dll crash debugging
	UE_LOG(LogW_MapBrowser, Display, TEXT(""));
	UE_LOG(LogW_MapBrowser, Display, TEXT("=== SEARCHBOX FOCUS RECEIVED ==="));
	UE_LOG(LogW_MapBrowser, Display, TEXT("  This event triggers Windows TSF (Text Services Framework)"));
	UE_LOG(LogW_MapBrowser, Display, TEXT("  If crash follows, check IME settings or keyboard layout"));

	if (CachedSearchBox)
	{
		UE_LOG(LogW_MapBrowser, Display, TEXT("  SearchBox.HasKeyboardFocus=%s"),
			CachedSearchBox->HasKeyboardFocus() ? TEXT("true") : TEXT("false"));
	}
}

void UW_MapBrowser::OnSearchBoxFocusLost()
{
	// Log focus event for TextInputFramework.dll crash debugging
	UE_LOG(LogW_MapBrowser, Display, TEXT(""));
	UE_LOG(LogW_MapBrowser, Display, TEXT("=== SEARCHBOX FOCUS LOST ==="));
	UE_LOG(LogW_MapBrowser, Display, TEXT("  TSF deactivation - may trigger TextInputFramework.dll"));

	if (CachedSearchBox)
	{
		UE_LOG(LogW_MapBrowser, Display, TEXT("  SearchBox.HasKeyboardFocus=%s"),
			CachedSearchBox->HasKeyboardFocus() ? TEXT("true") : TEXT("false"));
	}
}
