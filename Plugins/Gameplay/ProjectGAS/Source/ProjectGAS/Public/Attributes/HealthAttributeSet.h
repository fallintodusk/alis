// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ProjectGASAttributeMacros.h"
#include "HealthAttributeSet.generated.h"

/**
 * Condition (health) attributes for any damageable actor.
 *
 * Named "Condition" for The Long Dark / DayZ survival pattern consistency.
 * This is the primary "life" attribute players must manage.
 *
 * Composition Pattern:
 *   - Player: HealthAttributeSet + SurvivalAttributeSet + StaminaAttributeSet + StatusAttributeSet
 *   - Vehicle: HealthAttributeSet + VehicleAttributeSet
 *   - Turret: HealthAttributeSet only
 *   - Animal: HealthAttributeSet + SurvivalAttributeSet
 *
 * GAS ignores missing attributes, so GE_Damage works on any actor with Condition.
 *
 * SetByCaller tag: SetByCaller.Condition
 *
 * SOT: docs/gameplay/character.md, todo/current/gas_ui_mechanics.md
 */
UCLASS()
class PROJECTGAS_API UHealthAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UHealthAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// Current condition (0 = dead, 100 = full health)
	UPROPERTY(BlueprintReadOnly, Category = "Condition", ReplicatedUsing = OnRep_Condition)
	FGameplayAttributeData Condition;
	ATTRIBUTE_ACCESSORS(UHealthAttributeSet, Condition)

	// Maximum condition cap
	UPROPERTY(BlueprintReadOnly, Category = "Condition", ReplicatedUsing = OnRep_MaxCondition)
	FGameplayAttributeData MaxCondition;
	ATTRIBUTE_ACCESSORS(UHealthAttributeSet, MaxCondition)

	// Condition regeneration rate multiplier (1.0 = 100%, 0 = disabled)
	// Used by regen gating (set to 0 when bleeding, needs critical, etc.)
	UPROPERTY(BlueprintReadOnly, Category = "Condition", ReplicatedUsing = OnRep_ConditionRegenRate)
	FGameplayAttributeData ConditionRegenRate;
	ATTRIBUTE_ACCESSORS(UHealthAttributeSet, ConditionRegenRate)

protected:
	/**
	 * Clamps current value when Max changes downward.
	 * Common GAS gotcha: if MaxCondition goes from 100 to 50, Condition must also clamp to 50.
	 */
	void AdjustAttributeForMaxChange(
		const FGameplayAttributeData& AffectedAttribute,
		const FGameplayAttributeData& MaxAttribute,
		float NewMaxValue,
		const FGameplayAttribute& AffectedAttributeProperty) const;

	UFUNCTION()
	void OnRep_Condition(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxCondition(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ConditionRegenRate(const FGameplayAttributeData& OldValue);
};
