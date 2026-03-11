// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "ProjectVitalsComponent.h"
#include "Interfaces/ProjectViewModelConsumer.h"
#include "W_VitalsHUD.generated.h"

class UVitalsViewModel;
class UProgressBar;
class UTextBlock;
class UButton;

DECLARE_LOG_CATEGORY_EXTERN(LogVitalsHUD, Log, All);

/**
 * Mini HUD widget for vitals display.
 *
 * Shows:
 * - Condition bar (horizontal, color-coded by state)
 * - Warning icons (BMP symbols: U+26A0 warning, U+2620 skull, U+2623 biohazard)
 * - Clickable area to expand to detailed panel
 *
 * Layout loaded from Config/UI/VitalsHUD.json
 *
 * Design:
 * - No numbers on HUD (The Long Dark pattern)
 * - Icons appear only when vitals drop below OK
 * - Click bar or press hotkey to toggle expanded panel
 *
 * Usage (ProjectUI definitions):
 * 1. Register ui_definitions.json entry with slot HUD.Slot.VitalsMini
 * 2. ProjectUI factory creates widget and injects ViewModel
 * 3. Widget initializes ViewModel with ASC from owning pawn
 *
 * SOT: ProjectVitalsUI/README.md
 */
UCLASS()
class PROJECTVITALSUI_API UW_VitalsHUD : public UProjectUserWidget, public IProjectViewModelConsumer
{
	GENERATED_BODY()

public:
	UW_VitalsHUD(const FObjectInitializer& ObjectInitializer);

	// IProjectViewModelConsumer interface
	/**
	 * Called by HUD host to inject ViewModel.
	 * Initializes the ViewModel with ASC from owning pawn.
	 */
	virtual void SetViewModel(UProjectViewModel* ViewModel) override;

	/**
	 * Set the VitalsViewModel for this widget (legacy API).
	 * Use ProjectUI injection in new flow.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vitals HUD")
	void SetVitalsViewModel(UVitalsViewModel* InViewModel);

	/**
	 * Get the current VitalsViewModel.
	 */
	UFUNCTION(BlueprintPure, Category = "Vitals HUD")
	UVitalsViewModel* GetVitalsViewModel() const { return VitalsVM; }

protected:
	// UProjectUserWidget overrides
	// NOTE: BindCallbacks() not overridden - binding happens in NativeConstruct after widget caching
	virtual void OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel) override;
	virtual void RefreshFromViewModel_Implementation() override;
	virtual void OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme) override;

	// UUserWidget overrides
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** Cached VitalsViewModel reference */
	UPROPERTY()
	TObjectPtr<UVitalsViewModel> VitalsVM;

	/** Cached widget references from JSON layout */
	UPROPERTY()
	TObjectPtr<UProgressBar> ConditionBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> WarningIcons;

	UPROPERTY()
	TObjectPtr<UButton> ExpandButton;

	/** Current theme reference */
	UPROPERTY()
	TObjectPtr<UProjectUIThemeData> CurrentTheme;

	/** Handle click on health bar to toggle panel */
	UFUNCTION()
	void HandleExpandClicked();

	/** Handle VitalsViewModel property changes */
	UFUNCTION()
	void HandleVitalsViewModelPropertyChanged(FName PropertyName);

	/** Update condition bar color based on state */
	void UpdateConditionBarColor();

	/** Update warning icon text based on flags */
	void UpdateWarningIcons();

	/** Get color from theme based on vital state */
	FLinearColor GetStateColor(EVitalState State) const;

	// game-icons.ttf PUA codepoints (CSS \ffXXX -> BMP \uFXXX)
	// Lookup: Plugins/UI/ProjectUI/Content/Slate/Fonts/game-icons.css
	static constexpr TCHAR ICON_WARNING = 0xF718;    // hazard-sign
	static constexpr TCHAR ICON_SKULL = 0xFC9B;      // skull-crossed-bones
	static constexpr TCHAR ICON_BIOHAZARD = 0xF1AC;  // biohazard
	static constexpr TCHAR ICON_BLEEDING = 0xF1CA;   // bleeding-wound
	static constexpr TCHAR ICON_RADIOACTIVE = 0xFB46; // radioactive
};
