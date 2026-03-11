// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/UILayerTypes.h"
#include "UIExtensionSubsystem.generated.h"

class UProjectViewModel;
class UW_LayerStack;
class UCommonActivatableWidget;

/**
 * Extension handle - identifies a registered extension.
 * Use this to unregister extensions when they're no longer needed.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FUIExtensionHandle
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Id;

	bool IsValid() const { return Id.IsValid(); }

	bool operator==(const FUIExtensionHandle& Other) const { return Id == Other.Id; }
	bool operator!=(const FUIExtensionHandle& Other) const { return Id != Other.Id; }
};

/**
 * Slot extension data - what gets registered.
 * Defines a widget to be placed in a specific HUD slot.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FUISlotExtensionData
{
	GENERATED_BODY()

	/** Which slot this widget belongs to (e.g., HUD.Slot.VitalsMini) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Extension")
	FGameplayTag SlotTag;

	/** Widget class to instantiate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Extension")
	TSubclassOf<UUserWidget> WidgetClass;

	/** Optional ViewModel class - HUD host will create and inject via IProjectViewModelConsumer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Extension")
	TSubclassOf<UProjectViewModel> ViewModelClass;

	/** Priority for stacking - higher values draw on top */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Extension")
	int32 Priority = 0;
};

/**
 * Internal storage with monotonic order for stable sorting.
 * FGuid doesn't have operator<, so we use RegistrationOrder for tie-breaking.
 */
struct FStoredSlotExtension
{
	FUISlotExtensionData Data;
	uint64 RegistrationOrder;
};

/**
 * UI Extension Subsystem - Lyra-style slot-based widget composition.
 *
 * Provides a central registry for UI extensions. Feature plugins register
 * their widgets into named slots (e.g., HUD.Slot.VitalsMini), and the HUD
 * layout widget queries and displays them.
 *
 * Key features:
 * - Handle-based registration/unregistration for clean lifecycle
 * - Priority ordering for widget stacking (higher = on top)
 * - Monotonic registration order for stable tie-breaking
 * - Dynamic delegate for slot changes (Add/Remove triggers rebuild)
 *
 * Usage:
 * @code
 * // Register (typically in a bootstrap subsystem)
 * FUISlotExtensionData Data;
 * Data.SlotTag = ProjectTags::HUD_Slot_VitalsMini;
 * Data.WidgetClass = UW_VitalsHUD::StaticClass();
 * Data.ViewModelClass = UVitalsViewModel::StaticClass();
 * Handle = ExtSub->RegisterSlotExtension(Data);
 *
 * // Unregister (typically in Deinitialize)
 * ExtSub->UnregisterExtension(Handle);
 * @endcode
 *
 * SOT: todo/current/gas_ui_mechanics.md (Phase 10)
 */
UCLASS()
class PROJECTUI_API UUIExtensionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Register a widget extension for a slot.
	 *
	 * @param Data - Extension data (slot tag, widget class, etc.)
	 * @return Handle for unregistration
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Extensions")
	FUIExtensionHandle RegisterSlotExtension(const FUISlotExtensionData& Data);

	/**
	 * Unregister an extension by handle.
	 *
	 * @param Handle - Handle returned from RegisterSlotExtension
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Extensions")
	void UnregisterExtension(FUIExtensionHandle Handle);

	/**
	 * Query extensions registered for a specific slot.
	 * Results are sorted by Priority (descending), then by RegistrationOrder (ascending).
	 *
	 * @param SlotTag - The slot to query (e.g., HUD.Slot.VitalsMini)
	 * @return Array of extension data sorted by priority
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Extensions")
	TArray<FUISlotExtensionData> GetExtensionsForSlot(FGameplayTag SlotTag) const;

	/**
	 * Delegate fired when a slot's extensions change (register or unregister).
	 * HUD layout widgets bind to this to rebuild their slots.
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotExtensionChanged, FGameplayTag, SlotTag);

	UPROPERTY(BlueprintAssignable, Category = "UI|Extensions")
	FOnSlotExtensionChanged OnSlotExtensionChanged;

	// ==========================================================================
	// Layer Extensions (Phase 10.5)
	// ==========================================================================

	/**
	 * Register a widget extension for a layer.
	 *
	 * @param Data - Extension data (layer tag, widget class, etc.)
	 * @return Handle for unregistration
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Layers")
	FUIExtensionHandle RegisterLayerExtension(const FUILayerExtensionData& Data);

	/**
	 * Query extensions registered for a specific layer.
	 * Results are sorted by Priority (descending), then by RegistrationOrder (ascending).
	 *
	 * @param LayerTag - The layer to query (e.g., UI.Layer.GameMenu)
	 * @return Array of extension data sorted by priority
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Layers")
	TArray<FUILayerExtensionData> GetExtensionsForLayer(FGameplayTag LayerTag) const;

	/**
	 * Delegate fired when a layer's extensions change.
	 * Layer stack widget binds to this to rebuild layers.
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLayerExtensionChanged, FGameplayTag, LayerTag);

	UPROPERTY(BlueprintAssignable, Category = "UI|Layers")
	FOnLayerExtensionChanged OnLayerExtensionChanged;

	/**
	 * Set the layer stack widget reference.
	 * Called by W_LayerStack when it initializes.
	 */
	void SetLayerStackWidget(UW_LayerStack* InLayerStack);

	/**
	 * Get the layer stack widget.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Layers")
	UW_LayerStack* GetLayerStackWidget() const;

private:
	/** Map of Handle ID -> Stored slot extension data */
	TMap<FGuid, FStoredSlotExtension> SlotExtensions;

	/** Map of Handle ID -> Stored layer extension data */
	TMap<FGuid, FStoredLayerExtension> LayerExtensions;

	/** Monotonic counter for stable sorting (shared between slots and layers) */
	uint64 NextRegistrationOrder = 0;

	/** Weak reference to the layer stack widget */
	TWeakObjectPtr<UW_LayerStack> LayerStackWidget;
};
