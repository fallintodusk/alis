// Copyright ALIS. All Rights Reserved.

#include "VitalsConfig.h"
#include "ProjectPaths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogVitalsConfig, Log, All);

namespace
{
	void TryReadFloat(const TSharedPtr<FJsonObject>& Obj, const FString& Key, float& OutValue)
	{
		double Value;
		if (Obj.IsValid() && Obj->TryGetNumberField(Key, Value))
		{
			OutValue = static_cast<float>(Value);
		}
	}

	void ParseActivity(const TSharedPtr<FJsonObject>& Obj, FVitalsActivity& Out)
	{
		if (!Obj.IsValid()) return;

		TryReadFloat(Obj, TEXT("metIdle"), Out.MET_Idle);
		TryReadFloat(Obj, TEXT("metWalk"), Out.MET_Walk);
		TryReadFloat(Obj, TEXT("metJog"), Out.MET_Jog);
		TryReadFloat(Obj, TEXT("metSprint"), Out.MET_Sprint);
		TryReadFloat(Obj, TEXT("speedThresholdIdle"), Out.SpeedThreshold_Idle);
		TryReadFloat(Obj, TEXT("speedThresholdWalk"), Out.SpeedThreshold_Walk);
		TryReadFloat(Obj, TEXT("speedThresholdJog"), Out.SpeedThreshold_Jog);
	}

	void ParseMetabolism(const TSharedPtr<FJsonObject>& Obj, FVitalsMetabolism& Out)
	{
		if (!Obj.IsValid()) return;

		TryReadFloat(Obj, TEXT("characterWeightKg"), Out.CharacterWeightKg);
		TryReadFloat(Obj, TEXT("hydrationPerKcal"), Out.HydrationPerKcal);

		const TSharedPtr<FJsonObject>* ActivityObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("activity"), ActivityObj))
		{
			ParseActivity(*ActivityObj, Out.Activity);
		}
	}

	void ParseStatusEffect(const TSharedPtr<FJsonObject>& Obj, FVitalsStatusEffect& Out)
	{
		if (!Obj.IsValid()) return;
		TryReadFloat(Obj, TEXT("current"), Out.Current);
		TryReadFloat(Obj, TEXT("max"), Out.Max);
	}

	void ParseStatus(const TSharedPtr<FJsonObject>& Obj, FVitalsStatus& Out)
	{
		if (!Obj.IsValid()) return;

		const TSharedPtr<FJsonObject>* BleedObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("bleeding"), BleedObj))
		{
			ParseStatusEffect(*BleedObj, Out.Bleeding);
		}

		const TSharedPtr<FJsonObject>* PoisonObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("poisoned"), PoisonObj))
		{
			ParseStatusEffect(*PoisonObj, Out.Poisoned);
		}

		const TSharedPtr<FJsonObject>* RadObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("radiation"), RadObj))
		{
			ParseStatusEffect(*RadObj, Out.Radiation);
		}
	}

	void ParseSurvival(const TSharedPtr<FJsonObject>& Obj, FVitalsSurvival& Out)
	{
		if (!Obj.IsValid()) return;

		const TSharedPtr<FJsonObject>* CalObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("calories"), CalObj))
		{
			TryReadFloat(*CalObj, TEXT("current"), Out.Calories.Current);
			TryReadFloat(*CalObj, TEXT("max"), Out.Calories.Max);
			TryReadFloat(*CalObj, TEXT("conditionDeathDays"), Out.Calories.ConditionDeathDays);
		}

		const TSharedPtr<FJsonObject>* HydObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("hydration"), HydObj))
		{
			TryReadFloat(*HydObj, TEXT("current"), Out.Hydration.Current);
			TryReadFloat(*HydObj, TEXT("max"), Out.Hydration.Max);
			TryReadFloat(*HydObj, TEXT("conditionDeathDays"), Out.Hydration.ConditionDeathDays);
		}

		const TSharedPtr<FJsonObject>* FatObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("fatigue"), FatObj))
		{
			TryReadFloat(*FatObj, TEXT("current"), Out.Fatigue.Current);
			TryReadFloat(*FatObj, TEXT("max"), Out.Fatigue.Max);
			TryReadFloat(*FatObj, TEXT("hoursToEmpty"), Out.Fatigue.HoursToEmpty);
			TryReadFloat(*FatObj, TEXT("conditionDeathDays"), Out.Fatigue.ConditionDeathDays);
		}
	}

	void ParseCondition(const TSharedPtr<FJsonObject>& Obj, FVitalsConditionConfig& Out)
	{
		if (!Obj.IsValid()) return;

		TryReadFloat(Obj, TEXT("current"), Out.Current);
		TryReadFloat(Obj, TEXT("max"), Out.Max);
		TryReadFloat(Obj, TEXT("regenRate"), Out.RegenRate);
		TryReadFloat(Obj, TEXT("baseRegenDays"), Out.BaseRegenDays);

		const TSharedPtr<FJsonObject>* SurvObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("survival"), SurvObj))
		{
			ParseSurvival(*SurvObj, Out.Survival);
		}

		const TSharedPtr<FJsonObject>* StatusObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("status"), StatusObj))
		{
			ParseStatus(*StatusObj, Out.Status);
		}

		const TSharedPtr<FJsonObject>* MetabObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("metabolism"), MetabObj))
		{
			ParseMetabolism(*MetabObj, Out.Metabolism);
		}
	}

	void ParseStamina(const TSharedPtr<FJsonObject>& Obj, FVitalsStaminaConfig& Out)
	{
		if (!Obj.IsValid()) return;

		TryReadFloat(Obj, TEXT("current"), Out.Current);
		TryReadFloat(Obj, TEXT("max"), Out.Max);
		TryReadFloat(Obj, TEXT("regenRate"), Out.RegenRate);

		const TSharedPtr<FJsonObject>* MovObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("movement"), MovObj))
		{
			TryReadFloat(*MovObj, TEXT("sprintDrainPerSec"), Out.Movement.SprintDrainPerSec);
			TryReadFloat(*MovObj, TEXT("jogDrainPerSec"), Out.Movement.JogDrainPerSec);
			TryReadFloat(*MovObj, TEXT("baseRegenPerSec"), Out.Movement.BaseRegenPerSec);
		}

		const TSharedPtr<FJsonObject>* IntObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("interactions"), IntObj))
		{
			TryReadFloat(*IntObj, TEXT("light"), Out.Interactions.Light);
			TryReadFloat(*IntObj, TEXT("medium"), Out.Interactions.Medium);
			TryReadFloat(*IntObj, TEXT("heavy"), Out.Interactions.Heavy);
			TryReadFloat(*IntObj, TEXT("minRequired"), Out.Interactions.MinRequired);
		}

		const TSharedPtr<FJsonObject>* CombatObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("combat"), CombatObj))
		{
			TryReadFloat(*CombatObj, TEXT("lightAttack"), Out.Combat.LightAttack);
			TryReadFloat(*CombatObj, TEXT("heavyAttack"), Out.Combat.HeavyAttack);
			TryReadFloat(*CombatObj, TEXT("dodge"), Out.Combat.Dodge);
			TryReadFloat(*CombatObj, TEXT("blockPerSec"), Out.Combat.BlockPerSec);
		}
	}
}

