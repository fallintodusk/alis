// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_LayerStack.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Subsystems/UIExtensionSubsystem.h"
#include "ProjectGameplayTags.h"
#include "Widgets/SOverlay.h"

DEFINE_LOG_CATEGORY(LogLayerStack);

UW_LayerStack::UW_LayerStack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UW_LayerStack::RebuildWidget()
{
	// Create root overlay for layer stacking
	RootOverlay = SNew(SOverlay);

	return RootOverlay.ToSharedRef();
}

void UW_LayerStack::NativeConstruct()
{
	Super::NativeConstruct();

	// Register with UIExtensionSubsystem
	BindToExtensionSubsystem();

	// Initialize default layers
	InitializeDefaultLayers();

	UE_LOG(LogLayerStack, Log, TEXT("NativeConstruct: Layer stack initialized with %d layers"),
		Layers.Num());
}

void UW_LayerStack::NativeDestruct()
{
	UnbindFromExtensionSubsystem();

	// Clear layer containers
	Layers.Empty();
	LayerConfigs.Empty();
	RootOverlay.Reset();

	Super::NativeDestruct();
}

void UW_LayerStack::InitializeDefaultLayers()
{
	// Game layer - HUD, pass-through input
	{
		FUILayerConfig Config;
		Config.LayerTag = ProjectTags::UI_Layer_Game;
		Config.ZOrder = 0;
		Config.DefaultInputMode = EProjectWidgetInputMode::Game;
		Config.bBlocksLayersBelow = false;
		Config.bUseQueue = false;
		RegisterLayer(Config);
	}

	// GameMenu layer - Pause menu, blocks game input
	{
		FUILayerConfig Config;
		Config.LayerTag = ProjectTags::UI_Layer_GameMenu;
		Config.ZOrder = 100;
		Config.DefaultInputMode = EProjectWidgetInputMode::Menu;
		Config.bBlocksLayersBelow = true;
		Config.bUseQueue = false; // Stack for menus
		RegisterLayer(Config);
	}

	// Modal layer - Dialogs, blocks all below
	{
		FUILayerConfig Config;
		Config.LayerTag = ProjectTags::UI_Layer_Modal;
		Config.ZOrder = 200;
		Config.DefaultInputMode = EProjectWidgetInputMode::Menu;
		Config.bBlocksLayersBelow = true;
		Config.bUseQueue = false; // Stack for modals
		RegisterLayer(Config);
	}

	// Notification layer - Toasts, non-blocking
	{
		FUILayerConfig Config;
		Config.LayerTag = ProjectTags::UI_Layer_Notification;
		Config.ZOrder = 50;
		Config.DefaultInputMode = EProjectWidgetInputMode::Game;
		Config.bBlocksLayersBelow = false;
		Config.bUseQueue = true; // Queue for notifications
		RegisterLayer(Config);
	}
}

void UW_LayerStack::RegisterLayer(const FUILayerConfig& Config)
{
	if (!Config.LayerTag.IsValid())
	{
		UE_LOG(LogLayerStack, Warning, TEXT("RegisterLayer: Invalid layer tag"));
		return;
	}

	if (Layers.Contains(Config.LayerTag))
	{
		UE_LOG(LogLayerStack, Warning, TEXT("RegisterLayer: Layer already registered: %s"),
			*Config.LayerTag.ToString());
		return;
	}

	// Create container (Stack or Queue based on config)
	UCommonActivatableWidgetContainerBase* Container = nullptr;

	if (Config.bUseQueue)
	{
		Container = NewObject<UCommonActivatableWidgetQueue>(this);
	}
	else
	{
		Container = NewObject<UCommonActivatableWidgetStack>(this);
	}

	if (!Container)
	{
		UE_LOG(LogLayerStack, Error, TEXT("RegisterLayer: Failed to create container for %s"),
			*Config.LayerTag.ToString());
		return;
	}

	// Add container to overlay at specified Z-order
	if (RootOverlay.IsValid())
	{
		RootOverlay->AddSlot(Config.ZOrder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				Container->TakeWidget()
			];
	}

	// Store references
	Layers.Add(Config.LayerTag, Container);
	LayerConfigs.Add(Config.LayerTag, Config);

	UE_LOG(LogLayerStack, Log, TEXT("RegisterLayer: %s ZOrder=%d UseQueue=%s"),
		*Config.LayerTag.ToString(),
		Config.ZOrder,
		Config.bUseQueue ? TEXT("true") : TEXT("false"));
}

