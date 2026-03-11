// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// -------------------------------------------------------------------------
// Condition System (life/death, long-term days)
// -------------------------------------------------------------------------

struct FVitalsSurvivalCalories
{
	float Current = 2500.f;
	float Max = 2500.f;
	float ConditionDeathDays = 13.f;
};

struct FVitalsSurvivalHydration
{
	float Current = 3.f;
	float Max = 3.f;
	float ConditionDeathDays = 1.f;
};

struct FVitalsSurvivalFatigue
{
	float Current = 0.f;
	float Max = 100.f;
	float HoursToEmpty = 16.f;
	float ConditionDeathDays = 4.f;
};

struct FVitalsSurvival
{
	FVitalsSurvivalCalories Calories;
	FVitalsSurvivalHydration Hydration;
	FVitalsSurvivalFatigue Fatigue;
};

struct FVitalsStatusEffect
{
	float Current = 0.f;
	float Max = 1.f;
};

struct FVitalsStatus
{
	FVitalsStatusEffect Bleeding;
	FVitalsStatusEffect Poisoned;
	FVitalsStatusEffect Radiation;
};

struct FVitalsActivity
{
	float MET_Idle = 1.3f;
	float MET_Walk = 2.5f;
	float MET_Jog = 6.0f;
	float MET_Sprint = 10.0f;
	float SpeedThreshold_Idle = 10.0f;
	float SpeedThreshold_Walk = 150.0f;
	float SpeedThreshold_Jog = 350.0f;
};

struct FVitalsMetabolism
{
	float CharacterWeightKg = 75.0f;
	float HydrationPerKcal = 0.001f;
	FVitalsActivity Activity;
};

struct FVitalsConditionConfig
{
	float Current = 100.f;
	float Max = 100.f;
	float RegenRate = 1.f;
	float BaseRegenDays = 4.f;

	FVitalsSurvival Survival;
	FVitalsStatus Status;
	FVitalsMetabolism Metabolism;

	// Derived rates (computed from timelines on load)
	float BaseRegenPerSec = 0.f;
	float DehydrationDrainPerSec = 0.f;
	float StarvationDrainPerSec = 0.f;
	float ExhaustionDrainPerSec = 0.f;
	float FatigueGainPerSec = 0.f;

	// Derive per-second rates from timeline config
	void ComputeDerivedRates()
	{
		BaseRegenPerSec = (BaseRegenDays > 0.f)
			? (Max / (BaseRegenDays * 86400.f)) : 0.f;
		DehydrationDrainPerSec = (Survival.Hydration.ConditionDeathDays > 0.f)
			? (Max / (Survival.Hydration.ConditionDeathDays * 86400.f)) : 0.f;
		StarvationDrainPerSec = (Survival.Calories.ConditionDeathDays > 0.f)
			? (Max / (Survival.Calories.ConditionDeathDays * 86400.f)) : 0.f;
		ExhaustionDrainPerSec = (Survival.Fatigue.ConditionDeathDays > 0.f)
			? (Max / (Survival.Fatigue.ConditionDeathDays * 86400.f)) : 0.f;
		FatigueGainPerSec = (Survival.Fatigue.HoursToEmpty > 0.f)
			? (Survival.Fatigue.Max / (Survival.Fatigue.HoursToEmpty * 3600.f)) : 0.f;
	}
};

// -------------------------------------------------------------------------
// Stamina System (short-term energy, seconds/minutes)
// -------------------------------------------------------------------------

struct FVitalsStaminaMovement
{
	float SprintDrainPerSec = 10.0f;
	float JogDrainPerSec = 5.0f;
	float BaseRegenPerSec = 6.67f;
};

struct FVitalsStaminaInteractions
{
	float Light = 2.0f;
	float Medium = 5.0f;
	float Heavy = 15.0f;
	float MinRequired = 5.0f;
};

struct FVitalsStaminaCombat
{
	float LightAttack = 10.0f;
	float HeavyAttack = 25.0f;
	float Dodge = 15.0f;
	float BlockPerSec = 5.0f;
};

struct FVitalsStaminaConfig
{
	float Current = 100.f;
	float Max = 100.f;
	float RegenRate = 1.f;

	FVitalsStaminaMovement Movement;
	FVitalsStaminaInteractions Interactions;
	FVitalsStaminaCombat Combat;
};

// -------------------------------------------------------------------------
// Combined config (loaded from Data/vitals_config.json)
// -------------------------------------------------------------------------

struct FVitalsConfig
{
	FVitalsConditionConfig Condition;
	FVitalsStaminaConfig Stamina;
	bool bLoadedFromJson = false;
};

/**
 * Cached singleton loader for vitals config.
 * Reads Data/vitals_config.json via FProjectPaths, parses once, caches result.
 * Falls back to struct defaults if file is missing or malformed.
 */
class PROJECTVITALS_API FVitalsConfigLoader
{
public:
	// Cached singleton access (parses on first call)
	static const FVitalsConfig& Get();

	// Resolve path to vitals_config.json via FProjectPaths
	static FString ResolvePath();

	// Load from explicit path (for testing, bypasses cache)
	static bool LoadFromFile(const FString& FilePath, FVitalsConfig& OutConfig);
};
