// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "ProjectVitalsComponent.h"
#include "W_VitalsPanel.generated.h"

class UVitalsViewModel;
class UProgressBar;
class UTextBlock;
class UButton;
class UBorder;
class UHorizontalBox;
class UVerticalBox;

DECLARE_LOG_CATEGORY_EXTERN(LogVitalsPanel, Log, All);

/**
 * Expanded vitals panel widget (two-column layout).
 *
 * Left column: Condition system (life/death, survival, status, metabolism)
 * Right column: Stamina system (movement, interactions, combat)
 *
 * Layout loaded from Config/UI/VitalsPanel.json
 *
 * Design:
 * - Modal overlay (semi-transparent background)
 * - Close on: click outside, ESC, same hotkey, close button
 * - Updates in real-time from ViewModel
 * - Collapsible STATUS and METABOLISM sections (default collapsed)
 *
 * SOT: ProjectVitalsUI/README.md
 */
UCLASS()
class PROJECTVITALSUI_API UW_VitalsPanel : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_VitalsPanel(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Vitals Panel")
	void SetVitalsViewModel(UVitalsViewModel* InViewModel);

	UFUNCTION(BlueprintPure, Category = "Vitals Panel")
	UVitalsViewModel* GetVitalsViewModel() const { return VitalsVM; }

protected:
	virtual void OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel) override;
	virtual void RefreshFromViewModel_Implementation() override;
	virtual void OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme) override;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY()
	TObjectPtr<UVitalsViewModel> VitalsVM;

	UPROPERTY()
	TObjectPtr<UProjectUIThemeData> CurrentTheme;

	// --- Left column: Condition system ---

	UPROPERTY()
	TObjectPtr<UHorizontalBox> ConditionRow;
	UPROPERTY()
	TObjectPtr<UProgressBar> ConditionBar;
	UPROPERTY()
	TObjectPtr<UTextBlock> ConditionValue;
	UPROPERTY()
	TObjectPtr<UTextBlock> ConditionRegenLabel;
	UPROPERTY()
	TObjectPtr<UTextBlock> ConditionRegenDays;
	UPROPERTY()
	TObjectPtr<UTextBlock> ConditionRate;

	// Survival sub-section
	UPROPERTY()
	TObjectPtr<UHorizontalBox> CaloriesRow;
	UPROPERTY()
	TObjectPtr<UProgressBar> CaloriesBar;
	UPROPERTY()
	TObjectPtr<UTextBlock> CaloriesValue;
	UPROPERTY()
	TObjectPtr<UTextBlock> CaloriesRate;
	UPROPERTY()
	TObjectPtr<UTextBlock> CaloriesLifeDays;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> HydrationRow;
	UPROPERTY()
	TObjectPtr<UProgressBar> HydrationBar;
	UPROPERTY()
	TObjectPtr<UTextBlock> HydrationValue;
	UPROPERTY()
	TObjectPtr<UTextBlock> HydrationRate;
	UPROPERTY()
	TObjectPtr<UTextBlock> HydrationLifeDays;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> FatigueRow;
	UPROPERTY()
	TObjectPtr<UProgressBar> FatigueBar;
	UPROPERTY()
	TObjectPtr<UTextBlock> FatigueValue;
	UPROPERTY()
	TObjectPtr<UTextBlock> FatigueRate;
	UPROPERTY()
	TObjectPtr<UTextBlock> FatigueTimeline;

	// Collapsible STATUS section
	UPROPERTY()
	TObjectPtr<UButton> StatusToggle;
	UPROPERTY()
	TObjectPtr<UVerticalBox> StatusContent;
	UPROPERTY()
	TObjectPtr<UTextBlock> BleedingValue;
	UPROPERTY()
	TObjectPtr<UTextBlock> PoisonedValue;
	UPROPERTY()
	TObjectPtr<UTextBlock> RadiationValue;

	// Collapsible METABOLISM section
	UPROPERTY()
	TObjectPtr<UButton> MetabolismToggle;
	UPROPERTY()
	TObjectPtr<UVerticalBox> MetabolismContent;
	UPROPERTY()
	TObjectPtr<UTextBlock> MetabolismWeight;
	UPROPERTY()
	TObjectPtr<UTextBlock> MetabolismHydration;
	UPROPERTY()
	TObjectPtr<UTextBlock> ActivityIdle;
	UPROPERTY()
	TObjectPtr<UTextBlock> ActivityWalk;
	UPROPERTY()
	TObjectPtr<UTextBlock> ActivityJog;
	UPROPERTY()
	TObjectPtr<UTextBlock> ActivitySprint;

	// --- Right column: Stamina system ---

	UPROPERTY()
	TObjectPtr<UHorizontalBox> StaminaRow;
	UPROPERTY()
	TObjectPtr<UProgressBar> StaminaBar;
	UPROPERTY()
	TObjectPtr<UTextBlock> StaminaValue;
	UPROPERTY()
	TObjectPtr<UTextBlock> StaminaRate;

	// Static stamina config rows
	UPROPERTY()
	TObjectPtr<UTextBlock> MovementSprint;
	UPROPERTY()
	TObjectPtr<UTextBlock> MovementJog;
	UPROPERTY()
	TObjectPtr<UTextBlock> MovementRegen;
	UPROPERTY()
	TObjectPtr<UTextBlock> InteractionsRow1;
	UPROPERTY()
	TObjectPtr<UTextBlock> InteractionsRow2;
	UPROPERTY()
	TObjectPtr<UTextBlock> CombatRow1;
	UPROPERTY()
	TObjectPtr<UTextBlock> CombatRow2;

	// Status effects (bottom of right column)
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusEffects;

	// --- Buttons ---

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;
	UPROPERTY()
	TObjectPtr<UButton> OverlayButton;

	// Prevent GC of custom tooltip widgets
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> TooltipWidgets;

	// --- Handlers ---

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleOverlayClicked();

	UFUNCTION()
	void HandleVitalsPanelViewModelPropertyChanged(FName PropertyName);

	UFUNCTION()
	void HandleStatusToggle();

	UFUNCTION()
	void HandleMetabolismToggle();

	// --- Internal helpers ---

	void UpdateBarColors();
	void UpdateStatusEffects();
	void UpdateRateTexts();
	void UpdateStatusValues();
	void SetupRowTooltips();
	void PopulateStaticValues();
	UWidget* CreateTooltipWidget(const FText& Text);
	FLinearColor GetStateColor(EVitalState State) const;
	FLinearColor GetFatigueStateColor(EFatigueState State) const;

	static FText FormatConditionValue(float Current);
	static FText FormatStaminaValue(float Current);
	static FText FormatCaloriesValue(float Current);
	static FText FormatHydrationValue(float Current);
	static FText FormatFatigueValue(float Percent);
};
