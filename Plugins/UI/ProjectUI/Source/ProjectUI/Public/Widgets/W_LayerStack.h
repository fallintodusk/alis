// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "Subsystems/UILayerTypes.h"
#include "W_LayerStack.generated.h"

class UCommonActivatableWidgetContainerBase;
class UUIExtensionSubsystem;
class SOverlay;

DECLARE_LOG_CATEGORY_EXTERN(LogLayerStack, Log, All);

/**
 * Layer Stack Widget - Root container for all UI layers.
 *
 * Creates an SOverlay with layer containers at specific Z-orders.
 * Each layer uses either a Stack (menus) or Queue (notifications) container
 * from CommonUI for proper input routing and activation management.
 *
 * Default layers (registered on construct):
 * - UI.Layer.Game (ZOrder 0) - HUD, pass-through input
 * - UI.Layer.GameMenu (ZOrder 100) - Pause menu, blocks game input
 * - UI.Layer.Modal (ZOrder 200) - Dialogs, blocks all below
 * - UI.Layer.Notification (ZOrder 50) - Toasts, non-blocking
 *
 * Usage:
 * - Add to viewport as root UI widget
 * - Push widgets via UIExtensionSubsystem or directly via PushWidgetToLayer
 * - Layers auto-create their containers on first use
 *
 * SOT: todo/current/gas_ui_mechanics.md (Phase 10.5)
 */
UCLASS()
class PROJECTUI_API UW_LayerStack : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UW_LayerStack(const FObjectInitializer& ObjectInitializer);

	// ==========================================================================
	// Layer Management
	// ==========================================================================

	/**
	 * Register a layer configuration.
	 * Creates the layer container at the specified Z-order.
	 *
	 * @param Config - Layer configuration (tag, ZOrder, input mode, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Layers")
	void RegisterLayer(const FUILayerConfig& Config);

	/**
	 * Get the container for a specific layer.
	 * Returns nullptr if layer not registered.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Layers")
	UCommonActivatableWidgetContainerBase* GetLayerContainer(FGameplayTag LayerTag) const;

	/**
	 * Push a widget to a layer.
	 * Creates the widget and adds it to the layer's container.
	 *
	 * @param LayerTag - Target layer
	 * @param WidgetClass - Widget class to instantiate
	 * @return The created widget, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Layers", meta = (DeterminesOutputType = "WidgetClass"))
	UCommonActivatableWidget* PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	/**
	 * Initialize default layers.
	 * Called from NativeConstruct.
	 */
	void InitializeDefaultLayers();

	/**
	 * Bind to UIExtensionSubsystem.
	 */
	void BindToExtensionSubsystem();
	void UnbindFromExtensionSubsystem();

	/**
	 * Handle layer extension changes.
	 */
	UFUNCTION()
	void HandleLayerExtensionChanged(FGameplayTag LayerTag);

	/**
	 * Rebuild a layer's widgets from registered extensions.
	 */
	void RebuildLayer(FGameplayTag LayerTag);

	// ==========================================================================
	// Internal State
	// ==========================================================================

	/** Root Slate overlay - hosts all layer containers */
	TSharedPtr<SOverlay> RootOverlay;

	/**
	 * Map of layer tag -> container widget.
	 * CRITICAL: UPROPERTY required to prevent GC from collecting containers.
	 */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>> Layers;

	/** Layer configurations for registered layers */
	TMap<FGameplayTag, FUILayerConfig> LayerConfigs;

	/** Cached extension subsystem reference */
	TWeakObjectPtr<UUIExtensionSubsystem> CachedExtensionSubsystem;
};
