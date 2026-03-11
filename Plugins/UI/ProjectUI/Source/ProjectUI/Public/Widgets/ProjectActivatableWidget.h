// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Subsystems/UILayerTypes.h"
#include "ProjectActivatableWidget.generated.h"

class UProjectViewModel;
class UProjectUIThemeData;

DECLARE_LOG_CATEGORY_EXTERN(LogProjectActivatableWidget, Log, All);

/**
 * Base class for layered Project UI widgets (menus, dialogs, modals).
 *
 * Extends UCommonActivatableWidget with:
 * - Input mode presets via EProjectWidgetInputMode
 * - ViewModel binding (MVVM pattern)
 * - Theme integration
 *
 * Use this for widgets pushed to UI layers (GameMenu, Modal, etc.).
 * For HUD/slot-based widgets, use UProjectUserWidget instead.
 *
 * Input Routing:
 * - Override GetDesiredInputConfig() for custom input handling
 * - Use InputModeOverride for simple preset selection
 * - CommonUI handles input blocking automatically
 *
 * SOT: todo/current/gas_ui_mechanics.md (Phase 10.5)
 */
UCLASS(Abstract, Blueprintable)
class PROJECTUI_API UProjectActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UProjectActivatableWidget(const FObjectInitializer& ObjectInitializer);

	// ==========================================================================
	// UCommonActivatableWidget Overrides
	// ==========================================================================

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// ==========================================================================
	// Input Mode Configuration
	// ==========================================================================

	/**
	 * Input mode preset. Overrides default input behavior.
	 * Set in constructor or Blueprint defaults.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project UI|Input")
	EProjectWidgetInputMode InputModeOverride = EProjectWidgetInputMode::Default;

	// ==========================================================================
	// ViewModel Binding
	// ==========================================================================

	/**
	 * Set the ViewModel for this widget.
	 * Automatically calls OnViewModelChanged() and refreshes UI.
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|ViewModel")
	void SetViewModel(UProjectViewModel* InViewModel);

	/**
	 * Get the current ViewModel.
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|ViewModel")
	UProjectViewModel* GetViewModel() const { return ViewModel; }

	/**
	 * Refresh the widget's presentation from ViewModel data.
	 * Called automatically when ViewModel changes or properties update.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Project UI|ViewModel")
	void RefreshFromViewModel();
	virtual void RefreshFromViewModel_Implementation();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Called when ViewModel is set or changed.
	 * Override to bind to ViewModel properties and events.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Project UI|ViewModel")
	void OnViewModelChanged(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel);
	virtual void OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel);

	// ==========================================================================
	// Theme Support
	// ==========================================================================

	/**
	 * Called when the active theme changes.
	 * Override to apply theme colors, fonts, spacing to UI elements.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Project UI|Theme")
	void OnThemeChanged(UProjectUIThemeData* NewTheme);
	virtual void OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme);

	/**
	 * Get the active theme.
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Theme")
	UProjectUIThemeData* GetActiveTheme() const;

private:
	UPROPERTY()
	TObjectPtr<UProjectViewModel> ViewModel;

	UFUNCTION()
	void HandleViewModelPropertyChanged(FName PropertyName);

	UFUNCTION()
	void HandleThemeChanged(UProjectUIThemeData* NewTheme);

	/** Timing guard: true after NativeConstruct */
	bool bIsConstructed = false;

	/** Apply ViewModel if both constructed and ViewModel set */
	void TryApplyViewModel();
};
