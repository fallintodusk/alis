// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class PROJECTCORE_API FProjectSha256 final
{
public:
	static bool HashBuffer(const TArray<uint8>& Data, FString& OutHash);
	static bool HashFile(const FString& FilePath, FString& OutHash);
};
