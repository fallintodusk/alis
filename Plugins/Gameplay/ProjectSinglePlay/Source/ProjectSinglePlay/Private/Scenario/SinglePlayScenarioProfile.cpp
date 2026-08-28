// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Scenario/SinglePlayScenarioProfile.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "ProjectPaths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr TCHAR CanonicalSchema[] = TEXT("../Schemas/single_play_scenario.schema.json");
	constexpr double SupportedSchemaVersion = 2.0;

	bool IsAsciiAlpha(TCHAR Character)
	{
		return (Character >= TEXT('A') && Character <= TEXT('Z')) ||
			(Character >= TEXT('a') && Character <= TEXT('z'));
	}

	bool IsScenarioIdentifier(const FString& Value)
	{
		if (Value.IsEmpty() || !IsAsciiAlpha(Value[0]))
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			const bool bAsciiDigit = Character >= TEXT('0') && Character <= TEXT('9');
			if (!IsAsciiAlpha(Character) && !bAsciiDigit && Character != TEXT('_'))
			{
				return false;
			}
		}
		return true;
	}

	bool ValidateScenarioId(FName ScenarioId, FString& OutId, FString& OutError)
	{
		OutId = ScenarioId.ToString();
		if (!IsScenarioIdentifier(OutId))
		{
			OutError = TEXT("Scenario ID must match the safe owner-local grammar [A-Za-z][A-Za-z0-9_]*.");
			return false;
		}
		return true;
	}

	bool ValidateRootFields(const TSharedPtr<FJsonObject>& Object, FString& OutError)
	{
		static const TSet<FString> AllowedFields = {
			TEXT("$schema"), TEXT("schemaVersion"), TEXT("scenarioId"), TEXT("steps"),
			TEXT("cacheActorTag"), TEXT("shelterActorTag"), TEXT("requiredRationObjectId"),
			TEXT("requiredHydrationFraction"), TEXT("cacheInteractionRadius"),
			TEXT("shelterInteractionRadius"), TEXT("failureRestartDelaySeconds"),
			TEXT("startMessage"), TEXT("recoveryMessage"), TEXT("recoveredMessage"), TEXT("successMessage"),
			TEXT("failureMessage")};
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
		{
			if (!AllowedFields.Contains(Field.Key))
			{
				OutError = FString::Printf(TEXT("Unknown scenario profile field '%s'."), *Field.Key);
				return false;
			}
		}
		return true;
	}

	bool ReadRequiredString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FString& OutValue,
		FString& OutError)
	{
		if (!Object.IsValid() || !Object->TryGetStringField(Field, OutValue) || OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Missing required string field '%s'."), Field);
			return false;
		}
		return true;
	}

	bool ReadRequiredNumber(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		double& OutValue,
		FString& OutError)
	{
		if (!Object.IsValid() || !Object->TryGetNumberField(Field, OutValue))
		{
			OutError = FString::Printf(TEXT("Missing required number field '%s'."), Field);
			return false;
		}
		return true;
	}
}

