// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ProjectViewModel.h"
#include "ProjectVitalsComponent.h"
#include "VitalsViewModel.generated.h"

class UAbilitySystemComponent;

/**
 * ViewModel for vitals UI (HUD mini bar + expanded panel).
 *
 * Reads ASC attributes and exposes:
 * - Per-vital: Percent (0-1), State enum, Current, Max
 * - Warning flags (booleans, not combined string - font-safe)
 * - Status effects (bleeding, poisoned, radiation)
 *
 * Design:
 * - Auto-refreshes from ASC every 1 second via timer (started on SetAbilitySystemComponent)
 * - Reads State.* tags from ASC for state (SOT = gameplay with hysteresis)
 * - Falls back to percent-threshold compute if tags are missing
 * - Broadcasts OnPropertyChanged for each property update
 * - View refreshes presentation from ViewModel properties
 *
 * Usage:
 * 1. Create ViewModel: NewObject<UVitalsViewModel>()
 * 2. Initialize with ASC: ViewModel->Initialize(ASC)
 * 3. Bind widget to OnPropertyChanged
 * 4. Timer auto-refreshes; can also call RefreshFromASC() manually
 *
 * SOT: ProjectVitalsUI/README.md
 */
UCLASS(BlueprintType)
class PROJECTVITALSUI_API UVitalsViewModel : public UProjectViewModel
{
	GENERATED_BODY()

public:
	// =========================================================================
	// Condition (Primary HUD bar)
	// =========================================================================

	VIEWMODEL_PROPERTY_READONLY(float, ConditionPercent)
	VIEWMODEL_PROPERTY_READONLY(float, ConditionCurrent)
	VIEWMODEL_PROPERTY_READONLY(float, ConditionMax)
	VIEWMODEL_PROPERTY_READONLY(EVitalState, ConditionState)

	// =========================================================================
	// Stamina
	// =========================================================================

	VIEWMODEL_PROPERTY_READONLY(float, StaminaPercent)
	VIEWMODEL_PROPERTY_READONLY(float, StaminaCurrent)
	VIEWMODEL_PROPERTY_READONLY(float, StaminaMax)
	VIEWMODEL_PROPERTY_READONLY(EVitalState, StaminaState)

	// =========================================================================
	// Survival Vitals (Extended Panel)
	// =========================================================================

	// Calories
	VIEWMODEL_PROPERTY_READONLY(float, CaloriesPercent)
	VIEWMODEL_PROPERTY_READONLY(float, CaloriesCurrent)
	VIEWMODEL_PROPERTY_READONLY(float, CaloriesMax)
	VIEWMODEL_PROPERTY_READONLY(EVitalState, CaloriesState)

	// Hydration
	VIEWMODEL_PROPERTY_READONLY(float, HydrationPercent)
	VIEWMODEL_PROPERTY_READONLY(float, HydrationCurrent)
	VIEWMODEL_PROPERTY_READONLY(float, HydrationMax)
	VIEWMODEL_PROPERTY_READONLY(EVitalState, HydrationState)

	// Fatigue (inverted: 0=good, 100=bad)
	VIEWMODEL_PROPERTY_READONLY(float, FatiguePercent)
	VIEWMODEL_PROPERTY_READONLY(float, FatigueCurrent)
	VIEWMODEL_PROPERTY_READONLY(float, FatigueMax)
	VIEWMODEL_PROPERTY_READONLY(EFatigueState, FatigueState)

	// =========================================================================
	// Rate-of-Change Texts (per-vital, computed from config + state)
	// Shown in expanded panel next to each vital row
	// =========================================================================

	VIEWMODEL_PROPERTY_READONLY(FText, ConditionRateText)
	VIEWMODEL_PROPERTY_READONLY(FText, StaminaRateText)
	VIEWMODEL_PROPERTY_READONLY(FText, CaloriesRateText)
	VIEWMODEL_PROPERTY_READONLY(FText, HydrationRateText)
	VIEWMODEL_PROPERTY_READONLY(FText, FatigueRateText)

	// =========================================================================
	// Warning Flags (HUD icons - booleans for font-safe glyph selection)
	// UI decides which BMP glyph to show per flag
	// =========================================================================

	/** Any vital is in Low state (show warning icon) */
	VIEWMODEL_PROPERTY_READONLY(bool, bWarnAnyLow)

