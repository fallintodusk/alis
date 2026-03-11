// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectUILayerHostSubsystem.h"
#include "Subsystems/ProjectUIRegistrySubsystem.h"
#include "Subsystems/ProjectUIFactorySubsystem.h"
#include "Settings/ProjectUISettings.h"
#include "Interfaces/ProjectHUDSlotHost.h"
#include "MVVM/ProjectViewModel.h"
#include "ProjectGameplayTags.h"
#include "Widgets/W_LayerStack.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectUILayerHost, Log, All);

static TAutoConsoleVariable<int32> CVarUIPreloadHud(
	TEXT("ui.preload_hud"),
	1,
	TEXT("If 1, preload HUD shell and critical widgets."),
	ECVF_Default);

void UProjectUILayerHostSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UProjectUIRegistrySubsystem>();
	Collection.InitializeDependency<UProjectUIFactorySubsystem>();
	Super::Initialize(Collection);

	InitializeDefaultLayers();
}

void UProjectUILayerHostSubsystem::Deinitialize()
{
	TeardownAutoVisibilityBindings();
	ActiveWidgets.Empty();
	ActiveLayers.Empty();
	LayerConfigs.Empty();
	HUDLayout.Reset();
	LayerStack.Reset();
	PrimaryPlayerController.Reset();

	Super::Deinitialize();
}

void UProjectUILayerHostSubsystem::InitializeForPlayer(APlayerController* PlayerController, bool bInitializeHUD)
{
	if (!PlayerController)
	{
		return;
	}

	PrimaryPlayerController = PlayerController;

	if (bInitializeHUD)
	{
		EnsureHUDLayout(PlayerController);
	}

	EnsureLayerStack(PlayerController);

	if (bInitializeHUD && !bPreloadComplete && CVarUIPreloadHud.GetValueOnGameThread() != 0)
	{
		PreloadDefinitions(PlayerController);
		bPreloadComplete = true;
	}

	SetupAutoVisibilityBindings();
}

UUserWidget* UProjectUILayerHostSubsystem::ShowDefinition(FName DefinitionId)
{
	UProjectUIRegistrySubsystem* Registry = GetGameInstance()->GetSubsystem<UProjectUIRegistrySubsystem>();
	UProjectUIFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UProjectUIFactorySubsystem>();
	if (!Registry || !Factory)
	{
		return nullptr;
	}

	const FProjectUIDefinition* Definition = Registry->FindDefinition(DefinitionId);
	if (!Definition)
	{
		UE_LOG(LogProjectUILayerHost, Warning, TEXT("ShowDefinition: Missing definition %s"), *DefinitionId.ToString());
		return nullptr;
	}

	// Slot definitions are built by W_HUDLayout.
	if (Definition->SlotTag.IsValid())
	{
		RefreshHUDSlot(Definition->SlotTag);
		return nullptr;
	}

	if (FActiveWidgetEntry* Existing = ActiveWidgets.Find(DefinitionId))
	{
		if (UUserWidget* ExistingWidget = Existing->Widget.Get())
		{
			ExistingWidget->SetVisibility(ESlateVisibility::Visible);
			ActiveLayers.Add(Existing->LayerTag);
			ApplyInputForActiveLayers();
			return ExistingWidget;
		}
	}

	UObject* WorldContext = PrimaryPlayerController.IsValid() ? static_cast<UObject*>(PrimaryPlayerController.Get()) : GetGameInstance();
	FProjectUIWidgetCreateResult Result = Factory->CreateWidgetForDefinition(*Definition, WorldContext);
	if (!Result.Widget)
	{
		return nullptr;
	}

	int32 ViewportPriority = Definition->Priority;
	if (Definition->LayerTag.IsValid())
	{
		if (const FUILayerConfig* Config = LayerConfigs.Find(Definition->LayerTag))
		{
			ViewportPriority += Config->ZOrder;
		}
	}

	Result.Widget->AddToViewport(ViewportPriority);
	ApplyViewportSizing(Result.Widget, *Definition);
	Result.Widget->SetVisibility(ESlateVisibility::Visible);

	FActiveWidgetEntry Entry;
	Entry.Widget = Result.Widget;
	Entry.LayerTag = Definition->LayerTag;
	Entry.RequestedInput = Definition->RequestedInput;
	Entry.Priority = Definition->Priority;
	Entry.SpawnPolicy = Definition->SpawnPolicy;
	ActiveWidgets.Add(DefinitionId, Entry);

	if (Definition->LayerTag.IsValid())
	{
		ActiveLayers.Add(Definition->LayerTag);
	}

	ApplyInputForActiveLayers();
	return Result.Widget;
}

