// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ProjectGASAttributeMacros.h"
#include "SurvivalAttributeSet.generated.h"

/**
 * Survival attributes for living beings (player, NPCs, animals).
 *
 * Simplified survival model (The Long Dark / DayZ pattern):
 *   - Hydration: Water store (0 = dehydrated, max 3.0L)
 *   - Calories: Energy from food (0 = starving, max 2500 kcal)
 *   - Fatigue: Sleep debt (0 = rested, 100 = exhausted)
 *
 * Composition Pattern:
 *   - Player: HealthAttributeSet + SurvivalAttributeSet + StaminaAttributeSet + StatusAttributeSet
 *   - Animal: HealthAttributeSet + SurvivalAttributeSet
 *   - Robot: HealthAttributeSet only (no survival needs)
 *
 * GAS silently ignores effects targeting missing attributes.
 *
 * SetByCaller Tags (used by VitalsComponent::ApplyMagnitudes, GE_ItemConsumable):
 *   - SetByCaller.Hydration
 *   - SetByCaller.Calories
 *   - SetByCaller.Fatigue
 *
 * SOT: docs/gameplay/character.md, todo/current/gas_ui_mechanics.md
 */
UCLASS()
class PROJECTGAS_API USurvivalAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USurvivalAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// === Hydration (water store, 0 = dehydrated, max = 3.0 liters) ===
	UPROPERTY(BlueprintReadOnly, Category = "Survival", ReplicatedUsing = OnRep_Hydration)
	FGameplayAttributeData Hydration;
	ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Hydration)

	UPROPERTY(BlueprintReadOnly, Category = "Survival", ReplicatedUsing = OnRep_MaxHydration)
	FGameplayAttributeData MaxHydration;
	ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxHydration)

	// === Calories (energy from food, 0 = starving, max = 2500 kcal) ===
	UPROPERTY(BlueprintReadOnly, Category = "Survival", ReplicatedUsing = OnRep_Calories)
	FGameplayAttributeData Calories;
	ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Calories)

	UPROPERTY(BlueprintReadOnly, Category = "Survival", ReplicatedUsing = OnRep_MaxCalories)
	FGameplayAttributeData MaxCalories;
	ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxCalories)

	// === Fatigue (sleep debt, 0 = rested, 100 = exhausted) ===
	// Note: Inverted from old Sleep attribute (0 was exhausted, 100 was rested)
	UPROPERTY(BlueprintReadOnly, Category = "Survival", ReplicatedUsing = OnRep_Fatigue)
	FGameplayAttributeData Fatigue;
	ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, Fatigue)

	UPROPERTY(BlueprintReadOnly, Category = "Survival", ReplicatedUsing = OnRep_MaxFatigue)
	FGameplayAttributeData MaxFatigue;
	ATTRIBUTE_ACCESSORS(USurvivalAttributeSet, MaxFatigue)

protected:
	// Clamps current value when Max changes downward
	void AdjustAttributeForMaxChange(
		const FGameplayAttributeData& AffectedAttribute,
		const FGameplayAttributeData& MaxAttribute,
		float NewMaxValue,
		const FGameplayAttribute& AffectedAttributeProperty) const;

	UFUNCTION()
	void OnRep_Hydration(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxHydration(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Calories(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxCalories(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Fatigue(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxFatigue(const FGameplayAttributeData& OldValue);
};
