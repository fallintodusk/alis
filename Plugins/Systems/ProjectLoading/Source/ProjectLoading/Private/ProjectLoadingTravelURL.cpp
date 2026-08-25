// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectLoadingTravelURL.h"

#include "Engine/EngineBaseTypes.h"

FString ProjectLoadingTravelURL::Build(
	const FString& MapPath,
	const TMap<FString, FString>& CustomOptions)
{
	FString TravelURL = MapPath + TEXT("?") + ProvenanceOption;
	FString GameOptionValue;
	for (const TPair<FString, FString>& Option : CustomOptions)
	{
		if (Option.Key.Equals(TEXT("game"), ESearchCase::IgnoreCase))
		{
			GameOptionValue = Option.Value;
			continue;
		}
		if (Option.Key.Equals(TEXT("ProjectLoadingRoute"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		TravelURL += FString::Printf(TEXT("?%s=%s"), *Option.Key, *Option.Value);
	}
	if (!GameOptionValue.IsEmpty())
	{
		TravelURL += FString::Printf(TEXT("?game=%s"), *GameOptionValue);
	}
	return TravelURL;
}

bool ProjectLoadingTravelURL::HasProvenance(const FURL& URL)
{
	return FCString::Strcmp(URL.GetOption(TEXT("ProjectLoadingRoute="), TEXT("")), TEXT("1")) == 0;
}