void UProjectUILayerHostSubsystem::HideDefinition(FName DefinitionId)
{
	UE_LOG(LogProjectUILayerHost, Log, TEXT("HideDefinition START - Id=%s"), *DefinitionId.ToString());

	FActiveWidgetEntry* Entry = ActiveWidgets.Find(DefinitionId);
	if (!Entry)
	{
		UE_LOG(LogProjectUILayerHost, Warning, TEXT("HideDefinition - Entry not found for Id=%s"), *DefinitionId.ToString());
		return;
	}

	UE_LOG(LogProjectUILayerHost, Log, TEXT("HideDefinition - Found entry, LayerTag=%s, SpawnPolicy=%d"),
		*Entry->LayerTag.ToString(), static_cast<int32>(Entry->SpawnPolicy));

	if (UUserWidget* Widget = Entry->Widget.Get())
	{
		UE_LOG(LogProjectUILayerHost, Log, TEXT("HideDefinition - Widget=%s, IsVisible=%d, HasKeyboardFocus=%d, HasUserFocus=%d"),
			*Widget->GetName(), Widget->IsVisible(), Widget->HasKeyboardFocus(), Widget->HasUserFocus(nullptr));

		// Clear focus from the widget before hiding to ensure input mode changes properly
		if (Widget->HasKeyboardFocus() || Widget->HasUserFocus(nullptr))
		{
			UE_LOG(LogProjectUILayerHost, Log, TEXT("HideDefinition - Clearing keyboard focus"));
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
			}
		}

		if (Entry->SpawnPolicy == EProjectUISpawnPolicy::Transient)
		{
			UE_LOG(LogProjectUILayerHost, Log, TEXT("HideDefinition - Removing transient widget"));
			Widget->RemoveFromParent();
			ActiveWidgets.Remove(DefinitionId);
		}
		else
		{
			UE_LOG(LogProjectUILayerHost, Log, TEXT("HideDefinition - Setting visibility to Collapsed"));
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}

		UE_LOG(LogProjectUILayerHost, Log, TEXT("HideDefinition - After hide: IsVisible=%d"), Widget->IsVisible());
	}
	else
	{
		UE_LOG(LogProjectUILayerHost, Warning, TEXT("HideDefinition - Widget is null, removing entry"));
		ActiveWidgets.Remove(DefinitionId);
	}

	// Rebuild active layers
	UE_LOG(LogProjectUILayerHost, Log, TEXT("HideDefinition - Rebuilding ActiveLayers, ActiveWidgets count=%d"), ActiveWidgets.Num());
	ActiveLayers.Reset();
	for (const TPair<FName, FActiveWidgetEntry>& Pair : ActiveWidgets)
	{
		if (UUserWidget* ActiveWidget = Pair.Value.Widget.Get())
		{
			const bool bIsVisible = ActiveWidget->IsVisible();
			const bool bHasValidLayer = Pair.Value.LayerTag.IsValid();
			UE_LOG(LogProjectUILayerHost, Log, TEXT("  Widget=%s, IsVisible=%d, LayerTag=%s, Adding=%d"),
				*Pair.Key.ToString(), bIsVisible, *Pair.Value.LayerTag.ToString(), bIsVisible && bHasValidLayer);
			if (bIsVisible && bHasValidLayer)
			{
				ActiveLayers.Add(Pair.Value.LayerTag);
			}
		}
	}

	UE_LOG(LogProjectUILayerHost, Log, TEXT("HideDefinition - ActiveLayers count=%d"), ActiveLayers.Num());
	for (const FGameplayTag& Layer : ActiveLayers)
	{
		UE_LOG(LogProjectUILayerHost, Log, TEXT("  ActiveLayer: %s"), *Layer.ToString());
	}

	ApplyInputForActiveLayers();
	UE_LOG(LogProjectUILayerHost, Log, TEXT("HideDefinition END"));
}

UProjectViewModel* UProjectUILayerHostSubsystem::GetSharedViewModel(TSubclassOf<UProjectViewModel> ViewModelClass) const
{
	if (UProjectUIFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UProjectUIFactorySubsystem>())
	{
		return Factory->GetSharedViewModel(ViewModelClass);
	}

	return nullptr;
}

