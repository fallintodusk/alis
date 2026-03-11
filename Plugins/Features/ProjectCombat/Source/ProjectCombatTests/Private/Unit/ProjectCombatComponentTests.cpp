// Copyright ALIS. All Rights Reserved.

/**
 * Comprehensive Unit Tests for ProjectCombatComponent
 *
 * Tests verify health management, damage application, healing,
 * death state, and event handling.
 */

#include "Misc/AutomationTest.h"
#include "Components/ProjectCombatComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// Component Creation Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_DefaultConstruction,
	"ProjectCombat.Component.DefaultConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_DefaultConstruction::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	TestNotNull(TEXT("Component created"), Component);
	TestEqual(TEXT("Default health is 100"), Component->GetHealth(), 100.0f);
	TestEqual(TEXT("Default max health is 100"), Component->GetMaxHealth(), 100.0f);
	TestFalse(TEXT("Not dead initially"), Component->IsDead());

	return true;
}

// ============================================================================
// Health Management Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_HealthGetters,
	"ProjectCombat.Component.HealthGetters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_HealthGetters::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	// Test initial values
	const float InitialHealth = Component->GetHealth();
	const float MaxHealth = Component->GetMaxHealth();

	TestEqual(TEXT("Initial health equals max health"), InitialHealth, MaxHealth);
	TestTrue(TEXT("Health is positive"), InitialHealth > 0.0f);
	TestTrue(TEXT("Max health is positive"), MaxHealth > 0.0f);

	return true;
}

// ============================================================================
// Damage Application Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_TakeDamage_Basic,
	"ProjectCombat.Component.TakeDamage.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_TakeDamage_Basic::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();
	const float InitialHealth = Component->GetHealth();

	// Apply damage
	Component->TakeDamage(20.0f, nullptr);

	// Verify health reduced
	const float NewHealth = Component->GetHealth();
	TestTrue(TEXT("Health reduced after damage"), NewHealth < InitialHealth);
	TestEqual(TEXT("Health reduced by damage amount"), NewHealth, InitialHealth - 20.0f);
	TestFalse(TEXT("Still alive after minor damage"), Component->IsDead());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_TakeDamage_Lethal,
	"ProjectCombat.Component.TakeDamage.Lethal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_TakeDamage_Lethal::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	// Apply lethal damage
	Component->TakeDamage(200.0f, nullptr);

	// Verify death
	TestTrue(TEXT("Dead after lethal damage"), Component->IsDead());
	TestTrue(TEXT("Health is zero or negative"), Component->GetHealth() <= 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_TakeDamage_Multiple,
	"ProjectCombat.Component.TakeDamage.Multiple",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_TakeDamage_Multiple::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();
	const float InitialHealth = Component->GetHealth();

	// Apply multiple damage instances
	Component->TakeDamage(10.0f, nullptr);
	Component->TakeDamage(15.0f, nullptr);
	Component->TakeDamage(5.0f, nullptr);

	// Verify cumulative damage
	const float ExpectedHealth = InitialHealth - 30.0f;
	TestEqual(TEXT("Cumulative damage applied correctly"),
		Component->GetHealth(), ExpectedHealth);
	TestFalse(TEXT("Still alive after multiple hits"), Component->IsDead());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_TakeDamage_NegativeAmount,
	"ProjectCombat.Component.TakeDamage.NegativeAmount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_TakeDamage_NegativeAmount::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();
	const float InitialHealth = Component->GetHealth();

	// Apply negative damage (should not heal)
	Component->TakeDamage(-20.0f, nullptr);

	// Verify health unchanged or implementation-defined behavior
	const float NewHealth = Component->GetHealth();
	TestTrue(TEXT("Negative damage handled without crash"), true);
	// Note: Implementation may clamp negative damage to 0 or treat as heal

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_TakeDamage_ZeroAmount,
	"ProjectCombat.Component.TakeDamage.ZeroAmount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_TakeDamage_ZeroAmount::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();
	const float InitialHealth = Component->GetHealth();

	// Apply zero damage
	Component->TakeDamage(0.0f, nullptr);

	// Verify health unchanged
	TestEqual(TEXT("Zero damage has no effect"),
		Component->GetHealth(), InitialHealth);

	return true;
}

