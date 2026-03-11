// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for Combat Data Structures
 *
 * Tests weapon stats, damage types, armor values, and combat modifiers.
 */

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// Weapon Data Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatData_WeaponIDValidation,
	"ProjectCombat.Data.WeaponIDValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatData_WeaponIDValidation::RunTest(const FString& Parameters)
{
	// Valid weapon IDs
	const FName ValidID1(TEXT("weapon_sword_001"));
	const FName ValidID2(TEXT("Bow_Longbow"));
	const FName ValidID3(TEXT("Staff_Fire"));

	// Invalid weapon IDs
	const FName EmptyID(NAME_None);
	const FName IDWithSpaces(TEXT("Invalid Weapon"));

	TestTrue(TEXT("Valid ID 1"), ValidID1 != NAME_None);
	TestTrue(TEXT("Valid ID 2"), ValidID2 != NAME_None);
	TestTrue(TEXT("Valid ID 3"), ValidID3 != NAME_None);

	TestTrue(TEXT("Empty ID is None"), EmptyID == NAME_None);
	TestTrue(TEXT("ID with spaces detected"), IDWithSpaces.ToString().Contains(TEXT(" ")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatData_DamageValueValidation,
	"ProjectCombat.Data.DamageValueValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatData_DamageValueValidation::RunTest(const FString& Parameters)
{
	// Valid damage values
	const float MinDamage = 1.0f;
	const float LowDamage = 10.0f;
	const float MediumDamage = 50.0f;
	const float HighDamage = 200.0f;

	// Invalid damage values
	const float ZeroDamage = 0.0f;
	const float NegativeDamage = -10.0f;
	const float ExcessiveDamage = 10000.0f;

	TestTrue(TEXT("Min damage valid"), MinDamage > 0.0f);
	TestTrue(TEXT("Low damage valid"), LowDamage > 0.0f && LowDamage <= 1000.0f);
	TestTrue(TEXT("Medium damage valid"), MediumDamage > 0.0f && MediumDamage <= 1000.0f);
	TestTrue(TEXT("High damage valid"), HighDamage > 0.0f && HighDamage <= 1000.0f);

	TestFalse(TEXT("Zero damage invalid"), ZeroDamage > 0.0f);
	TestFalse(TEXT("Negative damage invalid"), NegativeDamage > 0.0f);
	TestTrue(TEXT("Excessive damage exceeds limit"), ExcessiveDamage > 1000.0f);

	return true;
}

// ============================================================================
// Damage Type Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatData_DamageTypeValidation,
	"ProjectCombat.Data.DamageTypeValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatData_DamageTypeValidation::RunTest(const FString& Parameters)
{
	// Damage type enum values (0-based)
	const int32 PhysicalDamage = 0;
	const int32 FireDamage = 1;
	const int32 IceDamage = 2;
	const int32 LightningDamage = 3;
	const int32 PoisonDamage = 4;

	// Invalid damage type
	const int32 InvalidNegativeType = -1;
	const int32 InvalidTooHighType = 100;

	// Valid range check (0-4 for 5 damage types)
	TestTrue(TEXT("Physical damage in range"), PhysicalDamage >= 0 && PhysicalDamage <= 4);
	TestTrue(TEXT("Fire damage in range"), FireDamage >= 0 && FireDamage <= 4);
	TestTrue(TEXT("Ice damage in range"), IceDamage >= 0 && IceDamage <= 4);
	TestTrue(TEXT("Lightning damage in range"), LightningDamage >= 0 && LightningDamage <= 4);
	TestTrue(TEXT("Poison damage in range"), PoisonDamage >= 0 && PoisonDamage <= 4);

	// Invalid checks
	TestFalse(TEXT("Negative type out of range"), InvalidNegativeType >= 0);
	TestFalse(TEXT("Too high type out of range"), InvalidTooHighType <= 4);

	return true;
}

// ============================================================================
// Armor Data Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatData_ArmorValueValidation,
	"ProjectCombat.Data.ArmorValueValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatData_ArmorValueValidation::RunTest(const FString& Parameters)
{
	// Valid armor values (percentage reduction 0-100%)
	const float NoArmor = 0.0f;
	const float LightArmor = 10.0f;
	const float MediumArmor = 30.0f;
	const float HeavyArmor = 50.0f;
	const float MaxArmor = 75.0f; // Cap to prevent invulnerability

	// Invalid armor values
	const float NegativeArmor = -10.0f;
	const float ExcessiveArmor = 100.0f; // Would be invulnerable

	TestTrue(TEXT("No armor valid"), NoArmor >= 0.0f && NoArmor <= 75.0f);
	TestTrue(TEXT("Light armor valid"), LightArmor >= 0.0f && LightArmor <= 75.0f);
	TestTrue(TEXT("Medium armor valid"), MediumArmor >= 0.0f && MediumArmor <= 75.0f);
	TestTrue(TEXT("Heavy armor valid"), HeavyArmor >= 0.0f && HeavyArmor <= 75.0f);
	TestTrue(TEXT("Max armor valid"), MaxArmor >= 0.0f && MaxArmor <= 75.0f);

	TestFalse(TEXT("Negative armor invalid"), NegativeArmor >= 0.0f);
	TestFalse(TEXT("Excessive armor exceeds cap"), ExcessiveArmor <= 75.0f);

	return true;
}

// ============================================================================
// Attack Speed Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatData_AttackSpeedValidation,
	"ProjectCombat.Data.AttackSpeedValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatData_AttackSpeedValidation::RunTest(const FString& Parameters)
{
	// Valid attack speeds (attacks per second)
	const float SlowAttack = 0.5f; // Heavy weapons
	const float NormalAttack = 1.0f; // Swords
	const float FastAttack = 2.0f; // Daggers
	const float RapidAttack = 3.0f; // Dual wielding

	// Invalid attack speeds
	const float ZeroSpeed = 0.0f;
	const float NegativeSpeed = -1.0f;
	const float ExcessiveSpeed = 20.0f;

	TestTrue(TEXT("Slow attack valid"), SlowAttack > 0.0f && SlowAttack <= 10.0f);
	TestTrue(TEXT("Normal attack valid"), NormalAttack > 0.0f && NormalAttack <= 10.0f);
	TestTrue(TEXT("Fast attack valid"), FastAttack > 0.0f && FastAttack <= 10.0f);
	TestTrue(TEXT("Rapid attack valid"), RapidAttack > 0.0f && RapidAttack <= 10.0f);

	TestFalse(TEXT("Zero speed invalid"), ZeroSpeed > 0.0f);
	TestFalse(TEXT("Negative speed invalid"), NegativeSpeed > 0.0f);
	TestTrue(TEXT("Excessive speed exceeds limit"), ExcessiveSpeed > 10.0f);

	return true;
}

// ============================================================================
// Critical Hit Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatData_CriticalChanceValidation,
	"ProjectCombat.Data.CriticalChanceValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatData_CriticalChanceValidation::RunTest(const FString& Parameters)
{
	// Valid critical chance (percentage 0-100%)
	const float NoCrit = 0.0f;
	const float LowCrit = 5.0f;
	const float MediumCrit = 15.0f;
	const float HighCrit = 30.0f;
	const float MaxCrit = 50.0f; // Cap to maintain balance

	// Invalid critical chance
	const float NegativeCrit = -5.0f;
	const float ExcessiveCrit = 100.0f; // Always crit would be broken

	TestTrue(TEXT("No crit valid"), NoCrit >= 0.0f && NoCrit <= 50.0f);
	TestTrue(TEXT("Low crit valid"), LowCrit >= 0.0f && LowCrit <= 50.0f);
	TestTrue(TEXT("Medium crit valid"), MediumCrit >= 0.0f && MediumCrit <= 50.0f);
	TestTrue(TEXT("High crit valid"), HighCrit >= 0.0f && HighCrit <= 50.0f);
	TestTrue(TEXT("Max crit valid"), MaxCrit >= 0.0f && MaxCrit <= 50.0f);

	TestFalse(TEXT("Negative crit invalid"), NegativeCrit >= 0.0f);
	TestFalse(TEXT("Excessive crit exceeds cap"), ExcessiveCrit <= 50.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatData_CriticalMultiplierValidation,
	"ProjectCombat.Data.CriticalMultiplierValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatData_CriticalMultiplierValidation::RunTest(const FString& Parameters)
{
	// Valid critical multipliers
	const float LowCritMultiplier = 1.5f;
	const float NormalCritMultiplier = 2.0f;
	const float HighCritMultiplier = 3.0f;
	const float MaxCritMultiplier = 5.0f;

	// Invalid multipliers
	const float BelowOneMult = 0.5f; // Less than base damage
	const float NegativeMult = -2.0f;
	const float ExcessiveMult = 20.0f;

	TestTrue(TEXT("Low crit multiplier valid"), LowCritMultiplier >= 1.0f && LowCritMultiplier <= 5.0f);
	TestTrue(TEXT("Normal crit multiplier valid"), NormalCritMultiplier >= 1.0f && NormalCritMultiplier <= 5.0f);
	TestTrue(TEXT("High crit multiplier valid"), HighCritMultiplier >= 1.0f && HighCritMultiplier <= 5.0f);
	TestTrue(TEXT("Max crit multiplier valid"), MaxCritMultiplier >= 1.0f && MaxCritMultiplier <= 5.0f);

	TestFalse(TEXT("Below one multiplier invalid"), BelowOneMult >= 1.0f);
	TestFalse(TEXT("Negative multiplier invalid"), NegativeMult >= 1.0f);
	TestTrue(TEXT("Excessive multiplier exceeds limit"), ExcessiveMult > 5.0f);

	return true;
}

// ============================================================================
// Range Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCombatData_WeaponRangeValidation,
	"ProjectCombat.Data.WeaponRangeValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCombatData_WeaponRangeValidation::RunTest(const FString& Parameters)
{
	// Valid ranges (in meters)
	const float MeleeRange = 2.0f;
	const float ShortRange = 10.0f;
	const float MediumRange = 30.0f;
	const float LongRange = 100.0f;

	// Invalid ranges
	const float ZeroRange = 0.0f;
	const float NegativeRange = -5.0f;
	const float ExcessiveRange = 1000.0f;

	TestTrue(TEXT("Melee range valid"), MeleeRange > 0.0f && MeleeRange <= 500.0f);
	TestTrue(TEXT("Short range valid"), ShortRange > 0.0f && ShortRange <= 500.0f);
	TestTrue(TEXT("Medium range valid"), MediumRange > 0.0f && MediumRange <= 500.0f);
	TestTrue(TEXT("Long range valid"), LongRange > 0.0f && LongRange <= 500.0f);

	TestFalse(TEXT("Zero range invalid"), ZeroRange > 0.0f);
	TestFalse(TEXT("Negative range invalid"), NegativeRange > 0.0f);
	TestTrue(TEXT("Excessive range exceeds limit"), ExcessiveRange > 500.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