void UProjectUILayerHostSubsystem::InitializeDefaultLayers()
{
	LayerConfigs.Empty();

	// Game layer - HUD, pass-through input.
	{
		FUILayerConfig Config;
		Config.LayerTag = ProjectTags::UI_Layer_Game;
		Config.ZOrder = 0;
		Config.DefaultInputMode = EProjectWidgetInputMode::Game;
		Config.bBlocksLayersBelow = false;
		LayerConfigs.Add(Config.LayerTag, Config);
	}

	// Menu layer - blocks gameplay input.
	{
		FUILayerConfig Config;
		Config.LayerTag = ProjectTags::UI_Layer_GameMenu;
		Config.ZOrder = 100;
		Config.DefaultInputMode = EProjectWidgetInputMode::Menu;
		Config.bBlocksLayersBelow = true;
		LayerConfigs.Add(Config.LayerTag, Config);
	}

	// Modal layer - topmost blocking.
	{
		FUILayerConfig Config;
		Config.LayerTag = ProjectTags::UI_Layer_Modal;
		Config.ZOrder = 200;
		Config.DefaultInputMode = EProjectWidgetInputMode::Menu;
		Config.bBlocksLayersBelow = true;
		LayerConfigs.Add(Config.LayerTag, Config);
	}

	// Overlay/notification layer - non-blocking.
	{
		FUILayerConfig Config;
		Config.LayerTag = ProjectTags::UI_Layer_Notification;
		Config.ZOrder = 50;
		Config.DefaultInputMode = EProjectWidgetInputMode::Game;
		Config.bBlocksLayersBelow = false;
		LayerConfigs.Add(Config.LayerTag, Config);
	}
}

void UProjectUILayerHostSubsystem::EnsureHUDLayout(APlayerController* PlayerController)
{
	if (HUDLayout.IsValid() || !PlayerController)
	{
		return;
	}

	if (UProjectUIFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UProjectUIFactorySubsystem>())
	{
		const UProjectUISettings* Settings = GetDefault<UProjectUISettings>();
		if (!Settings || !Settings->HUDLayoutClass.IsValid())
		{
			UE_LOG(LogProjectUILayerHost, Warning, TEXT("HUDLayoutClass is not set in ProjectUISettings"));
			return;
		}

		UClass* LayoutClass = Settings->HUDLayoutClass.TryLoadClass<UUserWidget>();
		if (!LayoutClass)
		{
			UE_LOG(LogProjectUILayerHost, Warning, TEXT("HUDLayoutClass failed to load: %s"),
				*Settings->HUDLayoutClass.ToString());
			return;
		}

		if (UUserWidget* Widget = Factory->CreateWidgetByClass(PlayerController, LayoutClass))
		{
			// Use stretch anchors to fill viewport - don't set DesiredSize (conflicts with stretch)
			Widget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
			Widget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
			Widget->AddToViewport(0);
			HUDLayout = Widget;
		}
	}
}

void UProjectUILayerHostSubsystem::EnsureLayerStack(APlayerController* PlayerController)
{
	if (LayerStack.IsValid() || !PlayerController)
	{
		return;
	}

	if (UProjectUIFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UProjectUIFactorySubsystem>())
	{
		if (UUserWidget* Widget = Factory->CreateWidgetByClass(PlayerController, UW_LayerStack::StaticClass()))
		{
			UW_LayerStack* Stack = Cast<UW_LayerStack>(Widget);
			if (Stack)
			{
				Stack->AddToViewport(10);
				LayerStack = Stack;
			}
		}
	}
}

void UProjectUILayerHostSubsystem::PreloadDefinitions(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	UProjectUIRegistrySubsystem* Registry = GetGameInstance()->GetSubsystem<UProjectUIRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	for (const TPair<FName, FProjectUIDefinition>& Pair : Registry->GetAllDefinitions())
	{
		const FProjectUIDefinition& Definition = Pair.Value;
		if (Definition.LoadPolicy != EProjectUILoadPolicy::Preload)
		{
			continue;
		}

		// Slot-based definitions are already built by W_HUDLayout::NativeConstruct.
		// Skip them here to avoid duplicate widget creation.
		if (Definition.SlotTag.IsValid())
		{
			continue;
		}

		ShowDefinition(Definition.Id);
	}
}