namespace FSinglePlayScenarioProfileLoader
{
	bool Parse(
		FName ScenarioId,
		const FString& Json,
		FSinglePlayScenarioProfile& OutProfile,
		FString& OutError)
	{
		OutProfile = {};
		OutError.Reset();
		FString Id;
		if (!ValidateScenarioId(ScenarioId, Id, OutError))
		{
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("Scenario profile is not valid JSON.");
			return false;
		}
		if (!ValidateRootFields(Root, OutError))
		{
			return false;
		}

		FString Schema;
		double SchemaVersion = 0.0;
		FString ParsedId;
		FString CacheTag;
		FString ShelterTag;
		FString RequiredItem;
		if (!ReadRequiredString(Root, TEXT("$schema"), Schema, OutError) ||
			!ReadRequiredNumber(Root, TEXT("schemaVersion"), SchemaVersion, OutError) ||
			!ReadRequiredString(Root, TEXT("scenarioId"), ParsedId, OutError) ||
			!ReadRequiredString(Root, TEXT("cacheActorTag"), CacheTag, OutError) ||
			!ReadRequiredString(Root, TEXT("shelterActorTag"), ShelterTag, OutError) ||
			!ReadRequiredString(Root, TEXT("requiredRationObjectId"), RequiredItem, OutError) ||
			!ReadRequiredNumber(Root, TEXT("requiredHydrationFraction"), OutProfile.RequiredHydrationFraction, OutError) ||
			!ReadRequiredNumber(Root, TEXT("cacheInteractionRadius"), OutProfile.CacheInteractionRadius, OutError) ||
			!ReadRequiredNumber(Root, TEXT("shelterInteractionRadius"), OutProfile.ShelterInteractionRadius, OutError) ||
			!ReadRequiredNumber(Root, TEXT("failureRestartDelaySeconds"), OutProfile.FailureRestartDelaySeconds, OutError) ||
			!ReadRequiredString(Root, TEXT("startMessage"), OutProfile.StartMessage, OutError) ||
			!ReadRequiredString(Root, TEXT("recoveryMessage"), OutProfile.RecoveryMessage, OutError) ||
			!ReadRequiredString(Root, TEXT("recoveredMessage"), OutProfile.RecoveredMessage, OutError) ||
			!ReadRequiredString(Root, TEXT("successMessage"), OutProfile.SuccessMessage, OutError) ||
			!ReadRequiredString(Root, TEXT("failureMessage"), OutProfile.FailureMessage, OutError))
		{
			return false;
		}
		if (!Schema.Equals(CanonicalSchema, ESearchCase::CaseSensitive) ||
			SchemaVersion != SupportedSchemaVersion)
		{
			OutError = TEXT("Scenario profile schema identity or version is unsupported.");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
		if (!Root->TryGetArrayField(TEXT("steps"), Steps) || Steps == nullptr || Steps->Num() != 3 ||
			(*Steps)[0]->AsString() != TEXT("ReachCache") ||
			(*Steps)[1]->AsString() != TEXT("RecoverHydrationAndCarryItem") ||
			(*Steps)[2]->AsString() != TEXT("ReachShelter"))
		{
			OutError = TEXT("Scenario steps must be the supported three-step sequence.");
			return false;
		}

		if (!ParsedId.Equals(Id, ESearchCase::CaseSensitive) ||
			OutProfile.RequiredHydrationFraction < 0.20 ||
			OutProfile.RequiredHydrationFraction > 1.0 ||
			OutProfile.CacheInteractionRadius <= 0.0 ||
			OutProfile.ShelterInteractionRadius <= 0.0 ||
			OutProfile.FailureRestartDelaySeconds < 2.0 ||
			OutProfile.FailureRestartDelaySeconds > 3.0)
		{
			OutError = TEXT("Scenario profile values violate the supported contract.");
			return false;
		}

		OutProfile.ScenarioId = FName(*ParsedId);
		OutProfile.CacheActorTag = FName(*CacheTag);
		OutProfile.ShelterActorTag = FName(*ShelterTag);
		OutProfile.RequiredRationObjectId = FName(*RequiredItem);
		return true;
	}

#if WITH_DEV_AUTOMATION_TESTS
	bool ParseForTests(
		FName ScenarioId,
		const FString& Json,
		FSinglePlayScenarioProfile& OutProfile,
		FString& OutError)
	{
		return Parse(ScenarioId, Json, OutProfile, OutError);
	}
#endif

	bool Load(FName ScenarioId, FSinglePlayScenarioProfile& OutProfile, FString& OutError)
	{
		OutProfile = {};
		OutError.Reset();
		FString Id;
		if (!ValidateScenarioId(ScenarioId, Id, OutError))
		{
			return false;
		}
		const FString Path = FProjectPaths::GetPluginDataDir(TEXT("ProjectSinglePlay")) /
			TEXT("Scenarios") / (Id + TEXT(".json"));
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			OutError = FString::Printf(TEXT("Failed to read scenario profile '%s'."), *Path);
			return false;
		}
		return Parse(ScenarioId, Json, OutProfile, OutError);
	}
}
