// Copyright ALIS. All Rights Reserved.

#include "Effects/GE_GenericInstant.h"
#include "ProjectGameplayTags.h"
#include "GameplayEffect.h"

#include "Attributes/HealthAttributeSet.h"
#include "Attributes/StaminaAttributeSet.h"
#include "Attributes/SurvivalAttributeSet.h"
#include "Attributes/StatusAttributeSet.h"

namespace
{
	FGameplayEffectModifierMagnitude MakeSetByCallerMagnitude(const FGameplayTag& Tag)
	{
		FSetByCallerFloat SetByCallerConfig;
		SetByCallerConfig.DataTag = Tag;
		return FGameplayEffectModifierMagnitude(SetByCallerConfig);
	}
}

UGE_GenericInstant::UGE_GenericInstant()
{
	// Instant effect - applies once, no duration
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// === Condition Modifier ===
	// Targets HealthAttributeSet::Condition, magnitude from SetByCaller.Condition
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UHealthAttributeSet::GetConditionAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectTags::SetByCaller_Condition.GetTag());
		Modifiers.Add(Modifier);
	}

	// === Stamina Modifier ===
	// Targets StaminaAttributeSet::Stamina, magnitude from SetByCaller.Stamina
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UStaminaAttributeSet::GetStaminaAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectTags::SetByCaller_Stamina.GetTag());
		Modifiers.Add(Modifier);
	}

	// === Hydration Modifier ===
	// Targets SurvivalAttributeSet::Hydration, magnitude from SetByCaller.Hydration
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = USurvivalAttributeSet::GetHydrationAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectTags::SetByCaller_Hydration.GetTag());
		Modifiers.Add(Modifier);
	}

	// === Calories Modifier ===
	// Targets SurvivalAttributeSet::Calories, magnitude from SetByCaller.Calories
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = USurvivalAttributeSet::GetCaloriesAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectTags::SetByCaller_Calories.GetTag());
		Modifiers.Add(Modifier);
	}

	// === Fatigue Modifier ===
	// Targets SurvivalAttributeSet::Fatigue, magnitude from SetByCaller.Fatigue
	// Note: Fatigue is inverted (0=rested, 100=exhausted), so positive values increase fatigue
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = USurvivalAttributeSet::GetFatigueAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectTags::SetByCaller_Fatigue.GetTag());
		Modifiers.Add(Modifier);
	}

	// === Bleeding Modifier ===
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UStatusAttributeSet::GetBleedingAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectTags::SetByCaller_Bleeding.GetTag());
		Modifiers.Add(Modifier);
	}

	// === Poisoned Modifier ===
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UStatusAttributeSet::GetPoisonedAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectTags::SetByCaller_Poisoned.GetTag());
		Modifiers.Add(Modifier);
	}

	// === Radiation Modifier ===
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UStatusAttributeSet::GetRadiationAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectTags::SetByCaller_Radiation.GetTag());
		Modifiers.Add(Modifier);
	}

	// === MovementSpeedMultiplier Modifier ===
	// Additive: -0.75 means 25% speed (base 1.0 - 0.75 = 0.25)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UStatusAttributeSet::GetMovementSpeedMultiplierAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectTags::SetByCaller_MovementSpeedMultiplier.GetTag());
		Modifiers.Add(Modifier);
	}

}