// Open/Closed: new features add "auto_visibility" to their ui_definitions.json
// instead of modifying PlayerController. LayerHost creates the Global ViewModel
// eagerly, subscribes to OnPropertyChangedNative, reads bool via FBoolProperty.
void UProjectUILayerHostSubsystem::SetupAutoVisibilityBindings()
{
	UProjectUIRegistrySubsystem* Registry = GetGameInstance()->GetSubsystem<UProjectUIRegistrySubsystem>();
	UProjectUIFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UProjectUIFactorySubsystem>();
	if (!Registry || !Factory)
	{
		return;
	}

	for (const TPair<FName, FProjectUIDefinition>& Pair : Registry->GetAllDefinitions())
	{
		const FProjectUIDefinition& Definition = Pair.Value;
		if (Definition.AutoVisibilityProperty.IsNone())
		{
			continue;
		}

		if (Definition.ViewModelCreationPolicy != EProjectUIViewModelCreationPolicy::Global)
		{
			UE_LOG(LogProjectUILayerHost, Warning,
				TEXT("auto_visibility requires vm_creation=Global: %s"), *Definition.Id.ToString());
			continue;
		}

		UProjectViewModel* VM = Factory->EnsureGlobalViewModel(Definition);
		if (!VM)
		{
			UE_LOG(LogProjectUILayerHost, Warning,
				TEXT("auto_visibility: failed to create ViewModel for %s"), *Definition.Id.ToString());
			continue;
		}

		const FName DefId = Definition.Id;
		const FName PropName = Definition.AutoVisibilityProperty;

		// UHT does not reflect UPROPERTYs inside user macros (VIEWMODEL_PROPERTY),
		// so FBoolProperty reflection fails. Use virtual GetBoolProperty() instead.
		FDelegateHandle Handle = VM->OnPropertyChangedNative.AddLambda(
			[this, VM, DefId, PropName](FName ChangedProp)
			{
				if (ChangedProp != PropName)
				{
					return;
				}

				const bool bValue = VM->GetBoolProperty(PropName);
				UE_LOG(LogProjectUILayerHost, Log,
					TEXT("AutoVisibility: %s.%s = %d -> %s"),
					*DefId.ToString(), *PropName.ToString(), bValue,
					bValue ? TEXT("Show") : TEXT("Hide"));

				if (bValue)
				{
					ShowDefinition(DefId);
				}
				else
				{
					HideDefinition(DefId);
				}
			});

		FAutoVisibilityBinding Binding;
		Binding.ViewModel = VM;
		Binding.DefinitionId = DefId;
		Binding.PropertyName = PropName;
		Binding.Handle = Handle;
		AutoVisibilityBindings.Add(Binding);

		UE_LOG(LogProjectUILayerHost, Log,
			TEXT("AutoVisibility: bound %s to %s.%s"),
			*DefId.ToString(), *VM->GetClass()->GetName(), *PropName.ToString());
	}
}

void UProjectUILayerHostSubsystem::TeardownAutoVisibilityBindings()
{
	for (FAutoVisibilityBinding& Binding : AutoVisibilityBindings)
	{
		if (UProjectViewModel* VM = Binding.ViewModel.Get())
		{
			VM->OnPropertyChangedNative.Remove(Binding.Handle);
		}
	}
	AutoVisibilityBindings.Empty();
}

