// Copyright ALIS. All Rights Reserved.

#include "Components/ProjectCombatComponent.h"
#include "ProjectCombat.h"

namespace
{
FString GetCombatComponentLogName(const UProjectCombatComponent* Component)
{
	if (!Component)
	{
		return TEXT("<null component>");
	}

	if (const AActor* Owner = Component->GetOwner())
	{
		return Owner->GetName();
	}

	return Component->GetName();
}
}

UProjectCombatComponent::UProjectCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UProjectCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize health to max
	Health = MaxHealth;

	UE_LOG(LogProjectCombat, Log, TEXT("ProjectCombatComponent initialized on %s (Health: %.1f/%.1f)"),
		*GetCombatComponentLogName(this), Health, MaxHealth);
}

void UProjectCombatComponent::TakeDamage(float DamageAmount, AActor* DamageCauser)
{
	if (IsDead())
	{
		return; // Already dead, ignore damage
	}

	// Apply damage multiplier
	const float FinalDamage = DamageAmount * DamageMultiplier;

	// Reduce health
	Health = FMath::Max(0.0f, Health - FinalDamage);

	UE_LOG(LogProjectCombat, Log, TEXT("%s took %.1f damage from %s (Health: %.1f/%.1f)"),
		*GetCombatComponentLogName(this),
		FinalDamage,
		DamageCauser ? *DamageCauser->GetName() : TEXT("Unknown"),
		Health,
		MaxHealth);

	// Fire damage received event
	OnDamageReceived(FinalDamage, DamageCauser);

	// Check for death
	if (IsDead())
	{
		UE_LOG(LogProjectCombat, Warning, TEXT("%s died"), *GetCombatComponentLogName(this));
		OnDeath();
	}
}

void UProjectCombatComponent::Heal(float HealAmount)
{
	if (IsDead())
	{
		return; // Dead actors cannot be healed (use resurrection mechanic instead)
	}

	// Increase health (clamped to max)
	Health = FMath::Min(MaxHealth, Health + HealAmount);

	UE_LOG(LogProjectCombat, Log, TEXT("%s healed %.1f (Health: %.1f/%.1f)"),
		*GetCombatComponentLogName(this), HealAmount, Health, MaxHealth);
}

void UProjectCombatComponent::OnDeath_Implementation()
{
	// Default implementation: log death event
	// Blueprint can override to trigger death animations, loot drops, etc.
	UE_LOG(LogProjectCombat, Display, TEXT("%s death event triggered"), *GetCombatComponentLogName(this));
}

void UProjectCombatComponent::OnDamageReceived_Implementation(float DamageAmount, AActor* DamageCauser)
{
	// Default implementation: no-op
	// Blueprint can override to trigger hit reactions, damage numbers, etc.
}
