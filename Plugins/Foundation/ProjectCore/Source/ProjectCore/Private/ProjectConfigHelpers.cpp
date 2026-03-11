#include "ProjectConfigHelpers.h"

#include "Misc/ConfigCacheIni.h"

bool FProjectConfigHelpers::GetString(const FString& Section, const FString& Key, FString& OutValue, const FString& ConfigFilename)
{
	if (!GConfig)
	{
		return false;
	}

	return GConfig->GetString(*Section, *Key, OutValue, ConfigFilename);
}

bool FProjectConfigHelpers::GetBool(const FString& Section, const FString& Key, bool& bOutValue, const FString& ConfigFilename)
{
	if (!GConfig)
	{
		return false;
	}

	return GConfig->GetBool(*Section, *Key, bOutValue, ConfigFilename);
}

bool FProjectConfigHelpers::GetInt32(const FString& Section, const FString& Key, int32& OutValue, const FString& ConfigFilename)
{
	if (!GConfig)
	{
		return false;
	}

	return GConfig->GetInt(*Section, *Key, OutValue, ConfigFilename);
}

bool FProjectConfigHelpers::GetFloat(const FString& Section, const FString& Key, float& OutValue, const FString& ConfigFilename)
{
	if (!GConfig)
	{
		return false;
	}

	return GConfig->GetFloat(*Section, *Key, OutValue, ConfigFilename);
}

bool FProjectConfigHelpers::GetArray(const FString& Section, const FString& Key, TArray<FString>& OutValues, const FString& ConfigFilename)
{
	if (!GConfig)
	{
		return false;
	}

	OutValues.Reset();
	const int32 NumValues = GConfig->GetArray(*Section, *Key, OutValues, ConfigFilename);
	return NumValues > 0;
}

bool FProjectConfigHelpers::GetSection(const FString& Section, TMap<FString, FString>& OutValues, const FString& ConfigFilename)
{
	if (!GConfig)
	{
		return false;
	}

	const FConfigSection* SectionData = GConfig->GetSection(*Section, /*Force=*/false, ConfigFilename);
	if (!SectionData)
	{
		return false;
	}

	OutValues.Reset();
	for (const TPair<FName, FConfigValue>& Pair : *SectionData)
	{
		OutValues.Add(Pair.Key.ToString(), Pair.Value.GetValue());
	}

	return true;
}