void UProjectUILayerHostSubsystem::ApplyInputForActiveLayers()
{
	UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers START - ActiveLayers count=%d"), ActiveLayers.Num());

	APlayerController* PlayerController = PrimaryPlayerController.Get();
	if (!PlayerController)
	{
		UE_LOG(LogProjectUILayerHost, Warning, TEXT("ApplyInputForActiveLayers - No PlayerController!"));
		return;
	}

	// Log current state BEFORE changes
	UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers - BEFORE: IgnoreMoveInput=%d, IgnoreLookInput=%d, ShowMouseCursor=%d"),
		PlayerController->IsMoveInputIgnored(), PlayerController->IsLookInputIgnored(), PlayerController->bShowMouseCursor);

	FGameplayTag TopLayer;
	int32 TopZ = MIN_int32;
	for (const FGameplayTag& LayerTag : ActiveLayers)
	{
		if (const FUILayerConfig* Config = LayerConfigs.Find(LayerTag))
		{
			UE_LOG(LogProjectUILayerHost, Log, TEXT("  Checking layer %s, ZOrder=%d"), *LayerTag.ToString(), Config->ZOrder);
			if (Config->ZOrder > TopZ)
			{
				TopZ = Config->ZOrder;
				TopLayer = LayerTag;
			}
		}
		else
		{
			UE_LOG(LogProjectUILayerHost, Warning, TEXT("  Layer %s has no config!"), *LayerTag.ToString());
		}
	}

	UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers - TopLayer=%s, TopZ=%d"),
		TopLayer.IsValid() ? *TopLayer.ToString() : TEXT("NONE"), TopZ);

	EProjectWidgetInputMode Effective = EProjectWidgetInputMode::Game;
	const FUILayerConfig* Config = nullptr;
	if (TopLayer.IsValid())
	{
		Config = LayerConfigs.Find(TopLayer);
		const EProjectWidgetInputMode Requested = GetRequestedInputForLayer(TopLayer);
		UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers - Requested=%d, ConfigDefault=%d"),
			static_cast<int32>(Requested), Config ? static_cast<int32>(Config->DefaultInputMode) : -1);
		Effective = (Requested != EProjectWidgetInputMode::Default && Config)
			? Requested
			: (Config ? Config->DefaultInputMode : EProjectWidgetInputMode::Default);
	}

	UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers - Effective InputMode=%d (0=Game, 1=Menu, 2=GameAndMenu, 3=Default)"),
		static_cast<int32>(Effective));

	switch (Effective)
	{
	case EProjectWidgetInputMode::Game:
		UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers - Applying GAME mode"));
		// Use Reset instead of Set(false) to fully clear the ignore counter
		// Set(true) increments counter, Set(false) decrements - if ShowDefinition was called multiple times,
		// the counter would be > 1 and a single Set(false) wouldn't restore input
		PlayerController->ResetIgnoreMoveInput();
		PlayerController->ResetIgnoreLookInput();
		PlayerController->bShowMouseCursor = false;
		PlayerController->SetInputMode(FInputModeGameOnly());
		if (UGameViewportClient* ViewportClient = PlayerController->GetWorld()->GetGameViewport())
		{
			ViewportClient->SetMouseLockMode(EMouseLockMode::LockAlways);
			UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers - Set MouseLockMode=LockAlways"));
		}
		break;
	case EProjectWidgetInputMode::Menu:
		{
			UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers - Applying MENU mode"));
			PlayerController->SetIgnoreMoveInput(true);
			PlayerController->SetIgnoreLookInput(true);
			PlayerController->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PlayerController->SetInputMode(InputMode);
		}
		break;
	case EProjectWidgetInputMode::GameAndMenu:
		{
			UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers - Applying GAME_AND_MENU mode"));
			// Use Reset to fully clear the ignore counter
			PlayerController->ResetIgnoreMoveInput();
			PlayerController->ResetIgnoreLookInput();
			PlayerController->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PlayerController->SetInputMode(InputMode);
		}
		break;
	case EProjectWidgetInputMode::Default:
	default:
		UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers - DEFAULT mode (no changes)"));
		break;
	}

	// Log final state AFTER changes
	UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers AFTER: IgnoreMoveInput=%d, IgnoreLookInput=%d, ShowMouseCursor=%d"),
		PlayerController->IsMoveInputIgnored(), PlayerController->IsLookInputIgnored(), PlayerController->bShowMouseCursor);
	UE_LOG(LogProjectUILayerHost, Log, TEXT("ApplyInputForActiveLayers END"));
}

EProjectWidgetInputMode UProjectUILayerHostSubsystem::GetRequestedInputForLayer(FGameplayTag LayerTag) const
{
	EProjectWidgetInputMode Requested = EProjectWidgetInputMode::Default;
	int32 BestPriority = MIN_int32;

	for (const TPair<FName, FActiveWidgetEntry>& Pair : ActiveWidgets)
	{
		if (Pair.Value.LayerTag != LayerTag)
		{
			continue;
		}

		if (Pair.Value.RequestedInput == EProjectWidgetInputMode::Default)
		{
			continue;
		}

		if (Pair.Value.Priority >= BestPriority)
		{
			BestPriority = Pair.Value.Priority;
			Requested = Pair.Value.RequestedInput;
		}
	}

	return Requested;
}

void UProjectUILayerHostSubsystem::ApplyViewportSizing(UUserWidget* Widget, const FProjectUIDefinition& Definition) const
{
	if (!Widget)
	{
		return;
	}

	if (Definition.SizePolicy == EProjectUISizePolicy::Fill)
	{
		Widget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
		Widget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
	}
}

void UProjectUILayerHostSubsystem::RefreshHUDSlot(FGameplayTag SlotTag) const
{
	if (!HUDLayout.IsValid())
	{
		return;
	}

	if (IProjectHUDSlotHost* SlotHost = Cast<IProjectHUDSlotHost>(HUDLayout.Get()))
	{
		SlotHost->RefreshSlot(SlotTag);
	}
	else
	{
		UE_LOG(LogProjectUILayerHost, Warning, TEXT("HUD layout does not implement ProjectHUDSlotHost"));
	}
}
