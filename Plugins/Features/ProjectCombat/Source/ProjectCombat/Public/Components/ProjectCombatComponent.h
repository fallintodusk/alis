// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProjectCombatComponent.generated.h"

/**
 * Example combat component that manages health, damage, and combat state.
 * Demonstrates a feature-specific component with zero UI dependencies.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class PROJECTCOMBAT_API UProjectCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProjectCombatComponent();

	virtual void BeginPlay() override;

	/** Current health value (0.0 = dead, 1.0 = full health). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetHealth() const { return Health; }

	/** Maximum health value. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetMaxHealth() const { return MaxHealth; }

	/** Apply damage to this actor. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TakeDamage(float DamageAmount, AActor* DamageCauser);

	/** Heal this actor. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void Heal(float HealAmount);

	/** True if actor is dead (Health <= 0). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDead() const { return Health <= 0.0f; }

protected:
	/** Current health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float Health = 100.0f;

	/** Maximum health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float MaxHealth = 100.0f;

	/** Damage multiplier (for difficulty scaling). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float DamageMultiplier = 1.0f;

	/** Called when health reaches zero. */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat")
	void OnDeath();
	virtual void OnDeath_Implementation();

	/** Called when damage is received. */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat")
	void OnDamageReceived(float DamageAmount, AActor* DamageCauser);
	virtual void OnDamageReceived_Implementation(float DamageAmount, AActor* DamageCauser);
};
