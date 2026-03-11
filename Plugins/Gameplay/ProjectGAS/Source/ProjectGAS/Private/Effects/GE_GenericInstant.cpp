// Copyright ALIS. All Rights Reserved.

#include "Effects/GE_GenericInstant.h"
#include "ProjectGameplayTags.h"
#include "GameplayEffect.h"

// Forward declare attribute classes for GetAttribute()
#include "Attributes/HealthAttributeSet.h"
#include "Attributes/StaminaAttributeSet.h"
#include "Attributes/SurvivalAttributeSet.h"

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

}