// ---------------------------------------------------------------------------
// Cached singleton
// ---------------------------------------------------------------------------

const FVitalsConfig& FVitalsConfigLoader::Get()
{
	static FVitalsConfig CachedConfig;
	static bool bInitialized = false;

	if (!bInitialized)
	{
		bInitialized = true;
		const FString Path = ResolvePath();

		if (Path.IsEmpty())
		{
			UE_LOG(LogVitalsConfig, Warning,
				TEXT("[FVitalsConfigLoader::Get] Plugin data dir not found, using defaults"));
			CachedConfig.Condition.ComputeDerivedRates();
		}
		else
		{
			LoadFromFile(Path, CachedConfig);
		}

		UE_LOG(LogVitalsConfig, Log,
			TEXT("[FVitalsConfigLoader::Get] loaded=%s path=%s"),
			CachedConfig.bLoadedFromJson ? TEXT("true") : TEXT("false"),
			*Path);

		const FVitalsConditionConfig& C = CachedConfig.Condition;
		UE_LOG(LogVitalsConfig, Log,
			TEXT("[FVitalsConfigLoader] Derived rates: BaseRegen=%.6f/s Dehydration=%.6f/s Starvation=%.6f/s Exhaustion=%.6f/s FatigueGain=%.6f/s"),
			C.BaseRegenPerSec, C.DehydrationDrainPerSec, C.StarvationDrainPerSec,
			C.ExhaustionDrainPerSec, C.FatigueGainPerSec);
	}

	return CachedConfig;
}

FString FVitalsConfigLoader::ResolvePath()
{
	const FString DataDir = FProjectPaths::GetPluginDataDir(TEXT("ProjectVitals"));
	if (DataDir.IsEmpty())
	{
		return FString();
	}
	return DataDir / TEXT("vitals_config.json");
}

// ---------------------------------------------------------------------------
// File loader (per-field fallback to struct defaults)
// ---------------------------------------------------------------------------

bool FVitalsConfigLoader::LoadFromFile(const FString& FilePath, FVitalsConfig& OutConfig)
{
	OutConfig = FVitalsConfig();

	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogVitalsConfig, Log,
			TEXT("[FVitalsConfigLoader] No config at %s, using defaults"), *FilePath);
		OutConfig.Condition.ComputeDerivedRates();
		return false;
	}

	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *FilePath))
	{
		UE_LOG(LogVitalsConfig, Warning,
			TEXT("[FVitalsConfigLoader] Failed to read %s"), *FilePath);
		OutConfig.Condition.ComputeDerivedRates();
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogVitalsConfig, Warning,
			TEXT("[FVitalsConfigLoader] Failed to parse JSON: %s"), *FilePath);
		OutConfig.Condition.ComputeDerivedRates();
		return false;
	}

	const TSharedPtr<FJsonObject>* CondObj = nullptr;
	if (RootObject->TryGetObjectField(TEXT("condition"), CondObj))
	{
		ParseCondition(*CondObj, OutConfig.Condition);
	}

	const TSharedPtr<FJsonObject>* StamObj = nullptr;
	if (RootObject->TryGetObjectField(TEXT("stamina"), StamObj))
	{
		ParseStamina(*StamObj, OutConfig.Stamina);
	}

	OutConfig.Condition.ComputeDerivedRates();

	OutConfig.bLoadedFromJson = true;
	UE_LOG(LogVitalsConfig, Log,
		TEXT("[FVitalsConfigLoader] Loaded from %s"), *FilePath);
	return true;
}
