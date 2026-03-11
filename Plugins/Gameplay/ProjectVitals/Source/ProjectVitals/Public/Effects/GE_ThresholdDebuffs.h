// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_ThresholdDebuffs.generated.h"

/**
 * Threshold debuff: Hydration Low
 * Applied when State.Hydration.Low tag is active.
 * Effect: MaxStamina -15%
 */
UCLASS()
class PROJECTVITALS_API UGE_ThresholdDebuff_HydrationLow : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ThresholdDebuff_HydrationLow();
};

/**
 * Threshold debuff: Calories Low
 * Applied when State.Calories.Low tag is active.
 * Effect: StaminaRegenRate -20%
 */
UCLASS()
class PROJECTVITALS_API UGE_ThresholdDebuff_CaloriesLow : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ThresholdDebuff_CaloriesLow();
};

/**
 * Threshold debuff: Calories Critical
 * Applied when State.Calories.Critical or State.Calories.Empty tag is active.
 * Effect: ConditionRegenRate = 0 (blocks condition regen)
 */
UCLASS()
class PROJECTVITALS_API UGE_ThresholdDebuff_CaloriesCritical : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ThresholdDebuff_CaloriesCritical();
};

/**
 * Threshold debuff: Hydration Critical
 * Applied when State.Hydration.Critical or State.Hydration.Empty tag is active.
 * Effect: ConditionRegenRate = 0 (blocks condition regen)
 */
UCLASS()
class PROJECTVITALS_API UGE_ThresholdDebuff_HydrationCritical : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ThresholdDebuff_HydrationCritical();
};

/**
 * Threshold debuff: Fatigue High (Exhausted/Critical)
 * Applied when State.Fatigue.Exhausted or State.Fatigue.Critical tag is active.
 * Effect: StaminaRegenRate -30%
 */
UCLASS()
class PROJECTVITALS_API UGE_ThresholdDebuff_FatigueHigh : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ThresholdDebuff_FatigueHigh();
};

/**
 * Condition drain effect: Needs Critical
 * Applied when any survival need is Critical/Empty.
 * Effect: Periodic Condition drain (-0.5% per second)
 */
UCLASS()
class PROJECTVITALS_API UGE_ConditionDrain_NeedsCritical : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ConditionDrain_NeedsCritical();
};

// -------------------------------------------------------------------------
// Weight Encumbrance Debuffs
// -------------------------------------------------------------------------

/**
 * Threshold debuff: Weight Heavy (80-100% capacity)
 * Applied when State.Weight.Heavy tag is active.
 * Effect: StaminaRegenRate -15%
 */
UCLASS()
class PROJECTVITALS_API UGE_ThresholdDebuff_WeightHeavy : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ThresholdDebuff_WeightHeavy();
};

/**
 * Threshold debuff: Weight Overweight (>100% capacity)
 * Applied when State.Weight.Overweight tag is active.
 * Effect: StaminaRegenRate -30%
 */
UCLASS()
class PROJECTVITALS_API UGE_ThresholdDebuff_WeightOverweight : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ThresholdDebuff_WeightOverweight();
};

// -------------------------------------------------------------------------
// Regeneration Effects
// -------------------------------------------------------------------------

/**
 * Condition regeneration effect.
 * Applied when regen is allowed (no bleeding, needs not critical).
 * Effect: Periodic Condition regen (+1% per second, scaled by ConditionRegenRate)
 *
 * Gating conditions (checked by VitalsComponent):
 * - Bleeding = 0
 * - ConditionRegenRate > 0 (set to 0 by CaloriesCritical/HydrationCritical debuffs)
 * - Condition < MaxCondition
 */
UCLASS()
class PROJECTVITALS_API UGE_ConditionRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ConditionRegen();
};
