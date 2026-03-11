// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "VitalsConfig.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// Test: vitals_config.json contract and loader integrity
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectVitalsConfigContractTest,
	"ProjectIntegrationTests.Gameplay.Vitals.ConfigContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FProjectVitalsConfigContractTest::RunTest(const FString& Parameters)
{
	const FVitalsConfig& Cfg = FVitalsConfigLoader::Get();
	const FVitalsConditionConfig& C = Cfg.Condition;
	const FVitalsStaminaConfig& S = Cfg.Stamina;
	const FVitalsMetabolism& M = C.Metabolism;

	// --- Struct defaults are always valid (positive maxes, non-negative values) ---
	TestTrue(TEXT("Condition Max > 0"), C.Max > 0.f);
	TestTrue(TEXT("Stamina Max > 0"), S.Max > 0.f);
	TestTrue(TEXT("Calories Max > 0"), C.Survival.Calories.Max > 0.f);
	TestTrue(TEXT("Hydration Max > 0"), C.Survival.Hydration.Max > 0.f);
	TestTrue(TEXT("Fatigue Max > 0"), C.Survival.Fatigue.Max > 0.f);

	TestTrue(TEXT("Condition >= 0"), C.Current >= 0.f);
	TestTrue(TEXT("Stamina >= 0"), S.Current >= 0.f);
	TestTrue(TEXT("Calories >= 0"), C.Survival.Calories.Current >= 0.f);
	TestTrue(TEXT("Hydration >= 0"), C.Survival.Hydration.Current >= 0.f);
	TestTrue(TEXT("Fatigue >= 0"), C.Survival.Fatigue.Current >= 0.f);

	// Current values must not exceed max
	TestTrue(TEXT("Condition <= Max"), C.Current <= C.Max);
	TestTrue(TEXT("Stamina <= Max"), S.Current <= S.Max);
	TestTrue(TEXT("Calories <= Max"), C.Survival.Calories.Current <= C.Survival.Calories.Max);
	TestTrue(TEXT("Hydration <= Max"), C.Survival.Hydration.Current <= C.Survival.Hydration.Max);
	TestTrue(TEXT("Fatigue <= Max"), C.Survival.Fatigue.Current <= C.Survival.Fatigue.Max);

	// Metabolism config sanity
	TestTrue(TEXT("CharacterWeightKg > 0"), M.CharacterWeightKg > 0.f);
	TestTrue(TEXT("MET_Idle > 0"), M.Activity.MET_Idle > 0.f);
	TestTrue(TEXT("MET_Sprint >= MET_Idle"), M.Activity.MET_Sprint >= M.Activity.MET_Idle);

	// Derived rates should be computed
	TestTrue(TEXT("BaseRegenPerSec > 0"), C.BaseRegenPerSec > 0.f);
	TestTrue(TEXT("DehydrationDrainPerSec > 0"), C.DehydrationDrainPerSec > 0.f);
	TestTrue(TEXT("StarvationDrainPerSec > 0"), C.StarvationDrainPerSec > 0.f);
	TestTrue(TEXT("FatigueGainPerSec > 0"), C.FatigueGainPerSec > 0.f);

	// --- If JSON was found (editor), verify exhausted spawn invariants ---
	if (Cfg.bLoadedFromJson)
	{
		TestTrue(TEXT("JSON loaded: bLoadedFromJson"), Cfg.bLoadedFromJson);

		// Exhausted spawn: not fully rested
		TestTrue(TEXT("Condition < Max (not full HP)"), C.Current < C.Max);
		TestTrue(TEXT("Calories < Max (hungry)"),
			C.Survival.Calories.Current < C.Survival.Calories.Max);
		TestTrue(TEXT("Hydration < Max (thirsty)"),
			C.Survival.Hydration.Current < C.Survival.Hydration.Max);
		TestTrue(TEXT("Fatigue > 0 (not rested)"),
			C.Survival.Fatigue.Current > 0.f);
	}

	// --- Path resolution works ---
	const FString Path = FVitalsConfigLoader::ResolvePath();
	TestFalse(TEXT("ResolvePath returns non-empty"), Path.IsEmpty());

	return true;
}

// ---------------------------------------------------------------------------
// Test: LoadFromFile with explicit path (bypasses cache)
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectVitalsConfigFileLoadTest,
	"ProjectIntegrationTests.Gameplay.Vitals.ConfigFileLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FProjectVitalsConfigFileLoadTest::RunTest(const FString& Parameters)
{
	const FString Path = FVitalsConfigLoader::ResolvePath();
	if (Path.IsEmpty())
	{
		AddWarning(TEXT("Plugin data dir not found, skipping file load test"));
		return true;
	}

	FVitalsConfig Config;
	const bool bLoaded = FVitalsConfigLoader::LoadFromFile(Path, Config);

	if (!bLoaded)
	{
		AddWarning(TEXT("vitals_config.json not found at resolved path, skipping"));
		return true;
	}

	TestTrue(TEXT("bLoadedFromJson after LoadFromFile"), Config.bLoadedFromJson);

	// Assert known committed JSON values (catches key casing/mapping mistakes)
	TestEqual(TEXT("Condition from JSON"), Config.Condition.Current, 75.f);
	TestEqual(TEXT("Calories from JSON"), Config.Condition.Survival.Calories.Current, 1100.f);
	TestEqual(TEXT("Fatigue from JSON"), Config.Condition.Survival.Fatigue.Current, 70.f);
	TestEqual(TEXT("Hydration from JSON"), Config.Condition.Survival.Hydration.Current, 1.5f);

	// Verify derived rates computed correctly
	// BaseRegenDays=4, Max=100: 100 / (4*86400) = ~0.000289
	TestTrue(TEXT("BaseRegenPerSec derived"),
		FMath::IsNearlyEqual(Config.Condition.BaseRegenPerSec, 100.f / (4.f * 86400.f), 0.000001f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
