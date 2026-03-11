// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CommonInputModeTypes.h"
#include "CommonActivatableWidget.h"
#include "UILayerTypes.generated.h"

class UProjectViewModel;

/**
 * Input mode preset for activatable widgets.
 * Maps to CommonUI's FUIInputConfig with sensible defaults.
 */
UENUM(BlueprintType)
enum class EProjectWidgetInputMode : uint8
{
	/** Use widget's default (no override) */
	Default,

	/** Game input only - HUD elements, pass-through to game */
	Game,

	/** Menu input only - cursor visible, blocks game input */
	Menu,

	/** Game + Menu - both game and UI can receive input */
	GameAndMenu
};

/**
 * Layer configuration - defines a UI layer's behavior.
 * Registered once per layer, not per widget.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FUILayerConfig
{
	GENERATED_BODY()

	/** Layer identifier (e.g., UI.Layer.Game, UI.Layer.Modal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layer")
	FGameplayTag LayerTag;

	/** Z-order for stacking (higher = on top). Sparse values (0, 100, 200) allow custom layers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layer")
	int32 ZOrder = 0;

	/** Default input mode for widgets in this layer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layer")
	EProjectWidgetInputMode DefaultInputMode = EProjectWidgetInputMode::Default;

	/** If true, blocks input to layers below */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layer")
	bool bBlocksLayersBelow = false;

	/** If true, uses Queue container (notifications). If false, uses Stack (menus). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layer")
	bool bUseQueue = false;
};

/**
 * Layer extension data - what gets registered.
 * Defines a widget to be pushed to a specific UI layer.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FUILayerExtensionData
{
	GENERATED_BODY()

	/** Which layer this widget belongs to (e.g., UI.Layer.GameMenu) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layer")
	FGameplayTag LayerTag;

	/** Widget class to instantiate (must derive from UCommonActivatableWidget) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layer")
	TSubclassOf<UCommonActivatableWidget> WidgetClass;

	/** Optional ViewModel class - layer host will create and inject */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layer")
	TSubclassOf<UProjectViewModel> ViewModelClass;

	/** Priority for stacking within layer - higher values draw on top */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layer")
	int32 Priority = 0;

	/** If true, widget is auto-pushed when registered. If false, push manually. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Layer")
	bool bAutoPush = false;
};

/**
 * Internal storage for layer extensions with monotonic order.
 */
struct FStoredLayerExtension
{
	FUILayerExtensionData Data;
	uint64 RegistrationOrder;
};
