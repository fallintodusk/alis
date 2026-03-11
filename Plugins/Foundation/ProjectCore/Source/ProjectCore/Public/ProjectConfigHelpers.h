// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Helpers for reading project configuration values with consistent defaults.
 */
class PROJECTCORE_API FProjectConfigHelpers final
{
public:
	static bool GetString(const FString& Section, const FString& Key, FString& OutValue, const FString& ConfigFilename = GGameIni);

	static bool GetBool(const FString& Section, const FString& Key, bool& bOutValue, const FString& ConfigFilename = GGameIni);

	static bool GetInt32(const FString& Section, const FString& Key, int32& OutValue, const FString& ConfigFilename = GGameIni);

	static bool GetFloat(const FString& Section, const FString& Key, float& OutValue, const FString& ConfigFilename = GGameIni);

	static bool GetArray(const FString& Section, const FString& Key, TArray<FString>& OutValues, const FString& ConfigFilename = GGameIni);

	static bool GetSection(const FString& Section, TMap<FString, FString>& OutValues, const FString& ConfigFilename = GGameIni);
};
