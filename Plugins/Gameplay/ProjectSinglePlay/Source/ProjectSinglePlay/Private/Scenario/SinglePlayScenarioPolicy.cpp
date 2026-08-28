// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Scenario/SinglePlayScenarioPolicy.h"

namespace
{
	constexpr TCHAR SupportedScenario[] = TEXT("UrbanSurvivalProofV1");
}

namespace FSinglePlayScenarioPolicy
{
	const TCHAR* OptionName()
	{
		return TEXT("Scenario");
	}

	FSinglePlayScenarioSelection Resolve(const FString& Options)
	{
		TArray<FString> Tokens;
		Options.ParseIntoArray(Tokens, TEXT("?"), true);
		for (const FString& Token : Tokens)
		{
			FString Key;
			FString Value;
			if (!Token.Split(TEXT("="), &Key, &Value) ||
				!Key.Equals(OptionName(), ESearchCase::CaseSensitive))
			{
				continue;
			}

			if (Value.Equals(SupportedScenario, ESearchCase::CaseSensitive))
			{
				return {FName(SupportedScenario), ESinglePlayScenarioParseResult::Selected};
			}
			return {NAME_None, ESinglePlayScenarioParseResult::Unknown};
		}
		return {};
	}
}
