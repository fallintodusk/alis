// Copyright ALIS. All Rights Reserved.

#include "Effects/GE_ThresholdDebuffs.h"
#include "Attributes/StaminaAttributeSet.h"
#include "Attributes/HealthAttributeSet.h"
#include "Attributes/StatusAttributeSet.h"
#include "GameplayEffect.h"

// -------------------------------------------------------------------------
// GE_ThresholdDebuff_HydrationLow
// Effect: MaxStamina -15% (multiplicative)
// -------------------------------------------------------------------------

UGE_ThresholdDebuff_HydrationLow::UGE_ThresholdDebuff_HydrationLow()
{
	// Infinite duration - removed when state tag changes
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Reduce MaxStamina by 15% (multiply by 0.85)
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UStaminaAttributeSet::GetMaxStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.85f));
	Modifiers.Add(Modifier);
}

// -------------------------------------------------------------------------
// GE_ThresholdDebuff_CaloriesLow
// Effect: StaminaRegenRate -20% (multiplicative)
// -------------------------------------------------------------------------

UGE_ThresholdDebuff_CaloriesLow::UGE_ThresholdDebuff_CaloriesLow()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Reduce StaminaRegenRate by 20% (multiply by 0.8)
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UStaminaAttributeSet::GetStaminaRegenRateAttribute();
	Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.8f));
	Modifiers.Add(Modifier);
}

// -------------------------------------------------------------------------
// GE_ThresholdDebuff_CaloriesCritical
// Effect: ConditionRegenRate = 0 (override)
// -------------------------------------------------------------------------

UGE_ThresholdDebuff_CaloriesCritical::UGE_ThresholdDebuff_CaloriesCritical()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Set ConditionRegenRate to 0 (override to block regen)
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UHealthAttributeSet::GetConditionRegenRateAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.f));
	Modifiers.Add(Modifier);
}

// -------------------------------------------------------------------------
// GE_ThresholdDebuff_HydrationCritical
// Effect: ConditionRegenRate = 0 (override)
// -------------------------------------------------------------------------

UGE_ThresholdDebuff_HydrationCritical::UGE_ThresholdDebuff_HydrationCritical()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Set ConditionRegenRate to 0 (override to block regen)
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UHealthAttributeSet::GetConditionRegenRateAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.f));
	Modifiers.Add(Modifier);
}

// -------------------------------------------------------------------------
// GE_ThresholdDebuff_FatigueHigh
// Effect: StaminaRegenRate -30% (multiplicative)
// -------------------------------------------------------------------------

UGE_ThresholdDebuff_FatigueHigh::UGE_ThresholdDebuff_FatigueHigh()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Reduce StaminaRegenRate by 30% (multiply by 0.7)
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UStaminaAttributeSet::GetStaminaRegenRateAttribute();
	Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.7f));
	Modifiers.Add(Modifier);
}

// -------------------------------------------------------------------------
// GE_ThresholdDebuff_WeightHeavy
// Effect: StaminaRegenRate -15%, MovementSpeedMultiplier -15%
// -------------------------------------------------------------------------

UGE_ThresholdDebuff_WeightHeavy::UGE_ThresholdDebuff_WeightHeavy()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Reduce StaminaRegenRate by 15% (multiply by 0.85)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UStaminaAttributeSet::GetStaminaRegenRateAttribute();
		Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.85f));
		Modifiers.Add(Modifier);
	}

	// Reduce MovementSpeedMultiplier by 15% (multiply by 0.85)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UStatusAttributeSet::GetMovementSpeedMultiplierAttribute();
		Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.85f));
		Modifiers.Add(Modifier);
	}
}

// -------------------------------------------------------------------------
// GE_ThresholdDebuff_WeightOverweight
// Effect: StaminaRegenRate -30%, MovementSpeedMultiplier -45%
// -------------------------------------------------------------------------

UGE_ThresholdDebuff_WeightOverweight::UGE_ThresholdDebuff_WeightOverweight()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Reduce StaminaRegenRate by 30% (multiply by 0.7)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UStaminaAttributeSet::GetStaminaRegenRateAttribute();
		Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.7f));
		Modifiers.Add(Modifier);
	}

	// Reduce MovementSpeedMultiplier by 45% (multiply by 0.55)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UStatusAttributeSet::GetMovementSpeedMultiplierAttribute();
		Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.55f));
		Modifiers.Add(Modifier);
	}
}

// -------------------------------------------------------------------------
// GE_ConditionDrain_NeedsCritical
// Effect: Periodic Condition drain (-0.5 per second)
// -------------------------------------------------------------------------

UGE_ConditionDrain_NeedsCritical::UGE_ConditionDrain_NeedsCritical()
{
	// Infinite duration with periodic execution
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 1.0f;  // Execute every 1 second

	// Drain Condition by 0.5 per tick
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UHealthAttributeSet::GetConditionAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-0.5f));
	Modifiers.Add(Modifier);
}

// -------------------------------------------------------------------------
// GE_ConditionRegen
// Effect: Periodic Condition regen (+1 per second)
// Note: ConditionRegenRate multiplier is applied by VitalsComponent when checking gating
// -------------------------------------------------------------------------

UGE_ConditionRegen::UGE_ConditionRegen()
{
	// Infinite duration with periodic execution
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 1.0f;  // Execute every 1 second

	// Regen Condition by 1 per tick (1% of 100 max)
	// Actual rate is gated by VitalsComponent checking ConditionRegenRate
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UHealthAttributeSet::GetConditionAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
	Modifiers.Add(Modifier);
}
