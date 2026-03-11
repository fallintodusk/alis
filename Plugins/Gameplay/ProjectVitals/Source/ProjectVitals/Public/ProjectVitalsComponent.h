// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "ProjectVitalsComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * Vital state enum for threshold-based states.
 * Used by UI to show appropriate icons/effects.
 */
UENUM(BlueprintType)
enum class EVitalState : uint8
{
	OK,       // >70%
	Low,      // 40-70%
	Critical, // 20-40%
	Empty     // <=20%
};

/**
 * Fatigue state enum (inverted: 0=good, 100=bad).
 */
UENUM(BlueprintType)
enum class EFatigueState : uint8
{
	Rested,    // <30%
	Tired,     // 30-60%
	Exhausted, // 60-85%
	Critical   // >=85%
};

/**
 * Vitals component: metabolism tick, threshold states, threshold debuffs.
 *
 * === Why ActorComponent (not Subsystem) ===
 * - Per-actor state: tick timer, cached ASC, active debuff handles
 * - Server authority check per actor (HasAuthority)
 * - Lifecycle tied to character possession, not game instance
 * - No dependency on ProjectCharacter (uses IAbilitySystemInterface)
 *
 * === Ownership ===
 * - Attached to characters (via ProjectCharacter constructor)
 * - Server-only tick (clients receive replicated attributes/tags)
 * - Character calls Start() after ASC init, Stop() on unpossess/endplay
 *
 * === Tick Flow (TickVitals) ===
 * 1. ApplyMetabolism() - MET-based calorie/hydration drain, fatigue gain
 * 2. ApplyBleedingDrain() - Condition drain from bleeding severity
 * 3. UpdateStateTags() - Apply State.* tags based on thresholds (with hysteresis)
 * 4. UpdateThresholdDebuffs() - Apply/remove GE debuffs that modify MULTIPLIERS
 * 5. ApplyConditionDelta() - Inline regen/drain computation (single source of truth)
 *
 * === Design: Debuffs Modify Multipliers, TickVitals Applies Deltas ===
 * - Threshold debuffs (infinite GEs) only change StaminaRegenRate, ConditionRegenRate
 * - Actual condition/stamina changes computed inline per tick using those multipliers
 * - Avoids periodic GE "evaluation gotchas" (tags checked on apply, not each tick)
 * - Single source of truth for regen/drain math
 *
 * === Hysteresis (Prevents Threshold Flickering) ===
 * - Entry threshold (e.g., 70% to enter OK)
 * - Exit threshold (e.g., 68% to leave OK)
 * - 2% buffer prevents tag churn when values hover at boundaries
 *
 * === Debuff Lifecycle ===
 * - Debuffs are infinite-duration GEs tracked by FActiveGameplayEffectHandle
 * - Applied when entering threshold, removed when leaving
 * - All debuffs removed in Stop()/EndPlay() to prevent leaking
 *
 * === Dependencies ===
 * - ProjectGAS: AttributeSets (read), ApplyMagnitudes (write), Tags
 * - Does NOT depend on ProjectCharacter (avoids cycle)
 *
 * SOT: ProjectVitals/README.md, gas_ui_mechanics.md
 */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class PROJECTVITALS_API UProjectVitalsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProjectVitalsComponent();

	// -------------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------------

	// Tick interval (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Config")
	float TickInterval = 1.0f;

	// Character weight in kg (used for MET calculations)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Config")
	float CharacterWeightKg = 75.0f;

	// -------------------------------------------------------------------------
	// Metabolism Rates (can be overridden per-character)
	// Formula: kcal/hr = WeightKg * MET
	// -------------------------------------------------------------------------

	// MET values for different activities
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Metabolism")
	float MET_Idle = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Metabolism")
	float MET_Walk = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Metabolism")
	float MET_Jog = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Metabolism")
	float MET_Sprint = 10.0f;

	// -------------------------------------------------------------------------
	// Speed Thresholds for MET Classification (cm/s)
	// -------------------------------------------------------------------------
	// Fixed thresholds based on human locomotion (not dynamic MaxWalkSpeed)
	// to ensure consistent metabolism regardless of sprint/crouch state.
	// Reference: biomechanics research on gait transition speeds
	// - Walk/Jog transition: ~7 km/h (195 cm/s)
	// - Jog/Run transition: ~10 km/h (278 cm/s)
	// -------------------------------------------------------------------------

	/** Speed below which character is considered idle (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Metabolism")
	float SpeedThreshold_Idle = 10.0f;

	/** Speed below which character is walking - low MET (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Metabolism")
	float SpeedThreshold_Walk = 150.0f;

	/** Speed below which character is jogging - moderate MET (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Metabolism")
	float SpeedThreshold_Jog = 350.0f;

	// Above SpeedThreshold_Jog = sprinting (high MET)

	// Hydration drain multiplier (L per kcal burned)
	// Example: 0.001 means 1L water per 1000 kcal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Metabolism")
	float HydrationPerKcal = 0.001f;

	// Fatigue gain rate (% per hour when awake)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Metabolism")
	float FatigueGainPerHour = 6.25f;  // 100% in 16 hours

	// Bleeding condition drain (% per second per severity level)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Metabolism")
	float BleedingDrainPerSecond = 0.5f;

	// -------------------------------------------------------------------------
	// Condition Regen/Drain (computed in TickVitals, not periodic GEs)
	// -------------------------------------------------------------------------

	// Base condition regen rate (units per second, scaled by ConditionRegenRate attribute)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Condition")
	float BaseConditionRegenPerSec = 0.000289f;

	// Condition drain when Hydration = 0% (2 days TOTAL: 1d vital + 1d Condition)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Condition")
	float DehydrationDrainPerSec = 0.001157f;

	// Condition drain when Calories = 0% (14 days TOTAL: 1d vital + 13d Condition)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Condition")
	float StarvationDrainPerSec = 0.000089f;

	// Condition drain when Fatigue >= 95% (5 days TOTAL: 1d vital + 4d Condition)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vitals|Condition")
	float ExhaustionDrainPerSec = 0.000289f;

	// -------------------------------------------------------------------------
	// Control
	// -------------------------------------------------------------------------

	// Start vitals tick (call after ASC is initialized)
	UFUNCTION(BlueprintCallable, Category = "Vitals")
	void Start();

	// Stop vitals tick
	UFUNCTION(BlueprintCallable, Category = "Vitals")
	void Stop();

	// Check if vitals tick is running
	UFUNCTION(BlueprintPure, Category = "Vitals")
	bool IsRunning() const { return bIsRunning; }

#if WITH_DEV_AUTOMATION_TESTS
	// Test-only wrappers for internal hysteresis and debuff cleanup behavior.
	EVitalState TestComputeVitalStateWithHysteresis(float Percent, EVitalState PrevState) const;
	EFatigueState TestComputeFatigueStateWithHysteresis(float Percent, EFatigueState PrevState) const;
	void TestTickVitalsOnce();
	void TestSeedDebuffHandles();
	void TestClearDebuffsWithoutASC();
	bool TestAreAllDebuffHandlesInvalid() const;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Timer handle for periodic tick
	FTimerHandle VitalsTickTimerHandle;

	// Running state
	bool bIsRunning = false;

	// Cached ASC reference
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	// -------------------------------------------------------------------------
	// Threshold Debuff Tracking
	// -------------------------------------------------------------------------

	// Active debuff effect handles (for removal when state changes)
	// NOTE: These debuffs only modify MULTIPLIERS (StaminaRegenRate, ConditionRegenRate)
	// Actual regen/drain is computed inline in TickVitals (single source of truth)
	FActiveGameplayEffectHandle DebuffHandle_HydrationLow;
	FActiveGameplayEffectHandle DebuffHandle_CaloriesLow;
	FActiveGameplayEffectHandle DebuffHandle_CaloriesCritical;
	FActiveGameplayEffectHandle DebuffHandle_HydrationCritical;
	FActiveGameplayEffectHandle DebuffHandle_FatigueHigh;
	FActiveGameplayEffectHandle DebuffHandle_WeightHeavy;
	FActiveGameplayEffectHandle DebuffHandle_WeightOverweight;

	// Previous states for hysteresis (prevents flickering at threshold boundaries)
	EVitalState PrevConditionState = EVitalState::OK;
	EVitalState PrevStaminaState = EVitalState::OK;
	EVitalState PrevCaloriesState = EVitalState::OK;
	EVitalState PrevHydrationState = EVitalState::OK;
	EFatigueState PrevFatigueState = EFatigueState::Rested;

	// Main tick function (server-only)
	void TickVitals();

	// Get ASC from owner
	UAbilitySystemComponent* GetASC() const;

	// Update State.* tags based on current attribute values
	void UpdateStateTags(UAbilitySystemComponent* ASC);

	// Helper: compute vital state from percentage with hysteresis
	// Uses PrevState to apply slightly different exit thresholds (prevents flickering)
	EVitalState ComputeVitalStateWithHysteresis(float Percent, EVitalState PrevState) const;

	// Helper: compute fatigue state from percentage (inverted) with hysteresis
	EFatigueState ComputeFatigueStateWithHysteresis(float Percent, EFatigueState PrevState) const;

	// Helper: apply/remove state tag
	void SetStateTag(UAbilitySystemComponent* ASC, const FGameplayTag& NewTag, const FGameplayTag& ParentTag);

	// -------------------------------------------------------------------------
	// Metabolism Calculations
	// -------------------------------------------------------------------------

	// Get current MET based on character movement state
	float GetCurrentMET() const;

	// Calculate and apply metabolism drains
	void ApplyMetabolism(UAbilitySystemComponent* ASC, float DeltaTime);

	// Apply bleeding condition drain
	void ApplyBleedingDrain(UAbilitySystemComponent* ASC, float DeltaTime);

	// -------------------------------------------------------------------------
	// Threshold Debuffs
	// -------------------------------------------------------------------------

	// Update threshold debuffs based on current vital states
	void UpdateThresholdDebuffs(UAbilitySystemComponent* ASC);

	// Helper: apply or remove a debuff based on condition
	void ApplyOrRemoveDebuff(
		UAbilitySystemComponent* ASC,
		FActiveGameplayEffectHandle& Handle,
		TSubclassOf<UGameplayEffect> EffectClass,
		bool bShouldBeActive);

	// Remove all active debuffs (called on Stop/EndPlay to prevent leaking effects)
	void RemoveAllDebuffs();

	// -------------------------------------------------------------------------
	// Condition Regen/Drain (inline computation, single source of truth)
	// -------------------------------------------------------------------------

	// Compute and apply condition delta per tick
	// Delta = (BaseRegenPerSec * ConditionRegenRate - DrainIfCritical) * DeltaTime
	// Gating: Bleeding=0, ConditionRegenRate>0, Condition<MaxCondition
	void ApplyConditionDelta(UAbilitySystemComponent* ASC, float DeltaTime);

	// Load vitals_config.json: apply metabolism tuning + initial attributes (server-only)
	void ApplyConfig();

	// -------------------------------------------------------------------------
	// Threshold Constants with Hysteresis
	// -------------------------------------------------------------------------

	// Entry thresholds (crossing INTO a state)
	// Condition/Stamina/Calories/Hydration: OK>70%, Low 40-70%, Critical 20-40%, Empty<=20%
	static constexpr float THRESHOLD_OK = 0.70f;
	static constexpr float THRESHOLD_LOW = 0.40f;
	static constexpr float THRESHOLD_CRITICAL = 0.20f;

	// Hysteresis buffer (2% prevents flickering at boundaries)
	// Exit threshold = Entry threshold - HYSTERESIS_BUFFER
	// Example: Enter OK at 70%, exit OK at 68%
	static constexpr float HYSTERESIS_BUFFER = 0.02f;

	// Fatigue (inverted): Rested<30%, Tired 30-60%, Exhausted 60-85%, Critical>=85%
	static constexpr float FATIGUE_RESTED = 0.30f;
	static constexpr float FATIGUE_TIRED = 0.60f;
	static constexpr float FATIGUE_EXHAUSTED = 0.85f;
};