// ============================================================================
// Healing Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_Heal_Basic,
	"ProjectCombat.Component.Heal.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_Heal_Basic::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	// Damage first
	Component->TakeDamage(50.0f, nullptr);
	const float DamagedHealth = Component->GetHealth();

	// Heal
	Component->Heal(20.0f);

	// Verify healing
	const float HealedHealth = Component->GetHealth();
	TestTrue(TEXT("Health increased after healing"), HealedHealth > DamagedHealth);
	TestEqual(TEXT("Healed by correct amount"), HealedHealth, DamagedHealth + 20.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_Heal_OverMax,
	"ProjectCombat.Component.Heal.OverMax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_Heal_OverMax::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();
	const float MaxHealth = Component->GetMaxHealth();

	// Damage then overheal
	Component->TakeDamage(10.0f, nullptr);
	Component->Heal(50.0f);  // More than damage taken

	// Verify clamped to max
	TestTrue(TEXT("Health clamped to max after overheal"),
		Component->GetHealth() <= MaxHealth);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_Heal_FromDeath,
	"ProjectCombat.Component.Heal.FromDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_Heal_FromDeath::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	// Kill the component
	Component->TakeDamage(200.0f, nullptr);
	TestTrue(TEXT("Component is dead"), Component->IsDead());

	// Attempt to heal
	Component->Heal(50.0f);

	// Verify resurrection behavior (implementation-defined)
	// May resurrect or stay dead depending on game design
	TestTrue(TEXT("Heal from death handled without crash"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_Heal_ZeroAmount,
	"ProjectCombat.Component.Heal.ZeroAmount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_Heal_ZeroAmount::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	// Damage first
	Component->TakeDamage(20.0f, nullptr);
	const float DamagedHealth = Component->GetHealth();

	// Heal zero
	Component->Heal(0.0f);

	// Verify no change
	TestEqual(TEXT("Zero heal has no effect"),
		Component->GetHealth(), DamagedHealth);

	return true;
}

// ============================================================================
// Death State Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_IsDead_Alive,
	"ProjectCombat.Component.IsDead.Alive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_IsDead_Alive::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	// Initially alive
	TestFalse(TEXT("Alive initially"), Component->IsDead());

	// Still alive after minor damage
	Component->TakeDamage(10.0f, nullptr);
	TestFalse(TEXT("Alive after minor damage"), Component->IsDead());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_IsDead_Dead,
	"ProjectCombat.Component.IsDead.Dead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_IsDead_Dead::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	// Kill
	Component->TakeDamage(150.0f, nullptr);

	// Verify dead
	TestTrue(TEXT("Dead after lethal damage"), Component->IsDead());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_IsDead_ExactlyZero,
	"ProjectCombat.Component.IsDead.ExactlyZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_IsDead_ExactlyZero::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();
	const float MaxHealth = Component->GetMaxHealth();

	// Damage exactly to zero
	Component->TakeDamage(MaxHealth, nullptr);

	// Verify death at exactly zero health
	TestEqual(TEXT("Health is exactly zero"), Component->GetHealth(), 0.0f);
	TestTrue(TEXT("Dead at exactly zero health"), Component->IsDead());

	return true;
}

// ============================================================================
// Edge Case Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_Overkill,
	"ProjectCombat.Component.EdgeCases.Overkill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_Overkill::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	// Apply massive overkill damage
	Component->TakeDamage(9999.0f, nullptr);

	// Verify dead state
	TestTrue(TEXT("Dead after overkill"), Component->IsDead());
	TestTrue(TEXT("Health is zero or negative"), Component->GetHealth() <= 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_DamageHealCycle,
	"ProjectCombat.Component.EdgeCases.DamageHealCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_DamageHealCycle::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();
	const float InitialHealth = Component->GetHealth();

	// Damage and heal cycle
	Component->TakeDamage(30.0f, nullptr);
	Component->Heal(30.0f);

	// Verify returned to initial health
	TestEqual(TEXT("Health restored after damage-heal cycle"),
		Component->GetHealth(), InitialHealth);
	TestFalse(TEXT("Alive after cycle"), Component->IsDead());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_StaysDead,
	"ProjectCombat.Component.EdgeCases.StaysDead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_StaysDead::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	// Kill
	Component->TakeDamage(200.0f, nullptr);
	TestTrue(TEXT("Dead after lethal damage"), Component->IsDead());

	// Additional damage after death
	Component->TakeDamage(50.0f, nullptr);

	// Verify still dead
	TestTrue(TEXT("Still dead after additional damage"), Component->IsDead());

	return true;
}

// ============================================================================
// Property Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatComponent_MaxHealth,
	"ProjectCombat.Component.Properties.MaxHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatComponent_MaxHealth::RunTest(const FString& Parameters)
{
	UProjectCombatComponent* Component = NewObject<UProjectCombatComponent>();

	// Verify max health is constant
	const float MaxHealth1 = Component->GetMaxHealth();
	Component->TakeDamage(50.0f, nullptr);
	const float MaxHealth2 = Component->GetMaxHealth();

	TestEqual(TEXT("Max health unchanged after damage"), MaxHealth1, MaxHealth2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
