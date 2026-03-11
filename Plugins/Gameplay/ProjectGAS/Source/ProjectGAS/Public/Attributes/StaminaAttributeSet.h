// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ProjectGASAttributeMacros.h"
#include "StaminaAttributeSet.generated.h"

/**
 * Stamina attributes for actors that can tire (running, attacking, etc.).
 *
 * === When to Use ===
 *
 * Use this set for actors that:
 *   - Can sprint/run (costs stamina)
 *   - Perform physical actions (attack, dodge, climb)
 *   - Need a "tiredness" mechanic separate from Energy/Sleep
 *
 * Stamina vs Energy:
 *   - Stamina: Short-term, regenerates quickly, depleted by actions
 *   - Energy: Long-term, affected by food/sleep, general fatigue
 *
 * === Composition ===
 *
 *   - Player: HealthAttributeSet + SurvivalAttributeSet + StaminaAttributeSet
 *   - Horse: HealthAttributeSet + StaminaAttributeSet (can tire, no survival)
 *   - Vehicle: HealthAttributeSet only (no stamina)
 *
 * SetByCaller tag: SetByCaller.Stamina
 */
UCLASS()
class PROJECTGAS_API UStaminaAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UStaminaAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// Current stamina (0 = exhausted, cannot sprint/attack)
	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UStaminaAttributeSet, Stamina)

	// Maximum stamina cap
	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UStaminaAttributeSet, MaxStamina)

	// Stamina regeneration rate multiplier (1.0 = 100%, 0.8 = 80%, 0 = disabled)
	// Used by threshold debuffs (e.g., CaloriesLow reduces by 20%)
	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_StaminaRegenRate)
	FGameplayAttributeData StaminaRegenRate;
	ATTRIBUTE_ACCESSORS(UStaminaAttributeSet, StaminaRegenRate)

protected:
	void AdjustAttributeForMaxChange(
		const FGameplayAttributeData& AffectedAttribute,
		const FGameplayAttributeData& MaxAttribute,
		float NewMaxValue,
		const FGameplayAttribute& AffectedAttributeProperty) const;

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_StaminaRegenRate(const FGameplayAttributeData& OldValue);
};