UCommonActivatableWidgetContainerBase* UW_LayerStack::GetLayerContainer(FGameplayTag LayerTag) const
{
	const TObjectPtr<UCommonActivatableWidgetContainerBase>* Container = Layers.Find(LayerTag);
	return Container ? Container->Get() : nullptr;
}

UCommonActivatableWidget* UW_LayerStack::PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!LayerTag.IsValid())
	{
		UE_LOG(LogLayerStack, Warning, TEXT("PushWidgetToLayer: Invalid layer tag"));
		return nullptr;
	}

	if (!WidgetClass)
	{
		UE_LOG(LogLayerStack, Warning, TEXT("PushWidgetToLayer: Null widget class"));
		return nullptr;
	}

	UCommonActivatableWidgetContainerBase* Container = GetLayerContainer(LayerTag);
	if (!Container)
	{
		UE_LOG(LogLayerStack, Warning, TEXT("PushWidgetToLayer: Layer not registered: %s"),
			*LayerTag.ToString());
		return nullptr;
	}

	// AddWidget creates and pushes the widget
	UCommonActivatableWidget* Widget = Container->AddWidget<UCommonActivatableWidget>(WidgetClass);

	if (Widget)
	{
		UE_LOG(LogLayerStack, Log, TEXT("PushWidgetToLayer: Pushed %s to %s"),
			*WidgetClass->GetName(),
			*LayerTag.ToString());
	}
	else
	{
		UE_LOG(LogLayerStack, Warning, TEXT("PushWidgetToLayer: Failed to create widget %s"),
			*WidgetClass->GetName());
	}

	return Widget;
}

void UW_LayerStack::BindToExtensionSubsystem()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UUIExtensionSubsystem* ExtSub = GI->GetSubsystem<UUIExtensionSubsystem>())
		{
			CachedExtensionSubsystem = ExtSub;
			ExtSub->SetLayerStackWidget(this);
			ExtSub->OnLayerExtensionChanged.AddUniqueDynamic(this, &UW_LayerStack::HandleLayerExtensionChanged);

			UE_LOG(LogLayerStack, Verbose, TEXT("BindToExtensionSubsystem: Bound to UIExtensionSubsystem"));
		}
		else
		{
			UE_LOG(LogLayerStack, Warning, TEXT("BindToExtensionSubsystem: UIExtensionSubsystem not found"));
		}
	}
}

void UW_LayerStack::UnbindFromExtensionSubsystem()
{
	if (UUIExtensionSubsystem* ExtSub = CachedExtensionSubsystem.Get())
	{
		ExtSub->SetLayerStackWidget(nullptr);
		ExtSub->OnLayerExtensionChanged.RemoveDynamic(this, &UW_LayerStack::HandleLayerExtensionChanged);
	}

	CachedExtensionSubsystem.Reset();
}

void UW_LayerStack::HandleLayerExtensionChanged(FGameplayTag LayerTag)
{
	UE_LOG(LogLayerStack, Log, TEXT("HandleLayerExtensionChanged: %s"), *LayerTag.ToString());
	RebuildLayer(LayerTag);
}

void UW_LayerStack::RebuildLayer(FGameplayTag LayerTag)
{
	UUIExtensionSubsystem* ExtSub = CachedExtensionSubsystem.Get();
	if (!ExtSub)
	{
		UE_LOG(LogLayerStack, Warning, TEXT("RebuildLayer: No extension subsystem"));
		return;
	}

	UCommonActivatableWidgetContainerBase* Container = GetLayerContainer(LayerTag);
	if (!Container)
	{
		UE_LOG(LogLayerStack, Warning, TEXT("RebuildLayer: Layer not registered: %s"),
			*LayerTag.ToString());
		return;
	}

	// Get registered extensions for this layer
	TArray<FUILayerExtensionData> LayerExts = ExtSub->GetExtensionsForLayer(LayerTag);

	// For layers with bAutoPush extensions, push them
	// Note: This is a simplified implementation. Full implementation would track
	// which extensions are already pushed and only push new ones.
	for (const FUILayerExtensionData& Ext : LayerExts)
	{
		if (Ext.bAutoPush && Ext.WidgetClass)
		{
			// TODO: Track already-pushed widgets to avoid duplicates
			// For now, this is called on extension change, so we expect fresh state
			PushWidgetToLayer(LayerTag, Ext.WidgetClass);
		}
	}

	UE_LOG(LogLayerStack, Verbose, TEXT("RebuildLayer: %s processed %d extensions"),
		*LayerTag.ToString(), LayerExts.Num());
}
