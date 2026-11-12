#pragma once
#include "CoreMinimal.h"

class FJsonObject;
namespace ProjectWorldAuthoredTerrainParsing
{
	using FRetainedInputs = TMap<FString,FString>;
	bool ReadInput(const TSharedPtr<FJsonObject>& Operation,const TSharedPtr<FJsonObject>& Input,
		const FString& Hash,FRetainedInputs& Out);
	bool ReadTerrain(const TSharedPtr<FJsonObject>& Object,const FString& Grid,const FRetainedInputs& Inputs);
	bool ValidateUsage(const FRetainedInputs& Inputs,const FString& Grid,const TSet<FString>& Used);
}