	/** Any vital is in Critical/Empty state (show danger icon) */
	VIEWMODEL_PROPERTY_READONLY(bool, bWarnAnyCritical)

	// =========================================================================
	// Status Effects
	// =========================================================================

	/** Currently bleeding (condition drain active) */
	VIEWMODEL_PROPERTY_READONLY(bool, bIsBleeding)

	/** Bleeding severity (0-1, for numeric display) */
	VIEWMODEL_PROPERTY_READONLY(float, BleedingSeverity)

	/** Currently poisoned */
	VIEWMODEL_PROPERTY_READONLY(bool, bIsPoisoned)

	/** Poisoned severity (0-1, for numeric display) */
	VIEWMODEL_PROPERTY_READONLY(float, PoisonedSeverity)

	/** Current radiation level (0 = none) */
	VIEWMODEL_PROPERTY_READONLY(float, RadiationLevel)

	/** Has any active status effect */
	VIEWMODEL_PROPERTY_READONLY(bool, bHasStatusEffect)

	// =========================================================================
	// Panel Visibility
	// =========================================================================

	/** Is expanded panel visible */
	VIEWMODEL_PROPERTY(bool, bPanelVisible)

	// =========================================================================
	// Commands
	// =========================================================================

	/** Toggle expanded panel visibility */
	UFUNCTION(BlueprintCallable, Category = "Vitals")
	void TogglePanel();

	/** Show expanded panel */
	UFUNCTION(BlueprintCallable, Category = "Vitals")
	void ShowPanel();

	/** Hide expanded panel */
	UFUNCTION(BlueprintCallable, Category = "Vitals")
	void HidePanel();

	/**
	 * Get localized text for all active State.* tags.
	 * Returns human-readable descriptions of current vitals states.
	 * Uses ProjectTagTextSubsystem for tag -> text mapping.
	 *
	 * @return Array of localized texts for active states (excludes OK states)
	 */
	UFUNCTION(BlueprintPure, Category = "Vitals")
	TArray<FText> GetActiveStateTexts() const;

	// =========================================================================
	// Lifecycle
	// =========================================================================

	virtual void Initialize(UObject* Context) override;
	virtual void Shutdown() override;

	/**
	 * Refresh all properties from ASC attributes.
	 * Call this periodically (e.g., on timer) or when attributes change.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vitals")
	void RefreshFromASC();

	/**
	 * Set the ASC to read attributes from.
	 * Typically the local player's character ASC.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vitals")
	void SetAbilitySystemComponent(UAbilitySystemComponent* InASC);

private:
	/** Cached ASC reference */
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	/** Periodic refresh timer (1 sec, matches VitalsComponent tick rate) */
	FTimerHandle RefreshTimerHandle;

	void StartRefreshTimer();
	void StopRefreshTimer();

	/**
	 * Read EVitalState from ASC State.* tags (SOT = gameplay with hysteresis).
	 * Falls back to percent-based compute if tags are missing.
	 */
	EVitalState ReadVitalStateFromTags(UAbilitySystemComponent* ASC, const struct FGameplayTag& ParentTag, float Percent) const;

	/**
	 * Read EFatigueState from ASC State.Fatigue.* tags (SOT = gameplay with hysteresis).
	 * Falls back to percent-based compute if tags are missing.
	 */
	EFatigueState ReadFatigueStateFromTags(UAbilitySystemComponent* ASC, float Percent) const;

	/** Fallback: Compute EVitalState from percent (no hysteresis) */
	EVitalState ComputeVitalState(float Percent) const;

	/** Fallback: Compute EFatigueState from percent (no hysteresis) */
	EFatigueState ComputeFatigueState(float Percent) const;

	/** Update warning flags based on current states */
	void UpdateWarningFlags();

	/** Compute per-vital rate-of-change display texts from config + current state */
	void UpdateRateTexts();

	// Threshold constants (must match ProjectVitalsComponent)
	static constexpr float THRESHOLD_OK = 0.70f;
	static constexpr float THRESHOLD_LOW = 0.40f;
	static constexpr float THRESHOLD_CRITICAL = 0.20f;

	static constexpr float FATIGUE_RESTED = 0.30f;
	static constexpr float FATIGUE_TIRED = 0.60f;
	static constexpr float FATIGUE_EXHAUSTED = 0.85f;
};
