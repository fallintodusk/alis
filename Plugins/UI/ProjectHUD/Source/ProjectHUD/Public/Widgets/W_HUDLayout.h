// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Widgets/ProjectUserWidget.h"
#include "Interfaces/ProjectHUDSlotHost.h"
#include "W_HUDLayout.generated.h"

class UPanelWidget;
class UProjectViewModel;
class UW_InteractionPrompt;

/**
 * HUD Layout Widget - Lyra-style composition root.
 *
 * This widget defines named slots (VitalsMiniSlot, StatusIconsSlot, etc.)
 * and populates them with widgets defined in ProjectUI registry.
 *
 * Key features:
 * - JSON-built layout (no BindWidget - finds slots by name at runtime)
 * - Builds slot widgets from ui_definitions.json
 * - Creates ViewModels for widgets that implement IProjectViewModelConsumer
 * - Priority-based stacking (higher priority draws on top)
 *
 * Slot containers are found by name using FindWidgetByNameTyped:
 * - "VitalsMiniSlot" -> HUD.Slot.VitalsMini
 * - "StatusIconsSlot" -> HUD.Slot.StatusIcons
 *
 * Usage:
 * ProjectUI creates W_HUDLayout and populates slot widgets from definitions.
 *
 * SOT: todo/current/gas_ui_mechanics.md (Phase 10)
 */
UCLASS()
class PROJECTHUD_API UW_HUDLayout : public UProjectUserWidget, public IProjectHUDSlotHost
{
	GENERATED_BODY()

public:
	UW_HUDLayout(const FObjectInitializer& ObjectInitializer);

	/**
	 * Get the most recently created ViewModel instance for a class.
	 * Returns nullptr if no ViewModel of that class exists.
	 */
	UFUNCTION(BlueprintPure, Category = "HUD")
	UProjectViewModel* GetViewModelByClass(TSubclassOf<UProjectViewModel> ViewModelClass) const;

	/** Rebuild a slot from registry definitions. */
	virtual void RefreshSlot(FGameplayTag SlotTag) override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// Slot containers found by name at runtime (NOT BindWidget - JSON-built)
	TWeakObjectPtr<UPanelWidget> VitalsMiniSlot;
	TWeakObjectPtr<UPanelWidget> StatusIconsSlot;
	TWeakObjectPtr<UPanelWidget> InteractionPromptContainer;
	TWeakObjectPtr<UPanelWidget> MindThoughtSlot;
	TWeakObjectPtr<UPanelWidget> ToastContainer;
	TMap<FGameplayTag, TWeakObjectPtr<UPanelWidget>> SlotContainers;

	// Fixed widgets (not slot-managed)
	TWeakObjectPtr<UW_InteractionPrompt> InteractionPrompt;

	// Slot -> widget instances (tracked for cleanup)
	TMap<FGameplayTag, TArray<TWeakObjectPtr<UUserWidget>>> SlotWidgets;

	// Slot -> ViewModel instances (tracked for cleanup)
	TMap<FGameplayTag, TArray<TWeakObjectPtr<UProjectViewModel>>> SlotViewModels;

	// ViewModelClass -> ViewModel instance (latest created)
	TMap<UClass*, TWeakObjectPtr<UProjectViewModel>> ViewModelsByClass;

	/**
	 * Rebuild a slot with fresh extension data.
	 * Clears existing widgets first, then creates new ones sorted by priority.
	 */
	void RebuildSlot(FGameplayTag SlotTag, UPanelWidget* Container);

	/**
	 * Clear all widgets from a slot.
	 * Removes widgets from parent and clears tracking array.
	 */
	void ClearSlot(FGameplayTag SlotTag, UPanelWidget* Container);

	/** Initialize toast system (sets up toast container). */
	void InitializeToastSystem();
};
